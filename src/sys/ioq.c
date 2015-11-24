/**
 * @file sys/ioq.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

/*
 * An FSP_IOQ encapsulates the main FSP mechanism for handling IRP's.
 * It has two queues: a "pending" queue for managing newly arrived IRP's
 * and a "processing" queue for managing IRP's currently being processed
 * (i.e. sent to the user-mode file system for further processing).
 *
 * IRP's arrive at a MajorFunction (MJ) and are then posted to the device's
 * FSP_IOQ and marked pending. When the user-mode file system performs
 * FSP_FSCTL_TRANSACT, the IRP's are removed from the pending queue and
 * are then marshalled to the user process; prior to that they are added
 * to the processing queue. At a later time the user-mode will perform
 * another FSP_FSCTL_TRANSACT at which time any processed IRP's will be
 * marshalled back to us and will be then removed from the processing queue
 * and completed.
 *
 * State diagram:
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
 *           v                     |                    | CompleteIrp
 *     +------------+              |                    v
 *     |  TRANSACT  |              |              +------------+
 *     |    OUT     |              |              | Completed  |
 *     +------------+              |              +------------+
 *           |                     |
 *           +---------------------+
 */

static NTSTATUS FspIoqPendingInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);

    if (0 > Ioq->Enabled)
        return STATUS_ACCESS_DENIED;

    InsertTailList(&Ioq->PendingIrpList, &Irp->Tail.Overlay.ListEntry);
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

static VOID FspIoqCompleteCanceledIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FspCompleteRequest(Irp, STATUS_CANCELLED);
}

VOID FspIoqInitialize(FSP_IOQ *Ioq)
{
    RtlZeroMemory(Ioq, sizeof *Ioq);
    KeInitializeSpinLock(&Ioq->SpinLock);
    InitializeListHead(&Ioq->PendingIrpList);
    InitializeListHead(&Ioq->ProcessIrpList);
    IoCsqInitializeEx(&Ioq->PendingIoCsq,
        FspIoqPendingInsertIrpEx,
        FspIoqPendingRemoveIrp,
        FspIoqPendingPeekNextIrp,
        FspIoqPendingAcquireLock,
        FspIoqPendingReleaseLock,
        FspIoqCompleteCanceledIrp);
    IoCsqInitializeEx(&Ioq->ProcessIoCsq,
        FspIoqProcessInsertIrpEx,
        FspIoqProcessRemoveIrp,
        FspIoqProcessPeekNextIrp,
        FspIoqProcessAcquireLock,
        FspIoqProcessReleaseLock,
        FspIoqCompleteCanceledIrp);
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
    return STATUS_SUCCESS == IoCsqInsertIrpEx(&Ioq->PendingIoCsq, Irp, 0, 0);
}

PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq)
{
    return IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, (PVOID)1);
}

BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp)
{
    return STATUS_SUCCESS == IoCsqInsertIrpEx(&Ioq->ProcessIoCsq, Irp, 0, 0);
}

PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint)
{
    return IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, (PVOID)IrpHint);
}

VOID FspIoqCancelAll(FSP_IOQ *Ioq)
{
    PIRP Irp;
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, 0)))
        FspCompleteRequest(Irp, STATUS_CANCELLED);
    while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->ProcessIoCsq, 0)))
        FspCompleteRequest(Irp, STATUS_CANCELLED);
}
