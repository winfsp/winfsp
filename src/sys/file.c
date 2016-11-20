/**
 * @file sys/file.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <sys/driver.h>

NTSTATUS FspFileNodeCopyList(PDEVICE_OBJECT DeviceObject,
    FSP_FILE_NODE ***PFileNodes, PULONG PFileNodeCount);
VOID FspFileNodeDeleteList(FSP_FILE_NODE **FileNodes, ULONG FileNodeCount);
NTSTATUS FspFileNodeCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_NODE **PFileNode);
VOID FspFileNodeDelete(FSP_FILE_NODE *FileNode);
VOID FspFileNodeAcquireSharedF(FSP_FILE_NODE *FileNode, ULONG Flags);
BOOLEAN FspFileNodeTryAcquireSharedF(FSP_FILE_NODE *FileNode, ULONG Flags, BOOLEAN Wait);
VOID FspFileNodeAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags);
BOOLEAN FspFileNodeTryAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags, BOOLEAN Wait);
VOID FspFileNodeConvertExclusiveToSharedF(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeSetOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner);
VOID FspFileNodeReleaseF(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeReleaseOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner);
NTSTATUS FspFileNodeOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    UINT32 GrantedAccess, UINT32 ShareAccess,
    FSP_FILE_NODE **POpenedFileNode, PULONG PSharingViolationReason);
VOID FspFileNodeCleanup(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    PBOOLEAN PDeletePending);
VOID FspFileNodeCleanupComplete(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject);
VOID FspFileNodeClose(FSP_FILE_NODE *FileNode,
    PFILE_OBJECT FileObject,    /* non-0 to remove share access */
    BOOLEAN HandleCleanup);     /* TRUE to decrement handle count */
NTSTATUS FspFileNodeFlushAndPurgeCache(FSP_FILE_NODE *FileNode,
    UINT64 FlushOffset64, ULONG FlushLength, BOOLEAN FlushAndPurge);
NTSTATUS FspFileNodeCheckBatchOplocksOnAllStreams(
    PDEVICE_OBJECT FsvolDeviceObject,
    PIRP OplockIrp,
    FSP_FILE_NODE *FileNode,
    PUNICODE_STRING StreamFileName);
BOOLEAN FspFileNodeRenameCheck(PDEVICE_OBJECT FsvolDeviceObject, PIRP OplockIrp,
    FSP_FILE_NODE *FileNode, PUNICODE_STRING FileName);
VOID FspFileNodeRename(FSP_FILE_NODE *FileNode, PUNICODE_STRING NewFileName);
VOID FspFileNodeGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTryGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
VOID FspFileNodeSetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTrySetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, ULONG InfoChangeNumber);
BOOLEAN FspFileNodeReferenceSecurity(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG SecurityChangeNumber);
BOOLEAN FspFileNodeReferenceDirInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG DirInfoChangeNumber);
static VOID FspFileNodeInvalidateDirInfo(FSP_FILE_NODE *FileNode);
BOOLEAN FspFileNodeReferenceStreamInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG StreamInfoChangeNumber);
static VOID FspFileNodeInvalidateStreamInfo(FSP_FILE_NODE *FileNode);
VOID FspFileNodeNotifyChange(FSP_FILE_NODE *FileNode,
    ULONG Filter, ULONG Action);
NTSTATUS FspFileNodeProcessLockIrp(FSP_FILE_NODE *FileNode, PIRP Irp);
static NTSTATUS FspFileNodeCompleteLockIrp(PVOID Context, PIRP Irp);
NTSTATUS FspFileDescCreate(FSP_FILE_DESC **PFileDesc);
VOID FspFileDescDelete(FSP_FILE_DESC *FileDesc);
NTSTATUS FspFileDescResetDirectoryPattern(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName, BOOLEAN Reset);
NTSTATUS FspMainFileOpen(
    PDEVICE_OBJECT FsvolDeviceObject,
    PDEVICE_OBJECT DeviceObjectHint,
    PUNICODE_STRING MainFileName, BOOLEAN CaseSensitive,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    ULONG FileAttributes,
    ULONG Disposition,
    PHANDLE PMainFileHandle,
    PFILE_OBJECT *PMainFileObject);
NTSTATUS FspMainFileClose(
    HANDLE MainFileHandle,
    PFILE_OBJECT MainFileObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFileNodeCopyList)
#pragma alloc_text(PAGE, FspFileNodeDeleteList)
#pragma alloc_text(PAGE, FspFileNodeCreate)
#pragma alloc_text(PAGE, FspFileNodeDelete)
#pragma alloc_text(PAGE, FspFileNodeAcquireSharedF)
#pragma alloc_text(PAGE, FspFileNodeTryAcquireSharedF)
#pragma alloc_text(PAGE, FspFileNodeAcquireExclusiveF)
#pragma alloc_text(PAGE, FspFileNodeTryAcquireExclusiveF)
#pragma alloc_text(PAGE, FspFileNodeConvertExclusiveToSharedF)
#pragma alloc_text(PAGE, FspFileNodeSetOwnerF)
#pragma alloc_text(PAGE, FspFileNodeReleaseF)
#pragma alloc_text(PAGE, FspFileNodeReleaseOwnerF)
#pragma alloc_text(PAGE, FspFileNodeOpen)
#pragma alloc_text(PAGE, FspFileNodeCleanup)
#pragma alloc_text(PAGE, FspFileNodeCleanupComplete)
#pragma alloc_text(PAGE, FspFileNodeClose)
#pragma alloc_text(PAGE, FspFileNodeFlushAndPurgeCache)
#pragma alloc_text(PAGE, FspFileNodeCheckBatchOplocksOnAllStreams)
#pragma alloc_text(PAGE, FspFileNodeRenameCheck)
#pragma alloc_text(PAGE, FspFileNodeRename)
#pragma alloc_text(PAGE, FspFileNodeGetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeTryGetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeSetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeTrySetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeReferenceSecurity)
#pragma alloc_text(PAGE, FspFileNodeSetSecurity)
#pragma alloc_text(PAGE, FspFileNodeTrySetSecurity)
// !#pragma alloc_text(PAGE, FspFileNodeReferenceDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeSetDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeTrySetDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeInvalidateDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeReferenceStreamInfo)
// !#pragma alloc_text(PAGE, FspFileNodeSetStreamInfo)
// !#pragma alloc_text(PAGE, FspFileNodeTrySetStreamInfo)
// !#pragma alloc_text(PAGE, FspFileNodeInvalidateStreamInfo)
#pragma alloc_text(PAGE, FspFileNodeNotifyChange)
#pragma alloc_text(PAGE, FspFileNodeProcessLockIrp)
#pragma alloc_text(PAGE, FspFileNodeCompleteLockIrp)
#pragma alloc_text(PAGE, FspFileDescCreate)
#pragma alloc_text(PAGE, FspFileDescDelete)
#pragma alloc_text(PAGE, FspFileDescResetDirectoryPattern)
#pragma alloc_text(PAGE, FspMainFileOpen)
#pragma alloc_text(PAGE, FspMainFileClose)
#endif

#define FSP_FILE_NODE_GET_FLAGS()       \
    PIRP Irp = IoGetTopLevelIrp();      \
    BOOLEAN IrpValid = (PIRP)FSRTL_MAX_TOP_LEVEL_IRP_FLAG < Irp &&\
        IO_TYPE_IRP == Irp->Type;       \
    if (IrpValid)                       \
        Flags &= ~FspIrpTopFlags(Irp)
#define FSP_FILE_NODE_ASSERT_FLAGS_CLR()\
    ASSERT(IrpValid ? (0 == (FspIrpFlags(Irp) & Flags)) : TRUE)
#define FSP_FILE_NODE_ASSERT_FLAGS_SET()\
    ASSERT(IrpValid ? (Flags == (FspIrpFlags(Irp) & Flags)) : TRUE)
#define FSP_FILE_NODE_SET_FLAGS()       \
    if (IrpValid)                       \
        FspIrpSetFlags(Irp, FspIrpFlags(Irp) | Flags)
#define FSP_FILE_NODE_CLR_FLAGS()       \
    if (IrpValid)                       \
        FspIrpSetFlags(Irp, FspIrpFlags(Irp) & (~Flags & 3))

NTSTATUS FspFileNodeCopyList(PDEVICE_OBJECT DeviceObject,
    FSP_FILE_NODE ***PFileNodes, PULONG PFileNodeCount)
{
    PAGED_CODE();

    NTSTATUS Result;
    ULONG Index;

    FspFsvolDeviceLockContextTable(DeviceObject);
    Result = FspFsvolDeviceCopyContextByNameList(DeviceObject, PFileNodes, PFileNodeCount);
    if (NT_SUCCESS(Result))
    {
        for (Index = 0; *PFileNodeCount > Index; Index++)
            FspFileNodeReference((*PFileNodes)[Index]);
    }
    FspFsvolDeviceUnlockContextTable(DeviceObject);

    return Result;
}

VOID FspFileNodeDeleteList(FSP_FILE_NODE **FileNodes, ULONG FileNodeCount)
{
    PAGED_CODE();

    ULONG Index;

    for (Index = 0; FileNodeCount > Index; Index++)
        FspFileNodeDereference(FileNodes[Index]);

    FspFsvolDeviceDeleteContextByNameList(FileNodes, FileNodeCount);
}

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
    KeInitializeSpinLock(&NonPaged->NpInfoSpinLock);

    RtlZeroMemory(FileNode, sizeof *FileNode + ExtraSize);
    FileNode->Header.NodeTypeCode = FspFileNodeFileKind;
    FileNode->Header.NodeByteSize = sizeof *FileNode;
    FileNode->Header.IsFastIoPossible = FastIoIsNotPossible;
    FileNode->Header.Resource = &NonPaged->Resource;
    FileNode->Header.PagingIoResource = &NonPaged->PagingIoResource;
    FileNode->Header.ValidDataLength.QuadPart = MAXLONGLONG;
        /* disable ValidDataLength functionality */
    FsRtlSetupAdvancedHeader(&FileNode->Header, &NonPaged->HeaderFastMutex);
    FileNode->NonPaged = NonPaged;
    FileNode->RefCount = 1;
    FileNode->FsvolDeviceObject = DeviceObject;
    FspDeviceReference(FileNode->FsvolDeviceObject);
    RtlInitEmptyUnicodeString(&FileNode->FileName, FileNode->FileNameBuf, (USHORT)ExtraSize);

    FsRtlInitializeFileLock(&FileNode->FileLock, FspFileNodeCompleteLockIrp, 0);
    FsRtlInitializeOplock(FspFileNodeAddrOfOplock(FileNode));

    *PFileNode = FileNode;

    return STATUS_SUCCESS;
}

VOID FspFileNodeDelete(FSP_FILE_NODE *FileNode)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);

    FsRtlUninitializeOplock(FspFileNodeAddrOfOplock(FileNode));
    FsRtlUninitializeFileLock(&FileNode->FileLock);

    FsRtlTeardownPerStreamContexts(&FileNode->Header);

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->StreamInfoCache, FileNode->NonPaged->StreamInfo);
    FspMetaCacheInvalidateItem(FsvolDeviceExtension->DirInfoCache, FileNode->NonPaged->DirInfo);
    FspMetaCacheInvalidateItem(FsvolDeviceExtension->SecurityCache, FileNode->Security);

    FspDeviceDereference(FileNode->FsvolDeviceObject);

    if (0 != FileNode->ExternalFileName)
        FspFree(FileNode->ExternalFileName);

    ExDeleteResourceLite(&FileNode->NonPaged->PagingIoResource);
    ExDeleteResourceLite(&FileNode->NonPaged->Resource);
    FspFree(FileNode->NonPaged);

    FspFree(FileNode);
}

VOID FspFileNodeAcquireSharedF(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();
    FSP_FILE_NODE_ASSERT_FLAGS_CLR();

    if (Flags & FspFileNodeAcquireMain)
        ExAcquireResourceSharedLite(FileNode->Header.Resource, TRUE);

    if (Flags & FspFileNodeAcquirePgio)
        ExAcquireResourceSharedLite(FileNode->Header.PagingIoResource, TRUE);

    FSP_FILE_NODE_SET_FLAGS();
}

BOOLEAN FspFileNodeTryAcquireSharedF(FSP_FILE_NODE *FileNode, ULONG Flags, BOOLEAN Wait)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();
    FSP_FILE_NODE_ASSERT_FLAGS_CLR();

    BOOLEAN Result = TRUE;

    if (Flags & FspFileNodeAcquireMain)
    {
        Result = ExAcquireResourceSharedLite(FileNode->Header.Resource, Wait);
        if (!Result)
            return FALSE;
    }

    if (Flags & FspFileNodeAcquirePgio)
    {
        Result = ExAcquireResourceSharedLite(FileNode->Header.PagingIoResource, Wait);
        if (!Result)
        {
            if (Flags & FspFileNodeAcquireMain)
                ExReleaseResourceLite(FileNode->Header.Resource);
            return FALSE;
        }
    }

    if (Result)
        FSP_FILE_NODE_SET_FLAGS();

    return Result;
}

VOID FspFileNodeAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();
    FSP_FILE_NODE_ASSERT_FLAGS_CLR();

    if (Flags & FspFileNodeAcquireMain)
        ExAcquireResourceExclusiveLite(FileNode->Header.Resource, TRUE);

    if (Flags & FspFileNodeAcquirePgio)
        ExAcquireResourceExclusiveLite(FileNode->Header.PagingIoResource, TRUE);

    FSP_FILE_NODE_SET_FLAGS();
}

BOOLEAN FspFileNodeTryAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags, BOOLEAN Wait)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();
    FSP_FILE_NODE_ASSERT_FLAGS_CLR();

    BOOLEAN Result = TRUE;

    if (Flags & FspFileNodeAcquireMain)
    {
        Result = ExAcquireResourceExclusiveLite(FileNode->Header.Resource, Wait);
        if (!Result)
            return FALSE;
    }

    if (Flags & FspFileNodeAcquirePgio)
    {
        Result = ExAcquireResourceExclusiveLite(FileNode->Header.PagingIoResource, Wait);
        if (!Result)
        {
            if (Flags & FspFileNodeAcquireMain)
                ExReleaseResourceLite(FileNode->Header.Resource);
            return FALSE;
        }
    }

    if (Result)
        FSP_FILE_NODE_SET_FLAGS();

    return Result;
}

VOID FspFileNodeConvertExclusiveToSharedF(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();

    if (Flags & FspFileNodeAcquirePgio)
        ExConvertExclusiveToSharedLite(FileNode->Header.PagingIoResource);

    if (Flags & FspFileNodeAcquireMain)
        ExConvertExclusiveToSharedLite(FileNode->Header.Resource);
}

VOID FspFileNodeSetOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();

    Owner = (PVOID)((UINT_PTR)Owner | 3);

    if (Flags & FspFileNodeAcquireMain)
        ExSetResourceOwnerPointer(FileNode->Header.Resource, Owner);

    if (Flags & FspFileNodeAcquirePgio)
        ExSetResourceOwnerPointer(FileNode->Header.PagingIoResource, Owner);
}

VOID FspFileNodeReleaseF(FSP_FILE_NODE *FileNode, ULONG Flags)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();
    FSP_FILE_NODE_ASSERT_FLAGS_SET();

    if (Flags & FspFileNodeAcquirePgio)
        ExReleaseResourceLite(FileNode->Header.PagingIoResource);

    if (Flags & FspFileNodeAcquireMain)
        ExReleaseResourceLite(FileNode->Header.Resource);

    FSP_FILE_NODE_CLR_FLAGS();
}

VOID FspFileNodeReleaseOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FILE_NODE_GET_FLAGS();
    FSP_FILE_NODE_ASSERT_FLAGS_SET();

    Owner = (PVOID)((UINT_PTR)Owner | 3);

    if (Flags & FspFileNodeAcquirePgio)
    {
        if (ExIsResourceAcquiredLite(FileNode->Header.PagingIoResource))
            ExReleaseResourceLite(FileNode->Header.PagingIoResource);
        else
            ExReleaseResourceForThreadLite(FileNode->Header.PagingIoResource, (ERESOURCE_THREAD)Owner);
    }

    if (Flags & FspFileNodeAcquireMain)
    {
        if (ExIsResourceAcquiredLite(FileNode->Header.Resource))
            ExReleaseResourceLite(FileNode->Header.Resource);
        else
            ExReleaseResourceForThreadLite(FileNode->Header.Resource, (ERESOURCE_THREAD)Owner);
    }

    FSP_FILE_NODE_CLR_FLAGS();
}

NTSTATUS FspFileNodeOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    UINT32 GrantedAccess, UINT32 ShareAccess,
    FSP_FILE_NODE **POpenedFileNode, PULONG PSharingViolationReason)
{
    /*
     * Attempt to insert our FileNode into the volume device's generic table.
     * If an FileNode with the same UserContext already exists, then use that
     * FileNode instead.
     *
     * There is no FileNode that can be acquired when calling this function.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FILE_NODE *OpenedFileNode = 0;
    BOOLEAN Inserted, DeletePending;
    NTSTATUS Result;

    *PSharingViolationReason = FspFileNodeSharingViolationGeneral;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    /*
     * If this is a named stream we must also check with our main file.
     * Note that FileNode->MainFileNode and OpenedFileNode->MainFileNode
     * will always be the same.
     */
    if (0 != FileNode->MainFileNode)
    {
        DeletePending = 0 != FileNode->MainFileNode->DeletePending;
        MemoryBarrier();
        if (DeletePending)
        {
            Result = STATUS_DELETE_PENDING;
            goto exit;
        }

        /*
         * Sharing violations between main file and streams were determined
         * through experimentation with NTFS. They may be wrong!
         */
        if (0 < FileNode->MainFileNode->MainFileDenyDeleteCount)
        {
            if (!FlagOn(ShareAccess, FILE_SHARE_DELETE) &&
                FlagOn(GrantedAccess,
                    FILE_EXECUTE | FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE))
            {
                OpenedFileNode = FileNode->MainFileNode;
                *PSharingViolationReason = FspFileNodeSharingViolationMainFile;
                Result = STATUS_SHARING_VIOLATION;
                goto exit;
            }
        }
    }

    OpenedFileNode = FspFsvolDeviceInsertContextByName(FsvolDeviceObject,
        &FileNode->FileName, FileNode, &FileNode->ContextByNameElementStorage, &Inserted);
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
        ASSERT(OpenedFileNode->MainFileNode == FileNode->MainFileNode);

        DeletePending = 0 != OpenedFileNode->DeletePending;
        MemoryBarrier();
        if (DeletePending)
        {
            Result = STATUS_DELETE_PENDING;
            goto exit;
        }

        /*
         * Sharing violations between main file and streams were determined
         * through experimentation with NTFS. They may be wrong!
         */
        if (0 < OpenedFileNode->StreamDenyDeleteCount)
        {
            /* we must be the main file! */
            ASSERT(0 == OpenedFileNode->MainFileNode);

            if (FlagOn(GrantedAccess, DELETE))
            {
                *PSharingViolationReason = FspFileNodeSharingViolationStream;
                Result = STATUS_SHARING_VIOLATION;
                goto exit;
            }
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
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    /*
     * No more failures allowed at this point!
     * This is because we have potentially inserted a new FileNode into the Context table.
     * We also updated OpenedFileNode->ShareAccess in IoSetShareAccess/IoCheckShareAccess.
     */

    /*
     * Sharing violations between main file and streams were determined
     * through experimentation with NTFS. They may be wrong!
     */
    if (0 == OpenedFileNode->MainFileNode)
    {
        if (FileObject->DeleteAccess)
            OpenedFileNode->MainFileDenyDeleteCount++;
    }
    else
    {
        if ((FileObject->ReadAccess || FileObject->WriteAccess || FileObject->DeleteAccess) &&
            !FileObject->SharedDelete)
            OpenedFileNode->MainFileNode->StreamDenyDeleteCount++;
    }

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && STATUS_SHARING_VIOLATION != Result)
        OpenedFileNode = 0;

    if (0 != OpenedFileNode)
    {
        FspFileNodeReference(OpenedFileNode);
        OpenedFileNode->OpenCount++;
        OpenedFileNode->HandleCount++;
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    *POpenedFileNode = OpenedFileNode;

    return Result;
}

VOID FspFileNodeCleanup(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    PBOOLEAN PDeletePending)
{
    /*
     * Determine whether a FileNode should be deleted. Note that when FileNode->DeletePending
     * is set, the OpenCount/HandleCount cannot be increased because FspFileNodeOpen() will
     * return STATUS_DELETE_PENDING.
     *
     * The FileNode must be acquired exclusive (Main) when calling this function.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    BOOLEAN DeletePending, SingleHandle;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    if (FileDesc->DeleteOnClose)
        FileNode->DeletePending = TRUE;
    DeletePending = 0 != FileNode->DeletePending;
    MemoryBarrier();

    SingleHandle = 1 == FileNode->HandleCount;

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (0 != PDeletePending)
        *PDeletePending = SingleHandle && DeletePending;
}

VOID FspFileNodeCleanupComplete(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject)
{
    /*
     * Complete the cleanup of a FileNode. Remove its share access and
     * finalize its cache.
     *
     * NOTE: If the FileNode is not being deleted (!FileNode->DeletePending)
     * the FileNode REMAINS in the Context table until Close time!
     * This is so that if there are mapped views or write behind's pending
     * when a file gets reopened the FileNode will be correctly reused.
     *
     * The FileNode must be acquired exclusive (Main) when calling this function.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    LARGE_INTEGER TruncateSize = { 0 }, *PTruncateSize = 0;
    BOOLEAN DeletePending;
    BOOLEAN DeletedFromContextTable = FALSE;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    /*
     * Sharing violations between main file and streams were determined
     * through experimentation with NTFS. They may be wrong!
     */
    if (0 == FileNode->MainFileNode)
    {
        if (FileObject->DeleteAccess)
            FileNode->MainFileDenyDeleteCount--;
    }
    else
    {
        if ((FileObject->ReadAccess || FileObject->WriteAccess || FileObject->DeleteAccess) &&
            !FileObject->SharedDelete)
            FileNode->MainFileNode->StreamDenyDeleteCount--;
    }

    IoRemoveShareAccess(FileObject, &FileNode->ShareAccess);

    if (0 == --FileNode->HandleCount)
    {
        DeletePending = 0 != FileNode->DeletePending;
        MemoryBarrier();

        if (DeletePending)
        {
            PTruncateSize = &TruncateSize;

            FspFsvolDeviceDeleteContextByName(FsvolDeviceObject, &FileNode->FileName,
                &DeletedFromContextTable);
            ASSERT(DeletedFromContextTable);

            FileNode->OpenCount = 0;
        }
        else if (FileNode->TruncateOnClose && FlagOn(FileObject->Flags, FO_CACHE_SUPPORTED))
        {
            /*
             * Even when the FileInfo is expired, this is the best guess for a file size
             * without asking the user-mode file system.
             */
            TruncateSize = FileNode->Header.FileSize;
            PTruncateSize = &TruncateSize;
        }
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    CcUninitializeCacheMap(FileObject, PTruncateSize, 0);

    if (DeletedFromContextTable)
        FspFileNodeDereference(FileNode);
}

VOID FspFileNodeClose(FSP_FILE_NODE *FileNode,
    PFILE_OBJECT FileObject,    /* non-0 to remove share access */
    BOOLEAN HandleCleanup)      /* TRUE to decrement handle count */
{
    /*
     * Close the FileNode. If the OpenCount becomes zero remove it
     * from the Context table.
     *
     * The FileNode may or may not be acquired when calling this function.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    BOOLEAN DeletedFromContextTable = FALSE;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    if (0 != FileObject)
    {
        /*
         * Sharing violations between main file and streams were determined
         * through experimentation with NTFS. They may be wrong!
         */
        if (0 == FileNode->MainFileNode)
        {
            if (FileObject->DeleteAccess)
                FileNode->MainFileDenyDeleteCount--;
        }
        else
        {
            if ((FileObject->ReadAccess || FileObject->WriteAccess || FileObject->DeleteAccess) &&
                !FileObject->SharedDelete)
                FileNode->MainFileNode->StreamDenyDeleteCount--;
        }

        IoRemoveShareAccess(FileObject, &FileNode->ShareAccess);
    }

    if (HandleCleanup)
        FileNode->HandleCount--;

    if (0 < FileNode->OpenCount && 0 == --FileNode->OpenCount)
    {
        FspFsvolDeviceDeleteContextByName(FsvolDeviceObject, &FileNode->FileName,
            &DeletedFromContextTable);
        ASSERT(DeletedFromContextTable);
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (DeletedFromContextTable)
        FspFileNodeDereference(FileNode);
}

NTSTATUS FspFileNodeFlushAndPurgeCache(FSP_FILE_NODE *FileNode,
    UINT64 FlushOffset64, ULONG FlushLength, BOOLEAN FlushAndPurge)
{
    /*
     * The FileNode must be acquired exclusive (Full) when calling this function.
     */

    PAGED_CODE();

    LARGE_INTEGER FlushOffset;
    PLARGE_INTEGER PFlushOffset = &FlushOffset;
    FSP_FSCTL_FILE_INFO FileInfo;
    IO_STATUS_BLOCK IoStatus = { STATUS_SUCCESS };

    FlushOffset.QuadPart = FlushOffset64;
    if (FILE_WRITE_TO_END_OF_FILE == FlushOffset.LowPart && -1L == FlushOffset.HighPart)
    {
        if (FspFileNodeTryGetFileInfo(FileNode, &FileInfo))
            FlushOffset.QuadPart = FileInfo.FileSize;
        else
            PFlushOffset = 0; /* we don't know how big the file is, so flush it all! */
    }

    /* it is unclear if the Cc* calls below can raise or not; wrap them just in case */
    try
    {
        if (0 != FspMvCcCoherencyFlushAndPurgeCache)
        {
            /* if we are on Win7+ use CcCoherencyFlushAndPurgeCache */
            FspMvCcCoherencyFlushAndPurgeCache(
                &FileNode->NonPaged->SectionObjectPointers, PFlushOffset, FlushLength, &IoStatus,
                FlushAndPurge ? 0 : CC_FLUSH_AND_PURGE_NO_PURGE);

            if (STATUS_CACHE_PAGE_LOCKED == IoStatus.Status)
                IoStatus.Status = STATUS_SUCCESS;
        }
        else
        {
            /* do it the old-fashioned way; non-cached and mmap'ed I/O are non-coherent */
            CcFlushCache(&FileNode->NonPaged->SectionObjectPointers, PFlushOffset, FlushLength, &IoStatus);
            if (NT_SUCCESS(IoStatus.Status))
            {
                if (FlushAndPurge)
                    CcPurgeCacheSection(&FileNode->NonPaged->SectionObjectPointers, PFlushOffset, FlushLength, FALSE);

                IoStatus.Status = STATUS_SUCCESS;
            }
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        IoStatus.Status = GetExceptionCode();
    }

    return IoStatus.Status;
}

NTSTATUS FspFileNodeCheckBatchOplocksOnAllStreams(
    PDEVICE_OBJECT FsvolDeviceObject,
    PIRP OplockIrp,
    FSP_FILE_NODE *FileNode,
    PUNICODE_STRING StreamFileName)
{
    /*
     * Called during Create processing. The device rename resource has been acquired shared.
     * No concurrent renames are allowed.
     */

    PAGED_CODE();

    ASSERT(0 == FileNode->MainFileNode);

    FSP_FILE_NODE *DescendantFileNode;
    FSP_FILE_NODE *DescendantFileNodeArray[16], **DescendantFileNodes;
    ULONG DescendantFileNodeCount, DescendantFileNodeIndex;
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY RestartKey;
    USHORT FileNameLength = FileNode->FileName.Length;
    BOOLEAN CaseInsensitive = !FspFsvolDeviceExtension(FsvolDeviceObject)->
        VolumeParams.CaseSensitiveSearch;
    NTSTATUS Result;

    DescendantFileNodes = DescendantFileNodeArray;
    DescendantFileNodeCount = 0;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    /* count descendant file nodes and try to gather them in a local array if possible */
    memset(&RestartKey, 0, sizeof RestartKey);
    for (;;)
    {
        DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
            &FileNode->FileName, FALSE, &RestartKey);
        if (0 == DescendantFileNode)
            break;
        if (DescendantFileNode->FileName.Length > FileNameLength &&
            L'\\' == DescendantFileNode->FileName.Buffer[FileNameLength / sizeof(WCHAR)])
            break;

        if (1 >= DescendantFileNode->HandleCount)
            continue;

        if (0 != StreamFileName)
        {
            if (DescendantFileNode != FileNode &&
                0 != FspFileNameCompare(&DescendantFileNode->FileName, StreamFileName,
                    CaseInsensitive, 0))
                continue;
        }

        /* keep a reference to the FileNode in case it goes away in later processing */
        FspFileNodeReference(DescendantFileNode);

        if (ARRAYSIZE(DescendantFileNodeArray) > DescendantFileNodeCount)
            DescendantFileNodes[DescendantFileNodeCount] = DescendantFileNode;
        DescendantFileNodeCount++;
    }

    ASSERT(0 != StreamFileName || DescendantFileNodeCount <= 2);

    /* if the local array is out of space, gather descendant file nodes in the pool */
    if (ARRAYSIZE(DescendantFileNodeArray) < DescendantFileNodeCount)
    {
        ASSERT(0 == StreamFileName);

        DescendantFileNodes = FspAllocMustSucceed(DescendantFileNodeCount * sizeof(FSP_FILE_NODE *));
        DescendantFileNodeIndex = 0;
        memset(&RestartKey, 0, sizeof RestartKey);
        for (;;)
        {
            DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
                &FileNode->FileName, FALSE, &RestartKey);
            if (0 == DescendantFileNode)
                break;
            if (DescendantFileNode->FileName.Length > FileNameLength &&
                L'\\' == DescendantFileNode->FileName.Buffer[FileNameLength / sizeof(WCHAR)])
                break;

            if (1 >= DescendantFileNode->HandleCount)
                continue;

            DescendantFileNodes[DescendantFileNodeIndex] = DescendantFileNode;
            DescendantFileNodeIndex++;
            ASSERT(DescendantFileNodeCount >= DescendantFileNodeIndex);
        }

        ASSERT(DescendantFileNodeCount == DescendantFileNodeIndex);
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    /*
     * At this point all descendant FileNode's are enumerated and referenced.
     */

    /* break any Batch or Handle oplocks on descendants */
    Result = STATUS_SUCCESS;
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];

        if (FspFileNodeOplockIsBatch(DescendantFileNode))
        {
            NTSTATUS Result0 = FspFileNodeOplockCheck(DescendantFileNode, OplockIrp);
            if (STATUS_SUCCESS != Result0)
                Result = STATUS_SHARING_VIOLATION;
            else
                OplockIrp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;
        }
        else
        if (FspFileNodeOplockIsHandle(DescendantFileNode))
        {
            NTSTATUS Result0 = FspFileNodeOplockBreakHandle(DescendantFileNode, OplockIrp, 0);
            ASSERT(STATUS_OPLOCK_BREAK_IN_PROGRESS != Result0);
            if (STATUS_SUCCESS != Result0)
                Result = STATUS_SHARING_VIOLATION;
        }
    }

    /* dereference all FileNode's referenced during initial enumeration */
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];

        FspFileNodeDereference(DescendantFileNode);
    }

    if (DescendantFileNodeArray != DescendantFileNodes)
        FspFree(DescendantFileNodes);

    return Result;
}

BOOLEAN FspFileNodeRenameCheck(PDEVICE_OBJECT FsvolDeviceObject, PIRP OplockIrp,
    FSP_FILE_NODE *FileNode, PUNICODE_STRING FileName)
{
    PAGED_CODE();

    /*
     * Special rules for renaming open files:
     * -   A file cannot be renamed if it has any open handles,
     *     unless it is only open because of a batch opportunistic lock (oplock)
     *     and the batch oplock can be broken immediately.
     * -   A file cannot be renamed if a file with the same name exists
     *     and has open handles (except in the batch-oplock case described earlier).
     * -   A directory cannot be renamed if it or any of its subdirectories contains a file
     *     that has open handles (except in the batch-oplock case described earlier).
     */

    FSP_FILE_NODE *DescendantFileNode;
    FSP_FILE_NODE *DescendantFileNodeArray[16], **DescendantFileNodes;
    ULONG DescendantFileNodeCount, DescendantFileNodeIndex;
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY RestartKey;
    BOOLEAN CheckingOldName = 0 != FileNode;
    BOOLEAN HasOpenHandles;
    BOOLEAN Success = TRUE;

    DescendantFileNodes = DescendantFileNodeArray;
    DescendantFileNodeCount = 0;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    /* if we are checking the existing file name, do a quick check here */
    if (CheckingOldName)
    {
        /* if file has single open handle (also means no streams open) and not a directory, exit now */
        if (1 == FileNode->HandleCount && !FileNode->IsDirectory)
        {
            ASSERT(Success);
            goto unlock_exit;
        }

        /* Note: when CheckingOldName==TRUE, the old FileNode is not included in enumerations below */
    }

    /* count descendant file nodes and try to gather them in a local array if possible */
    memset(&RestartKey, 0, sizeof RestartKey);
    for (;;)
    {
        DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
            FileName, CheckingOldName, &RestartKey);
        if (0 == DescendantFileNode)
            break;

        /* keep a reference to the FileNode in case it goes away in later processing */
        FspFileNodeReference(DescendantFileNode);

        HasOpenHandles = 0 < DescendantFileNode->HandleCount;

        if (ARRAYSIZE(DescendantFileNodeArray) > DescendantFileNodeCount)
            DescendantFileNodes[DescendantFileNodeCount] =
                (PVOID)((UINT_PTR)DescendantFileNode | HasOpenHandles);
        DescendantFileNodeCount++;
    }

    if (0 == DescendantFileNodeCount)
    {
        ASSERT(Success);
        goto unlock_exit;
    }

    /* if the local array is out of space, gather descendant file nodes in the pool */
    if (ARRAYSIZE(DescendantFileNodeArray) < DescendantFileNodeCount)
    {
        DescendantFileNodes = FspAllocMustSucceed(DescendantFileNodeCount * sizeof(FSP_FILE_NODE *));
        DescendantFileNodeIndex = 0;
        memset(&RestartKey, 0, sizeof RestartKey);
        for (;;)
        {
            DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
                FileName, CheckingOldName, &RestartKey);
            if (0 == DescendantFileNode)
                break;

            HasOpenHandles = 0 < DescendantFileNode->HandleCount;

            DescendantFileNodes[DescendantFileNodeIndex] =
                (PVOID)((UINT_PTR)DescendantFileNode | HasOpenHandles);
            DescendantFileNodeIndex++;
            ASSERT(DescendantFileNodeCount >= DescendantFileNodeIndex);
        }

        ASSERT(DescendantFileNodeCount == DescendantFileNodeIndex);
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    /*
     * At this point all descendant FileNode's are enumerated and referenced.
     * There can be no new FileNode's because Rename has acquired the device's
     * "rename" resource exclusively, which disallows new Opens.
     */

    if (!CheckingOldName)
    {
        /* make sure no processes are mapping any descendants as an image */
        for (
            DescendantFileNodeIndex = 0;
            DescendantFileNodeCount > DescendantFileNodeIndex;
            DescendantFileNodeIndex++)
        {
            DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
            DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~1);

            Success = MmFlushImageSection(&DescendantFileNode->NonPaged->SectionObjectPointers,
                MmFlushForDelete);
            if (!Success)
                goto unlock_exit;
        }
    }

    /* break any Batch or Handle oplocks on descendants */
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
        HasOpenHandles = (UINT_PTR)DescendantFileNode & 1;
        DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~1);

        if (HasOpenHandles)
            if (FspFileNodeOplockIsBatch(DescendantFileNode) ||
                FspFileNodeOplockIsHandle(DescendantFileNode))
                FspFileNodeOplockCheck(DescendantFileNode, OplockIrp);
    }

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    /* recheck whether there are still files with open handles */
    memset(&RestartKey, 0, sizeof RestartKey);
    for (;;)
    {
        DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
            FileName, CheckingOldName, &RestartKey);
        if (0 == DescendantFileNode)
            break;

        if (0 < DescendantFileNode->HandleCount)
        {
            Success = FALSE;
            goto unlock_exit;
        }
    }

unlock_exit:
    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    /* dereference all FileNode's referenced during initial enumeration */
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
        DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~1);

        FspFileNodeDereference(DescendantFileNode);
    }

    if (DescendantFileNodeArray != DescendantFileNodes)
        FspFree(DescendantFileNodes);

    return Success;
}

VOID FspFileNodeRename(FSP_FILE_NODE *FileNode, PUNICODE_STRING NewFileName)
{
    /*
     * FspFileNodeRename may block, because it attempts to acquire the Main
     * resource of descendant file nodes. FspFileNodeRename is called at the
     * completion path of IRP_MJ_SET_INFORMATION[Rename] and an IRP completion
     * path should not block, with the notable exception of Rename.
     *
     * The original reason that Rename completion is allowed to block was that
     * it was observed that IoCompleteRequest of a Rename could sometimes
     * trigger a recursive call into the FSD (likely due to a filter). WinFsp
     * was modified to accommodate this by allowing this recursive call to
     * proceed on a different thread.
     *
     * Since WinFsp can already accommodate blocking on Rename completions,
     * it is safe to acquire the Main resource of descendant file nodes.
     *
     * Note also that there can only be one rename at a time because of the
     * device's FileRenameResource.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FILE_NODE *DescendantFileNode;
    FSP_FILE_NODE *DescendantFileNodeArray[16], **DescendantFileNodes;
    ULONG DescendantFileNodeCount, DescendantFileNodeIndex;
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY RestartKey;
    BOOLEAN Deleted, Inserted, AcquireForeign;
    USHORT FileNameLength;
    PWSTR ExternalFileName;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    DescendantFileNodes = DescendantFileNodeArray;
    DescendantFileNodes[0] = FileNode;
    DescendantFileNodeCount = 1;
    memset(&RestartKey, 0, sizeof RestartKey);
    for (;;)
    {
        DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
            &FileNode->FileName, TRUE, &RestartKey);
        if (0 == DescendantFileNode)
            break;

        if (ARRAYSIZE(DescendantFileNodeArray) > DescendantFileNodeCount)
            DescendantFileNodes[DescendantFileNodeCount] = DescendantFileNode;
        DescendantFileNodeCount++;
    }

    if (ARRAYSIZE(DescendantFileNodeArray) < DescendantFileNodeCount)
    {
        DescendantFileNodes = FspAllocMustSucceed(DescendantFileNodeCount * sizeof(FSP_FILE_NODE *));
        DescendantFileNodes[0] = FileNode;
        DescendantFileNodeIndex = 1;
        memset(&RestartKey, 0, sizeof RestartKey);
        for (;;)
        {
            DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
                &FileNode->FileName, TRUE, &RestartKey);
            if (0 == DescendantFileNode)
                break;

            DescendantFileNodes[DescendantFileNodeIndex] = DescendantFileNode;
            DescendantFileNodeIndex++;
            ASSERT(DescendantFileNodeCount >= DescendantFileNodeIndex);
        }

        ASSERT(DescendantFileNodeCount == DescendantFileNodeIndex);
    }

    FileNameLength = FileNode->FileName.Length;
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
        ASSERT(DescendantFileNode->FileName.Length >= FileNameLength);

        AcquireForeign = DescendantFileNode->FileName.Length > FileNameLength &&
            L'\\' == DescendantFileNode->FileName.Buffer[FileNameLength / sizeof(WCHAR)];
        if (AcquireForeign)
            FspFileNodeAcquireExclusiveForeign(DescendantFileNode);

        FspFsvolDeviceDeleteContextByName(FsvolDeviceObject, &DescendantFileNode->FileName, &Deleted);
        ASSERT(Deleted);

        ExternalFileName = DescendantFileNode->ExternalFileName;

        DescendantFileNode->FileName.MaximumLength =
            DescendantFileNode->FileName.Length - FileNameLength + NewFileName->Length;
        DescendantFileNode->ExternalFileName = FspAllocMustSucceed(
            DescendantFileNode->FileName.MaximumLength);

        RtlCopyMemory((PUINT8)DescendantFileNode->ExternalFileName + NewFileName->Length,
            (PUINT8)DescendantFileNode->FileName.Buffer + FileNameLength,
            DescendantFileNode->FileName.Length - FileNameLength);
        RtlCopyMemory(DescendantFileNode->ExternalFileName,
            NewFileName->Buffer,
            NewFileName->Length);

        DescendantFileNode->FileName.Length = DescendantFileNode->FileName.MaximumLength;
        DescendantFileNode->FileName.Buffer = DescendantFileNode->ExternalFileName;

        if (0 != ExternalFileName)
            FspFree(ExternalFileName);

        FspFsvolDeviceInsertContextByName(FsvolDeviceObject, &DescendantFileNode->FileName, DescendantFileNode,
            &DescendantFileNode->ContextByNameElementStorage, &Inserted);
        ASSERT(Inserted);

        if (AcquireForeign)
            FspFileNodeReleaseForeign(DescendantFileNode);
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (DescendantFileNodeArray != DescendantFileNodes)
        FspFree(DescendantFileNodes);
}

VOID FspFileNodeGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    FileInfo->AllocationSize = FileNode->Header.AllocationSize.QuadPart;
    FileInfo->FileSize = FileNode->Header.FileSize.QuadPart;

    UINT32 FileAttributesMask = ~(UINT32)0;
    if (0 != FileNode->MainFileNode)
    {
        FileAttributesMask = ~(UINT32)FILE_ATTRIBUTE_DIRECTORY;
        FileNode = FileNode->MainFileNode;
    }

    FileInfo->FileAttributes = FileNode->FileAttributes & FileAttributesMask;
    FileInfo->ReparseTag = FileNode->ReparseTag;
    FileInfo->CreationTime = FileNode->CreationTime;
    FileInfo->LastAccessTime = FileNode->LastAccessTime;
    FileInfo->LastWriteTime = FileNode->LastWriteTime;
    FileInfo->ChangeTime = FileNode->ChangeTime;
}

BOOLEAN FspFileNodeTryGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    UINT64 CurrentTime = KeQueryInterruptTime();

    if (0 != FileNode->MainFileNode)
    {
        /* if this is a stream the main file basic info must have not expired as well! */
        if (!FspExpirationTimeValidEx(FileNode->MainFileNode->BasicInfoExpirationTime, CurrentTime))
            return FALSE;
    }

    if (!FspExpirationTimeValidEx(FileNode->FileInfoExpirationTime, CurrentTime))
        return FALSE;

    FspFileNodeGetFileInfo(FileNode, FileInfo);
    return TRUE;
}

VOID FspFileNodeSetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    UINT64 AllocationSize = FileInfo->AllocationSize > FileInfo->FileSize ?
        FileInfo->AllocationSize : FileInfo->FileSize;
    UINT64 AllocationUnit;

    AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
        FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;
    AllocationSize = (AllocationSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;

    FileNode->Header.AllocationSize.QuadPart = AllocationSize;
    FileNode->Header.FileSize.QuadPart = FileInfo->FileSize;

    FileNode->FileInfoExpirationTime = FileNode->BasicInfoExpirationTime =
        FspExpirationTimeFromMillis(FsvolDeviceExtension->VolumeParams.FileInfoTimeout);
    FileNode->FileInfoChangeNumber++;

    FSP_FILE_NODE *MainFileNode = FileNode;
    UINT32 FileAttributesMask = ~(UINT32)0;
    if (0 != FileNode->MainFileNode)
    {
        FileAttributesMask = ~(UINT32)FILE_ATTRIBUTE_DIRECTORY;
        MainFileNode = FileNode->MainFileNode;

        MainFileNode->BasicInfoExpirationTime = FileNode->BasicInfoExpirationTime;
        MainFileNode->FileInfoChangeNumber++;
    }

    MainFileNode->FileAttributes =
        (MainFileNode->FileAttributes & ~FileAttributesMask) |
        (FileInfo->FileAttributes & FileAttributesMask);
    MainFileNode->ReparseTag = FileInfo->ReparseTag;
    MainFileNode->CreationTime = FileInfo->CreationTime;
    MainFileNode->LastAccessTime = FileInfo->LastAccessTime;
    MainFileNode->LastWriteTime = FileInfo->LastWriteTime;
    MainFileNode->ChangeTime = FileInfo->ChangeTime;

    if (0 != CcFileObject)
    {
        NTSTATUS Result = FspCcSetFileSizes(
            CcFileObject, (PCC_FILE_SIZES)&FileNode->Header.AllocationSize);
        if (!NT_SUCCESS(Result))
        {
            /*
             * CcSetFileSizes failed. This is a hard case to handle, because it is
             * usually late in IRP processing. So we have the following strategy.
             *
             * Our goal is to completely stop all caching for this FileNode. The idea
             * is that if some I/O arrives later for this FileNode, CcInitializeCacheMap
             * will be executed (and possibly fail safely there). In fact we may decide
             * later to make such CcInitializeCacheMap failures benign (by not using the
             * cache when we cannot).
             *
             * In order to completely stop caching for the FileNode we do the following:
             *
             * -   We flush the cache using CcFlushCache.
             * -   We purge the cache and uninitialize all PrivateCacheMap's using
             *     CcPurgeCacheSection with UninitializeCacheMaps==TRUE.
             * -   If the SharedCacheMap is still around, we perform an additional
             *     CcUninitializeCacheMap with an UninitializeEvent. At this point
             *     CcUninitializeCacheMap should delete the SharedCacheMap and
             *     signal the UninitializeEvent.
             *
             * One potential gotcha is whether there is any possibility for another
             * system component to delay deletion of the SharedCacheMap and signaling
             * of the UninitializeEvent. This could result in a deadlock, because we
             * are already holding the FileNode exclusive and waiting for the
             * UninitializeEvent. But the thread that would signal our event would have
             * to first acquire our FileNode. Classic deadlock.
             *
             * I believe (but cannot prove) that this deadlock cannot happen. The reason
             * is that we have flushed and purged the cache and we have closed all
             * PrivateCacheMap's using this SharedCacheMap. There should be no reason for
             * any system component to keep the SharedCacheMap around (famous last words).
             */

            IO_STATUS_BLOCK IoStatus;
            CACHE_UNINITIALIZE_EVENT UninitializeEvent;

            FspCcFlushCache(CcFileObject->SectionObjectPointer, 0, 0, &IoStatus);
            CcPurgeCacheSection(CcFileObject->SectionObjectPointer, 0, 0, TRUE);
            if (0 != CcFileObject->SectionObjectPointer->SharedCacheMap)
            {
                UninitializeEvent.Next = 0;
                KeInitializeEvent(&UninitializeEvent.Event, NotificationEvent, FALSE);
                BOOLEAN CacheStopped = CcUninitializeCacheMap(CcFileObject, 0, &UninitializeEvent);
                (VOID)CacheStopped; ASSERT(CacheStopped);
                KeWaitForSingleObject(&UninitializeEvent.Event, Executive, KernelMode, FALSE, 0);
            }
        }
    }
}

BOOLEAN FspFileNodeTrySetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, ULONG InfoChangeNumber)
{
    PAGED_CODE();

    if (FspFileNodeFileInfoChangeNumber(FileNode) != InfoChangeNumber)
        return FALSE;

    FspFileNodeSetFileInfo(FileNode, CcFileObject, FileInfo);
    return TRUE;
}

BOOLEAN FspFileNodeReferenceSecurity(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);

    return FspMetaCacheReferenceItemBuffer(FsvolDeviceExtension->SecurityCache,
        FileNode->Security, PBuffer, PSize);
}

VOID FspFileNodeSetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size)
{
    PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->SecurityCache, FileNode->Security);
    FileNode->Security = 0 != Buffer ?
        FspMetaCacheAddItem(FsvolDeviceExtension->SecurityCache, Buffer, Size) : 0;
    FileNode->SecurityChangeNumber++;
}

BOOLEAN FspFileNodeTrySetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG SecurityChangeNumber)
{
    PAGED_CODE();

    if (FspFileNodeSecurityChangeNumber(FileNode) != SecurityChangeNumber)
        return FALSE;

    FspFileNodeSetSecurity(FileNode, Buffer, Size);
    return TRUE;
}

BOOLEAN FspFileNodeReferenceDirInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    UINT64 DirInfo;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    DirInfo = NonPaged->DirInfo;

    return FspMetaCacheReferenceItemBuffer(FsvolDeviceExtension->DirInfoCache,
        DirInfo, PBuffer, PSize);
}

VOID FspFileNodeSetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 DirInfo;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    DirInfo = NonPaged->DirInfo;

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->DirInfoCache, DirInfo);
    DirInfo = 0 != Buffer ?
        FspMetaCacheAddItem(FsvolDeviceExtension->DirInfoCache, Buffer, Size) : 0;
    FileNode->DirInfoChangeNumber++;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeInvalidateDirInfo */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    NonPaged->DirInfo = DirInfo;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);
}

BOOLEAN FspFileNodeTrySetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG DirInfoChangeNumber)
{
    // !PAGED_CODE();

    if (FspFileNodeDirInfoChangeNumber(FileNode) != DirInfoChangeNumber)
        return FALSE;

    FspFileNodeSetDirInfo(FileNode, Buffer, Size);
    return TRUE;
}

static VOID FspFileNodeInvalidateDirInfo(FSP_FILE_NODE *FileNode)
{
    // !PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 DirInfo;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeSetDirInfo */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    DirInfo = NonPaged->DirInfo;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->DirInfoCache, DirInfo);
}

BOOLEAN FspFileNodeReferenceStreamInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    UINT64 StreamInfo;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    StreamInfo = NonPaged->StreamInfo;

    return FspMetaCacheReferenceItemBuffer(FsvolDeviceExtension->StreamInfoCache,
        StreamInfo, PBuffer, PSize);
}

VOID FspFileNodeSetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 StreamInfo;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    StreamInfo = NonPaged->StreamInfo;

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->StreamInfoCache, StreamInfo);
    StreamInfo = 0 != Buffer ?
        FspMetaCacheAddItem(FsvolDeviceExtension->StreamInfoCache, Buffer, Size) : 0;
    FileNode->StreamInfoChangeNumber++;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeInvalidateStreamInfo */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    NonPaged->StreamInfo = StreamInfo;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);
}

BOOLEAN FspFileNodeTrySetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG StreamInfoChangeNumber)
{
    // !PAGED_CODE();

    if (FspFileNodeStreamInfoChangeNumber(FileNode) != StreamInfoChangeNumber)
        return FALSE;

    FspFileNodeSetStreamInfo(FileNode, Buffer, Size);
    return TRUE;
}

static VOID FspFileNodeInvalidateStreamInfo(FSP_FILE_NODE *FileNode)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 StreamInfo;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeSetStreamInfo */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    StreamInfo = NonPaged->StreamInfo;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->StreamInfoCache, StreamInfo);
}

VOID FspFileNodeNotifyChange(FSP_FILE_NODE *FileNode,
    ULONG Filter, ULONG Action)
{
    /* FileNode must be acquired (exclusive or shared) Main */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    UNICODE_STRING Parent, Suffix;
    FSP_FILE_NODE *ParentNode;

    FspFileNameSuffix(&FileNode->FileName, &Parent, &Suffix);

    switch (Action)
    {
    case FILE_ACTION_ADDED:
    case FILE_ACTION_REMOVED:
    //case FILE_ACTION_MODIFIED:
    case FILE_ACTION_RENAMED_OLD_NAME:
    case FILE_ACTION_RENAMED_NEW_NAME:
        FspFsvolDeviceInvalidateVolumeInfo(FsvolDeviceObject);

        FspFsvolDeviceLockContextTable(FsvolDeviceObject);
        ParentNode = FspFsvolDeviceLookupContextByName(FsvolDeviceObject, &Parent);
        if (0 != ParentNode)
            FspFileNodeReference(ParentNode);
        FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

        if (0 != ParentNode)
        {
            FspFileNodeInvalidateDirInfo(ParentNode);
            FspFileNodeDereference(ParentNode);
        }
        break;
    }

    if (0 != FileNode->MainFileNode)
    {
        if (FlagOn(Filter, FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME))
            SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_NAME);
        if (FlagOn(Filter, FILE_NOTIFY_CHANGE_SIZE))
            SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_SIZE);
        /* ???: what about FILE_NOTIFY_CHANGE_STREAM_WRITE */
        ClearFlag(Filter, ~(FILE_NOTIFY_CHANGE_STREAM_NAME | FILE_NOTIFY_CHANGE_STREAM_SIZE));

        switch (Action)
        {
        case FILE_ACTION_ADDED:
            Action = FILE_ACTION_ADDED_STREAM;
            FspFileNodeInvalidateStreamInfo(FileNode);
            break;
        case FILE_ACTION_REMOVED:
            Action = FILE_ACTION_REMOVED_STREAM;
            FspFileNodeInvalidateStreamInfo(FileNode);
            break;
        case FILE_ACTION_MODIFIED:
            Action = FILE_ACTION_MODIFIED_STREAM;
            //FspFileNodeInvalidateStreamInfo(FileNode);
            break;
        }
    }

    if (0 != Filter)
        FspNotifyReportChange(
            FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList,
            &FileNode->FileName,
            (USHORT)((PUINT8)Suffix.Buffer - (PUINT8)FileNode->FileName.Buffer),
            0, Filter, Action);
}

NTSTATUS FspFileNodeProcessLockIrp(FSP_FILE_NODE *FileNode, PIRP Irp)
{
    PAGED_CODE();

    IoMarkIrpPending(Irp);
    FspFileNodeSetOwnerF(FileNode, FspIrpFlags(Irp), Irp);

    try
    {
        FsRtlProcessFileLock(&FileNode->FileLock, Irp, FileNode);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Irp->IoStatus.Status = GetExceptionCode();
        Irp->IoStatus.Information = 0;

        FspFileNodeCompleteLockIrp(FileNode, Irp);
    }

    return STATUS_PENDING;
}

static NTSTATUS FspFileNodeCompleteLockIrp(PVOID Context, PIRP Irp)
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context;
    NTSTATUS Result = Irp->IoStatus.Status;

    FspFileNodeReleaseOwnerF(FileNode, FspIrpFlags(Irp), Irp);

    DEBUGLOGIRP(Irp, Result);

    FspIopCompleteIrp(Irp, Result);

    return Result;
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

    FspMainFileClose(FileDesc->MainFileHandle, FileDesc->MainFileObject);

    if (0 != FileDesc->DirectoryPattern.Buffer &&
        FspFileDescDirectoryPatternMatchAll != FileDesc->DirectoryPattern.Buffer)
    {
        if (FileDesc->CaseSensitive)
            FspFree(FileDesc->DirectoryPattern.Buffer);
        else
            RtlFreeUnicodeString(&FileDesc->DirectoryPattern);
    }

    FspFree(FileDesc);
}

NTSTATUS FspFileDescResetDirectoryPattern(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName, BOOLEAN Reset)
{
    PAGED_CODE();

    if (Reset || 0 == FileDesc->DirectoryPattern.Buffer)
    {
        UNICODE_STRING DirectoryPattern;

        if (0 == FileName || (sizeof(WCHAR) == FileName->Length && L'*' == FileName->Buffer[0]))
        {
            DirectoryPattern.Length = DirectoryPattern.MaximumLength = sizeof(WCHAR); /* L"*" */
            DirectoryPattern.Buffer = FspFileDescDirectoryPatternMatchAll;
        }
        else
        {
            if (FileDesc->CaseSensitive)
            {
                DirectoryPattern.Length = DirectoryPattern.MaximumLength = FileName->Length;
                DirectoryPattern.Buffer = FspAlloc(FileName->Length);
                if (0 == DirectoryPattern.Buffer)
                    return STATUS_INSUFFICIENT_RESOURCES;
                RtlCopyMemory(DirectoryPattern.Buffer, FileName->Buffer, FileName->Length);
            }
            else
            {
                NTSTATUS Result = RtlUpcaseUnicodeString(&DirectoryPattern, FileName, TRUE);
                if (!NT_SUCCESS(Result))
                    return Result;
            }
        }

        if (0 != FileDesc->DirectoryPattern.Buffer &&
            FspFileDescDirectoryPatternMatchAll != FileDesc->DirectoryPattern.Buffer)
        {
            if (FileDesc->CaseSensitive)
                FspFree(FileDesc->DirectoryPattern.Buffer);
            else
                RtlFreeUnicodeString(&FileDesc->DirectoryPattern);
        }

        FileDesc->DirectoryPattern = DirectoryPattern;
    }

    return STATUS_SUCCESS;
}

NTSTATUS FspMainFileOpen(
    PDEVICE_OBJECT FsvolDeviceObject,
    PDEVICE_OBJECT DeviceObjectHint,
    PUNICODE_STRING MainFileName, BOOLEAN CaseSensitive,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    ULONG FileAttributes,
    ULONG Disposition,
    PHANDLE PMainFileHandle,
    PFILE_OBJECT *PMainFileObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    UNICODE_STRING FullFileName = { 0 };
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatus;
    IO_DRIVER_CREATE_CONTEXT DriverCreateContext = { 0 };
    PVOID ExtraCreateParameter;
    HANDLE MainFileHandle;
    PFILE_OBJECT MainFileObject;

    /* assert that the supplied name is actually a main file name */
    ASSERT(FspFileNameIsValid(MainFileName, 0, 0));

    *PMainFileHandle = 0;
    *PMainFileObject = 0;

    switch (Disposition)
    {
    case FILE_CREATE:
    case FILE_OPEN_IF:
    case FILE_OVERWRITE_IF:
        Disposition = FILE_OPEN_IF;
        break;
    case FILE_OPEN:
    case FILE_OVERWRITE:
    case FILE_SUPERSEDE:
        Disposition = FILE_OPEN;
        break;
    default:
        IoStatus.Status = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    FullFileName.Length = 0;
    FullFileName.MaximumLength =
        FsvolDeviceExtension->VolumeName.Length +
        FsvolDeviceExtension->VolumePrefix.Length +
        MainFileName->Length;
    FullFileName.Buffer = FspAlloc(FullFileName.MaximumLength);
    if (0 == FullFileName.Buffer)
    {
        IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    RtlAppendUnicodeStringToString(&FullFileName, &FsvolDeviceExtension->VolumeName);
    RtlAppendUnicodeStringToString(&FullFileName, &FsvolDeviceExtension->VolumePrefix);
    RtlAppendUnicodeStringToString(&FullFileName, MainFileName);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &FullFileName,
        OBJ_KERNEL_HANDLE | OBJ_FORCE_ACCESS_CHECK | (CaseSensitive ? 0 : OBJ_CASE_INSENSITIVE),
        0/*RootDirectory*/,
        SecurityDescriptor);

    /* do not use SiloContext field as it is only available on Win10 v1607 and higher */
    DriverCreateContext.Size =
        FIELD_OFFSET(IO_DRIVER_CREATE_CONTEXT, TxnParameters) +
        sizeof(((PIO_DRIVER_CREATE_CONTEXT)0)->TxnParameters);
    DriverCreateContext.DeviceObjectHint = 0 != FsvolDeviceExtension->FsvrtDeviceObject ?
        FsvolDeviceObject : DeviceObjectHint;

    IoStatus.Status = FsRtlAllocateExtraCreateParameterList(0,
        &DriverCreateContext.ExtraCreateParameter);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    IoStatus.Status = FsRtlAllocateExtraCreateParameter(&FspMainFileOpenEcpGuid,
        sizeof(PVOID),
        0/*Flags*/,
        0/*CleanupCallback*/,
        FSP_ALLOC_INTERNAL_TAG,
        &ExtraCreateParameter);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    IoStatus.Status = FsRtlInsertExtraCreateParameter(DriverCreateContext.ExtraCreateParameter,
        ExtraCreateParameter);
    if (!NT_SUCCESS(IoStatus.Status))
    {
        FsRtlFreeExtraCreateParameter(ExtraCreateParameter);
        goto exit;
    }

    IoStatus.Status = IoCreateFileEx(
        &MainFileHandle,
        FILE_READ_ATTRIBUTES,
        &ObjectAttributes,
        &IoStatus,
        0/*AllocationSize*/,
        FileAttributes,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        Disposition,
        FILE_OPEN_REPARSE_POINT,
        0/*EaBuffer*/,
        0/*EaLength*/,
        CreateFileTypeNone,
        0/*InternalParameters*/,
        IO_FORCE_ACCESS_CHECK,
        &DriverCreateContext);
    if (!NT_SUCCESS(IoStatus.Status))
        goto exit;

    IoStatus.Status = ObReferenceObjectByHandle(
        MainFileHandle,
        0/*DesiredAccess*/,
        *IoFileObjectType,
        KernelMode,
        &MainFileObject,
        0/*HandleInformation*/);
    if (!NT_SUCCESS(IoStatus.Status))
    {
        ObCloseHandle(MainFileHandle, KernelMode);
        goto exit;
    }

    *PMainFileHandle = MainFileHandle;
    *PMainFileObject = MainFileObject;

    IoStatus.Status = STATUS_SUCCESS;

exit:
    if (0 != DriverCreateContext.ExtraCreateParameter)
        FsRtlFreeExtraCreateParameterList(DriverCreateContext.ExtraCreateParameter);

    if (0 != FullFileName.Buffer)
        FspFree(FullFileName.Buffer);

    return IoStatus.Status;
}

NTSTATUS FspMainFileClose(
    HANDLE MainFileHandle,
    PFILE_OBJECT MainFileObject)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;

    if (0 != MainFileObject)
        ObDereferenceObject(MainFileObject);

    if (0 != MainFileHandle)
    {
        Result = ObCloseHandle(MainFileHandle, KernelMode);
        if (!NT_SUCCESS(Result))
            DEBUGLOG("ObCloseHandle() = %s", NtStatusSym(Result));
    }

    return Result;
}

VOID FspFileNodeOplockPrepare(PVOID Context, PIRP Irp)
{
    PAGED_CODE();

    FSP_IOP_REQUEST_WORK *WorkRoutine = (FSP_IOP_REQUEST_WORK *)(UINT_PTR)
        FspFileNodeReleaseForOplock(Context);
    NTSTATUS Result;

    FSP_FSCTL_STATIC_ASSERT(sizeof(PVOID) == sizeof(VOID (*)(VOID)),
        "Data and code pointers must have same size!");

    Result = FspWqCreateIrpWorkItem(Irp, WorkRoutine, 0);
    if (!NT_SUCCESS(Result))
        /*
         * Only way to communicate failure is through ExRaiseStatus.
         * We will catch it in FspCheckOplock, etc.
         */
        ExRaiseStatus(Result);
}

VOID FspFileNodeOplockComplete(PVOID Context, PIRP Irp)
{
    PAGED_CODE();

    if (STATUS_SUCCESS == Irp->IoStatus.Status)
        FspWqPostIrpWorkItem(Irp);
    else
        FspIopCompleteIrp(Irp, Irp->IoStatus.Status);
}

WCHAR FspFileDescDirectoryPatternMatchAll[] = L"*";

// {904862B4-EB3F-461E-ACB2-4DF2B3FC898B}
const GUID FspMainFileOpenEcpGuid =
    { 0x904862b4, 0xeb3f, 0x461e, { 0xac, 0xb2, 0x4d, 0xf2, 0xb3, 0xfc, 0x89, 0x8b } };
