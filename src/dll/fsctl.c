/**
 * @file dll/fsctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/library.h>

#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"
#define PREFIXW                         L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX
#define PREFIXW_SIZE                    (sizeof PREFIXW - sizeof(WCHAR))

NTSTATUS FspFsctlStartService(VOID)
{
    PWSTR DriverName = L"" FSP_FSCTL_DRIVER_NAME;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    SERVICE_STATUS ServiceStatus;
    DWORD LastError;
    NTSTATUS Result;

    ScmHandle = OpenSCManagerW(0, 0, 0);
    if (0 == ScmHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    SvcHandle = OpenServiceW(ScmHandle, DriverName, SERVICE_QUERY_STATUS | SERVICE_START);
    if (0 == SvcHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (!StartServiceW(SvcHandle, 0, 0))
    {
        LastError = GetLastError();
        if (ERROR_SERVICE_ALREADY_RUNNING != LastError)
            Result = FspNtStatusFromWin32(LastError);
        else
            Result = STATUS_SUCCESS;
        goto exit;
    }

    /* Poll until the service is running! Yes, that's the best we can do! */
    Result = STATUS_DRIVER_UNABLE_TO_LOAD;
    for (DWORD Timeout = 500, N = 20, I = 0; N > I; I++)
        /* wait up to 500ms * 20 = 10000ms = 10s */
    {
        if (!QueryServiceStatus(SvcHandle, &ServiceStatus))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        if (SERVICE_RUNNING == ServiceStatus.dwCurrentState)
        {
            Result = STATUS_SUCCESS;
            break;
        }

        Sleep(Timeout);
    }

exit:
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    return Result;
}

FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    PWCHAR VolumeNameBuf, SIZE_T VolumeNameSize,
    PHANDLE PVolumeHandle)
{
    NTSTATUS Result;
    PWSTR DeviceRoot;
    SIZE_T DeviceRootSize, DevicePathSize;
    WCHAR DevicePathBuf[MAX_PATH], *DevicePathPtr, *DevicePathEnd;
    HANDLE VolumeHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;

    if (sizeof(WCHAR) <= VolumeNameSize)
        VolumeNameBuf[0] = L'\0';
    *PVolumeHandle = INVALID_HANDLE_VALUE;

    /* check lengths; everything (including encoded volume params) must fit within MAX_PATH */
    DeviceRoot = L'\\' == DevicePath[0] ? GLOBALROOT : GLOBALROOT "\\Device\\";
    DeviceRootSize = lstrlenW(DeviceRoot) * sizeof(WCHAR);
    DevicePathSize = lstrlenW(DevicePath) * sizeof(WCHAR);
    if (DeviceRootSize + DevicePathSize + PREFIXW_SIZE +
        sizeof *VolumeParams * sizeof(WCHAR) + sizeof(WCHAR) > sizeof DevicePathBuf)
        return STATUS_INVALID_PARAMETER;

    /* prepare the device path to be opened; encode the volume params in the Unicode private area */
    DevicePathPtr = DevicePathBuf;
    memcpy(DevicePathPtr, DeviceRoot, DeviceRootSize);
    DevicePathPtr = (PVOID)((PUINT8)DevicePathPtr + DeviceRootSize);
    memcpy(DevicePathPtr, DevicePath, DevicePathSize);
    DevicePathPtr = (PVOID)((PUINT8)DevicePathPtr + DevicePathSize);
    memcpy(DevicePathPtr, PREFIXW, PREFIXW_SIZE);
    DevicePathPtr = (PVOID)((PUINT8)DevicePathPtr + PREFIXW_SIZE);
    DevicePathEnd = (PVOID)((PUINT8)DevicePathPtr + sizeof *VolumeParams * sizeof(WCHAR));
    for (PUINT8 VolumeParamsPtr = (PVOID)VolumeParams;
        DevicePathEnd > DevicePathPtr; DevicePathPtr++, VolumeParamsPtr++)
    {
        WCHAR Value = 0xF000 | *VolumeParamsPtr;
        *DevicePathPtr = Value;
    }
    *DevicePathPtr = L'\0';

    Result = FspFsctlStartService();
    if (!NT_SUCCESS(Result))
        return Result;

    VolumeHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if (INVALID_HANDLE_VALUE == VolumeHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        if (STATUS_OBJECT_PATH_NOT_FOUND == Result ||
            STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_NO_SUCH_DEVICE;
        goto exit;
    }

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_VOLUME_NAME,
        0, 0,
        VolumeNameBuf, (DWORD)VolumeNameSize,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    if (NT_SUCCESS(Result))
        *PVolumeHandle = VolumeHandle;
    else
        CloseHandle(VolumeHandle);

    return Result;
}

FSP_API NTSTATUS FspFsctlTransact(HANDLE VolumeHandle,
    PVOID ResponseBuf, SIZE_T ResponseBufSize,
    PVOID RequestBuf, SIZE_T *PRequestBufSize,
    BOOLEAN Batch)
{
    NTSTATUS Result = STATUS_SUCCESS;
    DWORD Bytes = 0;

    if (0 != PRequestBufSize)
    {
        Bytes = (DWORD)*PRequestBufSize;
        *PRequestBufSize = 0;
    }

    if (!DeviceIoControl(VolumeHandle,
        Batch ? FSP_FSCTL_TRANSACT_BATCH : FSP_FSCTL_TRANSACT,
        ResponseBuf, (DWORD)ResponseBufSize, RequestBuf, Bytes,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (0 != PRequestBufSize)
        *PRequestBufSize = Bytes;

exit:
    return Result;
}

FSP_API NTSTATUS FspFsctlStop(HANDLE VolumeHandle)
{
    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_STOP, 0, 0, 0, 0, 0, 0))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFsctlGetVolumeList(PWSTR DevicePath,
    PWCHAR VolumeListBuf, PSIZE_T PVolumeListSize)
{
    NTSTATUS Result;
    PWSTR DeviceRoot;
    SIZE_T DeviceRootSize, DevicePathSize;
    WCHAR DevicePathBuf[MAX_PATH], *DevicePathPtr;
    HANDLE VolumeHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;

    /* check lengths; everything must fit within MAX_PATH */
    DeviceRoot = L'\\' == DevicePath[0] ? GLOBALROOT : GLOBALROOT "\\Device\\";
    DeviceRootSize = lstrlenW(DeviceRoot) * sizeof(WCHAR);
    DevicePathSize = lstrlenW(DevicePath) * sizeof(WCHAR);
    if (DeviceRootSize + DevicePathSize + sizeof(WCHAR) > sizeof DevicePathBuf)
        return STATUS_INVALID_PARAMETER;

    /* prepare the device path to be opened */
    DevicePathPtr = DevicePathBuf;
    memcpy(DevicePathPtr, DeviceRoot, DeviceRootSize);
    DevicePathPtr = (PVOID)((PUINT8)DevicePathPtr + DeviceRootSize);
    memcpy(DevicePathPtr, DevicePath, DevicePathSize);
    DevicePathPtr = (PVOID)((PUINT8)DevicePathPtr + DevicePathSize);
    *DevicePathPtr = L'\0';

    VolumeHandle = CreateFileW(DevicePathBuf,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if (INVALID_HANDLE_VALUE == VolumeHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        if (STATUS_OBJECT_PATH_NOT_FOUND == Result ||
            STATUS_OBJECT_NAME_NOT_FOUND == Result)
            Result = STATUS_NO_SUCH_DEVICE;
        goto exit;
    }

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_VOLUME_LIST,
        0, 0,
        VolumeListBuf, (DWORD)*PVolumeListSize,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *PVolumeListSize = Bytes;
    Result = STATUS_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != VolumeHandle)
        CloseHandle(VolumeHandle);

    return Result;
}
