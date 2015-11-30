/**
 * @file dll/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <winfsp/winfsp.h>
#include <winfsp/fsctl.h>
#include <strsafe.h>

#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"

static inline VOID GlobalDevicePath(PWCHAR DevicePathBuf, SIZE_T DevicePathSize, PWSTR DevicePath)
{
    StringCbPrintf(DevicePathBuf, DevicePathSize,
        L'\\' == DevicePath[0] ? GLOBALROOT "%S" : GLOBALROOT "\\Device\\%S", DevicePath);
}

FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *Params, PSECURITY_DESCRIPTOR SecurityDescriptor,
    PWCHAR VolumePathBuf, SIZE_T VolumePathSize)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[MAX_PATH];
    FSP_FSCTL_VOLUME_PARAMS *ParamsBuf;
    HANDLE Token;
    PVOID DaclBuf = 0;
    SECURITY_DESCRIPTOR SecurityDescriptorStruct;
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf = 0;
    DWORD SecurityDescriptorSize, Bytes;
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;

    VolumePathBuf[0] = L'\0';

    if (0 == SecurityDescriptor)
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        GetTokenInformation(Token, TokenDefaultDacl, 0, 0, &Bytes);
        DaclBuf = malloc(Bytes);
        if (0 == DaclBuf)
        {
            CloseHandle(Token);
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
        if (GetTokenInformation(Token, TokenDefaultDacl, DaclBuf, Bytes, &Bytes) &&
            InitializeSecurityDescriptor(&SecurityDescriptorStruct, SECURITY_DESCRIPTOR_REVISION) &&
            SetSecurityDescriptorDacl(&SecurityDescriptorStruct, TRUE, DaclBuf, FALSE))
        {
            SecurityDescriptor = &SecurityDescriptorStruct;
            CloseHandle(Token);
        }
        else
        {
            Result = FspNtStatusFromWin32(GetLastError());
            CloseHandle(Token);
            goto exit;
        }
    }

    SecurityDescriptorSize = GetSecurityDescriptorLength(SecurityDescriptor);
    ParamsBuf = malloc(sizeof *ParamsBuf + SecurityDescriptorSize);
    if (0 == ParamsBuf)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    SecurityDescriptorBuf = (PVOID)(ParamsBuf + 1);
    if (!MakeSelfRelativeSD(SecurityDescriptor, SecurityDescriptorBuf, &SecurityDescriptorSize))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    *ParamsBuf = *Params;

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, DevicePath);
    DeviceHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == DeviceHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (!DeviceIoControl(DeviceHandle, FSP_FSCTL_CREATE,
        ParamsBuf, sizeof *ParamsBuf + SecurityDescriptorSize, VolumePathBuf, (DWORD)VolumePathSize,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

exit:
    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);
    free(SecurityDescriptorBuf);
    free(DaclBuf);
    return Result;
}

FSP_API NTSTATUS FspFsctlOpenVolume(PWSTR VolumePath,
    PHANDLE *PVolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[MAX_PATH];
    HANDLE VolumeHandle;

    *PVolumeHandle = 0;

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, VolumePath);

    VolumeHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == VolumeHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
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
