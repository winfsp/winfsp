/**
 * @file sys/misc.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspCreateGuid(GUID *Guid);
BOOLEAN FspValidRelativeSecurityDescriptor(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG SecurityDescriptorLength,
    SECURITY_INFORMATION RequiredInformation);
NTSTATUS FspSecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode);
VOID FspInitializeWorkItemWithDelay(FSP_WORK_ITEM_WITH_DELAY *WorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspQueueWorkItemWithDelay(FSP_WORK_ITEM_WITH_DELAY *WorkItem, LARGE_INTEGER Timeout);
static KDEFERRED_ROUTINE FspQueueWorkItemWithDelayDPC;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCreateGuid)
#pragma alloc_text(PAGE, FspValidRelativeSecurityDescriptor)
#pragma alloc_text(PAGE, FspSecuritySubjectContextAccessCheck)
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

BOOLEAN FspValidRelativeSecurityDescriptor(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG SecurityDescriptorLength,
    SECURITY_INFORMATION RequiredInformation)
{
    PAGED_CODE();

    BOOLEAN Result;

    try
    {
        Result = RtlValidRelativeSecurityDescriptor(SecurityDescriptor, SecurityDescriptorLength,
            RequiredInformation);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = FALSE;
    }

    return Result;
}

NTSTATUS FspSecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_ACCESS_DENIED;
    SECURITY_SUBJECT_CONTEXT SecuritySubjectContext;
    ACCESS_MASK GrantedAccess;

    SeCaptureSubjectContext(&SecuritySubjectContext);
    if (SeAccessCheck(SecurityDescriptor,
        &SecuritySubjectContext, FALSE,
        DesiredAccess, 0, 0, IoGetFileObjectGenericMapping(), AccessMode,
        &GrantedAccess, &Result))
        Result = STATUS_SUCCESS;
    SeReleaseSubjectContext(&SecuritySubjectContext);

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
