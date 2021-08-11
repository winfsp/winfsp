/**
 * @file dll/mount.c
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

static INIT_ONCE FspMountInitOnce = INIT_ONCE_STATIC_INIT;
static NTSTATUS (NTAPI *FspNtOpenSymbolicLinkObject)(
    PHANDLE LinkHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
static NTSTATUS (NTAPI *FspNtMakeTemporaryObject)(
    HANDLE Handle);
static NTSTATUS (NTAPI *FspNtClose)(
    HANDLE Handle);
static BOOLEAN FspMountDoNotUseLauncher;

static VOID FspMountInitializeFromRegistry(VOID)
{
    HKEY RegKey;
    LONG Result;
    DWORD Size;
    DWORD MountDoNotUseLauncher;

    MountDoNotUseLauncher = 0;

    Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\" FSP_FSCTL_PRODUCT_NAME,
        0, KEY_READ | KEY_WOW64_32KEY, &RegKey);
    if (ERROR_SUCCESS == Result)
    {
        Size = sizeof MountDoNotUseLauncher;
        Result = RegGetValueW(RegKey, 0, L"MountDoNotUseLauncher",
            RRF_RT_REG_DWORD, 0, &MountDoNotUseLauncher, &Size);
        RegCloseKey(RegKey);
    }

    FspMountDoNotUseLauncher = !!MountDoNotUseLauncher;
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

static NTSTATUS FspMountmgrControl(ULONG IoControlCode,
    PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, PULONG POutputBufferLength)
{
    HANDLE MgrHandle = INVALID_HANDLE_VALUE;
    DWORD Bytes = 0;
    NTSTATUS Result;

    if (0 == POutputBufferLength)
        POutputBufferLength = &Bytes;

    MgrHandle = CreateFileW(L"\\\\.\\MountPointManager",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        OPEN_EXISTING,
        0,
        0);
    if (INVALID_HANDLE_VALUE == MgrHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (!DeviceIoControl(MgrHandle,
        IoControlCode,
        InputBuffer, InputBufferLength, OutputBuffer, *POutputBufferLength,
        &Bytes, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *POutputBufferLength = Bytes;
    Result = STATUS_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != MgrHandle)
        CloseHandle(MgrHandle);

    return Result;
}

static NTSTATUS FspMountSet_Mountmgr(HANDLE VolumeHandle, PWSTR VolumeName, PWSTR MountPoint)
{
    /* only support drives for now! (format: \\.\X:) */
    if (L'\0' != MountPoint[6])
        return STATUS_INVALID_PARAMETER;

    /* mountmgr.h */
    typedef enum
    {
        Disabled = 0,
        Enabled,
    } MOUNTMGR_AUTO_MOUNT_STATE;
    typedef struct
    {
        MOUNTMGR_AUTO_MOUNT_STATE CurrentState;
    } MOUNTMGR_QUERY_AUTO_MOUNT;
    typedef struct
    {
        MOUNTMGR_AUTO_MOUNT_STATE NewState;
    } MOUNTMGR_SET_AUTO_MOUNT;
    typedef struct
    {
        USHORT DeviceNameLength;
        WCHAR DeviceName[1];
    } MOUNTMGR_TARGET_NAME;
    typedef struct
    {
        USHORT SymbolicLinkNameOffset;
        USHORT SymbolicLinkNameLength;
        USHORT DeviceNameOffset;
        USHORT DeviceNameLength;
    } MOUNTMGR_CREATE_POINT_INPUT;

    GUID UniqueId;
    MOUNTMGR_QUERY_AUTO_MOUNT QueryAutoMount;
    MOUNTMGR_SET_AUTO_MOUNT SetAutoMount;
    MOUNTMGR_TARGET_NAME *TargetName = 0;
    MOUNTMGR_CREATE_POINT_INPUT *CreatePointInput = 0;
    ULONG VolumeNameSize, QueryAutoMountSize, TargetNameSize, CreatePointInputSize;
    HKEY RegKey;
    LONG RegResult;
    WCHAR RegValueName[MAX_PATH];
    UINT8 RegValueData[sizeof UniqueId];
    DWORD RegValueNameSize, RegValueDataSize;
    DWORD RegType;
    NTSTATUS Result;

    /* transform our volume into one that can be used by the MountManager */
    Result = FspFsctlMakeMountdev(VolumeHandle, FALSE, &UniqueId);
    if (!NT_SUCCESS(Result))
        goto exit;

    VolumeNameSize = lstrlenW(VolumeName) * sizeof(WCHAR);
    QueryAutoMountSize = sizeof QueryAutoMount;
    TargetNameSize = FIELD_OFFSET(MOUNTMGR_TARGET_NAME, DeviceName) + VolumeNameSize;
    CreatePointInputSize = sizeof *CreatePointInput +
        sizeof L"\\DosDevices\\X:" - sizeof(WCHAR) + VolumeNameSize;

    TargetName = MemAlloc(TargetNameSize);
    if (0 == TargetName)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    CreatePointInput = MemAlloc(CreatePointInputSize);
    if (0 == CreatePointInput)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    /* query the current AutoMount value and save it */
    Result = FspMountmgrControl(
        CTL_CODE('m', 15, METHOD_BUFFERED, FILE_ANY_ACCESS),
            /* IOCTL_MOUNTMGR_QUERY_AUTO_MOUNT */
        0, 0, &QueryAutoMount, &QueryAutoMountSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* disable AutoMount */
    SetAutoMount.NewState = 0;
    Result = FspMountmgrControl(
        CTL_CODE('m', 16, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS),
            /* IOCTL_MOUNTMGR_SET_AUTO_MOUNT */
        &SetAutoMount, sizeof SetAutoMount, 0, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* announce volume arrival */
    memset(TargetName, 0, sizeof *TargetName);
    TargetName->DeviceNameLength = (USHORT)VolumeNameSize;
    memcpy(TargetName->DeviceName,
        VolumeName, TargetName->DeviceNameLength);
    Result = FspMountmgrControl(
        CTL_CODE('m', 11, METHOD_BUFFERED, FILE_READ_ACCESS),
            /* IOCTL_MOUNTMGR_VOLUME_ARRIVAL_NOTIFICATION */
        TargetName, TargetNameSize, 0, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* reset the AutoMount value to the saved one */
    SetAutoMount.NewState = QueryAutoMount.CurrentState;
    FspMountmgrControl(
        CTL_CODE('m', 16, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS),
            /* IOCTL_MOUNTMGR_SET_AUTO_MOUNT */
        &SetAutoMount, sizeof SetAutoMount, 0, 0);
#if 0
    if (!NT_SUCCESS(Result))
        goto exit;
#endif

    /* create mount point */
    memset(CreatePointInput, 0, sizeof *CreatePointInput);
    CreatePointInput->SymbolicLinkNameOffset = sizeof *CreatePointInput;
    CreatePointInput->SymbolicLinkNameLength = sizeof L"\\DosDevices\\X:" - sizeof(WCHAR);
    CreatePointInput->DeviceNameOffset =
        CreatePointInput->SymbolicLinkNameOffset + CreatePointInput->SymbolicLinkNameLength;
    CreatePointInput->DeviceNameLength = (USHORT)VolumeNameSize;
    memcpy((PUINT8)CreatePointInput + CreatePointInput->SymbolicLinkNameOffset,
        L"\\DosDevices\\X:", CreatePointInput->SymbolicLinkNameLength);
    ((PWCHAR)((PUINT8)CreatePointInput + CreatePointInput->SymbolicLinkNameOffset))[12] =
        MountPoint[4] & ~0x20;
        /* convert to uppercase */
    memcpy((PUINT8)CreatePointInput + CreatePointInput->DeviceNameOffset,
        VolumeName, CreatePointInput->DeviceNameLength);
    Result = FspMountmgrControl(
        CTL_CODE('m', 0, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS),
            /* IOCTL_MOUNTMGR_CREATE_POINT */
        CreatePointInput, CreatePointInputSize, 0, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* HACK: delete the MountManager registry entries */
    RegResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\MountedDevices",
        0, KEY_READ | KEY_WRITE, &RegKey);
    if (ERROR_SUCCESS == RegResult)
    {
        for (DWORD I = 0;; I++)
        {
            RegValueNameSize = MAX_PATH;
            RegValueDataSize = sizeof RegValueData;
            RegResult = RegEnumValueW(RegKey,
                I, RegValueName, &RegValueNameSize, 0, &RegType, RegValueData, &RegValueDataSize);
            if (ERROR_NO_MORE_ITEMS == RegResult)
                break;
            else if (ERROR_SUCCESS != RegResult)
                continue;

            if (REG_BINARY == RegType &&
                sizeof RegValueData == RegValueDataSize &&
                InlineIsEqualGUID((GUID *)&RegValueData, &UniqueId))
            {
                RegResult = RegDeleteValueW(RegKey, RegValueName);
                if (ERROR_SUCCESS == RegResult)
                    /* reset index after modifying key; only safe way to use RegEnumValueW with modifications */
                    I = -1;
            }
        }

        RegCloseKey(RegKey);
    }

    Result = STATUS_SUCCESS;

exit:
    MemFree(CreatePointInput);
    MemFree(TargetName);

    return Result;
}

static NTSTATUS FspMountRemove_Mountmgr(PWSTR MountPoint)
{
    /* mountmgr.h */
    typedef struct
    {
        ULONG SymbolicLinkNameOffset;
        USHORT SymbolicLinkNameLength;
        USHORT Reserved1;
        ULONG UniqueIdOffset;
        USHORT UniqueIdLength;
        USHORT Reserved2;
        ULONG DeviceNameOffset;
        USHORT DeviceNameLength;
        USHORT Reserved3;
    } MOUNTMGR_MOUNT_POINT;
    typedef struct
    {
        ULONG Size;
        ULONG NumberOfMountPoints;
        MOUNTMGR_MOUNT_POINT MountPoints[1];
    } MOUNTMGR_MOUNT_POINTS;

    MOUNTMGR_MOUNT_POINT *Input = 0;
    MOUNTMGR_MOUNT_POINTS *Output = 0;
    ULONG InputSize, OutputSize;
    NTSTATUS Result;

    InputSize = sizeof *Input + sizeof L"\\DosDevices\\X:" - sizeof(WCHAR);
    OutputSize = 4096;

    Input = MemAlloc(InputSize);
    if (0 == Input)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Output = MemAlloc(OutputSize);
    if (0 == Output)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    memset(Input, 0, sizeof *Input);
    Input->SymbolicLinkNameOffset = sizeof *Input;
    Input->SymbolicLinkNameLength = sizeof L"\\DosDevices\\X:" - sizeof(WCHAR);
    memcpy((PUINT8)Input + Input->SymbolicLinkNameOffset,
        L"\\DosDevices\\X:", Input->SymbolicLinkNameLength);
    ((PWCHAR)((PUINT8)Input + Input->SymbolicLinkNameOffset))[12] = MountPoint[4] & ~0x20;
        /* convert to uppercase */
    Result = FspMountmgrControl(
        CTL_CODE('m', 1, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS),
            /* IOCTL_MOUNTMGR_DELETE_POINTS */
        Input, InputSize, Output, &OutputSize);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    MemFree(Output);
    MemFree(Input);

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

static NTSTATUS FspMountSet_Drive(PWSTR VolumeName, PWSTR MountPoint, PHANDLE PMountHandle)
{
    NTSTATUS Result;
    BOOLEAN IsLocalSystem, IsServiceContext;

    *PMountHandle = 0;

    Result = FspServiceContextCheck(0, &IsLocalSystem);
    IsServiceContext = NT_SUCCESS(Result) && !IsLocalSystem;
    if (IsServiceContext && !FspMountDoNotUseLauncher)
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
        Result = DefineDosDeviceW(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
            MountPoint, VolumeName) ? STATUS_SUCCESS : FspNtStatusFromWin32(GetLastError());

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

    MountHandle = CreateFileW(MountPoint,
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &SecurityAttributes,
        CREATE_NEW,
        FILE_ATTRIBUTE_DIRECTORY |
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE,
        0);
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
    else if (FspPathIsMountmgrMountPoint(Desc->MountPoint))
        return FspMountSet_Mountmgr(Desc->VolumeHandle, Desc->VolumeName, Desc->MountPoint);
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

    if (FspPathIsMountmgrMountPoint(Desc->MountPoint))
        return FspMountRemove_Mountmgr(Desc->MountPoint);
    else if (FspPathIsDrive(Desc->MountPoint))
        return FspMountRemove_Drive(Desc->VolumeName, Desc->MountPoint, Desc->MountHandle);
    else
        return FspMountRemove_Directory(Desc->MountHandle);
}
