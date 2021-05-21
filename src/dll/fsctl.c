/**
 * @file dll/fsctl.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
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

static NTSTATUS FspFsctlStartService(VOID);

FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    PWCHAR VolumeNameBuf, SIZE_T VolumeNameSize,
    PHANDLE PVolumeHandle)
{
    NTSTATUS Result;
    PWSTR DeviceRoot;
    SIZE_T DeviceRootSize, DevicePathSize, VolumeParamsSize;
    WCHAR DevicePathBuf[MAX_PATH + sizeof *VolumeParams], *DevicePathPtr, *DevicePathEnd;
    HANDLE VolumeHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes;

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

static NTSTATUS FspFsctlStartService(VOID)
{
    static SRWLOCK Lock = SRWLOCK_INIT;
    PWSTR DriverName = L"" FSP_FSCTL_DRIVER_NAME;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    SERVICE_STATUS ServiceStatus;
    DWORD LastError;
    NTSTATUS Result;

    AcquireSRWLockExclusive(&Lock);

    /* Determine if we are running inside container.
     *
     * See https://github.com/microsoft/perfview/blob/V1.9.65/src/TraceEvent/TraceEventSession.cs#L525
     * See https://stackoverflow.com/a/50748300
     */
    LastError = RegGetValueW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control",
        L"ContainerType",
        RRF_RT_REG_DWORD, 0,
        0, 0);
    if (ERROR_SUCCESS == LastError)
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

    ReleaseSRWLockExclusive(&Lock);

    return Result;
}

static NTSTATUS FspFsctlFixServiceSecurity(HANDLE SvcHandle)
{
    /*
     * This function adds an ACE that allows Everyone to start a service.
     */

    PSID WorldSid;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    PSECURITY_DESCRIPTOR NewSecurityDescriptor = 0;
    EXPLICIT_ACCESSW AccessEntry;
    ACCESS_MASK AccessRights;
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

    /* prepare an EXPLICIT_ACCESS for the SERVICE_QUERY_STATUS | SERVICE_START right for Everyone */
    AccessEntry.grfAccessPermissions = SERVICE_QUERY_STATUS | SERVICE_START;
    AccessEntry.grfAccessMode = GRANT_ACCESS;
    AccessEntry.grfInheritance = NO_INHERITANCE;
    AccessEntry.Trustee.pMultipleTrustee = 0;
    AccessEntry.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    AccessEntry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    AccessEntry.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    AccessEntry.Trustee.ptstrName = WorldSid;

    /* get the effective rights for Everyone */
    AccessRights = 0;
    if (DaclPresent && 0 != Dacl)
    {
        LastError = GetEffectiveRightsFromAclW(Dacl, &AccessEntry.Trustee, &AccessRights);
        if (0 != LastError)
            /*
             * Apparently GetEffectiveRightsFromAclW can fail with ERROR_CIRCULAR_DEPENDENCY
             * in some rare circumstances. Calling GetEffectiveRightsFromAclW is not essential
             * in this instance. It is only done to check whether the "Everyone/World" SID
             * already has the access required to start the FSD; if it does not have those
             * rights already they are added. It is probably safe to just assume that the
             * required rights are not there if GetEffectiveRightsFromAclW fails; the worst
             * that can happen is that the rights get added twice (which is benign).
             *
             * See https://github.com/billziss-gh/winfsp/issues/62
             */
            AccessRights = 0;
    }

    /* do we have the required access rights? */
    if (AccessEntry.grfAccessPermissions != (AccessRights & AccessEntry.grfAccessPermissions))
    {
        /* create a new security descriptor with the new access */
        LastError = BuildSecurityDescriptorW(0, 0, 1, &AccessEntry, 0, 0, SecurityDescriptor,
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
    PWSTR DriverName = L"" FSP_FSCTL_DRIVER_NAME;
    WCHAR DriverPath[MAX_PATH];
    DWORD Size;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    PVOID VersionInfo = 0;
    SERVICE_DESCRIPTION ServiceDescription;
    NTSTATUS Result;

    if (0 == GetModuleFileNameW(DllInstance, DriverPath, MAX_PATH))
        return FspNtStatusFromWin32(GetLastError());

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
    PWSTR DriverName = L"" FSP_FSCTL_DRIVER_NAME;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    DWORD LastError;
    NTSTATUS Result;

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
