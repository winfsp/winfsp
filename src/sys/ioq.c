/**
 * @file sys/ioq.c
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

#include <sys/driver.h>

/*
 * Overview
 *
 * [NOTE: this comment no longer describes accurately an FSP_IOQ. The main
 * difference is that an FSP_IOQ now has a third queue which is used to
 * retry IRP completions. Another difference is that the FSP_IOQ can now
 * use Queued Events (which are implemented on top of KQUEUE) instead of
 * SynchronizationEvent's. However the main ideas below are still valid, so
 * I am leaving the rest of the comment intact.]
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
 * Pending Queue Synchronization
 *
 * The FSP_IOQ object controls access to the pending IRP queue and blocks
 * threads waiting for pending IRP's as long as the queue is empty (and
 * not stopped). Currently it uses an auto-reset event (SynchronizationEvent)
 * for this purpose. Let's discuss why.
 *
 * The obvious synchronization object to use to control access to a queue
 * is a semaphore which counts how many items there are in a queue. When the
 * semaphore count reaches 0 the queue is empty and the thread that wants to
 * dequeue an item has to wait.
 *
 * Unfortunately we cannot use a semaphore in the obvious manner in the FSP_IOQ
 * implementation. The issue is that an FSP_IOQ allows IRP's to be removed from
 * the pending queue when they are cancelled. This means that the semaphore
 * count and the queue count would get out of synch after an IRP cancelation.
 * Furthermore when trying to dequeue a pending IRP we do so under conditions
 * which may result in not dequeueing an IRP even if it is present in the queue,
 * which further complicates efforts to keep the semaphore count in synch with
 * the actual queue count.
 *
 * The original solution to these problems was to use a manual-reset event
 * (NotificationEvent). This event would change to reflect the value of the
 * condition: "pending IRP queue not empty or stopped". The event would be
 * signaled when the condition was TRUE and non-signaled when FALSE. Because
 * this was a manual-reset event, the WaitForSingleObject call on this event
 * would not change its signaled state, the signaled state would only be
 * changed during queue manipulation (under the queue lock).
 *
 * This scheme works, but has an important problem. A manual-reset event wakes
 * up all threads that are waiting on it! Imagine a situation where many
 * threads are blocked waiting for an IRP to arrive. When an IRP finally arrives
 * ALL threads wake up and try to grab it, but only one succeeds! Waking up all
 * threads is a waste of resources and decreases performance; this is called
 * the "thundering herd" problem.
 *
 * For this reason we decided to use an auto-reset event (SynchronizationEvent)
 * to guard the condition: "pending IRP queue not empty or stopped". An auto-reset
 * event has the benefit that it wakes up a single thread when it is signaled.
 * There are two problems with the auto-reset event. One is the "lost wake-up"
 * problem, where we try to SetEvent when the event is already signaled, thus
 * potentially "losing" a wakeup. The second problem is similar to the issue
 * we were facing with the semaphore earlier, that the WaitForSingleObject call
 * also changes the state of the event (as it did earlier with the semaphore's
 * count), which is problematic because some times we may not remove an IRP that
 * is in the queue because of other conditions not holding TRUE.
 *
 * To deal with the first problem we build a simple abstraction around the auto-
 * reset event in function FspIoqPendingResetSynch. This function checks the
 * condition "pending IRP queue not empty or stopped" (under lock) and either
 * sets the event if the condition is TRUE or clears it if FALSE. This works
 * because if two insertions arrive at the same time (thus calling SetEvent twice
 * in succession), the first thread that removes an IRP will check the queue
 * condition and properly re-set the event's state, thus allowing another thread
 * to come in and dequeue another IRP.
 *
 * To deal with the second problem we simply call FspIoqPendingResetSynch after
 * a WaitForSingleObject call if the IRP dequeueing fails; this ensures that the
 * event is in the correst state.
 *
 * UPDATE: We can now use a Queued Event which behaves like a SynchronizationEvent,
 * but has better performance. Unfortunately Queued Events cannot cleanly implement
 * an EventClear operation. However the EventClear operation is not strictly needed.
 */

/*
 * FSP_IOQ_USE_QEVENT
 *
 * Define this macro to use Queued Events instead of simple SynchronizationEvent's.
 */
#if defined(FSP_IOQ_USE_QEVENT)
#define FspIoqEventInitialize(E)        FspQeventInitialize(E, 0)
#define FspIoqEventFinalize(E)          FspQeventFinalize(E)
#define FspIoqEventSet(E)               FspQeventSetNoLock(E)
#define FspIoqEventCancellableWait(E,T,I)   FspQeventCancellableWait(E,T,I)
#define FspIoqEventClear(E)             ((VOID)0)
#else
#define FspIoqEventInitialize(E)        KeInitializeEvent(E, SynchronizationEvent, FALSE)
#define FspIoqEventFinalize(E)          ((VOID)0)
#define FspIoqEventSet(E)               KeSetEvent(E, 1, FALSE)
#define FspIoqEventCancellableWait(E,T,I)   FsRtlCancellableWaitForSingleObject(E,T,I)
#define FspIoqEventClear(E)             KeClearEvent(E)
#endif

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

static inline VOID FspIoqPendingResetSynch(FSP_IOQ *Ioq)
{
    /*
     * Examine the actual condition of the pending queue and
     * set the PendingIrpEvent accordingly.
     */
    if (0 != Ioq->PendingIrpCount || Ioq->Stopped)
        /* list is not empty or is stopped; wake up a waiter */
        FspIoqEventSet(&Ioq->PendingIrpEvent);
    else
        /* list is empty and not stopped; future threads should go to sleep */
        /* NOTE: this is not stricly necessary! */
        FspIoqEventClear(&Ioq->PendingIrpEvent);
}

static NTSTATUS FspIoqPendingInsertIrpEx(PIO_CSQ IoCsq, PIRP Irp, PVOID InsertContext)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    if (Ioq->Stopped)
        return STATUS_CANCELLED;
    if (!InsertContext && Ioq->PendingIrpCapacity <= Ioq->PendingIrpCount)
        return STATUS_INSUFFICIENT_RESOURCES;
    Ioq->PendingIrpCount++;
    InsertTailList(&Ioq->PendingIrpList, &Irp->Tail.Overlay.ListEntry);
    FspIoqEventSet(&Ioq->PendingIrpEvent);
        /* equivalent to FspIoqPendingResetSynch(Ioq) */
    return STATUS_SUCCESS;
}

static VOID FspIoqPendingRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, PendingIoCsq);
    Ioq->PendingIrpCount--;
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
    FspIoqPendingResetSynch(Ioq);
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
    Ioq->ProcessIrpCount++;
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
    Ioq->ProcessIrpCount--;
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
    Ioq->RetriedIrpCount++;
    InsertTailList(&Ioq->RetriedIrpList, &Irp->Tail.Overlay.ListEntry);
    return STATUS_SUCCESS;
}

static VOID FspIoqRetriedRemoveIrp(PIO_CSQ IoCsq, PIRP Irp)
{
    FSP_IOQ *Ioq = CONTAINING_RECORD(IoCsq, FSP_IOQ, RetriedIoCsq);
    Ioq->RetriedIrpCount--;
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
    FspIoqEventInitialize(&Ioq->PendingIrpEvent);
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
    FspIoqStop(Ioq, TRUE);
    FspIoqEventFinalize(&Ioq->PendingIrpEvent);
    FspFree(Ioq);
}

VOID FspIoqStop(FSP_IOQ *Ioq, BOOLEAN CancelIrps)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Ioq->Stopped = TRUE;
    /* we are being stopped, permanently wake up waiters */
    FspIoqEventSet(&Ioq->PendingIrpEvent);
        /* equivalent to FspIoqPendingResetSynch(Ioq) */
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
    if (CancelIrps)
    {
        PIRP Irp;
        while (0 != (Irp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, 0)))
            Ioq->CompleteCanceledIrp(Irp);
        while (0 != (Irp = FspCsqRemoveNextIrp(&Ioq->ProcessIoCsq, 0)))
            Ioq->CompleteCanceledIrp(Irp);
        while (0 != (Irp = FspCsqRemoveNextIrp(&Ioq->RetriedIoCsq, 0)))
            Ioq->CompleteCanceledIrp(Irp);
    }
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

VOID FspIoqRemoveExpired(FSP_IOQ *Ioq, UINT64 InterruptTime)
{
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PeekContext.IrpHint = 0;
    PeekContext.ExpirationTime = ConvertInterruptTimeToSec(InterruptTime);
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
    FSP_IOQ_PEEK_CONTEXT PeekContext;
    PIRP PendingIrp;
    PeekContext.IrpHint = 0 != BoundaryIrp ? BoundaryIrp : (PVOID)1;
    PeekContext.ExpirationTime = 0;
    if (0 != Timeout)
    {
        NTSTATUS Result;
        Result = FspIoqEventCancellableWait(&Ioq->PendingIrpEvent, Timeout,
            CancellableIrp);
        if (STATUS_TIMEOUT == Result)
            return FspIoqTimeout;
        if (STATUS_CANCELLED == Result || STATUS_THREAD_IS_TERMINATING == Result)
            return FspIoqCancelled;
        ASSERT(STATUS_SUCCESS == Result);
        PendingIrp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, &PeekContext);
        if (0 == PendingIrp)
        {
            /*
             * The WaitForSingleObject call above has reset our PendingIrpEvent,
             * but we did not receive an IRP. For this reason we have to reset
             * our synchronization based on the actual condition of the pending
             * queue.
             */
            KIRQL Irql;
            KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
            FspIoqPendingResetSynch(Ioq);
            KeReleaseSpinLock(&Ioq->SpinLock, Irql);
        }
    }
    else
        PendingIrp = IoCsqRemoveNextIrp(&Ioq->PendingIoCsq, &PeekContext);
    return PendingIrp;
}

ULONG FspIoqPendingIrpCount(FSP_IOQ *Ioq)
{
    ULONG Result;
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Result = Ioq->PendingIrpCount;
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
    return Result;
}

BOOLEAN FspIoqPendingAboveWatermark(FSP_IOQ *Ioq, ULONG Watermark)
{
    BOOLEAN Result;
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Result = Watermark < 100 * Ioq->PendingIrpCount / Ioq->PendingIrpCapacity;
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
    return Result;
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

ULONG FspIoqProcessIrpCount(FSP_IOQ *Ioq)
{
    ULONG Result;
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Result = Ioq->ProcessIrpCount;
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
    return Result;
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
        /* wake up a waiter */
        KIRQL Irql;
        KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
        FspIoqEventSet(&Ioq->PendingIrpEvent);
        KeReleaseSpinLock(&Ioq->SpinLock, Irql);

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

ULONG FspIoqRetriedIrpCount(FSP_IOQ *Ioq)
{
    ULONG Result;
    KIRQL Irql;
    KeAcquireSpinLock(&Ioq->SpinLock, &Irql);
    Result = Ioq->RetriedIrpCount;
    KeReleaseSpinLock(&Ioq->SpinLock, Irql);
    return Result;
}
