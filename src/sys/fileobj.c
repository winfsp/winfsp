/**
 * @file sys/fileobj.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspFileContextCreate(ULONG ExtraSize, FSP_FILE_CONTEXT **PContext);
VOID FspFileContextDelete(FSP_FILE_CONTEXT *Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFileContextCreate)
#pragma alloc_text(PAGE, FspFileContextDelete)
#endif

NTSTATUS FspFileContextCreate(ULONG ExtraSize, FSP_FILE_CONTEXT **PFsContext)
{
    PAGED_CODE();

    *PFsContext = 0;

    FSP_FILE_CONTEXT_NONPAGED *NonPaged = ExAllocatePoolWithTag(NonPagedPool,
        sizeof *NonPaged, FSP_TAG);
    if (0 == NonPaged)
        return STATUS_INSUFFICIENT_RESOURCES;

    FSP_FILE_CONTEXT *FsContext = ExAllocatePoolWithTag(PagedPool,
        sizeof *FsContext + ExtraSize, FSP_TAG);
    if (0 == FsContext)
    {
        ExFreePoolWithTag(NonPaged, FSP_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NonPaged, sizeof *NonPaged);
    ExInitializeResourceLite(&NonPaged->Resource);
    ExInitializeResourceLite(&NonPaged->PagingIoResource);
    ExInitializeFastMutex(&NonPaged->HeaderFastMutex);

    RtlZeroMemory(FsContext, sizeof *FsContext + ExtraSize);
    FsContext->Header.NodeTypeCode = 'F';
    FsContext->Header.NodeByteSize = sizeof *FsContext;
    FsContext->Header.IsFastIoPossible = FastIoIsQuestionable;
    FsContext->Header.Resource = &NonPaged->Resource;
    FsContext->Header.PagingIoResource = &NonPaged->PagingIoResource;
    FsRtlSetupAdvancedHeader(&FsContext->Header, &NonPaged->HeaderFastMutex);
    FsContext->NonPaged = NonPaged;
    RtlInitEmptyUnicodeString(&FsContext->FileName, FsContext->FileNameBuf, (USHORT)ExtraSize);

    *PFsContext = FsContext;

    return STATUS_SUCCESS;
}

VOID FspFileContextDelete(FSP_FILE_CONTEXT *FsContext)
{
    PAGED_CODE();

    FsRtlTeardownPerStreamContexts(&FsContext->Header);
    ExFreePoolWithTag(FsContext->NonPaged, FSP_TAG);
    ExFreePoolWithTag(FsContext, FSP_TAG);
}
