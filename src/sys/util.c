/**
 * @file sys/util.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspCreateGuid(GUID *Guid);
VOID FspInitializeSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspExecuteSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem);
static WORKER_THREAD_ROUTINE FspExecuteSynchronousWorkItemRoutine;
VOID FspInitializeDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspQueueDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem, LARGE_INTEGER Delay);
static KDEFERRED_ROUTINE FspQueueDelayedWorkItemDPC;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCreateGuid)
#pragma alloc_text(PAGE, FspInitializeSynchronousWorkItem)
#pragma alloc_text(PAGE, FspExecuteSynchronousWorkItem)
#pragma alloc_text(PAGE, FspExecuteSynchronousWorkItemRoutine)
#pragma alloc_text(PAGE, FspInitializeDelayedWorkItem)
#pragma alloc_text(PAGE, FspQueueDelayedWorkItem)
#endif

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

NTSTATUS FspCcSetFileSizes(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes)
{
    try
    {
        CcSetFileSizes(FileObject, FileSizes);
        return STATUS_SUCCESS;
    }
    except(EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
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

    ExQueueWorkItem(&SynchronousWorkItem->WorkQueueItem, DelayedWorkQueue);

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
