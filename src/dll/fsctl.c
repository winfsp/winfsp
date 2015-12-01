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

static NTSTATUS CreateSelfRelativeSecurityDescriptor(PSECURITY_DESCRIPTOR SecurityDescriptor,
    PSECURITY_DESCRIPTOR *PSelfRelativeSecurityDescriptor, PDWORD PSelfRelativeSecurityDescriptorSize)
{
    NTSTATUS Result;
    PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor = 0;
    DWORD SelfRelativeSecurityDescriptorSize;
    SECURITY_DESCRIPTOR SecurityDescriptorStruct;
    PTOKEN_OWNER Owner = 0;
    PTOKEN_PRIMARY_GROUP PrimaryGroup = 0;
    PTOKEN_DEFAULT_DACL DefaultDacl = 0;
    DWORD OwnerSize = 0;
    DWORD PrimaryGroupSize = 0;
    DWORD DefaultDaclSize = 0;
    HANDLE Token = 0;

    *PSelfRelativeSecurityDescriptor = 0;
    *PSelfRelativeSecurityDescriptorSize = 0;

    if (0 == SecurityDescriptor)
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        if ((!GetTokenInformation(Token, TokenOwner, 0, 0, &OwnerSize) &&
                ERROR_INSUFFICIENT_BUFFER != GetLastError()) ||
            (!GetTokenInformation(Token, TokenPrimaryGroup, 0, 0, &PrimaryGroupSize) &&
                ERROR_INSUFFICIENT_BUFFER != GetLastError()) ||
            (!GetTokenInformation(Token, TokenDefaultDacl, 0, 0, &DefaultDaclSize) &&
                ERROR_INSUFFICIENT_BUFFER != GetLastError()))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        Owner = malloc(OwnerSize);
        PrimaryGroup = malloc(PrimaryGroupSize);
        DefaultDacl = malloc(DefaultDaclSize);
        if (0 == Owner || 0 == PrimaryGroup || 0 == DefaultDacl)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
        if (!GetTokenInformation(Token, TokenOwner, Owner, OwnerSize, &OwnerSize) ||
            !GetTokenInformation(Token, TokenPrimaryGroup, PrimaryGroup, PrimaryGroupSize, &PrimaryGroupSize) ||
            !GetTokenInformation(Token, TokenDefaultDacl, DefaultDacl, DefaultDaclSize, &DefaultDaclSize))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        if (!InitializeSecurityDescriptor(&SecurityDescriptorStruct, SECURITY_DESCRIPTOR_REVISION) ||
            !SetSecurityDescriptorOwner(&SecurityDescriptorStruct, Owner->Owner, FALSE) ||
            !SetSecurityDescriptorGroup(&SecurityDescriptorStruct, PrimaryGroup->PrimaryGroup, FALSE) ||
            !SetSecurityDescriptorDacl(&SecurityDescriptorStruct, TRUE, DefaultDacl->DefaultDacl, FALSE) ||
            !SetSecurityDescriptorControl(&SecurityDescriptorStruct, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        SecurityDescriptor = &SecurityDescriptorStruct;
    }

    SelfRelativeSecurityDescriptorSize = 0;
    if (!MakeSelfRelativeSD(SecurityDescriptor, 0, &SelfRelativeSecurityDescriptorSize) &&
        ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    SelfRelativeSecurityDescriptor = malloc(SelfRelativeSecurityDescriptorSize);
    if (0 == SelfRelativeSecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    if (!MakeSelfRelativeSD(SecurityDescriptor, SelfRelativeSecurityDescriptor, &SelfRelativeSecurityDescriptorSize))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *PSelfRelativeSecurityDescriptor = SelfRelativeSecurityDescriptor;
    *PSelfRelativeSecurityDescriptorSize = SelfRelativeSecurityDescriptorSize;
    Result = STATUS_SUCCESS;

exit:
    if (0 != Token)
        CloseHandle(Token);

    free(DefaultDacl);
    free(PrimaryGroup);
    free(Owner);

    if (STATUS_SUCCESS != Result)
        free(SelfRelativeSecurityDescriptor);

    return Result;
}

FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *Params, PSECURITY_DESCRIPTOR SecurityDescriptor,
    PWCHAR VolumePathBuf, SIZE_T VolumePathSize)
{
    NTSTATUS Result = STATUS_SUCCESS;
    PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor = 0;
    DWORD SelfRelativeSecurityDescriptorSize;
    FSP_FSCTL_VOLUME_PARAMS *ParamsBuf = 0;
    WCHAR DevicePathBuf[MAX_PATH];
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;

    if (sizeof(WCHAR) <= VolumePathSize)
        VolumePathBuf[0] = L'\0';

    Result = CreateSelfRelativeSecurityDescriptor(SecurityDescriptor,
        &SelfRelativeSecurityDescriptor, &SelfRelativeSecurityDescriptorSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    ParamsBuf = malloc(FSP_FSCTL_VOLUME_PARAMS_SIZE + SelfRelativeSecurityDescriptorSize);
    if (0 == ParamsBuf)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memset(ParamsBuf, 0, FSP_FSCTL_VOLUME_PARAMS_SIZE);
    *ParamsBuf = *Params;
    memcpy((PUINT8)ParamsBuf + FSP_FSCTL_VOLUME_PARAMS_SIZE,
        SelfRelativeSecurityDescriptor, SelfRelativeSecurityDescriptorSize);

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, DevicePath);

#if !defined(NDEBUG)
    {
        PSTR Sddl;
        if (ConvertSecurityDescriptorToStringSecurityDescriptorA(SelfRelativeSecurityDescriptor,
            SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            &Sddl, 0))
        {
            DEBUGLOG("Device=\"%S\", Sddl=\"%s\", SdSize=%lu",
                DevicePathBuf, Sddl, SelfRelativeSecurityDescriptorSize);
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
        ParamsBuf, FSP_FSCTL_VOLUME_PARAMS_SIZE + SelfRelativeSecurityDescriptorSize,
        VolumePathBuf, (DWORD)VolumePathSize,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

exit:

    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);

    free(ParamsBuf);
    free(SelfRelativeSecurityDescriptor);

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
