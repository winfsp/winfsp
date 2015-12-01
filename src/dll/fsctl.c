/**
 * @file dll/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>
#if !defined(NDEBUG)
#include <sddl.h>
#endif

#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"

static inline VOID GlobalDevicePath(PWCHAR DevicePathBuf, SIZE_T DevicePathSize, PWSTR DevicePath)
{
    StringCbPrintfW(DevicePathBuf, DevicePathSize,
        L'\\' == DevicePath[0] ? GLOBALROOT "%s" : GLOBALROOT "\\Device\\%s", DevicePath);
}

FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *Params, PSECURITY_DESCRIPTOR SecurityDescriptor,
    PWCHAR VolumePathBuf, SIZE_T VolumePathSize)
{
    NTSTATUS Result = STATUS_SUCCESS;
    FSP_FSCTL_VOLUME_PARAMS *ParamsBuf = 0;
    HANDLE Token = 0;
    PTOKEN_DEFAULT_DACL DefaultDacl = 0;
    SECURITY_DESCRIPTOR SecurityDescriptorStruct;
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf;
    DWORD SecurityDescriptorSize, Bytes;
    WCHAR DevicePathBuf[MAX_PATH];
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;

    VolumePathBuf[0] = L'\0';

    if (0 == SecurityDescriptor)
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        Bytes = 0;
        if (!GetTokenInformation(Token, TokenDefaultDacl, 0, 0, &Bytes) &&
            ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        DefaultDacl = malloc(Bytes);
        if (0 == DefaultDacl)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
        if (!GetTokenInformation(Token, TokenDefaultDacl, DefaultDacl, Bytes, &Bytes) ||
            !InitializeSecurityDescriptor(&SecurityDescriptorStruct, SECURITY_DESCRIPTOR_REVISION) ||
            !SetSecurityDescriptorDacl(&SecurityDescriptorStruct, TRUE, DefaultDacl->DefaultDacl, FALSE))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        SecurityDescriptor = &SecurityDescriptorStruct;
        CloseHandle(Token);
        Token = 0;
    }

    SecurityDescriptorSize = 0;
    if (!MakeSelfRelativeSD(SecurityDescriptor, 0, &SecurityDescriptorSize) &&
        ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    ParamsBuf = malloc(FSP_FSCTL_VOLUME_PARAMS_SIZE + SecurityDescriptorSize);
    if (0 == ParamsBuf)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memset(ParamsBuf, 0, FSP_FSCTL_VOLUME_PARAMS_SIZE);
    SecurityDescriptorBuf = (PVOID)((PUINT8)ParamsBuf + FSP_FSCTL_VOLUME_PARAMS_SIZE);
    if (!MakeSelfRelativeSD(SecurityDescriptor, SecurityDescriptorBuf, &SecurityDescriptorSize))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    *ParamsBuf = *Params;

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, DevicePath);

#if !defined(NDEBUG)
    {
        PSTR Sddl;
        if (ConvertSecurityDescriptorToStringSecurityDescriptorA(SecurityDescriptorBuf,
            SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &Sddl, 0))
        {
            DEBUGLOG("Device=\"%S\", Sddl=\"%s\", SdSize=%lu",
                DevicePathBuf, Sddl, SecurityDescriptorSize);
            LocalFree(Sddl);
        }
    }
#endif

    DeviceHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == DeviceHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_NO_SUCH_DEVICE;
        goto exit;
    }

    if (!DeviceIoControl(DeviceHandle, FSP_FSCTL_CREATE,
        ParamsBuf, FSP_FSCTL_VOLUME_PARAMS_SIZE + SecurityDescriptorSize,
        VolumePathBuf, (DWORD)VolumePathSize,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

exit:

    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);
    if (0 != Token)
        CloseHandle(Token);

    free(DefaultDacl);
    free(ParamsBuf);

    return Result;
}

FSP_API NTSTATUS FspFsctlOpenVolume(PWSTR VolumePath,
    PHANDLE PVolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[MAX_PATH];
    HANDLE VolumeHandle;

    *PVolumeHandle = 0;

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, VolumePath);

    DEBUGLOG("Device=\"%S\"", DevicePathBuf);

    VolumeHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == VolumeHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        if (STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_NO_SUCH_DEVICE;
        goto exit;
    }

    *PVolumeHandle = VolumeHandle;

exit:
    return Result;
}

FSP_API NTSTATUS FspFsctlDeleteVolume(HANDLE VolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    DWORD Bytes;

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_DELETE,
        0, 0, 0, 0,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

exit:
    return Result;
}

FSP_API NTSTATUS FspFsctlTransact(HANDLE VolumeHandle,
    FSP_FSCTL_TRANSACT_RSP *ResponseBuf, SIZE_T ResponseBufSize,
    FSP_FSCTL_TRANSACT_REQ *RequestBuf, SIZE_T *PRequestBufSize)
{
    NTSTATUS Result = STATUS_SUCCESS;
    DWORD Bytes;

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_TRANSACT,
        ResponseBuf, (DWORD)ResponseBufSize, RequestBuf, (DWORD)*PRequestBufSize,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *PRequestBufSize = Bytes;

exit:
    return Result;
}
