/**
 * @file dll/fsctl.c
 *
 * @copyright 2015-2022 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <dll/library.h>
#include <aclapi.h>

#define GLOBALROOT                      L"\\\\?\\GLOBALROOT"
#define PREFIXW                         L"" FSP_FSCTL_VOLUME_PARAMS_PREFIX
#define PREFIXW_SIZE                    (sizeof PREFIXW - sizeof(WCHAR))

static INIT_ONCE FspFsctlServiceVersionInitOnce = INIT_ONCE_STATIC_INIT;
static ULONG FspFsctlServiceVersionValue;
static DWORD FspFsctlTransactCode = FSP_FSCTL_TRANSACT;
static DWORD FspFsctlTransactBatchCode = FSP_FSCTL_TRANSACT_BATCH;

FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    PWCHAR VolumeNameBuf, SIZE_T VolumeNameSize,
    PHANDLE PVolumeHandle)
{
    WCHAR SxsDevicePathBuf[MAX_PATH];
    PWSTR DeviceRoot;
    SIZE_T DeviceRootSize, DevicePathSize, VolumeParamsSize;
    WCHAR DevicePathBuf[MAX_PATH + sizeof *VolumeParams], *DevicePathPtr, *DevicePathEnd;
    HANDLE VolumeHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;
    NTSTATUS Result;

    DevicePath = FspSxsAppendSuffix(SxsDevicePathBuf, sizeof SxsDevicePathBuf, DevicePath);

    if (sizeof(WCHAR) <= VolumeNameSize)
        VolumeNameBuf[0] = L'\0';
    *PVolumeHandle = INVALID_HANDLE_VALUE;

    /* check lengths; everything (including encoded volume params) must fit within DevicePathBuf */
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
    VolumeParamsSize = 0 == VolumeParams->Version ?
        sizeof(FSP_FSCTL_VOLUME_PARAMS_V0) :
        VolumeParams->Version;
    DevicePathPtr = (PVOID)((PUINT8)DevicePathPtr + PREFIXW_SIZE);
    DevicePathEnd = (PVOID)((PUINT8)DevicePathPtr + VolumeParamsSize * sizeof(WCHAR));
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

    /* initialize FspFsctlTransactCode */
    FspFsctlServiceVersion(0);

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

FSP_API NTSTATUS FspFsctlMakeMountdev(HANDLE VolumeHandle,
    BOOLEAN Persistent, GUID *UniqueId)
{
    DWORD Bytes;

    if (!DeviceIoControl(VolumeHandle,
        FSP_FSCTL_MOUNTDEV,
        &Persistent, sizeof Persistent, UniqueId, sizeof *UniqueId,
        &Bytes, 0))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFsctlUseMountmgr(HANDLE VolumeHandle,
    PWSTR MountPoint)
{
    DWORD Bytes;

    Bytes = 0 != MountPoint ? lstrlenW(MountPoint) * sizeof(WCHAR) : 0;

    if (!DeviceIoControl(VolumeHandle,
        FSP_FSCTL_MOUNTMGR,
        MountPoint, Bytes, 0, 0,
        &Bytes, 0))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFsctlTransact(HANDLE VolumeHandle,
    PVOID ResponseBuf, SIZE_T ResponseBufSize,
    PVOID RequestBuf, SIZE_T *PRequestBufSize,
    BOOLEAN Batch)
{
    NTSTATUS Result = STATUS_SUCCESS;
    DWORD ControlCode;
    DWORD Bytes = 0;

    if (0 != PRequestBufSize)
    {
        Bytes = (DWORD)*PRequestBufSize;
        *PRequestBufSize = 0;
    }

    /*
     * During file system volume creation FspFsctlCreateVolume called FspFsctlServiceVersion
     * which examined the version of the driver in use and initialized the variables
     * FspFsctlTransactCode and FspFsctlTransactBatchCode with either the new
     * FSP_IOCTL_TRANSACT* codes or the old FSP_FSCTL_TRANSACT* codes.
     */
    ControlCode = Batch ?
        (DEBUGTEST(50) ? FspFsctlTransactBatchCode : FSP_FSCTL_TRANSACT_BATCH) :
        (DEBUGTEST(50) ? FspFsctlTransactCode : FSP_FSCTL_TRANSACT);

    if (!DeviceIoControl(VolumeHandle,
        ControlCode,
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
    DWORD Bytes;

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_STOP, 0, 0, 0, 0, &Bytes, 0))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFsctlStop0(HANDLE VolumeHandle)
{
    DWORD Bytes;

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_STOP0, 0, 0, 0, 0, &Bytes, 0))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspFsctlNotify(HANDLE VolumeHandle,
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo, SIZE_T Size)
{
    NTSTATUS Result = STATUS_SUCCESS;
    DWORD Bytes = 0;

    if (!DeviceIoControl(VolumeHandle,
        FSP_FSCTL_NOTIFY,
        NotifyInfo, (DWORD)Size, 0, 0,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

exit:
    return Result;
}

FSP_API NTSTATUS FspFsctlGetVolumeList(PWSTR DevicePath,
    PWCHAR VolumeListBuf, PSIZE_T PVolumeListSize)
{
    WCHAR SxsDevicePathBuf[MAX_PATH];
    PWSTR DeviceRoot;
    SIZE_T DeviceRootSize, DevicePathSize;
    WCHAR DevicePathBuf[MAX_PATH], *DevicePathPtr;
    HANDLE VolumeHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;
    NTSTATUS Result;

    DevicePath = FspSxsAppendSuffix(SxsDevicePathBuf, sizeof SxsDevicePathBuf, DevicePath);

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

FSP_API NTSTATUS FspFsctlPreflight(PWSTR DevicePath)
{
    NTSTATUS Result;
    SIZE_T VolumeListSize;

    Result = FspFsctlStartService();
    if (!NT_SUCCESS(Result))
        return Result;

    VolumeListSize = 0;
    Result = FspFsctlGetVolumeList(DevicePath, 0, &VolumeListSize);
    if (!NT_SUCCESS(Result) && STATUS_BUFFER_TOO_SMALL != Result)
        return Result;

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsctlUnload(PWSTR DevicePath)
{
    WCHAR SxsDevicePathBuf[MAX_PATH];
    PWSTR DeviceRoot;
    SIZE_T DeviceRootSize, DevicePathSize;
    WCHAR DevicePathBuf[MAX_PATH], *DevicePathPtr;
    HANDLE VolumeHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;
    NTSTATUS Result;

    DevicePath = FspSxsAppendSuffix(SxsDevicePathBuf, sizeof SxsDevicePathBuf, DevicePath);

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

    if (!DeviceIoControl(VolumeHandle, FSP_FSCTL_UNLOAD, 0, 0, 0, 0, &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != VolumeHandle)
        CloseHandle(VolumeHandle);

    return Result;
}

static BOOL WINAPI FspFsctlServiceVersionInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    WCHAR DriverName[256];
    PWSTR ModuleFileName;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    QUERY_SERVICE_CONFIGW *ServiceConfig = 0;
    DWORD Size;

    FspSxsAppendSuffix(DriverName, sizeof DriverName, L"" FSP_FSCTL_DRIVER_NAME);

    ScmHandle = OpenSCManagerW(0, 0, 0);
    if (0 == ScmHandle)
        goto exit;

    SvcHandle = OpenServiceW(ScmHandle, DriverName, SERVICE_QUERY_CONFIG);
    if (0 == SvcHandle)
        goto exit;

    if (QueryServiceConfig(SvcHandle, 0, 0, &Size) ||
        ERROR_INSUFFICIENT_BUFFER != GetLastError())
        goto exit;

    ServiceConfig = MemAlloc(Size);
    if (0 == ServiceConfig)
        goto exit;

    if (!QueryServiceConfig(SvcHandle, ServiceConfig, Size, &Size))
        goto exit;

    ModuleFileName = ServiceConfig->lpBinaryPathName;
    if (L'\\' == ModuleFileName[0] &&
        L'?'  == ModuleFileName[1] &&
        L'?'  == ModuleFileName[2] &&
        L'\\' == ModuleFileName[3])
    {
        if (L'U'  == ModuleFileName[4] &&
            L'N'  == ModuleFileName[5] &&
            L'C'  == ModuleFileName[6] &&
            L'\\' == ModuleFileName[7])
        {
            ModuleFileName[6] = L'\\';
            ModuleFileName = ModuleFileName + 6;
        }
        else
            ModuleFileName = ModuleFileName + 4;
    }

    FspGetModuleVersion(ModuleFileName, &FspFsctlServiceVersionValue);

    if (0x0001000b /*v1.11*/ <= FspFsctlServiceVersionValue)
    {
        FspFsctlTransactCode = FSP_IOCTL_TRANSACT;
        FspFsctlTransactBatchCode = FSP_IOCTL_TRANSACT_BATCH;
    }

exit:
    MemFree(ServiceConfig);
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    return TRUE;
}

FSP_API NTSTATUS FspFsctlServiceVersion(PUINT32 PVersion)
{
    InitOnceExecuteOnce(&FspFsctlServiceVersionInitOnce, FspFsctlServiceVersionInitialize, 0, 0);

    if (0 != PVersion)
        *PVersion = FspFsctlServiceVersionValue;

    return 0 != FspFsctlServiceVersionValue ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

static SRWLOCK FspFsctlStartStopServiceLock = SRWLOCK_INIT;

static BOOLEAN FspFsctlRunningInContainer(VOID)
{
    /* Determine if we are running inside container.
     *
     * See https://github.com/microsoft/perfview/blob/V1.9.65/src/TraceEvent/TraceEventSession.cs#L525
     * See https://stackoverflow.com/a/50748300
     */
    return ERROR_SUCCESS == RegGetValueW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control",
        L"ContainerType",
        RRF_RT_REG_DWORD, 0,
        0, 0);
}

static NTSTATUS FspFsctlStartServiceByName(PWSTR DriverName)
{
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    SERVICE_STATUS ServiceStatus;
    DWORD LastError;
    NTSTATUS Result;

    AcquireSRWLockExclusive(&FspFsctlStartStopServiceLock);

    if (FspFsctlRunningInContainer())
    {
        Result = STATUS_SUCCESS;
        goto exit;
    }

    ScmHandle = OpenSCManagerW(0, 0, 0);
    if (0 == ScmHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    SvcHandle = OpenServiceW(ScmHandle, DriverName, SERVICE_QUERY_STATUS | SERVICE_START);
    if (0 == SvcHandle)
    {
        LastError = GetLastError();
        if (ERROR_SERVICE_DOES_NOT_EXIST != LastError)
            Result = FspNtStatusFromWin32(LastError);
        else
            Result = STATUS_NO_SUCH_DEVICE;
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

    ReleaseSRWLockExclusive(&FspFsctlStartStopServiceLock);

    return Result;
}

static VOID FspFsctlStartService_EnumFn(PVOID Context, PWSTR ServiceName, BOOLEAN Running)
{
    PWSTR DriverName = Context;
    if (0 > invariant_wcscmp(DriverName, ServiceName))
        lstrcpyW(DriverName, ServiceName);
}

FSP_API NTSTATUS FspFsctlStartService(VOID)
{
    /*
     * With the introduction of side-by-side (SxS) FSD installations,
     * we revisit how the FSD is started:
     *
     * - If the DLL is started in non-SxS mode, we first try to start
     * the non-SxS FSD. If that fails we then enumerate all SxS FSD's
     * and make a best guess on which one to start.
     *
     * - If the DLL is started in SxS mode, we only attempt to start
     * the associated SxS FSD.
     */

    if (L'\0' == FspSxsIdent()[0])
    {
        /* non-SxS mode */

        NTSTATUS Result;
        WCHAR DriverName[256];

        Result = FspFsctlStartServiceByName(L"" FSP_FSCTL_DRIVER_NAME);
        if (NT_SUCCESS(Result) || STATUS_NO_SUCH_DEVICE != Result)
            return Result;

        /* DO NOT CLOBBER Result. We will return it if our best effort below fails. */

        DriverName[0] = L'\0';
        FspFsctlEnumServices(FspFsctlStartService_EnumFn, DriverName);

        if (L'\0' == DriverName[0] || !NT_SUCCESS(FspFsctlStartServiceByName(DriverName)))
            return Result;

        return STATUS_SUCCESS;
    }
    else
    {
        /* SxS mode */

        WCHAR DriverName[256];
        FspSxsAppendSuffix(DriverName, sizeof DriverName, L"" FSP_FSCTL_DRIVER_NAME);
        return FspFsctlStartServiceByName(DriverName);
    }
}

FSP_API NTSTATUS FspFsctlStopService(VOID)
{
    WCHAR DriverName[256];
    HANDLE ThreadToken = 0, ProcessToken = 0;
    BOOL DidSetThreadToken = FALSE, DidAdjustTokenPrivileges = FALSE;
    TOKEN_PRIVILEGES Privileges, PreviousPrivileges;
    PRIVILEGE_SET RequiredPrivileges;
    DWORD PreviousPrivilegesLength;
    BOOL PrivilegeCheckResult;
    NTSTATUS Result;

    FspSxsAppendSuffix(DriverName, sizeof DriverName, L"" FSP_FSCTL_DRIVER_NAME);

    AcquireSRWLockExclusive(&FspFsctlStartStopServiceLock);

    if (FspFsctlRunningInContainer())
    {
        Result = STATUS_SUCCESS;
        goto exit;
    }

    /* enable and check SeLoadDriverPrivilege required for FSP_FSCTL_UNLOAD */
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, TRUE, &ThreadToken))
    {
        if (!OpenProcessToken(GetCurrentProcess(), MAXIMUM_ALLOWED, &ProcessToken) ||
            !DuplicateToken(ProcessToken, SecurityDelegation, &ThreadToken) ||
            !SetThreadToken(0, ThreadToken))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        DidSetThreadToken = TRUE;
        CloseHandle(ThreadToken);
        ThreadToken = 0;
        if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, TRUE, &ThreadToken))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }
    if (!LookupPrivilegeValueW(0, SE_LOAD_DRIVER_NAME, &Privileges.Privileges[0].Luid))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    Privileges.PrivilegeCount = 1;
    Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(ThreadToken, FALSE,
        &Privileges, sizeof PreviousPrivileges, &PreviousPrivileges, &PreviousPrivilegesLength))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    DidAdjustTokenPrivileges = 0 == GetLastError();
    RequiredPrivileges.PrivilegeCount = 1;
    RequiredPrivileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
    RequiredPrivileges.Privilege[0].Attributes = 0;
    RequiredPrivileges.Privilege[0].Luid = Privileges.Privileges[0].Luid;
    if (!PrivilegeCheck(ThreadToken, &RequiredPrivileges, &PrivilegeCheckResult))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    if (!PrivilegeCheckResult)
    {
        Result = STATUS_PRIVILEGE_NOT_HELD;
        goto exit;
    }

    Result = FspFsctlUnload(L"" FSP_FSCTL_DISK_DEVICE_NAME);
    if (!NT_SUCCESS(Result) && STATUS_NO_SUCH_DEVICE != Result)
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    if (DidAdjustTokenPrivileges)
        AdjustTokenPrivileges(ThreadToken, FALSE, &PreviousPrivileges, 0, 0, 0);
    if (DidSetThreadToken)
        SetThreadToken(0, 0);
    if (0 != ThreadToken)
        CloseHandle(ThreadToken);
    if (0 != ProcessToken)
        CloseHandle(ProcessToken);

    ReleaseSRWLockExclusive(&FspFsctlStartStopServiceLock);

    return Result;
}

FSP_API NTSTATUS FspFsctlEnumServices(
    VOID (*EnumFn)(PVOID Context, PWSTR ServiceName, BOOLEAN Running),
    PVOID Context)
{
    SC_HANDLE ScmHandle = 0;
    LPENUM_SERVICE_STATUSW Services = 0;
    DWORD Size, ServiceCount;
    DWORD LastError;
    NTSTATUS Result;

    ScmHandle = OpenSCManagerW(0, 0, SC_MANAGER_ENUMERATE_SERVICE);
    if (0 == ScmHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (!EnumServicesStatusW(ScmHandle,
        SERVICE_FILE_SYSTEM_DRIVER, SERVICE_STATE_ALL, 0, 0, &Size, &ServiceCount, 0))
    {
        LastError = GetLastError();
        if (ERROR_MORE_DATA != LastError)
        {
            Result = FspNtStatusFromWin32(LastError);
            goto exit;
        }
    }
    if (0 == Size)
    {
        Result = STATUS_SUCCESS;
        goto exit;
    }

    Services = MemAlloc(Size);
    if (0 == Services)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!EnumServicesStatusW(ScmHandle,
        SERVICE_FILE_SYSTEM_DRIVER, SERVICE_STATE_ALL, Services, Size, &Size, &ServiceCount, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    for (DWORD I = 0; ServiceCount > I; I++)
    {
        if (0 != invariant_wcsicmp(Services[I].lpServiceName, L"" FSP_FSCTL_DRIVER_NAME) &&
            0 != invariant_wcsnicmp(Services[I].lpServiceName,
            L"" FSP_FSCTL_DRIVER_NAME FSP_SXS_SEPARATOR_STRING,
            sizeof(FSP_FSCTL_DRIVER_NAME FSP_SXS_SEPARATOR_STRING) - 1))
            continue;
        EnumFn(Context,
            Services[I].lpServiceName,
            SERVICE_STOPPED != Services[I].ServiceStatus.dwCurrentState);
    }

    Result = STATUS_SUCCESS;

exit:
    MemFree(Services);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    return Result;
}

static NTSTATUS FspFsctlFixServiceSecurity(HANDLE SvcHandle)
{
    /*
     * This function adds two ACE's:
     * - An ACE that allows Everyone to start a service.
     * - An ACE that denies Everyone (including Administrators) to stop a service.
     */

    PSID WorldSid;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    PSECURITY_DESCRIPTOR NewSecurityDescriptor = 0;
    EXPLICIT_ACCESSW AccessEntries[2];
    PACL Dacl;
    BOOL DaclPresent, DaclDefaulted;
    DWORD Size;
    DWORD LastError;
    NTSTATUS Result;

    /* get the Everyone (World) SID */
    WorldSid = FspWksidGet(WinWorldSid);
    if (0 == WorldSid)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    /* get the service security descriptor DACL */
    Size = 0;
    if (!QueryServiceObjectSecurity(SvcHandle, DACL_SECURITY_INFORMATION, SecurityDescriptor, Size, &Size))
    {
        LastError = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER != LastError)
        {
            Result = FspNtStatusFromWin32(LastError);
            goto exit;
        }
    }
    SecurityDescriptor = MemAlloc(Size);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    if (!QueryServiceObjectSecurity(SvcHandle, DACL_SECURITY_INFORMATION, SecurityDescriptor, Size, &Size))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    /* extract the DACL */
    if (!GetSecurityDescriptorDacl(SecurityDescriptor, &DaclPresent, &Dacl, &DaclDefaulted))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    /* prepare an EXPLICIT_ACCESS for the SERVICE_QUERY_STATUS | SERVICE_START rights for Everyone */
    AccessEntries[0].grfAccessPermissions = SERVICE_QUERY_STATUS | SERVICE_START;
    AccessEntries[0].grfAccessMode = GRANT_ACCESS;
    AccessEntries[0].grfInheritance = NO_INHERITANCE;
    AccessEntries[0].Trustee.pMultipleTrustee = 0;
    AccessEntries[0].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    AccessEntries[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    AccessEntries[0].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    AccessEntries[0].Trustee.ptstrName = WorldSid;

    /* prepare an EXPLICIT_ACCESS to deny the SERVICE_STOP right to Everyone */
    AccessEntries[1].grfAccessPermissions = SERVICE_STOP;
    AccessEntries[1].grfAccessMode = DENY_ACCESS;
    AccessEntries[1].grfInheritance = NO_INHERITANCE;
    AccessEntries[1].Trustee.pMultipleTrustee = 0;
    AccessEntries[1].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    AccessEntries[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    AccessEntries[1].Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    AccessEntries[1].Trustee.ptstrName = WorldSid;

    /* create a new security descriptor with the new access */
    LastError = BuildSecurityDescriptorW(0, 0, 2, AccessEntries, 0, 0, SecurityDescriptor,
        &Size, &NewSecurityDescriptor);
    if (0 != LastError)
    {
        Result = FspNtStatusFromWin32(LastError);
        goto exit;
    }

    /* set the new service security descriptor DACL */
    if (!SetServiceObjectSecurity(SvcHandle, DACL_SECURITY_INFORMATION, NewSecurityDescriptor))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    LocalFree(NewSecurityDescriptor);
    MemFree(SecurityDescriptor);

    return Result;
}

NTSTATUS FspFsctlRegister(VOID)
{
    extern HINSTANCE DllInstance;
    WCHAR DriverName[256];
    WCHAR DriverPath[MAX_PATH];
    DWORD Size;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    PVOID VersionInfo = 0;
    SERVICE_DESCRIPTION ServiceDescription;
    NTSTATUS Result;

    FspSxsAppendSuffix(DriverName, sizeof DriverName, L"" FSP_FSCTL_DRIVER_NAME);

    Result = FspGetModuleFileName(DllInstance, DriverPath, MAX_PATH, L"" MyFsctlRegisterPath);
    if (!NT_SUCCESS(Result))
        return Result;

    Size = lstrlenW(DriverPath);
    if (4 < Size &&
        (L'.' == DriverPath[Size - 4]) &&
        (L'D' == DriverPath[Size - 3] || L'd' == DriverPath[Size - 3]) &&
        (L'L' == DriverPath[Size - 2] || L'l' == DriverPath[Size - 2]) &&
        (L'L' == DriverPath[Size - 1] || L'l' == DriverPath[Size - 1]) &&
        (L'\0' == DriverPath[Size]))
    {
        DriverPath[Size - 3] = L's';
        DriverPath[Size - 2] = L'y';
        DriverPath[Size - 1] = L's';
    }
    else if (4 < Size &&
        (L'.' == DriverPath[Size - 4]) &&
        (L'S' == DriverPath[Size - 3] || L's' == DriverPath[Size - 3]) &&
        (L'Y' == DriverPath[Size - 2] || L'y' == DriverPath[Size - 2]) &&
        (L'S' == DriverPath[Size - 1] || L's' == DriverPath[Size - 1]) &&
        (L'\0' == DriverPath[Size]))
    {
    }
    else
        /* should not happen! */
        return STATUS_NO_SUCH_DEVICE;

    ScmHandle = OpenSCManagerW(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (0 == ScmHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    SvcHandle = OpenServiceW(ScmHandle, DriverName, SERVICE_CHANGE_CONFIG | READ_CONTROL | WRITE_DAC);
    if (0 != SvcHandle)
    {
        if (!ChangeServiceConfigW(SvcHandle,
            SERVICE_FILE_SYSTEM_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, DriverPath,
            0, 0, 0, 0, 0, DriverName))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }
    else
    {
        SvcHandle = CreateServiceW(ScmHandle, DriverName, DriverName,
            SERVICE_CHANGE_CONFIG | READ_CONTROL | WRITE_DAC,
            SERVICE_FILE_SYSTEM_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, DriverPath,
            0, 0, 0, 0, 0);
        if (0 == SvcHandle)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    Size = GetFileVersionInfoSizeW(DriverPath, &Size/*dummy*/);
    if (0 < Size)
    {
        VersionInfo = MemAlloc(Size);
        if (0 != VersionInfo &&
            GetFileVersionInfoW(DriverPath, 0, Size, VersionInfo) &&
            VerQueryValueW(VersionInfo, L"\\StringFileInfo\\040904b0\\FileDescription",
                &ServiceDescription.lpDescription, &Size))
        {
            ChangeServiceConfig2W(SvcHandle, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription);
        }
    }

    Result = FspFsctlFixServiceSecurity(SvcHandle);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    MemFree(VersionInfo);
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    return Result;
}

NTSTATUS FspFsctlUnregister(VOID)
{
    WCHAR DriverName[256];
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    DWORD LastError;
    NTSTATUS Result;

    FspSxsAppendSuffix(DriverName, sizeof DriverName, L"" FSP_FSCTL_DRIVER_NAME);

    FspFsctlStopService();

    ScmHandle = OpenSCManagerW(0, 0, SC_MANAGER_CREATE_SERVICE);
        /*
         * The SC_MANAGER_CREATE_SERVICE access right is not strictly needed here,
         * but we use it to enforce admin rights.
         */
    if (0 == ScmHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    SvcHandle = OpenServiceW(ScmHandle, DriverName, DELETE);
    if (0 == SvcHandle)
    {
        LastError = GetLastError();
        if (ERROR_SERVICE_DOES_NOT_EXIST != LastError)
            Result = FspNtStatusFromWin32(LastError);
        else
            Result = STATUS_SUCCESS;
        goto exit;
    }

    if (!DeleteService(SvcHandle))
    {
        LastError = GetLastError();
        if (ERROR_SERVICE_MARKED_FOR_DELETE != LastError)
            Result = FspNtStatusFromWin32(LastError);
        else
            Result = STATUS_SUCCESS;
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    return Result;
}
