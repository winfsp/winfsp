/**
 * @file dll/fs.c
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

enum
{
    FspFileSystemDispatcherThreadCountMin = 2,
};

static FSP_FILE_SYSTEM_INTERFACE FspFileSystemNullInterface;

static INIT_ONCE FspFileSystemInitOnce = INIT_ONCE_STATIC_INIT;
static NTSTATUS (NTAPI *FspNtOpenSymbolicLinkObject)(
    PHANDLE LinkHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
static NTSTATUS (NTAPI *FspNtMakeTemporaryObject)(
    HANDLE Handle);
static NTSTATUS (NTAPI *FspNtClose)(
    HANDLE Handle);

static BOOL WINAPI FspFileSystemInitialize(
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

    return TRUE;
}

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem)
{
    NTSTATUS Result;
    FSP_FILE_SYSTEM *FileSystem;

    *PFileSystem = 0;

    if (0 == Interface)
        Interface = &FspFileSystemNullInterface;

    InitOnceExecuteOnce(&FspFileSystemInitOnce, FspFileSystemInitialize, 0, 0);

    FileSystem = MemAlloc(sizeof *FileSystem);
    if (0 == FileSystem)
        return STATUS_INSUFFICIENT_RESOURCES;
    memset(FileSystem, 0, sizeof *FileSystem);

    Result = FspFsctlCreateVolume(DevicePath, VolumeParams,
        FileSystem->VolumeName, sizeof FileSystem->VolumeName,
        &FileSystem->VolumeHandle);
    if (!NT_SUCCESS(Result))
    {
        MemFree(FileSystem);
        return Result;
    }

    FileSystem->Operations[FspFsctlTransactCreateKind] = FspFileSystemOpCreate;
    FileSystem->Operations[FspFsctlTransactOverwriteKind] = FspFileSystemOpOverwrite;
    FileSystem->Operations[FspFsctlTransactCleanupKind] = FspFileSystemOpCleanup;
    FileSystem->Operations[FspFsctlTransactCloseKind] = FspFileSystemOpClose;
    FileSystem->Operations[FspFsctlTransactReadKind] = FspFileSystemOpRead;
    FileSystem->Operations[FspFsctlTransactWriteKind] = FspFileSystemOpWrite;
    FileSystem->Operations[FspFsctlTransactQueryInformationKind] = FspFileSystemOpQueryInformation;
    FileSystem->Operations[FspFsctlTransactSetInformationKind] = FspFileSystemOpSetInformation;
    FileSystem->Operations[FspFsctlTransactFlushBuffersKind] = FspFileSystemOpFlushBuffers;
    FileSystem->Operations[FspFsctlTransactQueryVolumeInformationKind] = FspFileSystemOpQueryVolumeInformation;
    FileSystem->Operations[FspFsctlTransactSetVolumeInformationKind] = FspFileSystemOpSetVolumeInformation;
    FileSystem->Operations[FspFsctlTransactQueryDirectoryKind] = FspFileSystemOpQueryDirectory;
    FileSystem->Operations[FspFsctlTransactFileSystemControlKind] = FspFileSystemOpFileSystemControl;
    FileSystem->Operations[FspFsctlTransactQuerySecurityKind] = FspFileSystemOpQuerySecurity;
    FileSystem->Operations[FspFsctlTransactSetSecurityKind] = FspFileSystemOpSetSecurity;
    FileSystem->Interface = Interface;

    FileSystem->OpGuardStrategy = FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE;
    InitializeSRWLock(&FileSystem->OpGuardLock);
    FileSystem->EnterOperation = FspFileSystemOpEnter;
    FileSystem->LeaveOperation = FspFileSystemOpLeave;

    FileSystem->UmFileNodeIsUserContext2 = !!VolumeParams->UmFileNodeIsUserContext2;

    *PFileSystem = FileSystem;

    return STATUS_SUCCESS;
}

FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem)
{
    FspFileSystemRemoveMountPoint(FileSystem);
    CloseHandle(FileSystem->VolumeHandle);
    MemFree(FileSystem);
}

FSP_API NTSTATUS FspFileSystemSetMountPoint(FSP_FILE_SYSTEM *FileSystem, PWSTR MountPoint)
{
    if (0 != FileSystem->MountPoint)
        return STATUS_INVALID_PARAMETER;

    NTSTATUS Result;
    HANDLE MountHandle = 0;

    if (0 == MountPoint)
    {
        DWORD Drives;
        WCHAR Drive;

        MountPoint = MemAlloc(3 * sizeof(WCHAR));
        if (0 == MountPoint)
            return STATUS_INSUFFICIENT_RESOURCES;
        MountPoint[1] = L':';
        MountPoint[2] = L'\0';

        Drives = GetLogicalDrives();
        if (0 != Drives)
        {
            for (Drive = 'Z'; 'D' <= Drive; Drive--)
                if (0 == (Drives & (1 << (Drive - 'A'))))
                {
                    MountPoint[0] = Drive;
                    if (DefineDosDeviceW(DDD_RAW_TARGET_PATH, MountPoint, FileSystem->VolumeName))
                    {
                        Result = STATUS_SUCCESS;
                        goto exit;
                    }
                }
            Result = STATUS_NO_SUCH_DEVICE;
        }
        else
            Result = FspNtStatusFromWin32(GetLastError());
    }
    else
    {
        PWSTR P;
        ULONG L;

        for (P = MountPoint; *P; P++)
            ;
        L = (ULONG)((P - MountPoint + 1) * sizeof(WCHAR));

        P = MemAlloc(L);
        if (0 == P)
            return STATUS_INSUFFICIENT_RESOURCES;
        memcpy(P, MountPoint, L);
        MountPoint = P;

        if (DefineDosDeviceW(DDD_RAW_TARGET_PATH, MountPoint, FileSystem->VolumeName))
            Result = STATUS_SUCCESS;
        else
            Result = FspNtStatusFromWin32(GetLastError());
    }

exit:
    if (NT_SUCCESS(Result) && 0 != FspNtOpenSymbolicLinkObject)
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

        Result = FspNtOpenSymbolicLinkObject(&MountHandle, DELETE, &Obja);
        if (NT_SUCCESS(Result))
        {
            Result = FspNtMakeTemporaryObject(MountHandle);
            if (!NT_SUCCESS(Result))
            {
                FspNtClose(MountHandle);
                MountHandle = 0;
            }
        }

        /* this path always considered successful regardless if we made symlink temporary */
        Result = STATUS_SUCCESS;
    }

    if (NT_SUCCESS(Result))
    {
        FileSystem->MountPoint = MountPoint;
        FileSystem->MountHandle = MountHandle;
    }
    else
        MemFree(MountPoint);

    return Result;
}

FSP_API VOID FspFileSystemRemoveMountPoint(FSP_FILE_SYSTEM *FileSystem)
{
    if (0 == FileSystem->MountPoint)
        return;

    DefineDosDeviceW(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
        FileSystem->MountPoint, FileSystem->VolumeName);
    MemFree(FileSystem->MountPoint);
    FileSystem->MountPoint = 0;

    if (0 != FileSystem->MountHandle)
    {
        FspNtClose(FileSystem->MountHandle);
        FileSystem->MountHandle = 0;
    }
}

static DWORD WINAPI FspFileSystemDispatcherThread(PVOID FileSystem0)
{
    FSP_FILE_SYSTEM *FileSystem = FileSystem0;
    NTSTATUS Result;
    SIZE_T RequestSize, ResponseSize;
    FSP_FSCTL_TRANSACT_REQ *Request = 0;
    FSP_FSCTL_TRANSACT_RSP *Response = 0;
    HANDLE DispatcherThread = 0;

    Request = MemAlloc(FSP_FSCTL_TRANSACT_BUFFER_SIZEMIN);
    Response = MemAlloc(FSP_FSCTL_TRANSACT_RSP_SIZEMAX);
    if (0 == Request || 0 == Response)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (1 < FileSystem->DispatcherThreadCount)
    {
        FileSystem->DispatcherThreadCount--;
        DispatcherThread = CreateThread(0, 0, FspFileSystemDispatcherThread, FileSystem, 0, 0);
        if (0 == DispatcherThread)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    memset(Response, 0, sizeof *Response);
    for (;;)
    {
        RequestSize = FSP_FSCTL_TRANSACT_BUFFER_SIZEMIN;
        Result = FspFsctlTransact(FileSystem->VolumeHandle,
            Response, Response->Size, Request, &RequestSize, FALSE);
        if (!NT_SUCCESS(Result))
            goto exit;

        memset(Response, 0, sizeof *Response);
        if (0 == RequestSize)
            continue;

        if (FileSystem->DebugLog)
        {
            if (FspFsctlTransactKindCount <= Request->Kind ||
                (FileSystem->DebugLog & (1 << Request->Kind)))
                FspDebugLogRequest(Request);
        }

        Response->Size = sizeof *Response;
        Response->Kind = Request->Kind;
        Response->Hint = Request->Hint;
        if (FspFsctlTransactKindCount > Request->Kind && 0 != FileSystem->Operations[Request->Kind])
        {
            Response->IoStatus.Status =
                FspFileSystemEnterOperation(FileSystem, Request, Response);
            if (NT_SUCCESS(Response->IoStatus.Status))
            {
                Response->IoStatus.Status =
                    FileSystem->Operations[Request->Kind](FileSystem, Request, Response);
                FspFileSystemLeaveOperation(FileSystem, Request, Response);
            }
        }
        else
            Response->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

        if (FileSystem->DebugLog)
        {
            if (FspFsctlTransactKindCount <= Response->Kind ||
                (FileSystem->DebugLog & (1 << Response->Kind)))
                FspDebugLogResponse(Response);
        }

        ResponseSize = FSP_FSCTL_DEFAULT_ALIGN_UP(Response->Size);
        if (FSP_FSCTL_TRANSACT_RSP_SIZEMAX < ResponseSize/* should NOT happen */)
        {
            memset(Response, 0, sizeof *Response);
            Response->Size = sizeof *Response;
            Response->Kind = Request->Kind;
            Response->Hint = Request->Hint;
            Response->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
        }
        else if (STATUS_PENDING == Response->IoStatus.Status)
            memset(Response, 0, sizeof *Response);
        else
        {
            memset((PUINT8)Response + Response->Size, 0, ResponseSize - Response->Size);
            Response->Size = (UINT16)ResponseSize;
        }
    }

exit:
    MemFree(Response);
    MemFree(Request);

    FspFileSystemSetDispatcherResult(FileSystem, Result);

    FspFsctlStop(FileSystem->VolumeHandle);

    if (0 != DispatcherThread)
    {
        WaitForSingleObject(DispatcherThread, INFINITE);
        CloseHandle(DispatcherThread);
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemStartDispatcher(FSP_FILE_SYSTEM *FileSystem, ULONG ThreadCount)
{
    if (0 != FileSystem->DispatcherThread)
        return STATUS_INVALID_PARAMETER;

    if (0 == ThreadCount)
    {
        DWORD_PTR ProcessMask, SystemMask;

        if (!GetProcessAffinityMask(GetCurrentProcess(), &ProcessMask, &SystemMask))
            return FspNtStatusFromWin32(GetLastError());

        for (ThreadCount = 0; 0 != ProcessMask; ProcessMask >>= 1)
            ThreadCount += ProcessMask & 1;
    }

    if (ThreadCount < FspFileSystemDispatcherThreadCountMin)
        ThreadCount = FspFileSystemDispatcherThreadCountMin;

    FileSystem->DispatcherThreadCount = ThreadCount;
    FileSystem->DispatcherThread = CreateThread(0, 0,
        FspFileSystemDispatcherThread, FileSystem, 0, 0);
    if (0 == FileSystem->DispatcherThread)
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

FSP_API VOID FspFileSystemStopDispatcher(FSP_FILE_SYSTEM *FileSystem)
{
    if (0 == FileSystem->DispatcherThread)
        return;

    FspFsctlStop(FileSystem->VolumeHandle);

    WaitForSingleObject(FileSystem->DispatcherThread, INFINITE);
    CloseHandle(FileSystem->DispatcherThread);
    FileSystem->DispatcherThread = 0;
}

FSP_API VOID FspFileSystemSendResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;

    if (FileSystem->DebugLog)
    {
        if (FspFsctlTransactKindCount <= Response->Kind ||
            (FileSystem->DebugLog & (1 << Response->Kind)))
            FspDebugLogResponse(Response);
    }

    Result = FspFsctlTransact(FileSystem->VolumeHandle,
        Response, Response->Size, 0, 0, FALSE);
    if (!NT_SUCCESS(Result))
    {
        FspFileSystemSetDispatcherResult(FileSystem, Result);

        FspFsctlStop(FileSystem->VolumeHandle);
    }
}
