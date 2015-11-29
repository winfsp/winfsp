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
    PHANDLE *PVolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[(sizeof GLOBALROOT + FSP_FSCTL_CREATE_BUFFER_SIZE) / sizeof(WCHAR)];
    WCHAR VolumePathBuf[FSP_FSCTL_CREATE_BUFFER_SIZE / sizeof(WCHAR)];
    FSP_FSCTL_VOLUME_PARAMS *ParamsBuf;
    PSECURITY_DESCRIPTOR SecurityDescriptorBuf = 0;
    DWORD SecurityDescriptorSize, Bytes;
    HANDLE DeviceHandle = INVALID_HANDLE_VALUE;

    *PVolumeHandle = 0;

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, DevicePath);

    SecurityDescriptorSize = 0;
    if (!MakeSelfRelativeSD(SecurityDescriptor, 0, &SecurityDescriptorSize))
    {
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
    }

    DeviceHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == DeviceHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (!DeviceIoControl(DeviceHandle, FSP_FSCTL_CREATE,
        ParamsBuf, sizeof *ParamsBuf + SecurityDescriptorSize, VolumePathBuf, sizeof VolumePathBuf,
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

FSP_API NTSTATUS FspFsctlOpenVolume(PWSTR VolumePath,
    PHANDLE *PVolumeHandle)
{
    NTSTATUS Result = STATUS_SUCCESS;
    WCHAR DevicePathBuf[(sizeof GLOBALROOT + FSP_FSCTL_CREATE_BUFFER_SIZE) / sizeof(WCHAR)];
    HANDLE VolumeHandle;

    *PVolumeHandle = 0;

    GlobalDevicePath(DevicePathBuf, sizeof DevicePathBuf, VolumePath);

    VolumeHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
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
