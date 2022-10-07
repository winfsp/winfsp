/**
 * @file dll/mount.c
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
#include <dbt.h>

static INIT_ONCE FspMountInitOnce = INIT_ONCE_STATIC_INIT;
static NTSTATUS (NTAPI *FspNtOpenSymbolicLinkObject)(
    PHANDLE LinkHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
static NTSTATUS (NTAPI *FspNtMakeTemporaryObject)(
    HANDLE Handle);
static NTSTATUS (NTAPI *FspNtClose)(
    HANDLE Handle);
static BOOLEAN FspMountDoNotUseLauncherValue;
static BOOLEAN FspMountBroadcastDriveChangeValue;
static BOOLEAN FspMountUseMountmgrFromFSDValue;

static VOID FspMountInitializeFromRegistry(VOID)
{
    DWORD Value;
    DWORD Size;
    LONG Result;

    Value = 0;
    Size = sizeof Value;
    Result = RegGetValueW(HKEY_LOCAL_MACHINE, L"" FSP_FSCTL_PRODUCT_FULL_REGKEY,
        L"MountDoNotUseLauncher",
        RRF_RT_REG_DWORD, 0, &Value, &Size);
    if (ERROR_SUCCESS == Result)
        FspMountDoNotUseLauncherValue = !!Value;

    Value = 0;
    Size = sizeof Value;
    Result = RegGetValueW(HKEY_LOCAL_MACHINE, L"" FSP_FSCTL_PRODUCT_FULL_REGKEY,
        L"MountBroadcastDriveChange",
        RRF_RT_REG_DWORD, 0, &Value, &Size);
    if (ERROR_SUCCESS == Result)
        FspMountBroadcastDriveChangeValue = !!Value;

    Value = 0;
    Size = sizeof Value;
    Result = RegGetValueW(HKEY_LOCAL_MACHINE, L"" FSP_FSCTL_PRODUCT_FULL_REGKEY,
        L"MountUseMountmgrFromFSD",
        RRF_RT_REG_DWORD, 0, &Value, &Size);
    if (ERROR_SUCCESS == Result)
        FspMountUseMountmgrFromFSDValue = !!Value;
}

static BOOL WINAPI FspMountInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    HANDLE Handle;

    Handle = GetModuleHandleW(L"ntdll.dll");
    if (0 != Handle)
    {
        FspNtOpenSymbolicLinkObject = (PVOID)GetProcAddress(Handle, "NtOpenSymbolicLinkObject");
        FspNtMakeTemporaryObject = (PVOID)GetProcAddress(Handle, "NtMakeTemporaryObject");
        FspNtClose = (PVOID)GetProcAddress(Handle, "NtClose");

        if (0 == FspNtOpenSymbolicLinkObject || 0 == FspNtMakeTemporaryObject || 0 == FspNtClose)
        {
            FspNtOpenSymbolicLinkObject = 0;
            FspNtMakeTemporaryObject = 0;
            FspNtClose = 0;
        }
    }

    FspMountInitializeFromRegistry();

    return TRUE;
}

static NTSTATUS FspMountSet_Directory(PWSTR VolumeName, PWSTR MountPoint,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PHANDLE PMountHandle);
static NTSTATUS FspMountRemove_Directory(HANDLE MountHandle);

static NTSTATUS FspMountSet_MountmgrDrive(HANDLE VolumeHandle, PWSTR VolumeName, PWSTR MountPoint)
{
    if (FspMountUseMountmgrFromFSDValue)
        /* use MountManager from FSD and exit */
        return FspFsctlUseMountmgr(VolumeHandle, MountPoint + 4);

    GUID UniqueId;
    NTSTATUS Result;

    /* transform our volume into one that can be used by the MountManager */
    Result = FspFsctlMakeMountdev(VolumeHandle, FALSE, &UniqueId);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* use the MountManager to create the drive */
    UNICODE_STRING UVolumeName, UMountPoint;
    UVolumeName.Length = UVolumeName.MaximumLength = (USHORT)(lstrlenW(VolumeName) * sizeof(WCHAR));
    UVolumeName.Buffer = VolumeName;
    UMountPoint.Length = UMountPoint.MaximumLength = (USHORT)((lstrlenW(MountPoint) - 4) * sizeof(WCHAR));
    UMountPoint.Buffer = MountPoint + 4;
    Result = FspMountmgrCreateDrive(&UVolumeName, &UniqueId, &UMountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS FspMountSet_MountmgrDirectory(HANDLE VolumeHandle, PWSTR VolumeName, PWSTR MountPoint,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PHANDLE PMountHandle)
{
    GUID UniqueId;
    HANDLE MountHandle = INVALID_HANDLE_VALUE;
    NTSTATUS Result;

    *PMountHandle = 0;

    /* create the directory mount point */
    Result = FspMountSet_Directory(VolumeName, MountPoint + 4, SecurityDescriptor, &MountHandle);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (FspMountUseMountmgrFromFSDValue)
    {
        /* use MountManager from FSD and exit */
        Result = FspFsctlUseMountmgr(VolumeHandle, MountPoint + 4);
        if (!NT_SUCCESS(Result))
            goto exit;

        *PMountHandle = MountHandle;

        Result = STATUS_SUCCESS;
        goto exit;
    }

    /* transform our volume into one that can be used by the MountManager */
    Result = FspFsctlMakeMountdev(VolumeHandle, FALSE, &UniqueId);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* notify the MountManager about the created directory mount point */
    UNICODE_STRING UVolumeName, UMountPoint;
    UVolumeName.Length = UVolumeName.MaximumLength = (USHORT)(lstrlenW(VolumeName) * sizeof(WCHAR));
    UVolumeName.Buffer = VolumeName;
    UMountPoint.Length = UMountPoint.MaximumLength = (USHORT)((lstrlenW(MountPoint) - 4) * sizeof(WCHAR));
    UMountPoint.Buffer = MountPoint + 4;
    Result = FspMountmgrNotifyCreateDirectory(&UVolumeName, &UniqueId, &UMountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PMountHandle = MountHandle;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && INVALID_HANDLE_VALUE != MountHandle)
        FspMountRemove_Directory(MountHandle);

    return Result;
}

static NTSTATUS FspMountRemove_MountmgrDrive(HANDLE VolumeHandle, PWSTR MountPoint)
{
    if (FspMountUseMountmgrFromFSDValue)
        /* use MountManager from FSD and exit */
        return FspFsctlUseMountmgr(VolumeHandle, 0);

    /* use the MountManager to delete the drive */
    UNICODE_STRING UMountPoint;
    UMountPoint.Length = UMountPoint.MaximumLength = (USHORT)((lstrlenW(MountPoint) - 4) * sizeof(WCHAR));
    UMountPoint.Buffer = MountPoint + 4;
    return FspMountmgrDeleteDrive(&UMountPoint);
}

static NTSTATUS FspMountRemove_MountmgrDirectory(HANDLE VolumeHandle, PWSTR VolumeName, PWSTR MountPoint,
    HANDLE MountHandle)
{
    NTSTATUS Result;

    if (FspMountUseMountmgrFromFSDValue)
        /* use MountManager from FSD, but do not exit; additional processing is required below */
        FspFsctlUseMountmgr(VolumeHandle, 0);
    else
    {
        /* notify the MountManager about the deleted directory mount point */
        UNICODE_STRING UVolumeName, UMountPoint;
        UVolumeName.Length = UVolumeName.MaximumLength = (USHORT)(lstrlenW(VolumeName) * sizeof(WCHAR));
        UVolumeName.Buffer = VolumeName;
        UMountPoint.Length = UMountPoint.MaximumLength = (USHORT)((lstrlenW(MountPoint) - 4) * sizeof(WCHAR));
        UMountPoint.Buffer = MountPoint + 4;
        FspMountmgrNotifyDeleteDirectory(&UVolumeName, &UMountPoint);
    }

    /* delete the directory mount point */
    Result = FspMountRemove_Directory(MountHandle);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS FspLauncherDefineDosDevice(
    WCHAR Sign, PWSTR MountPoint, PWSTR VolumeName)
{
    if (2 != lstrlenW(MountPoint) ||
        FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR) <= lstrlenW(VolumeName))
        return STATUS_INVALID_PARAMETER;

    WCHAR Argv0[4];
    PWSTR Argv[2];
    NTSTATUS Result;
    ULONG ErrorCode;

    Argv0[0] = Sign;
    Argv0[1] = MountPoint[0];
    Argv0[2] = MountPoint[1];
    Argv0[3] = L'\0';

    Argv[0] = Argv0;
    Argv[1] = VolumeName;

    Result = FspLaunchCallLauncherPipe(FspLaunchCmdDefineDosDevice, 2, Argv, 0, 0, 0, &ErrorCode);
    return !NT_SUCCESS(Result) ? Result : FspNtStatusFromWin32(ErrorCode);
}

struct FspMountBroadcastDriveChangeData
{
    HMODULE Module;
    WPARAM WParam;
    WCHAR MountPoint[];
};

static DWORD WINAPI FspMountBroadcastDriveChangeThread(PVOID Data0)
{
    struct FspMountBroadcastDriveChangeData *Data = Data0;
    HMODULE Module = Data->Module;
    WPARAM WParam = Data->WParam;
    PWSTR MountPoint = Data->MountPoint;
    BOOLEAN IsLocalSystem;
    DEV_BROADCAST_VOLUME DriveChange;
    DWORD Recipients;

    FspServiceContextCheck(0, &IsLocalSystem);

    memset(&DriveChange, 0, sizeof DriveChange);
    DriveChange.dbcv_size = sizeof DriveChange;
    DriveChange.dbcv_devicetype = DBT_DEVTYP_VOLUME;
    DriveChange.dbcv_flags = DBTF_NET;
    DriveChange.dbcv_unitmask = 1 << ((MountPoint[0] | 0x20) - 'a');

    Recipients = BSM_APPLICATIONS | (IsLocalSystem ? BSM_ALLDESKTOPS : 0);
    BroadcastSystemMessageW(
        BSF_POSTMESSAGE,
        &Recipients,
        WM_DEVICECHANGE,
        WParam,
        (LPARAM)&DriveChange);

    MemFree(Data);

    FreeLibraryAndExitThread(Module, 0);

    return 0;
}

static NTSTATUS FspMountBroadcastDriveChange(PWSTR MountPoint, WPARAM WParam)
{
    /*
     * DefineDosDeviceW (either directly or via the CSRSS) broadcasts a WM_DEVICECHANGE message
     * when a drive is added/removed. Unfortunately on some systems this broadcast fails. The
     * result is that Explorer does not receive the WM_DEVICECHANGE notification and does not
     * become aware of the drive change. This results in only minor UI issues for local drives,
     * but more seriously it makes network drives inaccessible from some Explorer windows.
     *
     * The problem is that BroadcastSystemMessage can hang indefinitely when supplied the flags
     * NOHANG | FORCEIFHUNG | NOTIMEOUTIFNOTHUNG. The NOTIMEOUTIFNOTHUNG flag instructs the BSM
     * API to not timeout an app that is not considered hung; however an app that is not hung may
     * still fail to respond to a broadcasted message indefinitely. This can result in the BSM
     * API never returning ("hanging").
     *
     * To resolve this we simply call BroadcastSystemMessage with BSF_POSTMESSAGE. (It would work
     * with BSF_NOHANG | BSF_FORCEIFHUNG and without NOTIMEOUTIFNOTHUNG, but BSF_POSTMESSAGE is
     * faster). We do this in a separate thread to avoid blocking caller's thread.
     */

    NTSTATUS Result;
    HMODULE Module;
    HANDLE Thread;
    struct FspMountBroadcastDriveChangeData *Data = 0;
    int Size;

    if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (PVOID)FspMountBroadcastDriveChangeThread,
        &Module))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Size = (lstrlenW(MountPoint) + 1) * sizeof(WCHAR);
    Data = MemAlloc(sizeof *Data + Size);
    if (0 == Data)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Data->Module = Module;
    Data->WParam = WParam;
    memcpy(Data->MountPoint, MountPoint, Size);

    Thread = CreateThread(0, 0, FspMountBroadcastDriveChangeThread, Data, 0, 0);
    if (0 == Thread)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }
    CloseHandle(Thread);

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        MemFree(Data);
        FreeLibrary(Module);
    }

    return Result;
}

static NTSTATUS FspMountSet_Drive(PWSTR VolumeName, PWSTR MountPoint, PHANDLE PMountHandle)
{
    NTSTATUS Result;
    BOOLEAN IsLocalSystem, IsServiceContext;

    *PMountHandle = 0;

    Result = FspServiceContextCheck(0, &IsLocalSystem);
    IsServiceContext = NT_SUCCESS(Result) && !IsLocalSystem;
    if (IsServiceContext && !FspMountDoNotUseLauncherValue)
    {
        /*
         * If the current process is in the service context but not LocalSystem,
         * ask the launcher to DefineDosDevice for us. This is because the launcher
         * runs in the LocalSystem context and can create global drives.
         *
         * In this case the launcher will also add DELETE access to the drive symlink
         * for us, so that we can make it temporary below.
         */
        Result = FspLauncherDefineDosDevice(L'+', MountPoint, VolumeName);
        if (!NT_SUCCESS(Result))
            return Result;
    }
    else
    {
        if (!DefineDosDeviceW(DDD_RAW_TARGET_PATH, MountPoint, VolumeName))
            return FspNtStatusFromWin32(GetLastError());

        /*
         * On some systems DefineDosDeviceW fails to properly broadcast the WM_DEVICECHANGE
         * notification. So use a workaround. See comments in FspMountBroadcastDriveChange.
         */
        if (FspMountBroadcastDriveChangeValue)
            FspMountBroadcastDriveChange(MountPoint, DBT_DEVICEARRIVAL);
    }

    if (0 != FspNtOpenSymbolicLinkObject)
    {
        WCHAR SymlinkBuf[6];
        UNICODE_STRING Symlink;
        OBJECT_ATTRIBUTES Obja;

        memcpy(SymlinkBuf, L"\\??\\X:", sizeof SymlinkBuf);
        SymlinkBuf[4] = MountPoint[0];
        Symlink.Length = Symlink.MaximumLength = sizeof SymlinkBuf;
        Symlink.Buffer = SymlinkBuf;

        memset(&Obja, 0, sizeof Obja);
        Obja.Length = sizeof Obja;
        Obja.ObjectName = &Symlink;
        Obja.Attributes = OBJ_CASE_INSENSITIVE;

        Result = FspNtOpenSymbolicLinkObject(PMountHandle, DELETE, &Obja);
        if (NT_SUCCESS(Result))
        {
            Result = FspNtMakeTemporaryObject(*PMountHandle);
            if (!NT_SUCCESS(Result))
            {
                FspNtClose(*PMountHandle);
                *PMountHandle = 0;
            }
        }
    }

    /* HACK:
     *
     * Handles do not use the low 2 bits (unless they are console handles).
     * Abuse this fact to remember that we are running in the service context.
     */
    *PMountHandle = (HANDLE)(UINT_PTR)((DWORD)(UINT_PTR)*PMountHandle | IsServiceContext);

    return STATUS_SUCCESS;
}

static NTSTATUS FspMountRemove_Drive(PWSTR VolumeName, PWSTR MountPoint, HANDLE MountHandle)
{
    NTSTATUS Result;
    BOOLEAN IsServiceContext;

    IsServiceContext = 0 != ((DWORD)(UINT_PTR)MountHandle & 1);
    MountHandle = (HANDLE)(UINT_PTR)((DWORD)(UINT_PTR)MountHandle & ~1);
    if (IsServiceContext)
        /*
         * If the current process is in the service context but not LocalSystem,
         * ask the launcher to DefineDosDevice for us. This is because the launcher
         * runs in the LocalSystem context and can remove global drives.
         */
        Result = FspLauncherDefineDosDevice(L'-', MountPoint, VolumeName);
    else
    {
        Result = DefineDosDeviceW(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
            MountPoint, VolumeName) ? STATUS_SUCCESS : FspNtStatusFromWin32(GetLastError());

        /*
         * On some systems DefineDosDeviceW fails to properly broadcast the WM_DEVICECHANGE
         * notification. So use a workaround. See comments in FspMountBroadcastDriveChange.
         */
        if (FspMountBroadcastDriveChangeValue)
            FspMountBroadcastDriveChange(MountPoint, DBT_DEVICEREMOVECOMPLETE);
    }

    if (0 != MountHandle)
        FspNtClose(MountHandle);

    return Result;
}

static NTSTATUS FspMountSet_Directory(PWSTR VolumeName, PWSTR MountPoint,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PHANDLE PMountHandle)
{
    NTSTATUS Result;
    SECURITY_ATTRIBUTES SecurityAttributes;
    HANDLE MountHandle = INVALID_HANDLE_VALUE;
    DWORD Backslashes, Bytes;
    USHORT VolumeNameLength, BackslashLength, ReparseDataLength;
    PREPARSE_DATA_BUFFER ReparseData = 0;
    PWSTR P, PathBuffer;

    *PMountHandle = 0;

    /*
     * Windows does not allow mount points (junctions) to point to network file systems.
     *
     * Count how many backslashes our VolumeName has. If it is 3 or more this is a network
     * file system. Preemptively return STATUS_NETWORK_ACCESS_DENIED.
     */
    for (P = VolumeName, Backslashes = 0; *P; P++)
        if (L'\\' == *P)
            if (3 == ++Backslashes)
            {
                Result = STATUS_NETWORK_ACCESS_DENIED;
                goto exit;
            }

    memset(&SecurityAttributes, 0, sizeof SecurityAttributes);
    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    MountHandle = FspCreateDirectoryFileW(MountPoint,
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &SecurityAttributes,
        FILE_ATTRIBUTE_DIRECTORY |
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE);
    if (INVALID_HANDLE_VALUE == MountHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    VolumeNameLength = (USHORT)lstrlenW(VolumeName);
    BackslashLength = 0 == VolumeNameLength || L'\\' != VolumeName[VolumeNameLength - 1];
    VolumeNameLength *= sizeof(WCHAR);
    BackslashLength *= sizeof(WCHAR);

    ReparseDataLength = (USHORT)(
        FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer) -
        FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer)) +
        2 * (VolumeNameLength + BackslashLength + sizeof(WCHAR));
    ReparseData = MemAlloc(REPARSE_DATA_BUFFER_HEADER_SIZE + ReparseDataLength);
    if (0 == ReparseData)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    ReparseData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    ReparseData->ReparseDataLength = ReparseDataLength;
    ReparseData->Reserved = 0;
    ReparseData->MountPointReparseBuffer.SubstituteNameOffset = 0;
    ReparseData->MountPointReparseBuffer.SubstituteNameLength =
        VolumeNameLength + BackslashLength;
    ReparseData->MountPointReparseBuffer.PrintNameOffset =
        ReparseData->MountPointReparseBuffer.SubstituteNameLength + sizeof(WCHAR);
    ReparseData->MountPointReparseBuffer.PrintNameLength =
        VolumeNameLength + BackslashLength;

    PathBuffer = ReparseData->MountPointReparseBuffer.PathBuffer;
    memcpy(PathBuffer, VolumeName, VolumeNameLength);
    if (BackslashLength)
        PathBuffer[VolumeNameLength / sizeof(WCHAR)] = L'\\';
    PathBuffer[(VolumeNameLength + BackslashLength) / sizeof(WCHAR)] = L'\0';

    PathBuffer = ReparseData->MountPointReparseBuffer.PathBuffer +
        (ReparseData->MountPointReparseBuffer.PrintNameOffset) / sizeof(WCHAR);
    memcpy(PathBuffer, VolumeName, VolumeNameLength);
    if (BackslashLength)
        PathBuffer[VolumeNameLength / sizeof(WCHAR)] = L'\\';
    PathBuffer[(VolumeNameLength + BackslashLength) / sizeof(WCHAR)] = L'\0';

    if (!DeviceIoControl(MountHandle, FSCTL_SET_REPARSE_POINT,
        ReparseData, REPARSE_DATA_BUFFER_HEADER_SIZE + ReparseData->ReparseDataLength,
        0, 0,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *PMountHandle = MountHandle;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && INVALID_HANDLE_VALUE != MountHandle)
        CloseHandle(MountHandle);

    MemFree(ReparseData);

    return Result;
}

static NTSTATUS FspMountRemove_Directory(HANDLE MountHandle)
{
    /* directory is marked DELETE_ON_CLOSE */
    return CloseHandle(MountHandle) ? STATUS_SUCCESS : FspNtStatusFromWin32(GetLastError());
}

FSP_API NTSTATUS FspMountSet(FSP_MOUNT_DESC *Desc)
{
    InitOnceExecuteOnce(&FspMountInitOnce, FspMountInitialize, 0, 0);

    Desc->MountHandle = 0;

    if (L'*' == Desc->MountPoint[0] && ':' == Desc->MountPoint[1] && L'\0' == Desc->MountPoint[2])
    {
        NTSTATUS Result;
        DWORD Drives;
        WCHAR Drive;

        Drives = GetLogicalDrives();
        if (0 == Drives)
            return FspNtStatusFromWin32(GetLastError());

        for (Drive = 'Z'; 'D' <= Drive; Drive--)
            if (0 == (Drives & (1 << (Drive - 'A'))))
            {
                Desc->MountPoint[0] = Drive;
                Result = FspMountSet_Drive(Desc->VolumeName, Desc->MountPoint,
                    &Desc->MountHandle);
                if (NT_SUCCESS(Result))
                    return Result;
            }
        Desc->MountPoint[0] = L'*';
        return STATUS_NO_SUCH_DEVICE;
    }
    else if (FspPathIsMountmgrDrive(Desc->MountPoint))
        return FspMountSet_MountmgrDrive(Desc->VolumeHandle, Desc->VolumeName, Desc->MountPoint);
    else if (FspPathIsMountmgrMountPoint(Desc->MountPoint))
        return FspMountSet_MountmgrDirectory(Desc->VolumeHandle, Desc->VolumeName, Desc->MountPoint,
            Desc->Security, &Desc->MountHandle);
    else if (FspPathIsDrive(Desc->MountPoint))
        return FspMountSet_Drive(Desc->VolumeName, Desc->MountPoint,
            &Desc->MountHandle);
    else
        return FspMountSet_Directory(Desc->VolumeName, Desc->MountPoint, Desc->Security,
            &Desc->MountHandle);
}

FSP_API NTSTATUS FspMountRemove(FSP_MOUNT_DESC *Desc)
{
    InitOnceExecuteOnce(&FspMountInitOnce, FspMountInitialize, 0, 0);

    if (FspPathIsMountmgrDrive(Desc->MountPoint))
        return FspMountRemove_MountmgrDrive(Desc->VolumeHandle, Desc->MountPoint);
    else if (FspPathIsMountmgrMountPoint(Desc->MountPoint))
        return FspMountRemove_MountmgrDirectory(Desc->VolumeHandle, Desc->VolumeName, Desc->MountPoint,
            Desc->MountHandle);
    else if (FspPathIsDrive(Desc->MountPoint))
        return FspMountRemove_Drive(Desc->VolumeName, Desc->MountPoint, Desc->MountHandle);
    else
        return FspMountRemove_Directory(Desc->MountHandle);
}
