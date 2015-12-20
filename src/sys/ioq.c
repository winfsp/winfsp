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

typedef struct
{
    PVOID IrpHint;
    ULONGLONG ExpirationTime;
} FSP_IOQ_PEEK_CONTEXT;

static NTSTATUS FspIoqPendingInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    if (Ioq->Stopped)
        return STATUS_ACCESS_DENIED;
    InsertTailList(&Ioq->PendingIrpList, &Irp->Tail.Overlay.ListEntry);
    /* list is not empty; wake up any waiters */
    KeSetEvent(&Ioq->PendingIrpEvent, 1, FALSE);
    return STATUS_SUCCESS;
}

static VOID FspIoqPendingRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
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
    if (!PeekContext)
        return Head != Entry ? CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry) : 0;
    PVOID IrpHint = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->IrpHint;
    if (0 == IrpHint)
    {
        if (Head != Entry)
        {
            Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
            if (FspIrpTimestamp(Irp) <= ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->ExpirationTime)
                /* IRP has expired; return it */
                return Irp;
        }
        return 0;
    }
    return Head != Entry ? CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry) : 0;
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
        return STATUS_ACCESS_DENIED;
    InsertTailList(&Ioq->ProcessIrpList, &Irp->Tail.Overlay.ListEntry);
    return STATUS_SUCCESS;
}

static VOID FspIoqProcessRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

static PIRP FspIoqProcessPeekNextIrp(PIO_CSQ IoCsq, PIRP Irp, PVOID PeekContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    if (PeekContext && Ioq->Stopped)
        return 0;
    PLIST_ENTRY Head = &Ioq->ProcessIrpList;
    PLIST_ENTRY Entry = 0 == Irp ? Head->Flink : Irp->Tail.Overlay.ListEntry.Flink;
    if (!PeekContext)
        return Head != Entry ? CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry) : 0;
    PVOID IrpHint = ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->IrpHint;
    if (0 == IrpHint)
    {
        if (Head != Entry)
        {
            Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
            if (FspIrpTimestamp(Irp) <= ((FSP_IOQ_PEEK_CONTEXT *)PeekContext)->ExpirationTime)
                /* IRP has expired; return it */
                return Irp;
        }
        return 0;
    }
    for (; Head != Entry; Entry = Entry->Flink)
    {
        Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
        if (Irp == IrpHint)
            return Irp;
    }
    return 0;
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

VOID FspIoqInitialize(FSP_IOQ *Ioq, VOID (*CompleteCanceledIrp)(PIRP Irp))
{
    ASSERT(0 != CompleteCanceledIrp);

    RtlZeroMemory(Ioq, sizeof *Ioq);
    KeInitializeSpinLock(&Ioq->SpinLock);
    KeInitializeEvent(&Ioq->PendingIrpEvent, NotificationEvent, FALSE);
    InitializeListHead(&Ioq->PendingIrpList);
    InitializeListHead(&Ioq->ProcessIrpList);
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
    Ioq->CompleteCanceledIrp = CompleteCanceledIrp;
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
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->ProcessIoCsq, 0)))
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

VOID FspIoqRemoveExpired(FSP_IOQ *Ioq, PLARGE_INTEGER Timeout)
{
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = 0;
    PeekContext.ExpirationTime = KeQueryInterruptTime();
    if (PeekContext.ExpirationTime >= (ULONGLONG)Timeout->QuadPart)
        PeekContext.ExpirationTime -= Timeout->QuadPart;
    else
        PeekContext.ExpirationTime = 0;
    PIRP Irp;
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, &PeekContext)))
        Ioq->CompleteCanceledIrp(Irp);
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->ProcessIoCsq, &PeekContext)))
        Ioq->CompleteCanceledIrp(Irp);
}

BOOLEAN FspIoqPostIrp(FSP_IOQ *Ioq, PIRP Irp)
{
    NTSTATUS Result;
    FspIrpTimestamp(Irp) = KeQueryInterruptTime();
    Result = IoCsqInsertIrpEx(&Ioq->PendingIoCsq, Irp, 0, 0);
    return NT_SUCCESS(Result);
}

PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, PLARGE_INTEGER Timeout)
{
    /* timeout of 0 normally means infinite wait; for us it means do not do any wait at all! */
    if (0 != Timeout)
    {
        NTSTATUS Result;
        Result = KeWaitForSingleObject(&Ioq->PendingIrpEvent, Executive, KernelMode, FALSE,
            Timeout);
        ASSERT(STATUS_SUCCESS == Result || STATUS_TIMEOUT == Result);
        if (STATUS_TIMEOUT == Result)
            return FspIoqTimeout;
    }
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = (PVOID)1;
    PeekContext.ExpirationTime = 0;
    return IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, &PeekContext);
}

BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp)
{
    NTSTATUS Result;
    FspIrpTimestamp(Irp) = KeQueryInterruptTime();
    Result = IoCsqInsertIrpEx(&Ioq->ProcessIoCsq, Irp, 0, 0);
    return NT_SUCCESS(Result);
}

PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint)
{
    if (0 == IrpHint)
        return 0;
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = (PVOID)IrpHint;
    PeekContext.ExpirationTime = 0;
    return IoCsqRemoveNextIrp(&Ioq->ProcessIoCsq, &PeekContext);
}
