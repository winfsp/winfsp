/**
 * @file sys/util.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspCreateGuid(GUID *Guid);
VOID FspInitializeDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspQueueDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem, LARGE_INTEGER Delay);
static KDEFERRED_ROUTINE FspQueueDelayedWorkItemDPC;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCreateGuid)
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
