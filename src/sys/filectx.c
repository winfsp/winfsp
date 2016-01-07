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
    FsContext->Header.ValidDataLength.QuadPart = 0x7fffffffffffffffLL;
        /* disable ValidDataLength functionality */
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

FSP_FILE_CONTEXT *FspFileContextOpen(PDEVICE_OBJECT FsvolDeviceObject,
    FSP_FILE_CONTEXT *FsContext)
{
    /*
     * Attempt to insert our FsContext into the volume device's generic table.
     * If an FsContext with the same UserContext already exists, then use that
     * FsContext instead.
     */

    FSP_FILE_CONTEXT *OpenedFsContext;
    BOOLEAN Inserted;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    OpenedFsContext = FspFsvolDeviceInsertContext(FsvolDeviceObject,
        FsContext->UserContext, FsContext, &FsContext->ElementStorage, &Inserted);
    ASSERT(0 != OpenedFsContext);

    if (Inserted)
    {
        /*
         * The new FsContext was inserted into the Context table.
         * Retain it. There should be (at least) two references to this FsContext,
         * one from our caller and one from the Context table.
         */
        ASSERT(OpenedFsContext == FsContext);

        FspFileContextRetain(OpenedFsContext);
    }
    else
    {
        /*
         * The new FsContext was NOT inserted into the Context table,
         * instead a prior FsContext is being opened.
         * Release the new FsContext since the caller will no longer reference it,
         * and retain the prior FsContext TWICE, once for our caller and once for
         * the Context table.
         */
        ASSERT(OpenedFsContext != FsContext);

        FspFileContextRetain2(OpenedFsContext);
    }

    InterlockedIncrement(&OpenedFsContext->OpenCount);

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (!Inserted)
        FspFileContextRelease(FsContext);

    return OpenedFsContext;
}

VOID FspFileContextClose(PDEVICE_OBJECT FsvolDeviceObject,
    FSP_FILE_CONTEXT *FsContext)
{
    /*
     * Close the FsContext. If the OpenCount becomes zero remove it
     * from the Context table.
     */

    if (0 == InterlockedDecrement(&FsContext->OpenCount))
    {
        FspFsvolDeviceLockContextTable(FsvolDeviceObject);
        FspFsvolDeviceDeleteContext(FsvolDeviceObject, FsContext->UserContext, 0);
        FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);
    }

    FspFileContextRelease(FsContext);
}
