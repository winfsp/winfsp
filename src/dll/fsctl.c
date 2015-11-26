/**
 * @file dll/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <winfsp/winfsp.h>
#include <winfsp/fsctl.h>
#include <strsafe.h>

#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"

NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath, PSECURITY_DESCRIPTOR SecurityDescriptor,
    PHANDLE *PVolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[(sizeof GLOBALROOT + FSP_FSCTL_CREATE_BUFFER_SIZE) / sizeof(WCHAR)];
    WCHAR VolumePathBuf[FSP_FSCTL_CREATE_BUFFER_SIZE / sizeof(WCHAR)];
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf = 0;
    DWORD SecurityDescriptorLen, Bytes;
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;

    *PVolumeHandle = 0;

    DevicePathBuf[0] = L'\0';
    StringCbPrintf(DevicePathBuf, sizeof DevicePathBuf, GLOBALROOT "%S", DevicePath);

    if (!MakeSelfRelativeSD(SecurityDescriptor, 0, &SecurityDescriptorLen))
    {
        SecurityDescriptorBuf = malloc(SecurityDescriptorLen);
        if (0 == SecurityDescriptorBuf)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
        if (!MakeSelfRelativeSD(SecurityDescriptor, SecurityDescriptorBuf, &SecurityDescriptorLen))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    DeviceHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_WRITE | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == DeviceHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (!DeviceIoControl(DeviceHandle, FSP_FSCTL_CREATE,
        SecurityDescriptorBuf, SecurityDescriptorLen, VolumePathBuf, sizeof VolumePathBuf,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = FspFsctlOpenVolume(VolumePathBuf, PVolumeHandle);

exit:
    if (INVALID_HANDLE_VALUE != DeviceHandle)
        CloseHandle(DeviceHandle);
    free(SecurityDescriptorBuf);
    return Result;
}

NTSTATUS FspFsctlOpenVolume(PWSTR VolumePath,
    PHANDLE *PVolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[(sizeof GLOBALROOT + FSP_FSCTL_CREATE_BUFFER_SIZE) / sizeof(WCHAR)];
    HANDLE VolumeHandle;

    *PVolumeHandle = 0;

    DevicePathBuf[0] = L'\0';
    StringCbPrintf(DevicePathBuf, sizeof DevicePathBuf, GLOBALROOT "%S", VolumePath);

    VolumeHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_WRITE | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == VolumeHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *PVolumeHandle = VolumeHandle;

exit:
    return Result;
}

NTSTATUS FspFsctlDeleteVolume(HANDLE VolumeHandle)
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

NTSTATUS FspFsctlTransact(HANDLE VolumeHandle,
    const FSP_TRANSACT_RSP *Responses, size_t NumResponses,
    const FSP_TRANSACT_REQ *Requests, size_t *NumRequests)
{
    return STATUS_NOT_IMPLEMENTED;
}
