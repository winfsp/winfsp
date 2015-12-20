/**
 * @file sys/util.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspCreateGuid(GUID *Guid);
VOID FspInitializeWorkItemWithDelay(FSP_WORK_ITEM_WITH_DELAY *WorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspQueueWorkItemWithDelay(FSP_WORK_ITEM_WITH_DELAY *WorkItem, LARGE_INTEGER Timeout);
static KDEFERRED_ROUTINE FspQueueWorkItemWithDelayDPC;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCreateGuid)
#pragma alloc_text(PAGE, FspInitializeWorkItemWithDelay)
#pragma alloc_text(PAGE, FspQueueWorkItemWithDelay)
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

VOID FspInitializeWorkItemWithDelay(FSP_WORK_ITEM_WITH_DELAY *WorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context)
{
    PAGED_CODE();

    KeInitializeTimer(&WorkItem->Timer);
    KeInitializeDpc(&WorkItem->Dpc, FspQueueWorkItemWithDelayDPC, WorkItem);
    ExInitializeWorkItem(&WorkItem->WorkQueueItem, Routine, Context);
}

VOID FspQueueWorkItemWithDelay(FSP_WORK_ITEM_WITH_DELAY *WorkItem, LARGE_INTEGER Timeout)
{
    PAGED_CODE();

    KeSetTimer(&WorkItem->Timer, Timeout, &WorkItem->Dpc);
}

static VOID FspQueueWorkItemWithDelayDPC(PKDPC Dpc,
    PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
    // !PAGED_CODE();

    FSP_WORK_ITEM_WITH_DELAY *WorkItem = DeferredContext;

    ExQueueWorkItem(&WorkItem->WorkQueueItem, DelayedWorkQueue);
}
