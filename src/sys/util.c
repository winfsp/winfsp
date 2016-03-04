/**
 * @file sys/util.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

BOOLEAN FspUnicodePathIsValid(PUNICODE_STRING Path, BOOLEAN AllowStreams);
VOID FspUnicodePathSuffix(PUNICODE_STRING Path, PUNICODE_STRING Remain, PUNICODE_STRING Suffix);
NTSTATUS FspCreateGuid(GUID *Guid);
NTSTATUS FspLockUserBuffer(PVOID UserBuffer, ULONG Length,
    KPROCESSOR_MODE RequestorMode, LOCK_OPERATION Operation, PMDL *PMdl);
NTSTATUS FspMapLockedPagesInUserMode(PMDL Mdl, PVOID *PAddress);
NTSTATUS FspCcInitializeCacheMap(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes,
    BOOLEAN PinAccess, PCACHE_MANAGER_CALLBACKS Callbacks, PVOID CallbackContext);
NTSTATUS FspCcSetFileSizes(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes);
NTSTATUS FspCcCopyWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    BOOLEAN Wait, PVOID Buffer);
NTSTATUS FspCcPrepareMdlWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    PMDL *PMdlChain, PIO_STATUS_BLOCK IoStatus);
NTSTATUS FspCcMdlWriteComplete(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, PMDL MdlChain);
NTSTATUS FspQuerySecurityDescriptorInfo(SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PULONG PLength,
    PSECURITY_DESCRIPTOR ObjectsSecurityDescriptor);
VOID FspInitializeSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspExecuteSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem);
static WORKER_THREAD_ROUTINE FspExecuteSynchronousWorkItemRoutine;
VOID FspInitializeDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspQueueDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem, LARGE_INTEGER Delay);
static KDEFERRED_ROUTINE FspQueueDelayedWorkItemDPC;
BOOLEAN FspSafeMdlCheck(PMDL Mdl);
NTSTATUS FspSafeMdlCreate(PMDL UserMdl, LOCK_OPERATION Operation, FSP_SAFE_MDL **PSafeMdl);
VOID FspSafeMdlCopyBack(FSP_SAFE_MDL *SafeMdl);
VOID FspSafeMdlDelete(FSP_SAFE_MDL *SafeMdl);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspUnicodePathIsValid)
#pragma alloc_text(PAGE, FspUnicodePathSuffix)
#pragma alloc_text(PAGE, FspCreateGuid)
#pragma alloc_text(PAGE, FspLockUserBuffer)
#pragma alloc_text(PAGE, FspMapLockedPagesInUserMode)
#pragma alloc_text(PAGE, FspCcInitializeCacheMap)
#pragma alloc_text(PAGE, FspCcSetFileSizes)
#pragma alloc_text(PAGE, FspCcCopyWrite)
#pragma alloc_text(PAGE, FspCcPrepareMdlWrite)
#pragma alloc_text(PAGE, FspCcMdlWriteComplete)
#pragma alloc_text(PAGE, FspQuerySecurityDescriptorInfo)
#pragma alloc_text(PAGE, FspInitializeSynchronousWorkItem)
#pragma alloc_text(PAGE, FspExecuteSynchronousWorkItem)
#pragma alloc_text(PAGE, FspExecuteSynchronousWorkItemRoutine)
#pragma alloc_text(PAGE, FspInitializeDelayedWorkItem)
#pragma alloc_text(PAGE, FspQueueDelayedWorkItem)
#pragma alloc_text(PAGE, FspSafeMdlCheck)
#pragma alloc_text(PAGE, FspSafeMdlCreate)
#pragma alloc_text(PAGE, FspSafeMdlCopyBack)
#pragma alloc_text(PAGE, FspSafeMdlDelete)
#endif

static const LONG Delays[] =
{
    -100,
    -200,
    -300,
    -400,
    -500,
    -1000,
};

PVOID FspAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag)
{
    // !PAGED_CODE();

    PVOID Result;
    LARGE_INTEGER Delay;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = DEBUGRANDTEST(95, TRUE) ? ExAllocatePoolWithTag(PoolType, Size, Tag) : 0;
        if (0 != Result)
            return Result;

        Delay.QuadPart = n > i ? Delays[i] : Delays[n - 1];
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
    }
}

PVOID FspAllocateIrpMustSucceed(CCHAR StackSize)
{
    // !PAGED_CODE();

    PIRP Result;
    LARGE_INTEGER Delay;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = DEBUGRANDTEST(95, TRUE) ? IoAllocateIrp(StackSize, FALSE) : 0;
        if (0 != Result)
            return Result;

        Delay.QuadPart = n > i ? Delays[i] : Delays[n - 1];
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
    }
}

BOOLEAN FspUnicodePathIsValid(PUNICODE_STRING Path, BOOLEAN AllowStreams)
{
    PAGED_CODE();

    PWSTR PathBgn, PathEnd, PathPtr;

    PathBgn = Path->Buffer;
    PathEnd = (PWSTR)((PUINT8)PathBgn + Path->Length);
    PathPtr = PathBgn;

    while (PathEnd > PathPtr)
        if (L'\\' == *PathPtr)
        {
            PathPtr++;
            if (PathEnd > PathPtr && L'\\' == *PathPtr)
                return FALSE;
        }
        else if (!AllowStreams && L':' == *PathPtr)
            return FALSE;
        else
            PathPtr++;

    return TRUE;
}

VOID FspUnicodePathSuffix(PUNICODE_STRING Path, PUNICODE_STRING Remain, PUNICODE_STRING Suffix)
{
    PAGED_CODE();

    PWSTR PathBgn, PathEnd, PathPtr, RemainEnd, SuffixBgn;

    PathBgn = Path->Buffer;
    PathEnd = (PWSTR)((PUINT8)PathBgn + Path->Length);
    PathPtr = PathBgn;

    RemainEnd = PathEnd;
    SuffixBgn = PathEnd;

    while (PathEnd > PathPtr)
        if (L'\\' == *PathPtr)
        {
            RemainEnd = PathPtr++;
            for (; PathEnd > PathPtr && L'\\' == *PathPtr; PathPtr++)
                ;
            SuffixBgn = PathPtr;
        }
        else
            PathPtr++;

    Remain->Length = Remain->MaximumLength = (USHORT)((PUINT8)RemainEnd - (PUINT8)PathBgn);
    Remain->Buffer = PathBgn;
    if (0 == Remain->Length && PathBgn < PathEnd && L'\\' == *PathBgn)
        Remain->Length = Remain->MaximumLength = sizeof(WCHAR);
    Suffix->Length = Suffix->MaximumLength = (USHORT)((PUINT8)PathEnd - (PUINT8)SuffixBgn);
    Suffix->Buffer = SuffixBgn;
}

NTSTATUS FspCreateGuid(GUID *Guid)
{
    PAGED_CODE();

    NTSTATUS Result;

    int Retries = 3;
    do
    {
        Result = ExUuidCreate(Guid);
    } while (!NT_SUCCESS(Result) && 0 < --Retries);

    return Result;
}

NTSTATUS FspLockUserBuffer(PVOID UserBuffer, ULONG Length,
    KPROCESSOR_MODE RequestorMode, LOCK_OPERATION Operation, PMDL *PMdl)
{
    PAGED_CODE();

    *PMdl = 0;

    PMDL Mdl = IoAllocateMdl(UserBuffer, Length, FALSE, FALSE, 0);
    if (0 == Mdl)
        return STATUS_INSUFFICIENT_RESOURCES;

    try
    {
        MmProbeAndLockPages(Mdl, RequestorMode, Operation);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        IoFreeMdl(Mdl);
        return GetExceptionCode();
    }

    *PMdl = Mdl;
    return STATUS_SUCCESS;
}

NTSTATUS FspMapLockedPagesInUserMode(PMDL Mdl, PVOID *PAddress)
{
    PAGED_CODE();

    try
    {
        *PAddress = MmMapLockedPagesSpecifyCache(Mdl, UserMode, MmCached, 0, FALSE, NormalPagePriority);
        return 0 != *PAddress ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        *PAddress = 0;
        return GetExceptionCode();
    }
}

NTSTATUS FspCcInitializeCacheMap(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes,
    BOOLEAN PinAccess, PCACHE_MANAGER_CALLBACKS Callbacks, PVOID CallbackContext)
{
    PAGED_CODE();

    try
    {
        CcInitializeCacheMap(FileObject, FileSizes, PinAccess, Callbacks, CallbackContext);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcSetFileSizes(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes)
{
    PAGED_CODE();

    try
    {
        CcSetFileSizes(FileObject, FileSizes);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcCopyWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    BOOLEAN Wait, PVOID Buffer)
{
    PAGED_CODE();

    try
    {
        BOOLEAN Success = CcCopyWrite(FileObject, FileOffset, Length, Wait, Buffer);
        return Success ? STATUS_SUCCESS : STATUS_PENDING;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcPrepareMdlWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    PMDL *PMdlChain, PIO_STATUS_BLOCK IoStatus)
{
    PAGED_CODE();

    NTSTATUS Result;

    *PMdlChain = 0;

    try
    {
        CcPrepareMdlWrite(FileObject, FileOffset, Length, PMdlChain, IoStatus);
        Result = IoStatus->Status;
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    if (!NT_SUCCESS(Result))
    {
        if (0 != *PMdlChain)
        {
            CcMdlWriteAbort(FileObject, *PMdlChain);
            *PMdlChain = 0;
        }

        IoStatus->Information = 0;
        IoStatus->Status = Result;
    }

    return Result;
}

NTSTATUS FspCcMdlWriteComplete(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, PMDL MdlChain)
{
    PAGED_CODE();

    try
    {
        CcMdlWriteComplete(FileObject, FileOffset, MdlChain);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        CcMdlWriteAbort(FileObject, MdlChain);
        return GetExceptionCode();
    }
}

NTSTATUS FspQuerySecurityDescriptorInfo(SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PULONG PLength,
    PSECURITY_DESCRIPTOR ObjectsSecurityDescriptor)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        Result = SeQuerySecurityDescriptorInfo(&SecurityInformation,
            SecurityDescriptor, PLength, &ObjectsSecurityDescriptor);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = STATUS_INVALID_USER_BUFFER;
    }

    return STATUS_BUFFER_TOO_SMALL == Result ? STATUS_BUFFER_OVERFLOW : Result;
}

VOID FspInitializeSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context)
{
    PAGED_CODE();

    KeInitializeEvent(&SynchronousWorkItem->Event, NotificationEvent, FALSE);
    SynchronousWorkItem->Routine = Routine;
    SynchronousWorkItem->Context = Context;
    ExInitializeWorkItem(&SynchronousWorkItem->WorkQueueItem,
        FspExecuteSynchronousWorkItemRoutine, SynchronousWorkItem);
}

VOID FspExecuteSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem)
{
    PAGED_CODE();

    ExQueueWorkItem(&SynchronousWorkItem->WorkQueueItem, CriticalWorkQueue);

    NTSTATUS Result;
    Result = KeWaitForSingleObject(&SynchronousWorkItem->Event, Executive, KernelMode, FALSE, 0);
    ASSERT(STATUS_SUCCESS == Result);
}

static VOID FspExecuteSynchronousWorkItemRoutine(PVOID Context)
{
    PAGED_CODE();

    FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem = Context;
    SynchronousWorkItem->Routine(SynchronousWorkItem->Context);
    KeSetEvent(&SynchronousWorkItem->Event, 1, FALSE);
}

VOID FspInitializeDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context)
{
    PAGED_CODE();

    KeInitializeTimer(&DelayedWorkItem->Timer);
    KeInitializeDpc(&DelayedWorkItem->Dpc, FspQueueDelayedWorkItemDPC, DelayedWorkItem);
    ExInitializeWorkItem(&DelayedWorkItem->WorkQueueItem, Routine, Context);
}

VOID FspQueueDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem, LARGE_INTEGER Delay)
{
    PAGED_CODE();

    KeSetTimer(&DelayedWorkItem->Timer, Delay, &DelayedWorkItem->Dpc);
}

static VOID FspQueueDelayedWorkItemDPC(PKDPC Dpc,
    PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
    // !PAGED_CODE();

    FSP_DELAYED_WORK_ITEM *DelayedWorkItem = DeferredContext;

    ExQueueWorkItem(&DelayedWorkItem->WorkQueueItem, DelayedWorkQueue);
}

BOOLEAN FspSafeMdlCheck(PMDL Mdl)
{
    PAGED_CODE();

    PVOID VirtualAddress = MmGetMdlVirtualAddress(Mdl);
    ULONG ByteCount = MmGetMdlByteCount(Mdl);

    return 0 == BYTE_OFFSET(VirtualAddress) && 0 == BYTE_OFFSET(ByteCount);
}

NTSTATUS FspSafeMdlCreate(PMDL UserMdl, LOCK_OPERATION Operation, FSP_SAFE_MDL **PSafeMdl)
{
    PAGED_CODE();

    NTSTATUS Result;
    PVOID VirtualAddress = MmGetMdlVirtualAddress(UserMdl);
    ULONG ByteCount = MmGetMdlByteCount(UserMdl);
    ULONG PageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, ByteCount);
    FSP_SAFE_MDL *SafeMdl;
    PMDL TempMdl;
    PPFN_NUMBER UserPfnArray, SafePfnArray, TempPfnArray;
    ULONG ByteOffsetBgn0, ByteOffsetEnd0, ByteOffsetEnd1;
    BOOLEAN Buffer0, Buffer1;
    ULONG BufferPageCount;

    ASSERT(0 != PageCount);

    *PSafeMdl = 0;

    SafeMdl = FspAllocNonPaged(sizeof *SafeMdl);
    if (0 == SafeMdl)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    RtlZeroMemory(SafeMdl, sizeof *SafeMdl);

    SafeMdl->Mdl = IoAllocateMdl(VirtualAddress, ByteCount, FALSE, FALSE, 0);
    if (0 == SafeMdl->Mdl)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    UserPfnArray = MmGetMdlPfnArray(UserMdl);
    SafePfnArray = MmGetMdlPfnArray(SafeMdl->Mdl);
    RtlCopyMemory(SafePfnArray, UserPfnArray, PageCount * sizeof(PFN_NUMBER));

    /*
     * Possible cases:
     *
     * ----+---------+---------+----
     *
     *     *---------*         +
     *     +    *----*         +
     *     *----*    +         +
     *     +  *---*  +         +
     *     *--------...--------*
     *     +    *---...--------*
     *     *--------...---*    +
     *     +    *---...---*    +
     */
    if (1 == PageCount)
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = BYTE_OFFSET(ByteCount + (PAGE_SIZE - 1));
        ByteOffsetEnd1 = 0;
        Buffer0 = 0 != ByteOffsetBgn0 || PAGE_SIZE != ByteOffsetEnd0;
        Buffer1 = FALSE;
    }
    else
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = PAGE_SIZE;
        ByteOffsetEnd1 = BYTE_OFFSET((PUINT8)VirtualAddress + ByteCount + (PAGE_SIZE - 1));
        Buffer0 = 0 != ByteOffsetBgn0;
        Buffer1 = PAGE_SIZE != ByteOffsetEnd1;
    }
    BufferPageCount = Buffer0 + Buffer1;

    if (0 < BufferPageCount)
    {
        SafeMdl->Buffer = FspAllocNonPaged(PAGE_SIZE * BufferPageCount);
        if (0 == SafeMdl->Buffer)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        TempMdl = IoAllocateMdl(SafeMdl->Buffer, PAGE_SIZE * BufferPageCount, FALSE, FALSE, 0);
        if (0 == TempMdl)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        MmBuildMdlForNonPagedPool(TempMdl);

        TempPfnArray = MmGetMdlPfnArray(SafeMdl);
        if (IoReadAccess == Operation)
        {
            if (Buffer0)
            {
                RtlZeroMemory((PUINT8)SafeMdl->Buffer, ByteOffsetBgn0);
                RtlCopyMemory((PUINT8)SafeMdl->Buffer + ByteOffsetBgn0,
                    (PUINT8)VirtualAddress + ByteOffsetBgn0, ByteOffsetEnd0 - ByteOffsetBgn0);
                RtlZeroMemory((PUINT8)SafeMdl->Buffer + ByteOffsetEnd0, PAGE_SIZE - ByteOffsetEnd0);
                UserPfnArray[0] = TempPfnArray[0];
            }
            if (Buffer1)
            {
                RtlCopyMemory((PUINT8)SafeMdl->Buffer + (BufferPageCount - 1) * PAGE_SIZE,
                    (PUINT8)VirtualAddress + (PageCount - 1) * PAGE_SIZE, ByteOffsetEnd1);
                RtlZeroMemory((PUINT8)SafeMdl->Buffer + (BufferPageCount - 1) * PAGE_SIZE + ByteOffsetEnd1,
                    PAGE_SIZE - ByteOffsetEnd1);
                UserPfnArray[PageCount - 1] = TempPfnArray[BufferPageCount - 1];
            }
        }
        else
        {
            RtlZeroMemory((PUINT8)SafeMdl->Buffer, PAGE_SIZE * BufferPageCount);
            if (Buffer0)
                UserPfnArray[0] = TempPfnArray[0];
            if (Buffer1)
                UserPfnArray[PageCount - 1] = TempPfnArray[BufferPageCount - 1];
        }

        IoFreeMdl(TempMdl);
    }

    SafeMdl->UserMdl = UserMdl;
    *PSafeMdl = SafeMdl;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != SafeMdl)
    {
        if (0 != SafeMdl->Buffer)
            FspFree(SafeMdl->Buffer);
        if (0 != SafeMdl->Mdl)
            IoFreeMdl(SafeMdl->Mdl);
        FspFree(SafeMdl);
    }

    return Result;
}

VOID FspSafeMdlCopyBack(FSP_SAFE_MDL *SafeMdl)
{
    PAGED_CODE();

    if (IoReadAccess == SafeMdl->Operation)
        return;

    PVOID VirtualAddress = MmGetMdlVirtualAddress(SafeMdl->UserMdl);
    ULONG ByteCount = MmGetMdlByteCount(SafeMdl->UserMdl);
    ULONG PageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, ByteCount);
    ULONG ByteOffsetBgn0, ByteOffsetEnd0, ByteOffsetEnd1;
    BOOLEAN Buffer0, Buffer1;
    ULONG BufferPageCount;

    /*
     * Possible cases:
     *
     * ----+---------+---------+----
     *
     *     *---------*         +
     *     +    *----*         +
     *     *----*    +         +
     *     +  *---*  +         +
     *     *--------...--------*
     *     +    *---...--------*
     *     *--------...---*    +
     *     +    *---...---*    +
     */
    if (1 == PageCount)
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = BYTE_OFFSET(ByteCount + (PAGE_SIZE - 1));
        ByteOffsetEnd1 = 0;
        Buffer0 = 0 != ByteOffsetBgn0 || PAGE_SIZE != ByteOffsetEnd0;
        Buffer1 = FALSE;
    }
    else
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = PAGE_SIZE;
        ByteOffsetEnd1 = BYTE_OFFSET((PUINT8)VirtualAddress + ByteCount + (PAGE_SIZE - 1));
        Buffer0 = 0 != ByteOffsetBgn0;
        Buffer1 = PAGE_SIZE != ByteOffsetEnd1;
    }
    BufferPageCount = Buffer0 + Buffer1;

    if (0 < BufferPageCount)
    {
        if (Buffer0)
            RtlCopyMemory((PUINT8)VirtualAddress + ByteOffsetBgn0,
                (PUINT8)SafeMdl->Buffer + ByteOffsetBgn0, ByteOffsetEnd0 - ByteOffsetBgn0);
        if (Buffer1)
            RtlCopyMemory((PUINT8)VirtualAddress + (PageCount - 1) * PAGE_SIZE,
                (PUINT8)SafeMdl->Buffer + (BufferPageCount - 1) * PAGE_SIZE, ByteOffsetEnd1);
    }
}

VOID FspSafeMdlDelete(FSP_SAFE_MDL *SafeMdl)
{
    PAGED_CODE();

    if (0 != SafeMdl->Buffer)
        FspFree(SafeMdl->Buffer);
    if (0 != SafeMdl->Mdl)
        IoFreeMdl(SafeMdl->Mdl);
    FspFree(SafeMdl);
}
