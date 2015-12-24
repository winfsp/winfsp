/**
 * @file sys/filectx.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspFileContextCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_CONTEXT **PFsContext);
VOID FspFileContextDelete(FSP_FILE_CONTEXT *Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFileContextCreate)
#pragma alloc_text(PAGE, FspFileContextDelete)
#endif

NTSTATUS FspFileContextCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_CONTEXT **PFsContext)
{
    PAGED_CODE();

    *PFsContext = 0;

    FSP_FILE_CONTEXT_NONPAGED *NonPaged = FspAllocNonPaged(sizeof *NonPaged);
    if (0 == NonPaged)
        return STATUS_INSUFFICIENT_RESOURCES;

    FSP_FILE_CONTEXT *FsContext = FspAlloc(sizeof *FsContext + ExtraSize);
    if (0 == FsContext)
    {
        FspFree(NonPaged);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NonPaged, sizeof *NonPaged);
    ExInitializeResourceLite(&NonPaged->Resource);
    ExInitializeResourceLite(&NonPaged->PagingIoResource);
    ExInitializeFastMutex(&NonPaged->HeaderFastMutex);

    RtlZeroMemory(FsContext, sizeof *FsContext + ExtraSize);
    FsContext->Header.NodeTypeCode = FspFileContextFileKind;
    FsContext->Header.NodeByteSize = sizeof *FsContext;
    FsContext->Header.IsFastIoPossible = FastIoIsQuestionable;
    FsContext->Header.Resource = &NonPaged->Resource;
    FsContext->Header.PagingIoResource = &NonPaged->PagingIoResource;
    FsRtlSetupAdvancedHeader(&FsContext->Header, &NonPaged->HeaderFastMutex);
    FsContext->NonPaged = NonPaged;
    FsContext->RefCount = 1;
    FsContext->FsvolDeviceObject = DeviceObject;
    FspDeviceRetain(FsContext->FsvolDeviceObject);
    RtlInitEmptyUnicodeString(&FsContext->FileName, FsContext->FileNameBuf, (USHORT)ExtraSize);

    *PFsContext = FsContext;

    return STATUS_SUCCESS;
}

VOID FspFileContextDelete(FSP_FILE_CONTEXT *FsContext)
{
    PAGED_CODE();

    FsRtlTeardownPerStreamContexts(&FsContext->Header);

    FspDeviceRelease(FsContext->FsvolDeviceObject);

    ExDeleteResourceLite(&FsContext->NonPaged->PagingIoResource);
    ExDeleteResourceLite(&FsContext->NonPaged->Resource);
    FspFree(FsContext->NonPaged);

    FspFree(FsContext);
}
