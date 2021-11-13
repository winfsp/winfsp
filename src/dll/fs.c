/**
 * @file dll/fs.c
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

enum
{
    FspFileSystemDispatcherThreadCountMin = 2,
    FspFileSystemDispatcherDefaultThreadCountMin = 4,
    FspFileSystemDispatcherDefaultThreadCountMax = 16,
};

static FSP_FILE_SYSTEM_INTERFACE FspFileSystemNullInterface;

static INIT_ONCE FspFileSystemInitOnce = INIT_ONCE_STATIC_INIT;
static DWORD FspFileSystemTlsKey = TLS_OUT_OF_INDEXES;

static BOOL WINAPI FspFileSystemInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    FspFileSystemTlsKey = TlsAlloc();
    return TRUE;
}

VOID FspFileSystemFinalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     *
     * We must free our TLS key (if any). We only do so if the library
     * is being explicitly unloaded (rather than the process exiting).
     */

    if (Dynamic && TLS_OUT_OF_INDEXES != FspFileSystemTlsKey)
        TlsFree(FspFileSystemTlsKey);
}

FSP_API NTSTATUS FspFileSystemPreflight(PWSTR DevicePath,
    PWSTR MountPoint)
{
    NTSTATUS Result;
    WCHAR TargetPath[MAX_PATH];
    HANDLE DirHandle;

    Result = FspFsctlPreflight(DevicePath);
    if (!NT_SUCCESS(Result))
        return Result;

    if (0 == MountPoint)
        Result = STATUS_SUCCESS;
    else
    {
        if (FspPathIsMountmgrMountPoint(MountPoint))
            Result = STATUS_SUCCESS; /* cannot check with the mount manager, assume success */
        else if (FspPathIsDrive(MountPoint))
            Result = QueryDosDeviceW(MountPoint, TargetPath, MAX_PATH) ?
                STATUS_OBJECT_NAME_COLLISION : STATUS_SUCCESS;
        else
        {
            DirHandle = CreateFileW(MountPoint,
                FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                0,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                0);
            if (INVALID_HANDLE_VALUE != DirHandle)
            {
                CloseHandle(DirHandle);
                Result = STATUS_OBJECT_NAME_COLLISION;
            }
            else if (ERROR_FILE_NOT_FOUND != GetLastError())
                Result = STATUS_OBJECT_NAME_INVALID;
            else
                Result = STATUS_SUCCESS;
        }
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem)
{
    NTSTATUS Result;
    FSP_FILE_SYSTEM *FileSystem;

    *PFileSystem = 0;

    if (VolumeParams->UmFileContextIsUserContext2 &&
        VolumeParams->UmFileContextIsFullContext)
        return STATUS_INVALID_PARAMETER;

    if (0 == Interface)
        Interface = &FspFileSystemNullInterface;

    InitOnceExecuteOnce(&FspFileSystemInitOnce, FspFileSystemInitialize, 0, 0);
    if (TLS_OUT_OF_INDEXES == FspFileSystemTlsKey)
        return STATUS_INSUFFICIENT_RESOURCES;

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
    FileSystem->Operations[FspFsctlTransactQueryEaKind] = FspFileSystemOpQueryEa;
    FileSystem->Operations[FspFsctlTransactSetEaKind] = FspFileSystemOpSetEa;
    FileSystem->Operations[FspFsctlTransactFlushBuffersKind] = FspFileSystemOpFlushBuffers;
    FileSystem->Operations[FspFsctlTransactQueryVolumeInformationKind] = FspFileSystemOpQueryVolumeInformation;
    FileSystem->Operations[FspFsctlTransactSetVolumeInformationKind] = FspFileSystemOpSetVolumeInformation;
    FileSystem->Operations[FspFsctlTransactQueryDirectoryKind] = FspFileSystemOpQueryDirectory;
    FileSystem->Operations[FspFsctlTransactFileSystemControlKind] = FspFileSystemOpFileSystemControl;
    FileSystem->Operations[FspFsctlTransactDeviceControlKind] = FspFileSystemOpDeviceControl;
    FileSystem->Operations[FspFsctlTransactQuerySecurityKind] = FspFileSystemOpQuerySecurity;
    FileSystem->Operations[FspFsctlTransactSetSecurityKind] = FspFileSystemOpSetSecurity;
    FileSystem->Operations[FspFsctlTransactQueryStreamInformationKind] = FspFileSystemOpQueryStreamInformation;
    FileSystem->Interface = Interface;

    FileSystem->OpGuardStrategy = FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE;
    InitializeSRWLock(&FileSystem->OpGuardLock);
    FileSystem->EnterOperation = FspFileSystemOpEnter;
    FileSystem->LeaveOperation = FspFileSystemOpLeave;

    FileSystem->UmFileContextIsUserContext2 = !!VolumeParams->UmFileContextIsUserContext2;
    FileSystem->UmFileContextIsFullContext = !!VolumeParams->UmFileContextIsFullContext;
    FileSystem->UmNoReparsePointsDirCheck = VolumeParams->UmNoReparsePointsDirCheck;

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
    return FspFileSystemSetMountPointEx(FileSystem, MountPoint, 0);
}

FSP_API NTSTATUS FspFileSystemSetMountPointEx(FSP_FILE_SYSTEM *FileSystem, PWSTR MountPoint,
    PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    if (0 != FileSystem->MountPoint)
        return STATUS_INVALID_PARAMETER;

    FSP_MOUNT_DESC Desc;
    int Size;
    NTSTATUS Result;

    memset(&Desc, 0, sizeof Desc);
    Desc.VolumeHandle = FileSystem->VolumeHandle;
    Desc.VolumeName = FileSystem->VolumeName;
    Desc.Security = SecurityDescriptor;

    if (0 == MountPoint)
        MountPoint = L"*:";

    Size = (lstrlenW(MountPoint) + 1) * sizeof(WCHAR);
    Desc.MountPoint = MemAlloc(Size);
    if (0 == Desc.MountPoint)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memcpy(Desc.MountPoint, MountPoint, Size);

    Result = FspMountSet(&Desc);

exit:
    if (NT_SUCCESS(Result))
    {
        FileSystem->MountPoint = Desc.MountPoint;
        FileSystem->MountHandle = Desc.MountHandle;
    }
    else
        MemFree(Desc.MountPoint);

    return Result;
}

FSP_API VOID FspFileSystemRemoveMountPoint(FSP_FILE_SYSTEM *FileSystem)
{
    if (0 == FileSystem->MountPoint)
        return;

    FSP_MOUNT_DESC Desc;

    memset(&Desc, 0, sizeof Desc);
    Desc.VolumeHandle = FileSystem->VolumeHandle;
    Desc.VolumeName = FileSystem->VolumeName;
    Desc.MountPoint = FileSystem->MountPoint;
    Desc.MountHandle = FileSystem->MountHandle;

    FspMountRemove(&Desc);

    MemFree(FileSystem->MountPoint);
    FileSystem->MountPoint = 0;
    FileSystem->MountHandle = 0;
}

static DWORD WINAPI FspFileSystemDispatcherThread(PVOID FileSystem0)
{
    FSP_FILE_SYSTEM *FileSystem = FileSystem0;
    NTSTATUS Result;
    SIZE_T RequestSize, ResponseSize;
    FSP_FSCTL_TRANSACT_REQ *Request = 0;
    FSP_FSCTL_TRANSACT_RSP *Response = 0;
    FSP_FILE_SYSTEM_OPERATION_CONTEXT OperationContext;
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

    OperationContext.Request = Request;
    OperationContext.Response = Response;
    TlsSetValue(FspFileSystemTlsKey, &OperationContext);

#if defined(FSP_CFG_REJECT_EARLY_IRP)
    Result = FspFsctlTransact(FileSystem->VolumeHandle, 0, 0, 0, 0, FALSE);
        /* send a Transact0 to inform the FSD that the dispatcher is ready */
    if (!NT_SUCCESS(Result))
        goto exit;
#endif

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
    TlsSetValue(FspFileSystemTlsKey, 0);
    MemFree(Response);
    MemFree(Request);

    FspFileSystemSetDispatcherResult(FileSystem, Result);

    FspFsctlStop0(FileSystem->VolumeHandle);

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

        if (ThreadCount < FspFileSystemDispatcherDefaultThreadCountMin)
            ThreadCount = FspFileSystemDispatcherDefaultThreadCountMin;
        else if (ThreadCount > FspFileSystemDispatcherDefaultThreadCountMax)
            ThreadCount = FspFileSystemDispatcherDefaultThreadCountMax;
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

    FspFsctlStop0(FileSystem->VolumeHandle);

    WaitForSingleObject(FileSystem->DispatcherThread, INFINITE);
    CloseHandle(FileSystem->DispatcherThread);
    FileSystem->DispatcherThread = 0;

    FspFsctlStop(FileSystem->VolumeHandle);
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

        FspFsctlStop0(FileSystem->VolumeHandle);
    }
}

FSP_API FSP_FILE_SYSTEM_OPERATION_CONTEXT *FspFileSystemGetOperationContext(VOID)
{
    return (FSP_FILE_SYSTEM_OPERATION_CONTEXT *)TlsGetValue(FspFileSystemTlsKey);
}

FSP_API NTSTATUS FspFileSystemNotifyBegin(FSP_FILE_SYSTEM *FileSystem, ULONG Timeout)
{
    static const ULONG Delays[] =
    {
         10/*ms*/,
         10/*ms*/,
         50/*ms*/,
         50/*ms*/,
        100/*ms*/,
        100/*ms*/,
        300/*ms*/,
    };
    ULONG Total = 0, Delay;
    NTSTATUS Result;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = FspFsctlNotify(FileSystem->VolumeHandle, 0, 0);
        if (STATUS_CANT_WAIT != Result)
            return Result;

        Delay = n > i ? Delays[i] : Delays[n - 1];
        if (INFINITE == Timeout)
            Sleep(Delay);
        else
        {
            if (Total >= Timeout)
                break;
            if (Total + Delay > Timeout)
                Delay = Timeout - Total;
            Total += Delay;
            Sleep(Delay);
        }
    }

    return Result;
}

FSP_API NTSTATUS FspFileSystemNotifyEnd(FSP_FILE_SYSTEM *FileSystem)
{
    FSP_FSCTL_NOTIFY_INFO NotifyInfo;

    memset(&NotifyInfo, 0, sizeof NotifyInfo);
    return FspFsctlNotify(FileSystem->VolumeHandle, &NotifyInfo, sizeof NotifyInfo.Size);
}

FSP_API NTSTATUS FspFileSystemNotify(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo, SIZE_T Size)
{
    return FspFsctlNotify(FileSystem->VolumeHandle, NotifyInfo, Size);
}

/*
 * Out-of-Line
 */

FSP_API PWSTR FspFileSystemMountPointF(FSP_FILE_SYSTEM *FileSystem)
{
    return FspFileSystemMountPoint(FileSystem);
}

FSP_API NTSTATUS FspFileSystemEnterOperationF(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return FspFileSystemEnterOperation(FileSystem, Request, Response);
}

FSP_API NTSTATUS FspFileSystemLeaveOperationF(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    return FspFileSystemLeaveOperation(FileSystem, Request, Response);
}

FSP_API VOID FspFileSystemSetOperationGuardF(FSP_FILE_SYSTEM *FileSystem,
    FSP_FILE_SYSTEM_OPERATION_GUARD *EnterOperation,
    FSP_FILE_SYSTEM_OPERATION_GUARD *LeaveOperation)
{
    FspFileSystemSetOperationGuard(FileSystem, EnterOperation, LeaveOperation);
}

FSP_API VOID FspFileSystemSetOperationGuardStrategyF(FSP_FILE_SYSTEM *FileSystem,
    FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY GuardStrategy)
{
    FspFileSystemSetOperationGuardStrategy(FileSystem, GuardStrategy);
}

FSP_API VOID FspFileSystemSetOperationF(FSP_FILE_SYSTEM *FileSystem,
    ULONG Index,
    FSP_FILE_SYSTEM_OPERATION *Operation)
{
    FspFileSystemSetOperation(FileSystem, Index, Operation);
}

FSP_API VOID FspFileSystemGetDispatcherResultF(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS *PDispatcherResult)
{
    FspFileSystemGetDispatcherResult(FileSystem, PDispatcherResult);
}

FSP_API VOID FspFileSystemSetDispatcherResultF(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS DispatcherResult)
{
    FspFileSystemSetDispatcherResult(FileSystem, DispatcherResult);
}

FSP_API VOID FspFileSystemSetDebugLogF(FSP_FILE_SYSTEM *FileSystem,
    UINT32 DebugLog)
{
    FspFileSystemSetDebugLog(FileSystem, DebugLog);
}

FSP_API BOOLEAN FspFileSystemIsOperationCaseSensitiveF(VOID)
{
    return FspFileSystemIsOperationCaseSensitive();
}

FSP_API UINT32 FspFileSystemOperationProcessIdF(VOID)
{
    return FspFileSystemOperationProcessId();
}
