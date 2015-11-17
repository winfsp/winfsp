/**
 * @file sys/resource.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS
FspAcquireForSectionSynchronization(
    _In_ PFS_FILTER_CALLBACK_DATA Data,
    _Out_ PVOID *CompletionContext);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCreate)
#endif

NTSTATUS
FspAcquireForSectionSynchronization(
    _In_ PFS_FILTER_CALLBACK_DATA Data,
    _Out_ PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(CompletionContext);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}
