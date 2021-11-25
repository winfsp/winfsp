/**
 * @file sys/file.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <sys/driver.h>

NTSTATUS FspFileNodeCopyActiveList(PDEVICE_OBJECT DeviceObject,
    FSP_FILE_NODE ***PFileNodes, PULONG PFileNodeCount);
NTSTATUS FspFileNodeCopyOpenList(PDEVICE_OBJECT DeviceObject,
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
    UINT32 GrantedAccess, UINT32 AdditionalGrantedAccess, UINT32 ShareAccess,
    FSP_FILE_NODE **POpenedFileNode, PULONG PSharingViolationReason);
VOID FspFileNodeCleanup(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject, PULONG PCleanupFlags);
VOID FspFileNodeCleanupFlush(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject);
VOID FspFileNodeCleanupComplete(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject, BOOLEAN Delete);
VOID FspFileNodeClose(FSP_FILE_NODE *FileNode,
    PFILE_OBJECT FileObject,    /* non-0 to remove share access */
    BOOLEAN HandleCleanup);     /* TRUE to decrement handle count */
NTSTATUS FspFileNodeFlushAndPurgeCache(FSP_FILE_NODE *FileNode,
    UINT64 FlushOffset64, ULONG FlushLength, BOOLEAN FlushAndPurge);
VOID FspFileNodeOverwriteStreams(FSP_FILE_NODE *FileNode);
NTSTATUS FspFileNodeCheckBatchOplocksOnAllStreams(
    PDEVICE_OBJECT FsvolDeviceObject,
    PIRP OplockIrp,
    FSP_FILE_NODE *FileNode,
    ULONG AcquireFlags,
    PUNICODE_STRING StreamFileName);
NTSTATUS FspFileNodeRenameCheck(PDEVICE_OBJECT FsvolDeviceObject, PIRP OplockIrp,
    FSP_FILE_NODE *FileNode, ULONG AcquireFlags,
    PUNICODE_STRING FileName, BOOLEAN CheckingOldName,
    BOOLEAN PosixRename);
VOID FspFileNodeRename(FSP_FILE_NODE *FileNode, PUNICODE_STRING NewFileName);
VOID FspFileNodeGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTryGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTryGetFileInfoByName(PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp,
    PUNICODE_STRING FileName, FSP_FSCTL_FILE_INFO *FileInfo);
VOID FspFileNodeSetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, BOOLEAN TruncateOnClose);
BOOLEAN FspFileNodeTrySetFileInfoAndSecurityOnOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo,
    const PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG SecurityDescriptorSize,
    BOOLEAN TruncateOnClose);
BOOLEAN FspFileNodeTrySetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, ULONG InfoChangeNumber);
VOID FspFileNodeInvalidateFileInfo(FSP_FILE_NODE *FileNode);
BOOLEAN FspFileNodeReferenceSecurity(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG SecurityChangeNumber);
VOID FspFileNodeInvalidateSecurity(FSP_FILE_NODE *FileNode);
BOOLEAN FspFileNodeReferenceDirInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG DirInfoChangeNumber);
static VOID FspFileNodeInvalidateDirInfo(FSP_FILE_NODE *FileNode);
static VOID FspFileNodeInvalidateDirInfoByName(PDEVICE_OBJECT FsvolDeviceObject,
    PUNICODE_STRING FileName);
VOID FspFileNodeInvalidateParentDirInfo(FSP_FILE_NODE *FileNode);
BOOLEAN FspFileNodeReferenceStreamInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG StreamInfoChangeNumber);
VOID FspFileNodeInvalidateStreamInfo(FSP_FILE_NODE *FileNode);
BOOLEAN FspFileNodeReferenceEa(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetEa(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetEa(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG EaChangeNumber);
VOID FspFileNodeInvalidateEa(FSP_FILE_NODE *FileNode);
VOID FspFileNodeNotifyChange(FSP_FILE_NODE *FileNode, ULONG Filter, ULONG Action,
    BOOLEAN InvalidateCaches);
VOID FspFileNodeInvalidateCachesAndNotifyChangeByName(PDEVICE_OBJECT FsvolDeviceObject,
    PUNICODE_STRING FileName, ULONG Filter, ULONG Action,
    BOOLEAN InvalidateParentCaches);
NTSTATUS FspFileNodeProcessLockIrp(FSP_FILE_NODE *FileNode, PIRP Irp);
static NTSTATUS FspFileNodeCompleteLockIrp(PVOID Context, PIRP Irp);
NTSTATUS FspFileDescCreate(FSP_FILE_DESC **PFileDesc);
VOID FspFileDescDelete(FSP_FILE_DESC *FileDesc);
NTSTATUS FspFileDescResetDirectory(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName, BOOLEAN RestartScan, BOOLEAN IndexSpecified);
NTSTATUS FspFileDescSetDirectoryMarker(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName);
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
VOID FspFileNodeOplockPrepare(PVOID Context, PIRP Irp);
VOID FspFileNodeOplockComplete(PVOID Context, PIRP Irp);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFileNodeCopyActiveList)
#pragma alloc_text(PAGE, FspFileNodeCopyOpenList)
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
#pragma alloc_text(PAGE, FspFileNodeCleanupFlush)
#pragma alloc_text(PAGE, FspFileNodeCleanupComplete)
#pragma alloc_text(PAGE, FspFileNodeClose)
#pragma alloc_text(PAGE, FspFileNodeFlushAndPurgeCache)
#pragma alloc_text(PAGE, FspFileNodeOverwriteStreams)
#pragma alloc_text(PAGE, FspFileNodeCheckBatchOplocksOnAllStreams)
#pragma alloc_text(PAGE, FspFileNodeRenameCheck)
#pragma alloc_text(PAGE, FspFileNodeRename)
#pragma alloc_text(PAGE, FspFileNodeGetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeTryGetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeTryGetFileInfoByName)
#pragma alloc_text(PAGE, FspFileNodeSetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeTrySetFileInfoAndSecurityOnOpen)
#pragma alloc_text(PAGE, FspFileNodeTrySetFileInfo)
#pragma alloc_text(PAGE, FspFileNodeInvalidateFileInfo)
// !#pragma alloc_text(PAGE, FspFileNodeReferenceSecurity)
// !#pragma alloc_text(PAGE, FspFileNodeSetSecurity)
// !#pragma alloc_text(PAGE, FspFileNodeTrySetSecurity)
// !#pragma alloc_text(PAGE, FspFileNodeInvalidateSecurity)
// !#pragma alloc_text(PAGE, FspFileNodeReferenceDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeSetDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeTrySetDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeInvalidateDirInfo)
#pragma alloc_text(PAGE, FspFileNodeInvalidateDirInfoByName)
#pragma alloc_text(PAGE, FspFileNodeInvalidateParentDirInfo)
// !#pragma alloc_text(PAGE, FspFileNodeReferenceStreamInfo)
// !#pragma alloc_text(PAGE, FspFileNodeSetStreamInfo)
// !#pragma alloc_text(PAGE, FspFileNodeTrySetStreamInfo)
// !#pragma alloc_text(PAGE, FspFileNodeInvalidateStreamInfo)
// !#pragma alloc_text(PAGE, FspFileNodeReferenceEa)
// !#pragma alloc_text(PAGE, FspFileNodeSetEa)
// !#pragma alloc_text(PAGE, FspFileNodeTrySetEa)
// !#pragma alloc_text(PAGE, FspFileNodeInvalidateEa)
#pragma alloc_text(PAGE, FspFileNodeNotifyChange)
#pragma alloc_text(PAGE, FspFileNodeInvalidateCachesAndNotifyChangeByName)
#pragma alloc_text(PAGE, FspFileNodeProcessLockIrp)
#pragma alloc_text(PAGE, FspFileNodeCompleteLockIrp)
#pragma alloc_text(PAGE, FspFileDescCreate)
#pragma alloc_text(PAGE, FspFileDescDelete)
#pragma alloc_text(PAGE, FspFileDescResetDirectory)
#pragma alloc_text(PAGE, FspFileDescSetDirectoryMarker)
#pragma alloc_text(PAGE, FspMainFileOpen)
#pragma alloc_text(PAGE, FspMainFileClose)
#pragma alloc_text(PAGE, FspFileNodeOplockPrepare)
#pragma alloc_text(PAGE, FspFileNodeOplockComplete)
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

#define GATHER_DESCENDANTS(FILENAME, REFERENCE, ...)\
    FSP_FILE_NODE *DescendantFileNode;\
    FSP_FILE_NODE *DescendantFileNodeArray[16], **DescendantFileNodes;\
    ULONG DescendantFileNodeCount, DescendantFileNodeIndex;\
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY RestartKey;\
    DescendantFileNodes = DescendantFileNodeArray;\
    DescendantFileNodeCount = 0;\
    memset(&RestartKey, 0, sizeof RestartKey);\
    for (;;)                            \
    {                                   \
        DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,\
            FILENAME, FALSE, &RestartKey);\
        if (0 == DescendantFileNode)    \
            break;                      \
        ASSERT(0 == ((UINT_PTR)DescendantFileNode & 7));\
        __VA_ARGS__;                    \
        if (REFERENCE)                  \
            FspFileNodeReference((PVOID)((UINT_PTR)DescendantFileNode & ~7));\
        if (ARRAYSIZE(DescendantFileNodeArray) > DescendantFileNodeCount)\
            DescendantFileNodes[DescendantFileNodeCount] = DescendantFileNode;\
        DescendantFileNodeCount++;      \
    }                                   \
    if (ARRAYSIZE(DescendantFileNodeArray) < DescendantFileNodeCount ||\
        DEBUGTEST_EX(0 != DescendantFileNodeCount, 10, FALSE))  \
    {                                   \
        DescendantFileNodes = FspAllocMustSucceed(DescendantFileNodeCount * sizeof(FSP_FILE_NODE *));\
        DescendantFileNodeIndex = 0;    \
        memset(&RestartKey, 0, sizeof RestartKey);\
        for (;;)                        \
        {                               \
            DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,\
                FILENAME, FALSE, &RestartKey);\
            if (0 == DescendantFileNode)\
                break;                  \
            ASSERT(0 == ((UINT_PTR)DescendantFileNode & 7));\
            __VA_ARGS__;                \
            DescendantFileNodes[DescendantFileNodeIndex] = DescendantFileNode;\
            DescendantFileNodeIndex++;  \
            ASSERT(DescendantFileNodeCount >= DescendantFileNodeIndex);\
        }                               \
        ASSERT(DescendantFileNodeCount == DescendantFileNodeIndex);\
    }                                   \
    ((VOID)0)
#define SCATTER_DESCENDANTS(REFERENCE)  \
    if (REFERENCE)                      \
    {                                   \
        for (                           \
            DescendantFileNodeIndex = 0;\
            DescendantFileNodeCount > DescendantFileNodeIndex;\
            DescendantFileNodeIndex++)  \
        {                               \
            DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];\
            FspFileNodeDereference((PVOID)((UINT_PTR)DescendantFileNode & ~7));\
        }                               \
    }                                   \
    if (DescendantFileNodeArray != DescendantFileNodes)\
        FspFree(DescendantFileNodes);   \
    ((VOID)0)

NTSTATUS FspFileNodeCopyActiveList(PDEVICE_OBJECT DeviceObject,
    FSP_FILE_NODE ***PFileNodes, PULONG PFileNodeCount)
{
    PAGED_CODE();

    NTSTATUS Result;
    ULONG Index;

    FspFsvolDeviceLockContextTable(DeviceObject);
    Result = FspFsvolDeviceCopyContextList(DeviceObject, PFileNodes, PFileNodeCount);
    if (NT_SUCCESS(Result))
    {
        for (Index = 0; *PFileNodeCount > Index; Index++)
            FspFileNodeReference((*PFileNodes)[Index]);
    }
    FspFsvolDeviceUnlockContextTable(DeviceObject);

    return Result;
}

NTSTATUS FspFileNodeCopyOpenList(PDEVICE_OBJECT DeviceObject,
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

    FspFsvolDeviceDeleteContextList(FileNodes, FileNodeCount);
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

    if (0 != FileNode->MainFileNode)
        FspFileNodeDereference(FileNode->MainFileNode);

    FsRtlUninitializeOplock(FspFileNodeAddrOfOplock(FileNode));
    FsRtlUninitializeFileLock(&FileNode->FileLock);

    FsRtlTeardownPerStreamContexts(&FileNode->Header);

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->EaCache, FileNode->NonPaged->Ea);
    FspMetaCacheInvalidateItem(FsvolDeviceExtension->StreamInfoCache, FileNode->NonPaged->StreamInfo);
    FspMetaCacheInvalidateItem(FsvolDeviceExtension->DirInfoCache, FileNode->NonPaged->DirInfo);
    FspMetaCacheInvalidateItem(FsvolDeviceExtension->SecurityCache, FileNode->NonPaged->Security);

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
    UINT32 GrantedAccess, UINT32 AdditionalGrantedAccess, UINT32 ShareAccess,
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
        if (0 != AdditionalGrantedAccess)
        {
            /* Additional share check for FILE_OVERWRITE*, FILE_SUPERSEDE. Fixes GitHub issue #364. */
            Result = IoCheckShareAccess(GrantedAccess | AdditionalGrantedAccess, ShareAccess, FileObject,
                &OpenedFileNode->ShareAccess, FALSE);
            if (!NT_SUCCESS(Result))
                goto exit;
        }
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

        ASSERT(0 <= OpenedFileNode->ActiveCount);
        ASSERT(0 <= OpenedFileNode->OpenCount);
        ASSERT(0 <= OpenedFileNode->HandleCount);

        if (0 == OpenedFileNode->ActiveCount++)
            InsertTailList(&FspFsvolDeviceExtension(FsvolDeviceObject)->ContextList,
                &FileNode->ActiveEntry);
        OpenedFileNode->OpenCount++;
        OpenedFileNode->HandleCount++;
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    *POpenedFileNode = OpenedFileNode;

    return Result;
}

VOID FspFileNodeCleanup(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject, PULONG PCleanupFlags)
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
    BOOLEAN DeletePending, Delete, SetAllocationSize, SingleHandle;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    if (FileDesc->DeleteOnClose)
        FileNode->DeletePending = TRUE;
    DeletePending = 0 != FileNode->DeletePending;
    MemoryBarrier();

    SetAllocationSize = !DeletePending && FileNode->TruncateOnClose;

    SingleHandle = 1 == FileNode->HandleCount;

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    Delete = FALSE;
    if (!FileNode->PosixDelete)
    {
        if (FileDesc->PosixDelete)
        {
            FileNode->PosixDelete = TRUE;
            Delete = TRUE;
        }
        else if (SingleHandle)
            Delete = DeletePending;
    }

    *PCleanupFlags = SingleHandle ? Delete | (SetAllocationSize << 1) : Delete;
}

VOID FspFileNodeCleanupFlush(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject)
{
    /*
     * Optionally flush the FileNode during Cleanup.
     *
     * The FileNode must be acquired exclusive (Full) when calling this function.
     */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    if (!FsvolDeviceExtension->VolumeParams.FlushAndPurgeOnCleanup)
        return; /* nothing to do! */

    BOOLEAN DeletePending, SingleHandle;
    LARGE_INTEGER TruncateSize, *PTruncateSize = 0;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    DeletePending = 0 != FileNode->DeletePending;
    MemoryBarrier();

    SingleHandle = 1 == FileNode->HandleCount;

    if (SingleHandle && FileNode->TruncateOnClose)
    {
        /*
         * Even when the FileInfo is expired, this is the best guess for a file size
         * without asking the user-mode file system.
         */
        TruncateSize = FileNode->Header.FileSize;
        PTruncateSize = &TruncateSize;
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    /* Flush and purge on last Cleanup. Keeps files off the "standby" list. (GitHub issue #104) */
    if (SingleHandle && !DeletePending)
    {
        IO_STATUS_BLOCK IoStatus;
        LARGE_INTEGER ZeroOffset = { 0 };

        if (0 != PTruncateSize && 0 == PTruncateSize->HighPart)
            FspCcFlushCache(FileObject->SectionObjectPointer, &ZeroOffset, PTruncateSize->LowPart,
                &IoStatus);
        else
            FspCcFlushCache(FileObject->SectionObjectPointer, 0, 0,
                &IoStatus);
    }
}

VOID FspFileNodeCleanupComplete(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject, BOOLEAN Delete)
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
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    LARGE_INTEGER TruncateSize, *PTruncateSize = 0;
    BOOLEAN DeletePending, DeletedFromContextTable = FALSE, SingleHandle = FALSE;

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

    if (Delete)
    {
        FspFsvolDeviceDeleteContextByName(FsvolDeviceObject, &FileNode->FileName,
            &DeletedFromContextTable);
        ASSERT(DeletedFromContextTable);

        FileNode->OpenCount = 0;

        /*
         * We now have to deal with the scenario where there are cleaned up,
         * but unclosed streams for this file still in the context table.
         */
        if (FsvolDeviceExtension->VolumeParams.NamedStreams &&
            0 == FileNode->MainFileNode)
        {
            BOOLEAN StreamDeletedFromContextTable;
            USHORT FileNameLength = FileNode->FileName.Length;

            GATHER_DESCENDANTS(&FileNode->FileName, FALSE,
                if (DescendantFileNode->FileName.Length > FileNameLength &&
                    L'\\' == DescendantFileNode->FileName.Buffer[FileNameLength / sizeof(WCHAR)])
                    break;
                ASSERT(FileNode != DescendantFileNode);
                ASSERT(0 != DescendantFileNode->OpenCount);
                );

            for (
                DescendantFileNodeIndex = 0;
                DescendantFileNodeCount > DescendantFileNodeIndex;
                DescendantFileNodeIndex++)
            {
                DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];

                FspFsvolDeviceDeleteContextByName(FsvolDeviceObject, &DescendantFileNode->FileName,
                    &StreamDeletedFromContextTable);
                if (StreamDeletedFromContextTable)
                {
                    DescendantFileNode->OpenCount = 0;
                    FspFileNodeDereference(DescendantFileNode);
                }
            }

            SCATTER_DESCENDANTS(FALSE);
        }
    }

    ASSERT(0 < FileNode->HandleCount);
    if (0 == --FileNode->HandleCount)
    {
        SingleHandle = TRUE;

        DeletePending = 0 != FileNode->DeletePending;
        MemoryBarrier();

        if (DeletePending)
            FileNode->Header.FileSize.QuadPart = 0;

        if (DeletePending || FileNode->TruncateOnClose)
        {
            UINT64 AllocationUnit =
                FsvolDeviceExtension->VolumeParams.SectorSize *
                FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;

            /*
             * Even when the FileInfo is expired, this is the best guess for a file size
             * without asking the user-mode file system.
             */
            TruncateSize = FileNode->Header.FileSize;
            PTruncateSize = &TruncateSize;

            FileNode->Header.AllocationSize.QuadPart = (TruncateSize.QuadPart + AllocationUnit - 1)
                / AllocationUnit * AllocationUnit;
        }

        FileNode->TruncateOnClose = FALSE;
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    /* Flush and purge on last Cleanup. Keeps files off the "standby" list. (GitHub issue #104) */
    if (SingleHandle && FsvolDeviceExtension->VolumeParams.FlushAndPurgeOnCleanup)
    {
        /*
         * There is an important difference in behavior with respect to DeletePending when
         * FlushAndPurgeOnCleanup is FALSE vs when it is TRUE.
         *
         * With FlushAndPurgeOnCleanup==FALSE (the default), the WinFsp FSD preserves data
         * and allows a deleted file to have memory-mapped I/O done on it after the CLEANUP
         * completes. It is up to the user mode file system to decide whether to handle
         * this scenario or not. The MEMFS reference file system does.
         *
         * With FlushAndPurgeOnCleanup==TRUE, the FSD simply purges the cache section (if any),
         * which means that CACHED DATA WILL BE LOST. This is desirable, because we do not want
         * to unnecessarily flush data that are soon going to be deleted.
         *
         * This could affect a program that does memory-mapped I/O on a deleted file that has
         * been CloseHandle'd. Tests have shown that even NTFS cannot properly handle this
         * scenario in all cases (for example, when the file is not cached), so it is unlikely
         * that there are any useful programs out there that do this.
         *
         * So we deem this difference in behavior ok and desirable.
         */

        TruncateSize.QuadPart = 0;
        PTruncateSize = &TruncateSize;
    }

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

    ASSERT(0 < FileNode->ActiveCount);
    if (0 == --FileNode->ActiveCount)
    {
        ASSERT(0 == FileNode->OpenCount);
        ASSERT(0 == FileNode->HandleCount);

        RemoveEntryList(&FileNode->ActiveEntry);
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

VOID FspFileNodeOverwriteStreams(FSP_FILE_NODE *FileNode)
{
    /*
     * Called during Create processing. The device rename resource has been acquired shared.
     * No concurrent renames are allowed.
     */

    PAGED_CODE();

    ASSERT(0 == FileNode->MainFileNode);

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    USHORT FileNameLength = FileNode->FileName.Length;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    GATHER_DESCENDANTS(&FileNode->FileName, FALSE,
        if (DescendantFileNode->FileName.Length > FileNameLength &&
            L'\\' == DescendantFileNode->FileName.Buffer[FileNameLength / sizeof(WCHAR)])
            break;
        if (FileNode == DescendantFileNode || 0 >= DescendantFileNode->HandleCount)
            continue;
        );

    /* mark any open named streams as DeletePending */
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
        DescendantFileNode->DeletePending = TRUE;
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    SCATTER_DESCENDANTS(FALSE);
}

NTSTATUS FspFileNodeCheckBatchOplocksOnAllStreams(
    PDEVICE_OBJECT FsvolDeviceObject,
    PIRP OplockIrp,
    FSP_FILE_NODE *FileNode,
    ULONG AcquireFlags,
    PUNICODE_STRING StreamFileName)
{
    /*
     * Called during Create processing. The device rename resource has been acquired shared.
     * No concurrent renames are allowed.
     */

    PAGED_CODE();

    ASSERT(0 == FileNode->MainFileNode);

    USHORT FileNameLength = FileNode->FileName.Length;
    BOOLEAN CaseInsensitive = !FspFsvolDeviceExtension(FsvolDeviceObject)->
        VolumeParams.CaseSensitiveSearch;
    ULONG IsBatchOplock, IsHandleOplock;
    NTSTATUS Result;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    GATHER_DESCENDANTS(&FileNode->FileName, TRUE,
        if (DescendantFileNode->FileName.Length > FileNameLength &&
            L'\\' == DescendantFileNode->FileName.Buffer[FileNameLength / sizeof(WCHAR)])
            break;
        if (0 >= DescendantFileNode->HandleCount)
            continue;
        if (0 != StreamFileName)
        {
            if (DescendantFileNode != FileNode &&
                0 != FspFileNameCompare(&DescendantFileNode->FileName, StreamFileName,
                    CaseInsensitive, 0))
                continue;
        });

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

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
            NTSTATUS Result0 = FspFileNodeOplockCheckEx(DescendantFileNode, OplockIrp,
                OPLOCK_FLAG_COMPLETE_IF_OPLOCKED);
            if (STATUS_SUCCESS == Result0)
                OplockIrp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;
            else
            if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result0)
            {
                OplockIrp->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;
                DescendantFileNodes[DescendantFileNodeIndex] =
                    (PVOID)((UINT_PTR)DescendantFileNode | 2);
            }
            else
                Result = STATUS_SHARING_VIOLATION;
        }
        else
        if (FspFileNodeOplockIsHandle(DescendantFileNode))
        {
            NTSTATUS Result0 = FspFileNodeOplockBreakHandle(DescendantFileNode, OplockIrp,
                OPLOCK_FLAG_COMPLETE_IF_OPLOCKED);
            if (STATUS_SUCCESS == Result0)
                ;
            else
            if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result0)
                DescendantFileNodes[DescendantFileNodeIndex] =
                    (PVOID)((UINT_PTR)DescendantFileNode | 4);
            else
                Result = STATUS_SHARING_VIOLATION;
        }
    }

    /* release the FileNode so that we can safely wait without deadlocks */
    FspFileNodeReleaseF(FileNode, AcquireFlags);

    /* wait for oplock breaks to finish */
    Result = STATUS_SUCCESS;
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
        IsBatchOplock = (UINT_PTR)DescendantFileNode & 2;
        IsHandleOplock = (UINT_PTR)DescendantFileNode & 4;
        DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~7);

        if (IsBatchOplock)
        {
            NTSTATUS Result0 = FspFileNodeOplockCheck(DescendantFileNode, OplockIrp);
            ASSERT(STATUS_OPLOCK_BREAK_IN_PROGRESS != Result0);
            if (STATUS_SUCCESS != Result0)
                Result = STATUS_SHARING_VIOLATION;
        }
        else
        if (IsHandleOplock)
        {
            NTSTATUS Result0 = FspFileNodeOplockBreakHandle(DescendantFileNode, OplockIrp, 0);
            ASSERT(STATUS_OPLOCK_BREAK_IN_PROGRESS != Result0);
            if (STATUS_SUCCESS != Result0)
                Result = STATUS_SHARING_VIOLATION;
        }
    }

    SCATTER_DESCENDANTS(TRUE);

    return Result;
}

NTSTATUS FspFileNodeRenameCheck(PDEVICE_OBJECT FsvolDeviceObject, PIRP OplockIrp,
    FSP_FILE_NODE *FileNode, ULONG AcquireFlags,
    PUNICODE_STRING FileName, BOOLEAN CheckingOldName,
    BOOLEAN PosixRename)
{
    PAGED_CODE();

    NTSTATUS Result;
    ULONG HasHandles, IsBatchOplock, IsHandleOplock;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    if (CheckingOldName && !FileNode->IsDirectory && 1 == FileNode->HandleCount)
    {
        /* quick exit */
        FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);
        return STATUS_SUCCESS;
    }

    GATHER_DESCENDANTS(FileName, TRUE,
        DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode |
            (0 < DescendantFileNode->HandleCount)));

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (0 == DescendantFileNodeCount)
    {
        Result = STATUS_SUCCESS;
        goto exit;
    }

    /*
     * At this point all descendant FileNode's are enumerated and referenced.
     * There can be no new FileNode's because Rename has acquired the device's
     * "rename" resource exclusively, which disallows new Opens.
     */

    if (!CheckingOldName)
    {
        /* replaced file cannot be a directory or mapped as an image */
        for (
            DescendantFileNodeIndex = 0;
            DescendantFileNodeCount > DescendantFileNodeIndex;
            DescendantFileNodeIndex++)
        {
            DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
            DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~7);

            /*
             * Windows file systems do not allow the replaced file to be a directory.
             * However POSIX allows this (when the directory is empty).
             *
             * For this reason we will allow the case where the replaced file is a directory
             * (without any open files within it). The user mode file system can always fail
             * such requests if it wants.
             */

            if ((DescendantFileNode->FileName.Length > FileName->Length &&
                L'\\' == DescendantFileNode->FileName.Buffer[FileName->Length / sizeof(WCHAR)]) ||
                (0 != DescendantFileNode->NonPaged->SectionObjectPointers.ImageSectionObject &&
                !MmFlushImageSection(&DescendantFileNode->NonPaged->SectionObjectPointers,
                    MmFlushForDelete)))
            {
                /* release the FileNode and rename lock in case of failure! */
                FspFileNodeReleaseF(FileNode, AcquireFlags);
                FspFsvolDeviceFileRenameRelease(FsvolDeviceObject);

                Result = STATUS_ACCESS_DENIED;
                goto exit;
            }
        }
    }

    /* flush and purge cleaned up but still open files affected by rename (github issue #45) */
    {
        PIRP TopLevelIrp;
        IO_STATUS_BLOCK IoStatus;

        /* reset the top-level IRP to avoid deadlock on the FileNodes' resources */
        TopLevelIrp = IoGetTopLevelIrp();
        IoSetTopLevelIrp(0);

        /* enumerate in reverse order so that files are flushed before containing directories */
        for (
            DescendantFileNodeIndex = DescendantFileNodeCount - 1;
            DescendantFileNodeCount > DescendantFileNodeIndex;
            DescendantFileNodeIndex--)
        {
            DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
            HasHandles = (UINT_PTR)DescendantFileNode & 1;
            DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~7);

            if (HasHandles)
                continue;
            if (CheckingOldName &&
                (DescendantFileNode->FileName.Length <= FileName->Length ||
                    L'\\' != DescendantFileNode->FileName.Buffer[FileName->Length / sizeof(WCHAR)]))
                continue;
            if (MmDoesFileHaveUserWritableReferences(&DescendantFileNode->NonPaged->SectionObjectPointers))
                continue;

            /*
             * There are no handles and no writable user mappings. [Ideally we would want to know
             * that there are no handles and no user mappings, period. Is there an DDI/method to
             * do that?] There may be a read-only user mapping, but in this case CcFlushCache
             * should be a no-op and MmForceSectionClosed will fail (which is fine).
             */

            ASSERT(DescendantFileNode != FileNode && DescendantFileNode->MainFileNode != FileNode);

            FspCcFlushCache(&DescendantFileNode->NonPaged->SectionObjectPointers, 0, 0, &IoStatus);
            MmForceSectionClosed(&DescendantFileNode->NonPaged->SectionObjectPointers, FALSE);
        }

        IoSetTopLevelIrp(TopLevelIrp);
    }

    /* break any Batch or Handle oplocks on descendants */
    Result = STATUS_SUCCESS;
    for (
        DescendantFileNodeIndex = 0;
        DescendantFileNodeCount > DescendantFileNodeIndex;
        DescendantFileNodeIndex++)
    {
        DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
        HasHandles = (UINT_PTR)DescendantFileNode & 1;
        DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~7);

        if (!HasHandles)
            continue;

        if (FspFileNodeOplockIsBatch(DescendantFileNode))
        {
            NTSTATUS Result0 = FspFileNodeOplockCheckEx(DescendantFileNode, OplockIrp,
                OPLOCK_FLAG_COMPLETE_IF_OPLOCKED);
            if (STATUS_SUCCESS == Result0)
                Result = NT_SUCCESS(Result) ? STATUS_OPLOCK_BREAK_IN_PROGRESS : Result;
            else
            if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result0)
            {
                Result = NT_SUCCESS(Result) ? STATUS_OPLOCK_BREAK_IN_PROGRESS : Result;
                DescendantFileNodes[DescendantFileNodeIndex] =
                    (PVOID)((UINT_PTR)DescendantFileNode | 2);
            }
            else
                Result = STATUS_ACCESS_DENIED;
        }
        else
        if (FspFileNodeOplockIsHandle(DescendantFileNode))
        {
            NTSTATUS Result0 = FspFileNodeOplockBreakHandle(DescendantFileNode, OplockIrp,
                OPLOCK_FLAG_COMPLETE_IF_OPLOCKED);
            if (STATUS_SUCCESS == Result0)
                Result = NT_SUCCESS(Result) ? STATUS_OPLOCK_BREAK_IN_PROGRESS : Result;
            else
            if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result0)
            {
                Result = NT_SUCCESS(Result) ? STATUS_OPLOCK_BREAK_IN_PROGRESS : Result;
                DescendantFileNodes[DescendantFileNodeIndex] =
                    (PVOID)((UINT_PTR)DescendantFileNode | 2);
            }
            else
                Result = STATUS_ACCESS_DENIED;
        }
    }

    if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result || !NT_SUCCESS(Result))
    {
        /* release the FileNode and rename lock so that we can safely wait without deadlocks */
        FspFileNodeReleaseF(FileNode, AcquireFlags);
        FspFsvolDeviceFileRenameRelease(FsvolDeviceObject);

        /* wait for oplock breaks to finish */
        for (
            DescendantFileNodeIndex = 0;
            DescendantFileNodeCount > DescendantFileNodeIndex;
            DescendantFileNodeIndex++)
        {
            DescendantFileNode = DescendantFileNodes[DescendantFileNodeIndex];
            IsBatchOplock = (UINT_PTR)DescendantFileNode & 2;
            IsHandleOplock = (UINT_PTR)DescendantFileNode & 4;
            DescendantFileNode = (PVOID)((UINT_PTR)DescendantFileNode & ~7);

            if (IsBatchOplock)
            {
                NTSTATUS Result0 = FspFileNodeOplockCheck(DescendantFileNode, OplockIrp);
                ASSERT(STATUS_OPLOCK_BREAK_IN_PROGRESS != Result0);
                if (STATUS_SUCCESS != Result0)
                    Result = STATUS_ACCESS_DENIED;
            }
            else
            if (IsHandleOplock)
            {
                NTSTATUS Result0 = FspFileNodeOplockBreakHandle(DescendantFileNode, OplockIrp, 0);
                ASSERT(STATUS_OPLOCK_BREAK_IN_PROGRESS != Result0);
                if (STATUS_SUCCESS != Result0)
                    Result = STATUS_ACCESS_DENIED;
            }
        }

        goto exit;
    }

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    /* recheck whether there are still files with open handles */
    memset(&RestartKey, 0, sizeof RestartKey);
    for (;;)
    {
        DescendantFileNode = FspFsvolDeviceEnumerateContextByName(FsvolDeviceObject,
            FileName, FALSE, &RestartKey);
        if (0 == DescendantFileNode)
            break;

        if (DescendantFileNode != FileNode && 0 < DescendantFileNode->HandleCount)
        {
            /*
             * If we are doing a POSIX rename, then it is ok if we have open handles,
             * provided that we do not have sharing violations.
             *
             * Check our share access:
             *
             * - If all openers are allowing FILE_SHARE_DELETE.
             * - And all named streams openers are allowing FILE_SHARE_DELETE.
             *
             * Then we are good to go.
             *
             * (WinFsp cannot rename streams and there is no need to check MainFileDenyDeleteCount).
             *
             * NOTE: These are derived rules. AFAIK there is no documentation on how NTFS
             * does this in the case of POSIX rename.
             */
            if (PosixRename &&
                DescendantFileNode->ShareAccess.OpenCount == DescendantFileNode->ShareAccess.SharedDelete &&
                0 == DescendantFileNode->StreamDenyDeleteCount)
                continue;

            /* release the FileNode and rename lock in case of failure! */
            FspFileNodeReleaseF(FileNode, AcquireFlags);
            FspFsvolDeviceFileRenameRelease(FsvolDeviceObject);

            Result = PosixRename ? STATUS_SHARING_VIOLATION : STATUS_ACCESS_DENIED;
            break;
        }
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

exit:
    SCATTER_DESCENDANTS(TRUE);

    return Result;
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
    BOOLEAN Deleted, Inserted, AcquireForeign;
    FSP_FILE_NODE *InsertedFileNode;
    USHORT FileNameLength;
    PWSTR ExternalFileName;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);

    GATHER_DESCENDANTS(&FileNode->FileName, FALSE, {});

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

        InsertedFileNode = FspFsvolDeviceInsertContextByName(
            FsvolDeviceObject, &DescendantFileNode->FileName, DescendantFileNode,
            &DescendantFileNode->ContextByNameElementStorage, &Inserted);
        if (!Inserted)
        {
            /*
             * Handle files that have been replaced after a Rename.
             * For example, this can happen when the user has mapped and closed a file
             * or immediately after breaking a Batch oplock or
             * when doing a POSIX rename.
             */

            ASSERT(FspFileNodeIsValid(InsertedFileNode));
            ASSERT(DescendantFileNode != InsertedFileNode);
            ASSERT(0 != InsertedFileNode->OpenCount);

            InsertedFileNode->OpenCount = 0;
            FspFsvolDeviceDeleteContextByName(FsvolDeviceObject, &InsertedFileNode->FileName, &Deleted);
            ASSERT(Deleted);

            FspFileNodeDereference(InsertedFileNode);

            FspFsvolDeviceInsertContextByName(
                FsvolDeviceObject, &DescendantFileNode->FileName, DescendantFileNode,
                &DescendantFileNode->ContextByNameElementStorage, &Inserted);
            ASSERT(Inserted);
        }

        if (AcquireForeign)
            FspFileNodeReleaseForeign(DescendantFileNode);
    }

    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    SCATTER_DESCENDANTS(FALSE);
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
    FileInfo->EaSize = FileNode->EaSize;
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

BOOLEAN FspFileNodeTryGetFileInfoByName(PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp,
    PUNICODE_STRING FileName, FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PACCESS_STATE AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;
    ACCESS_MASK DesiredAccess = AccessState->RemainingDesiredAccess;
    ACCESS_MASK GrantedAccess = AccessState->PreviouslyGrantedAccess;
    KPROCESSOR_MODE RequestorMode =
        FlagOn(IrpSp->Flags, SL_FORCE_ACCESS_CHECK) ? UserMode : Irp->RequestorMode;
    BOOLEAN HasTraversePrivilege =
        BooleanFlagOn(AccessState->Flags, TOKEN_HAS_TRAVERSE_PRIVILEGE);
    FSP_FILE_NODE *FileNode;
    PVOID SecurityBuffer;
    BOOLEAN Result;

    if (UserMode == RequestorMode)
    {
        /* user mode: allow only FILE_READ_ATTRIBUTES with traverse privilege */
        if (FILE_READ_ATTRIBUTES != DesiredAccess || !HasTraversePrivilege)
            return FALSE;
    }
    else
        /* kernel mode: anything goes! */
        DesiredAccess = 0;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);
    FileNode = FspFsvolDeviceLookupContextByName(FsvolDeviceObject, FileName);
    if (0 != FileNode)
        FspFileNodeReference(FileNode);
    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    Result = FALSE;
    if (0 != FileNode)
    {
        FspFileNodeAcquireShared(FileNode, Main);

        if (0 != DesiredAccess)
        {
            ASSERT(FILE_READ_ATTRIBUTES == DesiredAccess);

            if (FspFileNodeReferenceSecurity(FileNode, &SecurityBuffer, 0))
            {
                NTSTATUS AccessStatus;
                Result = SeAccessCheck(
                    SecurityBuffer,
                    &AccessState->SubjectSecurityContext,
                    TRUE,
                    DesiredAccess,
                    GrantedAccess,
                    0,
                    IoGetFileObjectGenericMapping(),
                    RequestorMode,
                    &GrantedAccess,
                    &AccessStatus);

                FspFileNodeDereferenceSecurity(SecurityBuffer);

                Result = Result && FspFileNodeTryGetFileInfo(FileNode, FileInfo);
            }
        }
        else
            Result = FspFileNodeTryGetFileInfo(FileNode, FileInfo);

        FspFileNodeRelease(FileNode, Main);
        FspFileNodeDereference(FileNode);
    }

    return Result;
}

VOID FspFileNodeSetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, BOOLEAN TruncateOnClose)
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

    if (TruncateOnClose)
    {
        if ((UINT64)FileNode->Header.AllocationSize.QuadPart != AllocationSize ||
            (UINT64)FileNode->Header.FileSize.QuadPart != FileInfo->FileSize)
            FileNode->TruncateOnClose = TRUE;

        FileNode->Header.AllocationSize.QuadPart = AllocationSize;
        FileNode->Header.FileSize.QuadPart = FileInfo->FileSize;
    }
    else
    {
        FileNode->Header.AllocationSize.QuadPart = AllocationSize;
        FileNode->Header.FileSize.QuadPart = FileInfo->FileSize;
    }

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
    MainFileNode->EaSize = FileInfo->EaSize;

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

BOOLEAN FspFileNodeTrySetFileInfoAndSecurityOnOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo,
    const PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG SecurityDescriptorSize,
    BOOLEAN TruncateOnClose)
{
    PAGED_CODE();

    BOOLEAN EarlyExit;

    FspFsvolDeviceLockContextTable(FileNode->FsvolDeviceObject);
    EarlyExit = 1 < FileNode->OpenCount;
    FspFsvolDeviceUnlockContextTable(FileNode->FsvolDeviceObject);

    if (EarlyExit)
    {
        if (TruncateOnClose)
        {
            FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
                FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
            UINT64 AllocationSize = FileInfo->AllocationSize > FileInfo->FileSize ?
                FileInfo->AllocationSize : FileInfo->FileSize;
            UINT64 AllocationUnit;

            AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
                FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;
            AllocationSize = (AllocationSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;

            if ((UINT64)FileNode->Header.AllocationSize.QuadPart != AllocationSize ||
                (UINT64)FileNode->Header.FileSize.QuadPart != FileInfo->FileSize)
                FileNode->TruncateOnClose = TRUE;
        }

        return FALSE;
    }

    FspFileNodeSetFileInfo(FileNode, CcFileObject, FileInfo, TruncateOnClose);
    if (0 != SecurityDescriptor)
        FspFileNodeSetSecurity(FileNode, SecurityDescriptor, SecurityDescriptorSize);
    return TRUE;
}

BOOLEAN FspFileNodeTrySetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, ULONG InfoChangeNumber)
{
    PAGED_CODE();

    if (FspFileNodeFileInfoChangeNumber(FileNode) != InfoChangeNumber)
        return FALSE;

    FspFileNodeSetFileInfo(FileNode, CcFileObject, FileInfo, FALSE);
    return TRUE;
}

VOID FspFileNodeInvalidateFileInfo(FSP_FILE_NODE *FileNode)
{
    PAGED_CODE();

    FileNode->FileInfoExpirationTime = FileNode->BasicInfoExpirationTime = 0;

    if (0 != FileNode->MainFileNode)
        FileNode->MainFileNode->BasicInfoExpirationTime = 0;
}

BOOLEAN FspFileNodeReferenceSecurity(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    UINT64 Security;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    Security = NonPaged->Security;

    return FspMetaCacheReferenceItemBuffer(FsvolDeviceExtension->SecurityCache,
        Security, PBuffer, PSize);
}

VOID FspFileNodeSetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 Security;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    Security = NonPaged->Security;

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->SecurityCache, Security);
    Security = 0 != Buffer ?
        FspMetaCacheAddItem(FsvolDeviceExtension->SecurityCache, Buffer, Size) : 0;
    FileNode->SecurityChangeNumber++;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeInvalidateSecurity */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    NonPaged->Security = Security;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);
}

BOOLEAN FspFileNodeTrySetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG SecurityChangeNumber)
{
    // !PAGED_CODE();

    if (FspFileNodeSecurityChangeNumber(FileNode) != SecurityChangeNumber)
        return FALSE;

    FspFileNodeSetSecurity(FileNode, Buffer, Size);
    return TRUE;
}

VOID FspFileNodeInvalidateSecurity(FSP_FILE_NODE *FileNode)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 Security;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeSetSecurity */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    Security = NonPaged->Security;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->SecurityCache, Security);
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

static VOID FspFileNodeInvalidateDirInfoByName(PDEVICE_OBJECT FsvolDeviceObject,
    PUNICODE_STRING FileName)
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);
    FileNode = FspFsvolDeviceLookupContextByName(FsvolDeviceObject, FileName);
    if (0 != FileNode)
        FspFileNodeReference(FileNode);
    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (0 != FileNode)
    {
        FspFileNodeInvalidateDirInfo(FileNode);
        FspFileNodeDereference(FileNode);
    }
}

VOID FspFileNodeInvalidateParentDirInfo(FSP_FILE_NODE *FileNode)
{
    PAGED_CODE();

    if (sizeof(WCHAR) == FileNode->FileName.Length && L'\\' == FileNode->FileName.Buffer[0])
        return; /* root does not have a parent */

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    UNICODE_STRING Parent, Suffix;

    FspFileNameSuffix(&FileNode->FileName, &Parent, &Suffix);
    FspFileNodeInvalidateDirInfoByName(FsvolDeviceObject, &Parent);
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

VOID FspFileNodeInvalidateStreamInfo(FSP_FILE_NODE *FileNode)
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

BOOLEAN FspFileNodeReferenceEa(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    UINT64 Ea;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    Ea = NonPaged->Ea;

    return FspMetaCacheReferenceItemBuffer(FsvolDeviceExtension->EaCache,
        Ea, PBuffer, PSize);
}

VOID FspFileNodeSetEa(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 Ea;

    /* no need to acquire the NpInfoSpinLock as the FileNode is acquired */
    Ea = NonPaged->Ea;

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->EaCache, Ea);
    Ea = 0 != Buffer ?
        FspMetaCacheAddItem(FsvolDeviceExtension->EaCache, Buffer, Size) : 0;
    FileNode->EaChangeNumber++;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeInvalidateEa */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    NonPaged->Ea = Ea;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);
}

BOOLEAN FspFileNodeTrySetEa(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG EaChangeNumber)
{
    // !PAGED_CODE();

    if (FspFileNodeEaChangeNumber(FileNode) != EaChangeNumber)
        return FALSE;

    FspFileNodeSetEa(FileNode, Buffer, Size);
    return TRUE;
}

VOID FspFileNodeInvalidateEa(FSP_FILE_NODE *FileNode)
{
    // !PAGED_CODE();

    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FILE_NODE_NONPAGED *NonPaged = FileNode->NonPaged;
    KIRQL Irql;
    UINT64 Ea;

    /* acquire the NpInfoSpinLock to protect against concurrent FspFileNodeSetEa */
    KeAcquireSpinLock(&NonPaged->NpInfoSpinLock, &Irql);
    Ea = NonPaged->Ea;
    KeReleaseSpinLock(&NonPaged->NpInfoSpinLock, Irql);

    FspMetaCacheInvalidateItem(FsvolDeviceExtension->EaCache, Ea);
}

VOID FspFileNodeNotifyChange(FSP_FILE_NODE *FileNode, ULONG Filter, ULONG Action,
    BOOLEAN InvalidateCaches)
{
    /* FileNode must be acquired (exclusive or shared) Main */

    PAGED_CODE();

    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    UNICODE_STRING Parent, Suffix;

    if (0 != FileNode->MainFileNode)
    {
        if (FlagOn(Filter, FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME))
            SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_NAME);
        if (FlagOn(Filter, FILE_NOTIFY_CHANGE_SIZE))
            SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_SIZE);
        if (FlagOn(Filter, FILE_NOTIFY_CHANGE_LAST_WRITE))
            SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_WRITE);
        ClearFlag(Filter, ~(FILE_NOTIFY_CHANGE_STREAM_NAME | FILE_NOTIFY_CHANGE_STREAM_SIZE |
            FILE_NOTIFY_CHANGE_STREAM_WRITE));

        switch (Action)
        {
        case FILE_ACTION_ADDED:
            Action = FILE_ACTION_ADDED_STREAM;
            break;
        case FILE_ACTION_REMOVED:
            Action = FILE_ACTION_REMOVED_STREAM;
            break;
        case FILE_ACTION_MODIFIED:
            Action = FILE_ACTION_MODIFIED_STREAM;
            break;
        }
    }

    if (0 != Filter)
    {
        FspFileNameSuffix(&FileNode->FileName, &Parent, &Suffix);

        if (InvalidateCaches)
        {
            FspFsvolDeviceInvalidateVolumeInfo(FsvolDeviceObject);
            if (0 == FileNode->MainFileNode)
            {
                if (sizeof(WCHAR) == FileNode->FileName.Length && L'\\' == FileNode->FileName.Buffer[0])
                    ; /* root does not have a parent */
                else
                    FspFileNodeInvalidateDirInfoByName(FsvolDeviceObject, &Parent);
            }
            else
                FspFileNodeInvalidateStreamInfo(FileNode);
        }

        FspNotifyReportChange(
            FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList,
            &FileNode->FileName,
            (USHORT)((PUINT8)Suffix.Buffer - (PUINT8)FileNode->FileName.Buffer),
            0, Filter, Action);
    }
}

VOID FspFileNodeInvalidateCachesAndNotifyChangeByName(PDEVICE_OBJECT FsvolDeviceObject,
    PUNICODE_STRING FileName, ULONG Filter, ULONG Action,
    BOOLEAN InvalidateParentCaches)
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode;

    FspFsvolDeviceLockContextTable(FsvolDeviceObject);
    FileNode = FspFsvolDeviceLookupContextByName(FsvolDeviceObject, FileName);
    if (0 != FileNode)
        FspFileNodeReference(FileNode);
    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);

    if (0 != FileNode)
    {
        FspFileNodeAcquireExclusive(FileNode, Full);

        if (0 != FileNode->NonPaged->SectionObjectPointers.DataSectionObject)
        {
            IO_STATUS_BLOCK IoStatus;
            FspCcFlushCache(&FileNode->NonPaged->SectionObjectPointers, 0, 0, &IoStatus);
            if (NT_SUCCESS(IoStatus.Status))
                CcPurgeCacheSection(&FileNode->NonPaged->SectionObjectPointers, 0, 0, FALSE);
        }

        FspFileNodeInvalidateFileInfo(FileNode);
        FspFileNodeInvalidateSecurity(FileNode);
        FspFileNodeInvalidateDirInfo(FileNode);
        FspFileNodeInvalidateStreamInfo(FileNode);
        FspFileNodeInvalidateEa(FileNode);

        FspFileNodeNotifyChange(FileNode, Filter, Action, InvalidateParentCaches);

        FspFileNodeRelease(FileNode, Full);

        FspFileNodeDereference(FileNode);
    }
    else
    {
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
        UNICODE_STRING Parent, Suffix;
        BOOLEAN IsStream;

        IsStream = FALSE;
        for (PWSTR P = FileName->Buffer, EndP = P + FileName->Length / sizeof(WCHAR); EndP > P; P++)
            if (L':' == *P)
            {
                IsStream = TRUE;
                break;
            }

        if (IsStream)
        {
            if (FlagOn(Filter, FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME))
                SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_NAME);
            if (FlagOn(Filter, FILE_NOTIFY_CHANGE_SIZE))
                SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_SIZE);
            if (FlagOn(Filter, FILE_NOTIFY_CHANGE_LAST_WRITE))
                SetFlag(Filter, FILE_NOTIFY_CHANGE_STREAM_WRITE);
            ClearFlag(Filter, ~(FILE_NOTIFY_CHANGE_STREAM_NAME | FILE_NOTIFY_CHANGE_STREAM_SIZE |
                FILE_NOTIFY_CHANGE_STREAM_WRITE));

            switch (Action)
            {
            case FILE_ACTION_ADDED:
                Action = FILE_ACTION_ADDED_STREAM;
                break;
            case FILE_ACTION_REMOVED:
                Action = FILE_ACTION_REMOVED_STREAM;
                break;
            case FILE_ACTION_MODIFIED:
                Action = FILE_ACTION_MODIFIED_STREAM;
                break;
            }
        }

        if (0 != Filter)
        {
            FspFileNameSuffix(FileName, &Parent, &Suffix);

            if (InvalidateParentCaches)
            {
                FspFsvolDeviceInvalidateVolumeInfo(FsvolDeviceObject);
                if (!IsStream)
                {
                    if (sizeof(WCHAR) == FileName->Length && L'\\' == FileName->Buffer[0])
                        ; /* root does not have a parent */
                    else
                        FspFileNodeInvalidateDirInfoByName(FsvolDeviceObject, &Parent);
                }
            }

            FspNotifyReportChange(
                FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList,
                FileName,
                (USHORT)((PUINT8)Suffix.Buffer - (PUINT8)FileName->Buffer),
                0, Filter, Action);
        }
    }
}

NTSTATUS FspFileNodeProcessLockIrp(FSP_FILE_NODE *FileNode, PIRP Irp)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        Result = FsRtlProcessFileLock(&FileNode->FileLock, Irp, FileNode);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Irp->IoStatus.Status = GetExceptionCode();
        Irp->IoStatus.Information = 0;

        Result = FspFileNodeCompleteLockIrp(FileNode, Irp);
    }

    return Result;
}

static NTSTATUS FspFileNodeCompleteLockIrp(PVOID Context, PIRP Irp)
{
    PAGED_CODE();

    NTSTATUS Result = Irp->IoStatus.Status;

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

    if (0 != FileDesc->DirectoryMarker.Buffer)
        FspFree(FileDesc->DirectoryMarker.Buffer);

    FspFree(FileDesc);
}

NTSTATUS FspFileDescResetDirectory(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName, BOOLEAN RestartScan, BOOLEAN IndexSpecified)
{
    PAGED_CODE();

    if (0 == FileDesc->DirectoryPattern.Buffer ||
        (RestartScan && 0 != FileName && 0 != FileName->Length))
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
        FileDesc->DirectoryHasSuchFile = FALSE;

        if (0 != FileDesc->DirectoryMarker.Buffer)
        {
            FspFree(FileDesc->DirectoryMarker.Buffer);
            FileDesc->DirectoryMarker.Buffer = 0;
        }
    }
    else if (RestartScan)
    {
        ASSERT(0 == FileName || 0 == FileName->Length);

        FileDesc->DirectoryHasSuchFile = FALSE;

        if (0 != FileDesc->DirectoryMarker.Buffer)
        {
            FspFree(FileDesc->DirectoryMarker.Buffer);
            FileDesc->DirectoryMarker.Buffer = 0;
        }
    }
    else if (IndexSpecified && 0 != FileName && 0 != FileName->Length)
    {
        NTSTATUS Result;

        Result = FspFileDescSetDirectoryMarker(FileDesc, FileName);
        if (!NT_SUCCESS(Result))
            return Result;

        FileDesc->DirectoryHasSuchFile = FALSE;
    }

    return STATUS_SUCCESS;
}

NTSTATUS FspFileDescSetDirectoryMarker(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName)
{
    PAGED_CODE();

    if (&FileDesc->DirectoryMarker == FileName)
        return STATUS_SUCCESS;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileDesc->FileNode->FsvolDeviceObject);
    UNICODE_STRING DirectoryMarker;

    if (FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR) < FileName->Length)
        return STATUS_OBJECT_NAME_INVALID;

    DirectoryMarker.Length = DirectoryMarker.MaximumLength = FileName->Length;
    DirectoryMarker.Buffer = FspAlloc(FileName->Length);
    if (0 == DirectoryMarker.Buffer)
        return STATUS_INSUFFICIENT_RESOURCES;
    RtlCopyMemory(DirectoryMarker.Buffer, FileName->Buffer, FileName->Length);

    if (0 != FileDesc->DirectoryMarker.Buffer)
        FspFree(FileDesc->DirectoryMarker.Buffer);

    FileDesc->DirectoryMarker = DirectoryMarker;

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
    ASSERT(FspFileNameIsValid(MainFileName,
        FsvolDeviceExtension->VolumeParams.MaxComponentLength,
        0, 0));

    *PMainFileHandle = 0;
    *PMainFileObject = 0;

    switch (Disposition)
    {
    case FILE_CREATE:
    case FILE_OPEN_IF:
    case FILE_OVERWRITE_IF:
    case FILE_SUPERSEDE:
        Disposition = FILE_OPEN_IF;
        break;
    case FILE_OPEN:
    case FILE_OVERWRITE:
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
