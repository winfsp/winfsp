/**
 * @file sys/ioq.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

/*
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
 * IRP State diagram:
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
 * Note that the Pending queue is controlled by a semaphore object, which
 * counts how many IRP's are in the queue. The semaphore is incremented for
 * every IRP that is inserted into the Pending queue and decremented every
 * time an IRP is removed from the Pending queue.
 */

static NTSTATUS FspIoqPendingInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);

    if (0 > Ioq->Enabled)
        return STATUS_ACCESS_DENIED;

    /* SpinLock: acquired, IRQL: DISPATCH_LEVEL */
    InsertTailList(&Ioq->PendingIrpList, &Irp->Tail.Overlay.ListEntry);
    KeReleaseSemaphore(&Ioq->PendingSemaphore, FSP_IO_INCREMENT, 1, FALSE);
        /* KeReleaseSemaphore allowed at DISPATCH_LEVEL with Wait==FALSE */

    return STATUS_SUCCESS;
}

static VOID FspIoqPendingRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

static PIRP FspIoqPendingPeekNextIrp(PIO_CSQ IoCsq, PIRP Irp, PVOID PeekContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    PLIST_ENTRY Head, Entry;

    if (!PeekContext && 0 > Ioq->Enabled)
        return 0;

    Head = &Ioq->PendingIrpList;
    if (0 == Irp)
        Entry = Head->Flink;
    else
        Entry = Irp->Tail.Overlay.ListEntry.Flink;

    return Head != Entry ? CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry) : 0;
}

static VOID FspIoqPendingAcquireLock(PIO_CSQ IoCsq, PKIRQL Irql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    KeAcquireSpinLock(&Ioq->SpinLock, Irql);
}

static VOID FspIoqPendingReleaseLock(PIO_CSQ IoCsq, KIRQL Irql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

static VOID FspIoqPendingCompleteCanceledIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    LARGE_INTEGER Timeout = { 0 };

    /* SpinLock: not acquired */
    KeWaitForSingleObject(&Ioq->PendingSemaphore, Executive, KernelMode, FALSE, &Timeout);
    FspCompleteRequest(Irp, STATUS_CANCELLED);
}

static NTSTATUS FspIoqProcessInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);

    if (0 > Ioq->Enabled)
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
    PLIST_ENTRY Head, Entry;

    if (!PeekContext && 0 > Ioq->Enabled)
        return 0;

    Head = &Ioq->ProcessIrpList;
    if (0 == Irp)
        Entry = Head->Flink;
    else
        Entry = Irp->Tail.Overlay.ListEntry.Flink;

    for (; Head != Entry; Entry = Entry->Flink)
    {
        Irp = CONTAINING_RECORD(Entry, IRP, Tail.Overlay.ListEntry);
        if (Irp == PeekContext)
            return Irp;
    }

    return 0;
}

static VOID FspIoqProcessAcquireLock(PIO_CSQ IoCsq, PKIRQL Irql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    KeAcquireSpinLock(&Ioq->SpinLock, Irql);
}

static VOID FspIoqProcessReleaseLock(PIO_CSQ IoCsq, KIRQL Irql)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, ProcessIoCsq);
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

static VOID FspIoqProcessCompleteCanceledIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FspCompleteRequest(Irp, STATUS_CANCELLED);
}

VOID FspIoqInitialize(FSP_IOQ *Ioq)
{
    RtlZeroMemory(Ioq, sizeof *Ioq);
    KeInitializeSpinLock(&Ioq->SpinLock);
    KeInitializeSemaphore(&Ioq->PendingSemaphore, 0, 1000000000);
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
}

VOID FspIoqEnable(FSP_IOQ *Ioq, int Delta)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Ioq->Enabled += Delta;
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
}

BOOLEAN FspIoqPostIrp(FSP_IOQ *Ioq, PIRP Irp)
{
    NTSTATUS Result;
    Result = IoCsqInsertIrpEx(&Ioq->PendingIoCsq, Irp, 0, 0);
    return NT_SUCCESS(Result);
}

PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, ULONG millis)
{
    NTSTATUS Result;
    LARGE_INTEGER Timeout;
    PIRP Irp;

    Timeout.QuadPart = (LONGLONG)millis * 10000;
    Result = KeWaitForSingleObject(&Ioq->PendingSemaphore, Executive, KernelMode, FALSE,
        -1 == millis ? 0 : &Timeout);
    if (!NT_SUCCESS(Result))
        return 0;

    Irp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, (PVOID)1);

    if (0 == Irp)
        /* signal the semaphore again; turns out we did not get an IRP! */
        KeReleaseSemaphore(&Ioq->PendingSemaphore, FSP_IO_INCREMENT, 1, FALSE);

    return Irp;
}

BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp)
{
    NTSTATUS Result;
    Result = IoCsqInsertIrpEx(&Ioq->ProcessIoCsq, Irp, 0, 0);
    return NT_SUCCESS(Result);
}

PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint)
{
    return IoCsqRemoveNextIrp(&Ioq->ProcessIoCsq, (PVOID)IrpHint);
}

VOID FspIoqCancelAll(FSP_IOQ *Ioq)
{
    PIRP Irp;
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, 0)))
        FspIoqPendingCompleteCanceledIrp(&Ioq->PendingIoCsq, Irp);
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->ProcessIoCsq, 0)))
        FspIoqProcessCompleteCanceledIrp(&Ioq->ProcessIoCsq, Irp);
}
