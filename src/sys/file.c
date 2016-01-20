/**
 * @file sys/file.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

#undef FspFileNodeAcquireShared
#undef FspFileNodeTryAcquireShared
#undef FspFileNodeAcquireExclusive
#undef FspFileNodeTryAcquireExclusive
#undef FspFileNodeRelease

NTSTATUS FspFileNodeCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_NODE **PFileNode);
VOID FspFileNodeDelete(FSP_FILE_NODE *FileNode);
VOID FspFileNodeAcquireShared(FSP_FILE_NODE *FileNode, ULONG Flags);
BOOLEAN FspFileNodeTryAcquireShared(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeAcquireExclusive(FSP_FILE_NODE *FileNode, ULONG Flags);
BOOLEAN FspFileNodeTryAcquireExclusive(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeRelease(FSP_FILE_NODE *FileNode, ULONG Flags);
FSP_FILE_NODE *FspFileNodeOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    DWORD GrantedAccess, DWORD ShareAccess, BOOLEAN DeleteOnClose, NTSTATUS *PResult);
VOID FspFileNodeClose(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    PBOOLEAN PDeletePending);

NTSTATUS FspFileDescCreate(FSP_FILE_DESC **PFileDesc);
VOID FspFileDescDelete(FSP_FILE_DESC *FileDesc);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFileNodeCreate)
#pragma alloc_text(PAGE, FspFileNodeDelete)
#pragma alloc_text(PAGE, FspFileNodeAcquireShared)
#pragma alloc_text(PAGE, FspFileNodeTryAcquireShared)
#pragma alloc_text(PAGE, FspFileNodeAcquireExclusive)
#pragma alloc_text(PAGE, FspFileNodeTryAcquireExclusive)
#pragma alloc_text(PAGE, FspFileNodeRelease)
#pragma alloc_text(PAGE, FspFileNodeOpen)
#pragma alloc_text(PAGE, FspFileNodeClose)
#pragma alloc_text(PAGE, FspFileDescCreate)
#pragma alloc_text(PAGE, FspFileDescDelete)
#endif

NTSTATUS FspFileNodeCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_NODE **PFileNode)
{
    PAGED_CODE();

    *PFileNode = 0;

    FSP_FILE_NODE_NONPAGED *NonPaged = FspAllocNonPaged(sizeof *NonPaged);
    if (0 == NonPaged)
        return STATUS_INSUFFICIENT_RESOURCES;

    FSP_FILE_NODE *FileNode = FspAlloc(sizeof *FileNode + ExtraSize);
    if (0 == FileNode)
    {
        FspFree(NonPaged);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NonPaged, sizeof *NonPaged);
    ExInitializeResourceLite(&NonPaged->Resource);
    ExInitializeResourceLite(&NonPaged->PagingIoResource);
    ExInitializeFastMutex(&NonPaged->HeaderFastMutex);

    RtlZeroMemory(FileNode, sizeof *FileNode + ExtraSize);
    FileNode->Header.NodeTypeCode = FspFileNodeFileKind;
    FileNode->Header.NodeByteSize = sizeof *FileNode;
    FileNode->Header.IsFastIoPossible = FastIoIsQuestionable;
    FileNode->Header.Resource = &NonPaged->Resource;
    FileNode->Header.PagingIoResource = &NonPaged->PagingIoResource;
    FileNode->Header.ValidDataLength.QuadPart = 0x7fffffffffffffffLL;
        /* disable ValidDataLength functionality */
    FsRtlSetupAdvancedHeader(&FileNode->Header, &NonPaged->HeaderFastMutex);
    FileNode->NonPaged = NonPaged;
    FileNode->RefCount = 1;
    FileNode->FsvolDeviceObject = DeviceObject;
    FspDeviceReference(FileNode->FsvolDeviceObject);
    RtlInitEmptyUnicodeString(&FileNode->FileName, FileNode->FileNameBuf, (USHORT)ExtraSize);

    *PFileNode = FileNode;

    return STATUS_SUCCESS;
}

VOID FspFileNodeDelete(FSP_FILE_NODE *FileNode)
{
    PAGED_CODE();

    FsRtlTeardownPerStreamContexts(&FileNode->Header);

    FspDeviceDereference(FileNode->FsvolDeviceObject);

    ExDeleteResourceLite(&FileNode->NonPaged->PagingIoResource);
    ExDeleteResourceLite(&FileNode->NonPaged->Resource);
    FspFree(FileNode->NonPaged);

    FspFree(FileNode);
}

VOID FspFileNodeAcquireShared(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    if (Flags & FspFileNodeAcquireMain)
        ExAcquireResourceSharedLite(FileNode->Header.Resource, TRUE);

    if (Flags & FspFileNodeAcquirePgio)
        ExAcquireResourceSharedLite(FileNode->Header.PagingIoResource, TRUE);
}

BOOLEAN FspFileNodeTryAcquireShared(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    BOOLEAN Result = FALSE;

    if (Flags & FspFileNodeAcquireMain)
    {
        Result = ExAcquireResourceSharedLite(FileNode->Header.Resource, FALSE);
        if (!Result)
            return FALSE;
    }

    if (Flags & FspFileNodeAcquirePgio)
    {
        Result = ExAcquireResourceSharedLite(FileNode->Header.PagingIoResource, FALSE);
        if (!Result)
        {
            if (Flags & FspFileNodeAcquireMain)
                ExReleaseResourceLite(FileNode->Header.Resource);
            return FALSE;
        }
    }

    return Result;
}

VOID FspFileNodeAcquireExclusive(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    if (Flags & FspFileNodeAcquireMain)
        ExAcquireResourceExclusiveLite(FileNode->Header.Resource, TRUE);

    if (Flags & FspFileNodeAcquirePgio)
        ExAcquireResourceExclusiveLite(FileNode->Header.PagingIoResource, TRUE);
}

BOOLEAN FspFileNodeTryAcquireExclusive(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    BOOLEAN Result = FALSE;

    if (Flags & FspFileNodeAcquireMain)
    {
        Result = ExAcquireResourceExclusiveLite(FileNode->Header.Resource, FALSE);
        if (!Result)
            return FALSE;
    }

    if (Flags & FspFileNodeAcquirePgio)
    {
        Result = ExAcquireResourceExclusiveLite(FileNode->Header.PagingIoResource, FALSE);
        if (!Result)
        {
            if (Flags & FspFileNodeAcquireMain)
                ExReleaseResourceLite(FileNode->Header.Resource);
            return FALSE;
        }
    }

    return Result;
}

VOID FspFileNodeRelease(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    if (Flags & FspFileNodeAcquirePgio)
        ExReleaseResourceLite(FileNode->Header.PagingIoResource);

    if (Flags & FspFileNodeAcquireMain)
        ExReleaseResourceLite(FileNode->Header.Resource);
}

FSP_FILE_NODE *FspFileNodeOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    DWORD GrantedAccess, DWORD ShareAccess, BOOLEAN DeleteOnClose, NTSTATUS *PResult)
{
    /*
     * Attempt to insert our FileNode into the volume device's generic table.
     * If an FileNode with the same UserContext already exists, then use that
     * FileNode instead.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FILE_NODE *OpenedFileNode;
    BOOLEAN Inserted;
    NTSTATUS Result;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    OpenedFileNode = FspFsvolDeviceInsertContext(FsvolDeviceObject,
        FileNode->UserContext, FileNode, &FileNode->ElementStorage, &Inserted);
    ASSERT(0 != OpenedFileNode);

    if (Inserted)
    {
        /*
         * The new FileNode was inserted into the Context table. Set its share access
         * and reference and open it. There should be (at least) two references to this
         * FileNode, one from our caller and one from the Context table.
         */
        ASSERT(OpenedFileNode == FileNode);

        IoSetShareAccess(GrantedAccess, ShareAccess, FileObject,
            &OpenedFileNode->ShareAccess);
    }
    else
    {
        /*
         * The new FileNode was NOT inserted into the Context table. Instead we are
         * opening a prior FileNode that we found in the table.
         */
        ASSERT(OpenedFileNode != FileNode);

        if (OpenedFileNode->Flags.DeletePending)
        {
            Result = STATUS_DELETE_PENDING;
            goto exit;
        }

        /*
         * FastFat says to do the following on Vista and above.
         *
         * Quote:
         *     Do an extra test for writeable user sections if the user did not allow
         *     write sharing - this is neccessary since a section may exist with no handles
         *     open to the file its based against.
         */
        if (!FlagOn(ShareAccess, FILE_SHARE_WRITE) &&
            FlagOn(GrantedAccess,
                FILE_EXECUTE | FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE) &&
            MmDoesFileHaveUserWritableReferences(&OpenedFileNode->NonPaged->SectionObjectPointers))
        {
            Result = STATUS_SHARING_VIOLATION;
            goto exit;
        }

        /* share access check */
        Result = IoCheckShareAccess(GrantedAccess, ShareAccess, FileObject,
            &OpenedFileNode->ShareAccess, TRUE);

    exit:
        if (!NT_SUCCESS(Result))
        {
            if (0 != PResult)
                *PResult = Result;

            OpenedFileNode = 0;
        }
    }

    if (0 != OpenedFileNode)
    {
        FspFileNodeReference(OpenedFileNode);
        OpenedFileNode->OpenCount++;

        if (DeleteOnClose)
            OpenedFileNode->Flags.DeleteOnClose = TRUE;
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    return OpenedFileNode;
}

VOID FspFileNodeClose(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    PBOOLEAN PDeletePending)
{
    /*
     * Close the FileNode. If the OpenCount becomes zero remove it
     * from the Context table.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    BOOLEAN Deleted = FALSE, DeletePending;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    if (FileNode->Flags.DeleteOnClose)
        FileNode->Flags.DeletePending = TRUE;
    DeletePending = 0 != FileNode->Flags.DeletePending;

    IoRemoveShareAccess(FileObject, &FileNode->ShareAccess);
    if (0 == --FileNode->OpenCount)
        FspFsvolDeviceDeleteContext(FsvolDeviceObject, FileNode->UserContext, &Deleted);

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (Deleted)
        FspFileNodeDereference(FileNode);

    if (0 != PDeletePending)
        *PDeletePending = Deleted && DeletePending;
}

NTSTATUS FspFileDescCreate(FSP_FILE_DESC **PFileDesc)
{
    PAGED_CODE();

    *PFileDesc = FspAlloc(sizeof(FSP_FILE_DESC));
    if (0 == *PFileDesc)
        return STATUS_INSUFFICIENT_RESOURCES;

    RtlZeroMemory(*PFileDesc, sizeof(FSP_FILE_DESC));

    return STATUS_SUCCESS;
}

VOID FspFileDescDelete(FSP_FILE_DESC *FileDesc)
{
    PAGED_CODE();

    FspFree(FileDesc);
}
