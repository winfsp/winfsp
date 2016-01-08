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

FSP_FILE_CONTEXT *FspFileContextOpen(FSP_FILE_CONTEXT *FsContext, PFILE_OBJECT FileObject,
    DWORD GrantedAccess, DWORD ShareAccess, NTSTATUS *PResult)
{
    /*
     * Attempt to insert our FsContext into the volume device's generic table.
     * If an FsContext with the same UserContext already exists, then use that
     * FsContext instead.
     */

    PDEVICE_OBJECT FsvolDeviceObject = FsContext->FsvolDeviceObject;
    FSP_FILE_CONTEXT *OpenedFsContext;
    BOOLEAN Inserted;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    OpenedFsContext = FspFsvolDeviceInsertContext(FsvolDeviceObject,
        FsContext->UserContext, FsContext, &FsContext->ElementStorage, &Inserted);
    ASSERT(0 != OpenedFsContext);

    if (Inserted)
    {
        /*
         * The new FsContext was inserted into the Context table. Set its share access
         * and retain and open it. There should be (at least) two references to this
         * FsContext, one from our caller and one from the Context table.
         */
        ASSERT(OpenedFsContext == FsContext);

        IoSetShareAccess(GrantedAccess, ShareAccess, FileObject, &FsContext->ShareAccess);
        FspFileContextRetain(OpenedFsContext);
        OpenedFsContext->OpenCount++;
    }
    else
    {
        /*
         * The new FsContext was NOT inserted into the Context table. Instead we are
         * opening a prior FsContext that we found in the table.
         *
         * First check and update the share access. If successful then retain the
         * prior FsContext for our caller and release the original FsContext.
         */
        ASSERT(OpenedFsContext != FsContext);

        /*
         * FastFat says to do the following on Vista and above.
         *
         * Quote:
         *     Do an extra test for writeable user sections if the user did not allow
         *     write sharing - this is neccessary since a section may exist with no handles
         *     open to the file its based against.
         */
        NTSTATUS Result = STATUS_SUCCESS;
        if (!FlagOn(ShareAccess, FILE_SHARE_WRITE) &&
            FlagOn(GrantedAccess,
                FILE_EXECUTE | FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE) &&
            MmDoesFileHaveUserWritableReferences(&FsContext->NonPaged->SectionObjectPointers))
            Result = STATUS_SHARING_VIOLATION;

        /* share access check */
        if (NT_SUCCESS(Result))
            Result = IoCheckShareAccess(GrantedAccess, ShareAccess, FileObject, &FsContext->ShareAccess, TRUE);
        if (NT_SUCCESS(Result))
        {
            FspFileContextRetain(OpenedFsContext);
            OpenedFsContext->OpenCount++;
        }
        else
        {
            if (0 != PResult)
                *PResult = Result;

            OpenedFsContext = 0;
        }
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (!Inserted)
        FspFileContextRelease(FsContext);

    return OpenedFsContext;
}

VOID FspFileContextClose(FSP_FILE_CONTEXT *FsContext, PFILE_OBJECT FileObject)
{
    /*
     * Close the FsContext. If the OpenCount becomes zero remove it
     * from the Context table.
     */

    PDEVICE_OBJECT FsvolDeviceObject = FsContext->FsvolDeviceObject;
    BOOLEAN Deleted = FALSE;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    IoRemoveShareAccess(FileObject, &FsContext->ShareAccess);
    if (0 == --FsContext->OpenCount)
        FspFsvolDeviceDeleteContext(FsvolDeviceObject, FsContext->UserContext, &Deleted);

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (Deleted)
        FspFileContextRelease(FsContext);
}
