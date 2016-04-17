/**
 * @file dll/fs.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <dll/library.h>

enum
{
    FspFileSystemDispatcherThreadCountMin = 2,
};

static FSP_FILE_SYSTEM_INTERFACE FspFileSystemNullInterface;

static CRITICAL_SECTION FspFileSystemMountListGuard;
static LIST_ENTRY FspFileSystemMountList = { &FspFileSystemMountList, &FspFileSystemMountList };

static inline
BOOL FspFileSystemDefineDosDevice(DWORD Flags, PWSTR MountPoint, FSP_FILE_SYSTEM *FileSystem)
{
    WCHAR TargetBuf[(
        sizeof(((FSP_FILE_SYSTEM *)0)->VolumePrefix) +
        sizeof(((FSP_FILE_SYSTEM *)0)->VolumeName)) / sizeof(WCHAR)];
    PWSTR P;
    ULONG L0, L1;

    for (P = FileSystem->VolumeName; *P; P++)
        ;
    L0 = (ULONG)(P - FileSystem->VolumeName);
    memcpy(TargetBuf, FileSystem->VolumeName, L0 * sizeof(WCHAR));

    for (P = FileSystem->VolumePrefix; *P; P++)
        ;
    L1 = (ULONG)(P - FileSystem->VolumePrefix);
    memcpy(TargetBuf + L0, FileSystem->VolumePrefix, L1 * sizeof(WCHAR));
    TargetBuf[L0 + L1] = L'\0';

    return DefineDosDeviceW(Flags, MountPoint, TargetBuf);
}

VOID FspFileSystemInitialize(VOID)
{
    /*
     * This function is called during DLL_PROCESS_ATTACH. We must therefore keep initialization
     * tasks to a minimum.
     *
     * Initialization of synchronization objects is allowed! See:
     *     https://msdn.microsoft.com/en-us/library/windows/desktop/dn633971(v=vs.85).aspx
     */

    InitializeCriticalSection(&FspFileSystemMountListGuard);
}

VOID FspFileSystemFinalize(VOID)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep finalization
     * tasks to a minimum.
     *
     * Very few things can be safely done during DLL_PROCESS_DETACH. See:
     *     https://msdn.microsoft.com/en-us/library/windows/desktop/dn633971(v=vs.85).aspx
     *     https://blogs.msdn.microsoft.com/oldnewthing/20070503-00/?p=27003/
     *     https://blogs.msdn.microsoft.com/oldnewthing/20100122-00/?p=15193/
     *
     * We enter our FspFileSystemMountListGuard critical section here and then attempt to cleanup
     * our mount points using DefineDosDeviceW. On Vista and later orphaned critical sections
     * become "electrified", so our process will be forcefully terminated if one of its threads
     * was already modifying the list when the ExitProcess happened. This is a good thing!
     *
     * The use of DefineDosDeviceW is rather suspect and probably unsafe. DefineDosDeviceW reaches
     * out to CSRSS, which is probably not the safest thing to do when in DllMain! There is also
     * some evidence that it may attempt to load DLL's under some circumstances, which is a
     * definite no-no as we are under the loader lock!
     */

    FSP_FILE_SYSTEM *FileSystem;
    PLIST_ENTRY MountEntry;

    EnterCriticalSection(&FspFileSystemMountListGuard);

    for (MountEntry = FspFileSystemMountList.Flink;
        &FspFileSystemMountList != MountEntry;
        MountEntry = MountEntry->Flink)
    {
        FileSystem = CONTAINING_RECORD(MountEntry, FSP_FILE_SYSTEM, MountEntry);

        FspFileSystemDefineDosDevice(
            DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
            FileSystem->MountPoint, FileSystem);
    }

    LeaveCriticalSection(&FspFileSystemMountListGuard);
}

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem)
{
    NTSTATUS Result;
    FSP_FILE_SYSTEM *FileSystem;
    ULONG PrefixLength;

    *PFileSystem = 0;

    if (0 == Interface)
        Interface = &FspFileSystemNullInterface;

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

    memcpy(FileSystem->VolumePrefix, VolumeParams->Prefix, sizeof VolumeParams->Prefix);
    FileSystem->VolumePrefix[sizeof FileSystem->VolumePrefix / sizeof(WCHAR) - 1] = L'\0';
    for (PrefixLength = 0; L'\0' != FileSystem->VolumePrefix[PrefixLength]; PrefixLength++)
        ;
    for (; 0 < PrefixLength && L'\\' == FileSystem->VolumePrefix[PrefixLength - 1]; PrefixLength--)
        ;
    FileSystem->VolumePrefix[PrefixLength] = L'\0';

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
    FileSystem->Operations[FspFsctlTransactQuerySecurityKind] = FspFileSystemOpQuerySecurity;
    FileSystem->Operations[FspFsctlTransactSetSecurityKind] = FspFileSystemOpSetSecurity;
    // !!!: ...
    FileSystem->Interface = Interface;

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
                    if (FspFileSystemDefineDosDevice(DDD_RAW_TARGET_PATH, MountPoint, FileSystem))
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

        if (FspFileSystemDefineDosDevice(DDD_RAW_TARGET_PATH, MountPoint, FileSystem))
            Result = STATUS_SUCCESS;
        else
            Result = FspNtStatusFromWin32(GetLastError());
    }

exit:
    if (NT_SUCCESS(Result))
    {
        FileSystem->MountPoint = MountPoint;

        EnterCriticalSection(&FspFileSystemMountListGuard);
        InsertTailList(&FspFileSystemMountList, &FileSystem->MountEntry);
        LeaveCriticalSection(&FspFileSystemMountListGuard);
    }
    else
        MemFree(MountPoint);

    return Result;
}

FSP_API VOID FspFileSystemRemoveMountPoint(FSP_FILE_SYSTEM *FileSystem)
{
    if (0 == FileSystem->MountPoint)
        return;

    EnterCriticalSection(&FspFileSystemMountListGuard);
    RemoveEntryList(&FileSystem->MountEntry);
    LeaveCriticalSection(&FspFileSystemMountListGuard);

    FspFileSystemDefineDosDevice(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE,
        FileSystem->MountPoint, FileSystem);

    MemFree(FileSystem->MountPoint);
    FileSystem->MountPoint = 0;
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

#if 0
        FspDebugLog("FspFileSystemDispatcherThread: TID=%ld, Request={Kind=%d, Hint=%p}\n",
            GetCurrentThreadId(), Request->Kind, (PVOID)Request->Hint);
#endif

        Response->Size = sizeof *Response;
        Response->Kind = Request->Kind;
        Response->Hint = Request->Hint;
        if (FspFsctlTransactKindCount > Request->Kind && 0 != FileSystem->Operations[Request->Kind])
        {
            FspFileSystemEnterOperation(FileSystem, Request, Response);
            Response->IoStatus.Status =
                FileSystem->Operations[Request->Kind](FileSystem, Request, Response);
            FspFileSystemLeaveOperation(FileSystem, Request, Response);
        }
        else
            Response->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;

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
}

FSP_API VOID FspFileSystemSendResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response)
{
    NTSTATUS Result;

    Result = FspFsctlTransact(FileSystem->VolumeHandle,
        Response, Response->Size, 0, 0, FALSE);
    if (!NT_SUCCESS(Result))
    {
        FspFileSystemSetDispatcherResult(FileSystem, Result);

        FspFsctlStop(FileSystem->VolumeHandle);
    }
}
