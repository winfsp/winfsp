/**
 * @file sys/ioq.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

/*
 * Overview
 *
 * An FSP_IOQ encapsulates the main FSP mechanism for handling IRP's.
 * It has two queues: a "Pending" queue for managing newly arrived IRP's
 * and a "Processing" queue for managing IRP's currently being processed
 * (i.e. sent to the user-mode file system for further processing).
 *
 * IRP's arrive at a MajorFunction (MJ) and are then posted to the device's
 * FSP_IOQ and marked pending. When the user-mode file system performs
 * FSP_FSCTL_TRANSACT, the IRP's are removed from the Pending queue and
 * are then marshalled to the user process; prior to that they are added
 * to the Processing queue. At a later time the user-mode will perform
 * another FSP_FSCTL_TRANSACT at which time any processed IRP's will be
 * marshalled back to us and will be then removed from the Processing queue
 * and completed.
 *
 *
 * IRP State Diagram
 *                                 +--------------------+
 *           |                     |                    | StartProcessingIrp
 *           v                     |                    v
 *     +------------+              |              +------------+
 *     |     MJ     |              |              | Processing |
 *     +------------+              |              +------------+
 *           |                     |                    |
 *           | PostIrp             |                    | EndProcessingIrp
 *           v                     |                    v
 *     +------------+              |              +------------+
 *     |   Pending  |              |              |  TRANSACT  |
 *     +------------+              |              |     IN     |
 *           |                     |              +------------+
 *           | NextPendingIrp      |                    |
 *           v                     |                    | CompleteRequest
 *     +------------+              |                    v
 *     |  TRANSACT  |              |              +------------+
 *     |    OUT     |              |              | Completed  |
 *     +------------+              |              +------------+
 *           |                     |
 *           +---------------------+
 *
 *
 * Event Object
 *
 * The FSP_IOQ includes a manual event object. The event object becomes
 * signaled when the FSP_IOQ object is stopped or for as long as the Pending
 * queue is not empty.
 */

/*
 * FSP_IOQ_PROCESS_NO_CANCEL
 *
 * Define this macro to disallow cancelation (other than the FSP_IOQ being stopped)
 * after an IRP has entered the Processing phase.
 *
 * Once a file-system operation has been started its effects cannot be ignored. Even
 * if the process that originated the operation decides to cancel it, we must correctly
 * inform it of whether the operation was successful or not. We can only do this reliably
 * if we do not allow cancelation after an operation has been started.
 */
#define FSP_IOQ_PROCESS_NO_CANCEL

#if defined(FSP_IOQ_PROCESS_NO_CANCEL)
static NTSTATUS FspCsqInsertIrpEx(PIO_CSQ Csq, PIRP Irp, PIO_CSQ_IRP_CONTEXT Context, PVOID InsertContext)
{
    /*
     * Modelled after IoCsqInsertIrpEx. Does NOT set a cancelation routine.
     */

    NTSTATUS Result;
    KIRQL Irql;

    Irp->Tail.Overlay.DriverContext[3] = Csq;
    Csq->CsqAcquireLock(Csq, &Irql);
    Result = ((PIO_CSQ_INSERT_IRP_EX)Csq->CsqInsertIrp)(Csq, Irp, InsertContext);
    Csq->CsqReleaseLock(Csq, Irql);

    return Result;
}

static PIRP FspCsqRemoveNextIrp(PIO_CSQ Csq, PVOID PeekContext)
{
    /*
     * Modelled after IoCsqRemoveNextIrp. Used with FspCsqInsertIrpEx.
     */

    KIRQL Irql;
    PIRP Irp;

    Csq->CsqAcquireLock(Csq, &Irql);
    Irp = Csq->CsqPeekNextIrp(Csq, 0, PeekContext);
    if (0 != Irp)
    {
        Csq->CsqRemoveIrp(Csq, Irp);
        Irp->Tail.Overlay.DriverContext[3] = 0;
    }
    Csq->CsqReleaseLock(Csq, Irql);

    return Irp;
}
#else
#define FspCsqInsertIrpEx(Q, I, U, C)   IoCsqInsertIrpEx(Q, I, U, C)
#define FspCsqRemoveNextIrp(Q, C)       IoCsqRemoveNextIrp(Q, C)
#endif

#define InterruptTimeToSecFactor        10000000ULL
#define ConvertInterruptTimeToSec(Time) ((ULONG)((Time) / InterruptTimeToSecFactor))
#define QueryInterruptTimeInSec()       ConvertInterruptTimeToSec(KeQueryInterruptTime())

typedef struct
{
    PVOID IrpHint;
    ULONG ExpirationTime;
} FSP_IOQ_PEEK_CONTEXT;

static NTSTATUS FspIoqPendingInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    if (Ioq->Stopped)
        return STATUS_CANCELLED;
    if (!InsertContext && Ioq->PendingIrpCapacity <= Ioq->PendingIrpCount)
        return STATUS_INSUFFICIENT_RESOURCES;
    Ioq->PendingIrpCount++;
    InsertTailList(&Ioq->PendingIrpList, &Irp->Tail.Overlay.ListEntry);
    /* list is not empty; wake up any waiters */
    KeSetEvent(&Ioq->PendingIrpEvent, 1, FALSE);
    return STATUS_SUCCESS;
}

static VOID FspIoqPendingRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    Ioq->PendingIrpCount--;
    if (RemoveEntryList(&Irp->Tail.Overlay.ListEntry) && !Ioq->Stopped)
        /* list is empty; future threads should go to sleep */
        KeClearEvent(&Ioq->PendingIrpEvent);
}

static PIRP FspIoqPendingPeekNextIrp(PIO_CSQ IoCsq, PIRP Irp, PVOID PeekContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    if (PeekContext && Ioq->Stopped)
        return 0;
    PLIST_ENTRY Head = &Ioq->PendingIrpList;
    PLIST_ENTRY Entry = 0 == Irp ? Head->Flink : Irp->Tail.Overlay.ListEntry.Flink;
    if (Head == Entry)
        return 0;
    Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
    if (!PeekContext)
        return Irp;
    PVOID IrpHint = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->IrpHint;
    if (0 == IrpHint)
    {
        ULONG ExpirationTime = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->ExpirationTime;
        for (;;)
        {
            if (FspIrpTimestampInfinity != FspIrpTimestamp(Irp))
                return FspIrpTimestamp(Irp) <= ExpirationTime ? Irp : 0;
            Entry = Entry->Flink;
            if (Head == Entry)
                return 0;
            Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
        }
    }
    else
    {
        if (Irp == IrpHint)
            return 0;
        return Irp;
    }
}

_IRQL_raises_(DISPATCH_LEVEL)
static VOID FspIoqPendingAcquireLock(PIO_CSQ IoCsq, _At_(*PIrql, _IRQL_saves_) PKIRQL PIrql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    KeAcquireSpinLock(&Ioq->SpinLock, PIrql);
}

_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspIoqPendingReleaseLock(PIO_CSQ IoCsq, _IRQL_restores_ KIRQL Irql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

static VOID FspIoqPendingCompleteCanceledIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    Ioq->CompleteCanceledIrp(Irp);
}

static NTSTATUS FspIoqProcessInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    if (Ioq->Stopped)
        return STATUS_CANCELLED;
    InsertTailList(&Ioq->ProcessIrpList, &Irp->Tail.Overlay.ListEntry);
    ULONG Index = FspHashMixPointer(Irp) % Ioq->ProcessIrpBucketCount;
#if DBG
    for (PIRP IrpX = Ioq->ProcessIrpBuckets[Index]; IrpX; IrpX = FspIrpDictNext(IrpX))
        ASSERT(IrpX != Irp);
#endif
    ASSERT(0 == FspIrpDictNext(Irp));
    FspIrpDictNext(Irp) = Ioq->ProcessIrpBuckets[Index];
    Ioq->ProcessIrpBuckets[Index] = Irp;
    return STATUS_SUCCESS;
}

static VOID FspIoqProcessRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    ULONG Index = FspHashMixPointer(Irp) % Ioq->ProcessIrpBucketCount;
    for (PIRP *PIrp = (PIRP *)&Ioq->ProcessIrpBuckets[Index];; PIrp = &FspIrpDictNext(*PIrp))
    {
        ASSERT(0 != *PIrp);
        if (*PIrp == Irp)
        {
            *PIrp = FspIrpDictNext(Irp);
            FspIrpDictNext(Irp) = 0;
            break;
        }
    }
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

static PIRP FspIoqProcessPeekNextIrp(PIO_CSQ IoCsq, PIRP Irp, PVOID PeekContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    if (PeekContext && Ioq->Stopped)
        return 0;
    PLIST_ENTRY Head = &Ioq->ProcessIrpList;
    PLIST_ENTRY Entry = 0 == Irp ? Head->Flink : Irp->Tail.Overlay.ListEntry.Flink;
    if (Head == Entry)
        return 0;
    Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
    if (!PeekContext)
        return Irp;
    PVOID IrpHint = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->IrpHint;
    if (0 == IrpHint)
    {
        ULONG ExpirationTime = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->ExpirationTime;
        for (;;)
        {
            if (FspIrpTimestampInfinity != FspIrpTimestamp(Irp))
                return FspIrpTimestamp(Irp) <= ExpirationTime ? Irp : 0;
            Entry = Entry->Flink;
            if (Head == Entry)
                return 0;
            Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
        }
    }
    else
    {
        ULONG Index = FspHashMixPointer(IrpHint) % Ioq->ProcessIrpBucketCount;
        for (Irp = Ioq->ProcessIrpBuckets[Index]; Irp; Irp = FspIrpDictNext(Irp))
            if (Irp == IrpHint)
                return Irp;
        return 0;
    }
}

_IRQL_raises_(DISPATCH_LEVEL)
static VOID FspIoqProcessAcquireLock(PIO_CSQ IoCsq, _At_(*PIrql, _IRQL_saves_) PKIRQL PIrql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    KeAcquireSpinLock(&Ioq->SpinLock, PIrql);
}

_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspIoqProcessReleaseLock(PIO_CSQ IoCsq, _IRQL_restores_ KIRQL Irql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

static VOID FspIoqProcessCompleteCanceledIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    Ioq->CompleteCanceledIrp(Irp);
}

static NTSTATUS FspIoqRetriedInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, RetriedIoCsq);
    if (Ioq->Stopped)
        return STATUS_CANCELLED;
    InsertTailList(&Ioq->RetriedIrpList, &Irp->Tail.Overlay.ListEntry);
    return STATUS_SUCCESS;
}

static VOID FspIoqRetriedRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

static PIRP FspIoqRetriedPeekNextIrp(PIO_CSQ IoCsq, PIRP Irp, PVOID PeekContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, RetriedIoCsq);
    if (PeekContext && Ioq->Stopped)
        return 0;
    PLIST_ENTRY Head = &Ioq->RetriedIrpList;
    PLIST_ENTRY Entry = 0 == Irp ? Head->Flink : Irp->Tail.Overlay.ListEntry.Flink;
    if (Head == Entry)
        return 0;
    Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
    if (!PeekContext)
        return Irp;
    PVOID IrpHint = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->IrpHint;
    if (0 == IrpHint)
    {
        ULONG ExpirationTime = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->ExpirationTime;
        for (;;)
        {
            if (FspIrpTimestampInfinity != FspIrpTimestamp(Irp))
                return FspIrpTimestamp(Irp) <= ExpirationTime ? Irp : 0;
            Entry = Entry->Flink;
            if (Head == Entry)
                return 0;
            Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
        }
    }
    else
    {
        if (Irp == IrpHint)
            return 0;
        return Irp;
    }
}

_IRQL_raises_(DISPATCH_LEVEL)
static VOID FspIoqRetriedAcquireLock(PIO_CSQ IoCsq, _At_(*PIrql, _IRQL_saves_) PKIRQL PIrql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, RetriedIoCsq);
    KeAcquireSpinLock(&Ioq->SpinLock, PIrql);
}

_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspIoqRetriedReleaseLock(PIO_CSQ IoCsq, _IRQL_restores_ KIRQL Irql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, RetriedIoCsq);
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

static VOID FspIoqRetriedCompleteCanceledIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, RetriedIoCsq);
    Ioq->CompleteCanceledIrp(Irp);
}

NTSTATUS FspIoqCreate(
    ULONG IrpCapacity, PLARGE_INTEGER IrpTimeout, VOID (*CompleteCanceledIrp)(PIRP Irp),
    FSP_IOQ **PIoq)
{
    ASSERT(0 != CompleteCanceledIrp);

    *PIoq = 0;

    FSP_IOQ *Ioq;
    ULONG BucketCount = (PAGE_SIZE - sizeof *Ioq) / sizeof Ioq->ProcessIrpBuckets[0];
    Ioq = FspAllocNonPaged(PAGE_SIZE);
    if (0 == Ioq)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlZeroMemory(Ioq, PAGE_SIZE);

    KeInitializeSpinLock(&Ioq->SpinLock);
    KeInitializeEvent(&Ioq->PendingIrpEvent, NotificationEvent, FALSE);
    InitializeListHead(&Ioq->PendingIrpList);
    InitializeListHead(&Ioq->ProcessIrpList);
    InitializeListHead(&Ioq->RetriedIrpList);
    IoCsqInitializeEx(&Ioq->PendingIoCsq,
        FspIoqPendingInsertIrpEx,
        FspIoqPendingRemoveIrp,
        FspIoqPendingPeekNextIrp,
        FspIoqPendingAcquireLock,
        FspIoqPendingReleaseLock,
        FspIoqPendingCompleteCanceledIrp);
    IoCsqInitializeEx(&Ioq->ProcessIoCsq,
        FspIoqProcessInsertIrpEx,
        FspIoqProcessRemoveIrp,
        FspIoqProcessPeekNextIrp,
        FspIoqProcessAcquireLock,
        FspIoqProcessReleaseLock,
        FspIoqProcessCompleteCanceledIrp);
    IoCsqInitializeEx(&Ioq->RetriedIoCsq,
        FspIoqRetriedInsertIrpEx,
        FspIoqRetriedRemoveIrp,
        FspIoqRetriedPeekNextIrp,
        FspIoqRetriedAcquireLock,
        FspIoqRetriedReleaseLock,
        FspIoqRetriedCompleteCanceledIrp);
    Ioq->IrpTimeout = ConvertInterruptTimeToSec(IrpTimeout->QuadPart + InterruptTimeToSecFactor - 1);
        /* convert to seconds (and round up) */
    Ioq->PendingIrpCapacity = IrpCapacity;
    Ioq->CompleteCanceledIrp = CompleteCanceledIrp;
    Ioq->ProcessIrpBucketCount = BucketCount;

    *PIoq = Ioq;

    return STATUS_SUCCESS;
}

VOID FspIoqDelete(FSP_IOQ *Ioq)
{
    FspIoqStop(Ioq);
    FspFree(Ioq);
}

VOID FspIoqStop(FSP_IOQ *Ioq)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Ioq->Stopped = TRUE;
    /* we are being stopped, permanently wake up waiters */
    KeSetEvent(&Ioq->PendingIrpEvent, 1, FALSE);
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
    PIRP Irp;
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, 0)))
        Ioq->CompleteCanceledIrp(Irp);
    while (0 != (Irp = FspCsqRemoveNextIrp(&Ioq->ProcessIoCsq, 0)))
        Ioq->CompleteCanceledIrp(Irp);
    while (0 != (Irp = FspCsqRemoveNextIrp(&Ioq->RetriedIoCsq, 0)))
        Ioq->CompleteCanceledIrp(Irp);
}

BOOLEAN FspIoqStopped(FSP_IOQ *Ioq)
{
    BOOLEAN Result;
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Result = Ioq->Stopped;
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
    return Result;
}

VOID FspIoqRemoveExpired(FSP_IOQ *Ioq)
{
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = 0;
    PeekContext.ExpirationTime = QueryInterruptTimeInSec();
    PIRP Irp;
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, &PeekContext)))
        Ioq->CompleteCanceledIrp(Irp);
#if !defined(FSP_IOQ_PROCESS_NO_CANCEL)
    while (0 != (Irp = FspCsqRemoveNextIrp(&Ioq->ProcessIoCsq, &PeekContext)))
        Ioq->CompleteCanceledIrp(Irp);
    while (0 != (Irp = FspCsqRemoveNextIrp(&Ioq->RetryIoCsq, &PeekContext)))
        Ioq->CompleteCanceledIrp(Irp);
#endif
}

BOOLEAN FspIoqPostIrpEx(FSP_IOQ *Ioq, PIRP Irp, BOOLEAN BestEffort, NTSTATUS *PResult)
{
    NTSTATUS Result;
    FspIrpTimestamp(Irp) = BestEffort ? FspIrpTimestampInfinity :
        QueryInterruptTimeInSec() + Ioq->IrpTimeout;
    Result = IoCsqInsertIrpEx(&Ioq->PendingIoCsq, Irp, 0, (PVOID)BestEffort);
    if (NT_SUCCESS(Result))
    {
        if (0 != PResult)
            *PResult = STATUS_PENDING;
        return TRUE;
    }
    else
    {
        if (0 != PResult)
            *PResult = Result;
        return FALSE;
    }
}

PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp, PLARGE_INTEGER Timeout,
    PIRP CancellableIrp)
{
    /* timeout of 0 normally means infinite wait; for us it means do not do any wait at all! */
    if (0 != Timeout)
    {
        NTSTATUS Result;
        Result = FsRtlCancellableWaitForSingleObject(&Ioq->PendingIrpEvent, Timeout,
            CancellableIrp);
        if (STATUS_TIMEOUT == Result)
            return FspIoqTimeout;
        if (STATUS_CANCELLED == Result || STATUS_THREAD_IS_TERMINATING == Result)
            return FspIoqCancelled;
        ASSERT(STATUS_SUCCESS == Result);
    }
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = 0 != BoundaryIrp ? BoundaryIrp : (PVOID)1;
    PeekContext.ExpirationTime = 0;
    return IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, &PeekContext);
}

BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp)
{
    NTSTATUS Result;
#if defined(FSP_IOQ_PROCESS_NO_CANCEL)
    FspIrpTimestamp(Irp) = FspIrpTimestampInfinity;
#else
    if (FspIrpTimestampInfinity != FspIrpTimestamp(Irp))
        FspIrpTimestamp(Irp) = QueryInterruptTimeInSec() + Ioq->IrpTimeout;
#endif
    Result = FspCsqInsertIrpEx(&Ioq->ProcessIoCsq, Irp, 0, 0);
    return NT_SUCCESS(Result);
}

PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint)
{
    if (0 == IrpHint)
        return 0;
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = (PVOID)IrpHint;
    PeekContext.ExpirationTime = 0;
    return FspCsqRemoveNextIrp(&Ioq->ProcessIoCsq, &PeekContext);
}

BOOLEAN FspIoqRetryCompleteIrp(FSP_IOQ *Ioq, PIRP Irp, NTSTATUS *PResult)
{
    NTSTATUS Result;
#if defined(FSP_IOQ_PROCESS_NO_CANCEL)
    FspIrpTimestamp(Irp) = FspIrpTimestampInfinity;
#else
    if (FspIrpTimestampInfinity != FspIrpTimestamp(Irp))
        FspIrpTimestamp(Irp) = QueryInterruptTimeInSec() + Ioq->IrpTimeout;
#endif
    Result = FspCsqInsertIrpEx(&Ioq->RetriedIoCsq, Irp, 0, 0);
    if (NT_SUCCESS(Result))
    {
        if (0 != PResult)
            *PResult = STATUS_PENDING;
        return TRUE;
    }
    else
    {
        if (0 != PResult)
            *PResult = Result;
        return FALSE;
    }
}

PIRP FspIoqNextCompleteIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp)
{
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = 0 != BoundaryIrp ? BoundaryIrp : (PVOID)1;
    PeekContext.ExpirationTime = 0;
    return FspCsqRemoveNextIrp(&Ioq->RetriedIoCsq, &PeekContext);
}
