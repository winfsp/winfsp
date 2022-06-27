/**
 * @file shared/ku/mountmgr.c
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

#include <shared/ku/library.h>

#pragma warning(push)
#pragma warning(disable:4459)           /* declaration of 'identifier' hides global declaration */

static NTSTATUS FspMountmgrControl(ULONG IoControlCode,
    PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, PULONG POutputBufferLength);
static NTSTATUS FspMountmgrNotifyVolumeArrival(
    PUNICODE_STRING VolumeName, GUID *UniqueId);
static NTSTATUS FspMountmgrNotifyMountPoint(
    PUNICODE_STRING VolumeName, PUNICODE_STRING MountPoint, BOOLEAN Created);
static NTSTATUS FspMountmgrCreateMountPoint(
    PUNICODE_STRING VolumeName, PUNICODE_STRING MountPoint);
static NTSTATUS FspMountmgrDeleteMountPoint(
    PUNICODE_STRING MountPoint);
static VOID FspMountmgrDeleteRegistry(
    GUID *UniqueId);
NTSTATUS FspMountmgrCreateDrive(
    PUNICODE_STRING VolumeName, GUID *UniqueId, PUNICODE_STRING MountPoint);
NTSTATUS FspMountmgrDeleteDrive(
    PUNICODE_STRING MountPoint);
NTSTATUS FspMountmgrNotifyCreateDirectory(
    PUNICODE_STRING VolumeName, GUID *UniqueId, PUNICODE_STRING MountPoint);
NTSTATUS FspMountmgrNotifyDeleteDirectory(
    PUNICODE_STRING VolumeName, PUNICODE_STRING MountPoint);

#if defined(_KERNEL_MODE)
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspMountmgrControl)
#pragma alloc_text(PAGE, FspMountmgrNotifyVolumeArrival)
#pragma alloc_text(PAGE, FspMountmgrNotifyMountPoint)
#pragma alloc_text(PAGE, FspMountmgrCreateMountPoint)
#pragma alloc_text(PAGE, FspMountmgrDeleteMountPoint)
#pragma alloc_text(PAGE, FspMountmgrCreateDrive)
#pragma alloc_text(PAGE, FspMountmgrDeleteDrive)
#pragma alloc_text(PAGE, FspMountmgrNotifyCreateDirectory)
#pragma alloc_text(PAGE, FspMountmgrNotifyDeleteDirectory)
#endif
#endif

static NTSTATUS FspMountmgrControl(ULONG IoControlCode,
    PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, PULONG POutputBufferLength)
{
#if defined(_KERNEL_MODE)
    FSP_KU_CODE;

    UNICODE_STRING DeviceName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatus;
    HANDLE MgrHandle = 0;
    ULONG Bytes = 0;

    if (0 == POutputBufferLength)
        POutputBufferLength = &Bytes;

    RtlInitUnicodeString(&DeviceName, L"\\Device\\MountPointManager");
    InitializeObjectAttributes(
        &ObjectAttributes,
        &DeviceName,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        0/*RootDirectory*/,
        0);

    IoStatus.Status = ZwOpenFile(
        &MgrHandle,
        GENERIC_READ | GENERIC_WRITE,
        &ObjectAttributes,
        &IoStatus,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_SYNCHRONOUS_IO_ALERT);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    IoStatus.Status = ZwDeviceIoControlFile(
        MgrHandle,
        0, 0, 0,
        &IoStatus,
        IoControlCode,
        InputBuffer, InputBufferLength, OutputBuffer, *POutputBufferLength);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    *POutputBufferLength = (ULONG)IoStatus.Information;
    IoStatus.Status = STATUS_SUCCESS;

exit:
    if (0 != MgrHandle)
        ZwClose(MgrHandle);

    return IoStatus.Status;
#else
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
#endif
}

static NTSTATUS FspMountmgrNotifyVolumeArrival(
    PUNICODE_STRING VolumeName, GUID *UniqueId)
{
    FSP_KU_CODE;

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

    MOUNTMGR_QUERY_AUTO_MOUNT QueryAutoMount;
    MOUNTMGR_SET_AUTO_MOUNT SetAutoMount;
    MOUNTMGR_TARGET_NAME *TargetName = 0;
    ULONG QueryAutoMountSize, TargetNameSize;
    NTSTATUS Result;

    QueryAutoMountSize = sizeof QueryAutoMount;
    TargetNameSize = FIELD_OFFSET(MOUNTMGR_TARGET_NAME, DeviceName) + VolumeName->Length;

    TargetName = MemAlloc(TargetNameSize);
    if (0 == TargetName)
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
    TargetName->DeviceNameLength = VolumeName->Length;
    memcpy(TargetName->DeviceName, VolumeName->Buffer, VolumeName->Length);
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

    Result = STATUS_SUCCESS;

exit:
    MemFree(TargetName);

    return Result;
}

static NTSTATUS FspMountmgrNotifyMountPoint(
    PUNICODE_STRING VolumeName, PUNICODE_STRING MountPoint, BOOLEAN Created)
{
    FSP_KU_CODE;

    /* mountmgr.h */
    typedef struct
    {
        USHORT SourceVolumeNameOffset;
        USHORT SourceVolumeNameLength;
        USHORT TargetVolumeNameOffset;
        USHORT TargetVolumeNameLength;
    } MOUNTMGR_VOLUME_MOUNT_POINT;

    MOUNTMGR_VOLUME_MOUNT_POINT *VolumeMountPoint = 0;
    ULONG VolumeMountPointSize;
    NTSTATUS Result;

    VolumeMountPointSize = sizeof *VolumeMountPoint +
        sizeof L"\\DosDevices\\" - sizeof(WCHAR) + MountPoint->Length + VolumeName->Length;

    VolumeMountPoint = MemAlloc(VolumeMountPointSize);
    if (0 == VolumeMountPoint)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    /* notify volume mount point created/deleted */
    memset(VolumeMountPoint, 0, sizeof *VolumeMountPoint);
    VolumeMountPoint->SourceVolumeNameOffset = sizeof *VolumeMountPoint;
    VolumeMountPoint->SourceVolumeNameLength = (USHORT)(
        sizeof L"\\DosDevices\\" - sizeof(WCHAR) + MountPoint->Length);
    VolumeMountPoint->TargetVolumeNameOffset =
        VolumeMountPoint->SourceVolumeNameOffset + VolumeMountPoint->SourceVolumeNameLength;
    VolumeMountPoint->TargetVolumeNameLength = VolumeName->Length;
    memcpy((PUINT8)VolumeMountPoint + VolumeMountPoint->SourceVolumeNameOffset,
        L"\\DosDevices\\", sizeof L"\\DosDevices\\" - sizeof(WCHAR));
    memcpy((PUINT8)VolumeMountPoint +
        VolumeMountPoint->SourceVolumeNameOffset + (sizeof L"\\DosDevices\\" - sizeof(WCHAR)),
        MountPoint->Buffer, MountPoint->Length);
    memcpy((PUINT8)VolumeMountPoint + VolumeMountPoint->TargetVolumeNameOffset,
        VolumeName->Buffer, VolumeName->Length);
    Result = FspMountmgrControl(
        Created ?
            CTL_CODE('m', 6, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS) :
                /* IOCTL_MOUNTMGR_VOLUME_MOUNT_POINT_CREATED */
            CTL_CODE('m', 7, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS),
                /* IOCTL_MOUNTMGR_VOLUME_MOUNT_POINT_DELETED */
        VolumeMountPoint, VolumeMountPointSize, 0, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    MemFree(VolumeMountPoint);

    return Result;
}

static NTSTATUS FspMountmgrCreateMountPoint(
    PUNICODE_STRING VolumeName, PUNICODE_STRING MountPoint)
{
    FSP_KU_CODE;

    /* mountmgr.h */
    typedef struct
    {
        USHORT SymbolicLinkNameOffset;
        USHORT SymbolicLinkNameLength;
        USHORT DeviceNameOffset;
        USHORT DeviceNameLength;
    } MOUNTMGR_CREATE_POINT_INPUT;

    MOUNTMGR_CREATE_POINT_INPUT *CreatePointInput = 0;
    ULONG CreatePointInputSize;
    NTSTATUS Result;

    CreatePointInputSize = sizeof *CreatePointInput +
        sizeof L"\\DosDevices\\X:" - sizeof(WCHAR) + VolumeName->Length;

    CreatePointInput = MemAlloc(CreatePointInputSize);
    if (0 == CreatePointInput)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    /* create mount point */
    memset(CreatePointInput, 0, sizeof *CreatePointInput);
    CreatePointInput->SymbolicLinkNameOffset = sizeof *CreatePointInput;
    CreatePointInput->SymbolicLinkNameLength = sizeof L"\\DosDevices\\X:" - sizeof(WCHAR);
    CreatePointInput->DeviceNameOffset =
        CreatePointInput->SymbolicLinkNameOffset + CreatePointInput->SymbolicLinkNameLength;
    CreatePointInput->DeviceNameLength = VolumeName->Length;
    memcpy((PUINT8)CreatePointInput + CreatePointInput->SymbolicLinkNameOffset,
        L"\\DosDevices\\X:", CreatePointInput->SymbolicLinkNameLength);
    ((PWCHAR)((PUINT8)CreatePointInput + CreatePointInput->SymbolicLinkNameOffset))[12] =
        MountPoint->Buffer[0] & ~0x20;
        /* convert to uppercase */
    memcpy((PUINT8)CreatePointInput + CreatePointInput->DeviceNameOffset,
        VolumeName->Buffer, VolumeName->Length);
    Result = FspMountmgrControl(
        CTL_CODE('m', 0, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS),
            /* IOCTL_MOUNTMGR_CREATE_POINT */
        CreatePointInput, CreatePointInputSize, 0, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    MemFree(CreatePointInput);

    return Result;
}

static NTSTATUS FspMountmgrDeleteMountPoint(
    PUNICODE_STRING MountPoint)
{
    FSP_KU_CODE;

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

    /* delete mount point */
    memset(Input, 0, sizeof *Input);
    Input->SymbolicLinkNameOffset = sizeof *Input;
    Input->SymbolicLinkNameLength = sizeof L"\\DosDevices\\X:" - sizeof(WCHAR);
    memcpy((PUINT8)Input + Input->SymbolicLinkNameOffset,
        L"\\DosDevices\\X:", Input->SymbolicLinkNameLength);
    ((PWCHAR)((PUINT8)Input + Input->SymbolicLinkNameOffset))[12] =
        MountPoint->Buffer[0] & ~0x20;
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

static VOID FspMountmgrDeleteRegistry(GUID *UniqueId)
{
#if defined(_KERNEL_MODE)
    FSP_KU_CODE;

    UNICODE_STRING Path;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE Handle = 0;
    union
    {
        KEY_VALUE_FULL_INFORMATION V;
        UINT8 B[FIELD_OFFSET(KEY_VALUE_FULL_INFORMATION, Name) + 255 + sizeof *UniqueId];
    } FullInformation;
    ULONG FullInformationLength;
    UNICODE_STRING ValueName;
    NTSTATUS Result;

    RtlInitUnicodeString(&Path, L"\\Registry\\Machine\\System\\MountedDevices");
    InitializeObjectAttributes(
        &ObjectAttributes,
        &Path,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        0/*RootDirectory*/,
        0);

    Result = ZwOpenKey(&Handle, KEY_QUERY_VALUE, &ObjectAttributes);
    if (NT_SUCCESS(Result))
    {
        for (ULONG I = 0;; I++)
        {
            Result = ZwEnumerateValueKey(Handle,
                I, KeyValueFullInformation, &FullInformation,
                sizeof FullInformation, &FullInformationLength);
            if (STATUS_NO_MORE_ENTRIES == Result)
                break;
            else if (!NT_SUCCESS(Result))
                continue;

            if (REG_BINARY == FullInformation.V.Type &&
                sizeof *UniqueId == FullInformation.V.DataLength &&
                InlineIsEqualGUID((GUID *)((PUINT8)&FullInformation.V + FullInformation.V.DataOffset),
                    UniqueId))
            {
                ValueName.Length = ValueName.MaximumLength = (USHORT)FullInformation.V.NameLength;
                ValueName.Buffer = FullInformation.V.Name;
                Result = ZwDeleteValueKey(Handle, &ValueName);
                if (NT_SUCCESS(Result))
                    /* reset index after modifying key; only safe way to use RegEnumValueW with modifications */
                    I = (ULONG)-1;
            }
        }

        ZwClose(Handle);
    }
#else
    HKEY RegKey;
    LONG RegResult;
    WCHAR RegValueName[MAX_PATH];
    UINT8 RegValueData[sizeof *UniqueId];
    DWORD RegValueNameSize, RegValueDataSize;
    DWORD RegType;

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
                InlineIsEqualGUID((GUID *)&RegValueData, UniqueId))
            {
                RegResult = RegDeleteValueW(RegKey, RegValueName);
                if (ERROR_SUCCESS == RegResult)
                    /* reset index after modifying key; only safe way to use RegEnumValueW with modifications */
                    I = -1;
            }
        }

        RegCloseKey(RegKey);
    }
#endif
}

NTSTATUS FspMountmgrCreateDrive(
    PUNICODE_STRING VolumeName, GUID *UniqueId, PUNICODE_STRING MountPoint)
{
    FSP_KU_CODE;

    NTSTATUS Result;

    /* notify the MountManager about the new volume */
    Result = FspMountmgrNotifyVolumeArrival(VolumeName, UniqueId);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* use the MountManager to create the drive */
    Result = FspMountmgrCreateMountPoint(VolumeName, MountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* HACK: delete the MountManager registry entries */
    FspMountmgrDeleteRegistry(UniqueId);

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

NTSTATUS FspMountmgrDeleteDrive(
    PUNICODE_STRING MountPoint)
{
    FSP_KU_CODE;

    NTSTATUS Result;

    /* use the MountManager to delete the drive */
    Result = FspMountmgrDeleteMountPoint(MountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

NTSTATUS FspMountmgrNotifyCreateDirectory(
    PUNICODE_STRING VolumeName, GUID *UniqueId, PUNICODE_STRING MountPoint)
{
    FSP_KU_CODE;

    NTSTATUS Result;

    /* notify the MountManager about the new volume */
    Result = FspMountmgrNotifyVolumeArrival(VolumeName, UniqueId);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* notify the MountManager about the created directory mount point */
    Result = FspMountmgrNotifyMountPoint(VolumeName, MountPoint, TRUE);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* HACK: delete the MountManager registry entries */
    FspMountmgrDeleteRegistry(UniqueId);

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

NTSTATUS FspMountmgrNotifyDeleteDirectory(
    PUNICODE_STRING VolumeName, PUNICODE_STRING MountPoint)
{
    FSP_KU_CODE;

    /* notify the MountManager about the deleted directory mount point */
    return FspMountmgrNotifyMountPoint(VolumeName, MountPoint, FALSE);
}

#pragma warning(pop)
