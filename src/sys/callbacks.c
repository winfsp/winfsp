/**
 * @file sys/callbacks.c
 * Fast I/O and resource acquisition callbacks.
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;
FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;
BOOLEAN FspAcquireForLazyWrite(
    PVOID Context,
    BOOLEAN Wait);
VOID FspReleaseFromLazyWrite(
    PVOID Context);
BOOLEAN FspAcquireForReadAhead(
    PVOID Context,
    BOOLEAN Wait);
VOID FspReleaseFromReadAhead(
    PVOID Context);
VOID FspPropagateTopFlags(PIRP Irp, PIRP TopLevelFlags);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFastIoCheckIfPossible)
#pragma alloc_text(PAGE, FspAcquireFileForNtCreateSection)
#pragma alloc_text(PAGE, FspReleaseFileForNtCreateSection)
#pragma alloc_text(PAGE, FspAcquireForModWrite)
#pragma alloc_text(PAGE, FspReleaseForModWrite)
#pragma alloc_text(PAGE, FspAcquireForCcFlush)
#pragma alloc_text(PAGE, FspReleaseForCcFlush)
#pragma alloc_text(PAGE, FspAcquireForLazyWrite)
#pragma alloc_text(PAGE, FspReleaseFromLazyWrite)
#pragma alloc_text(PAGE, FspAcquireForReadAhead)
#pragma alloc_text(PAGE, FspReleaseFromReadAhead)
#pragma alloc_text(PAGE, FspPropagateTopFlags)
#endif

BOOLEAN FspFastIoCheckIfPossible(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    ULONG LockKey,
    BOOLEAN CheckForReadOperation,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER_BOOL(PAGED_CODE());

    Result = FALSE;

    FSP_LEAVE_BOOL("FileObject=%p", FileObject);
}

VOID FspAcquireFileForNtCreateSection(
    PFILE_OBJECT FileObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    FspFileNodeAcquireExclusive(FileNode, Full);

    FSP_LEAVE_VOID("FileObject=%p", FileObject);
}

VOID FspReleaseFileForNtCreateSection(
    PFILE_OBJECT FileObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    FspFileNodeRelease(FileNode, Full);

    FSP_LEAVE_VOID("FileObject=%p", FileObject);
}

NTSTATUS FspAcquireForModWrite(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER EndingOffset,
    PERESOURCE *ResourceToRelease,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    ASSERT((PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP == IoGetTopLevelIrp());
    FspFileNodeAcquireExclusive(FileNode, Full);
    *ResourceToRelease = FileNode->Header.PagingIoResource;
        /* ignored by us, but ModWriter expects it; any (non-NULL) resource will do */

    FSP_LEAVE("FileObject=%p", FileObject);
}

NTSTATUS FspReleaseForModWrite(
    PFILE_OBJECT FileObject,
    PERESOURCE ResourceToRelease,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    FspFileNodeRelease(FileNode, Full);
    ASSERT((PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP == IoGetTopLevelIrp());

    FSP_LEAVE("FileObject=%p", FileObject);
}

NTSTATUS FspAcquireForCcFlush(
    PFILE_OBJECT FileObject,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PIRP TopLevelIrp = IoGetTopLevelIrp();

    ASSERT(0 == TopLevelIrp ||
        (PIRP)FSRTL_MAX_TOP_LEVEL_IRP_FLAG < TopLevelIrp);
    FspFileNodeAcquireExclusive(FileNode, Full);
    if (0 == TopLevelIrp)
        IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

    FSP_LEAVE("FileObject=%p", FileObject);
}

NTSTATUS FspReleaseForCcFlush(
    PFILE_OBJECT FileObject,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PIRP TopLevelIrp = IoGetTopLevelIrp();

    ASSERT((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == TopLevelIrp ||
        (PIRP)FSRTL_MAX_TOP_LEVEL_IRP_FLAG < TopLevelIrp);
    if ((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == TopLevelIrp)
        IoSetTopLevelIrp(0);
    FspFileNodeRelease(FileNode, Full);

    FSP_LEAVE("FileObject=%p", FileObject);
}

BOOLEAN FspAcquireForLazyWrite(
    PVOID Context,
    BOOLEAN Wait)
{
    FSP_ENTER_BOOL(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT(0 == IoGetTopLevelIrp());
    Result = FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, Wait);
    if (Result)
        IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

    FSP_LEAVE_BOOL("Context=%p, Wait=%d", Context, Wait);
}

VOID FspReleaseFromLazyWrite(
    PVOID Context)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == IoGetTopLevelIrp());
    IoSetTopLevelIrp(0);
    FspFileNodeRelease(FileNode, Full);

    FSP_LEAVE_VOID("Context=%p", Context);
}

BOOLEAN FspAcquireForReadAhead(
    PVOID Context,
    BOOLEAN Wait)
{
    FSP_ENTER_BOOL(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT(0 == IoGetTopLevelIrp());
    Result = FspFileNodeTryAcquireSharedF(FileNode, FspFileNodeAcquireFull, Wait);
    if (Result)
        IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

    FSP_LEAVE_BOOL("Context=%p, Wait=%d", Context, Wait);
}

VOID FspReleaseFromReadAhead(
    PVOID Context)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FSP_FILE_NODE *FileNode = Context;

    ASSERT((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP == IoGetTopLevelIrp());
    IoSetTopLevelIrp(0);
    FspFileNodeRelease(FileNode, Full);

    FSP_LEAVE_VOID("Context=%p", Context);
}

VOID FspPropagateTopFlags(PIRP Irp, PIRP TopLevelIrp)
{
    /*
     * We place FspPropagateTopFlags in this file, because the top flags
     * are related to the resources acquired in FspAcquire*.
     */

    PAGED_CODE();

    ASSERT(0 != Irp && 0 != TopLevelIrp);

    if ((PIRP)FSRTL_MAX_TOP_LEVEL_IRP_FLAG >= TopLevelIrp)
    {
        DEBUGBREAK_EX(iorecu);

        FspIrpSetTopFlags(Irp, FspFileNodeAcquireFull);
    }
    else if (IO_TYPE_IRP == TopLevelIrp->Type)
    {
        PFILE_OBJECT FileObject = IoGetCurrentIrpStackLocation(Irp)->FileObject;
        PFILE_OBJECT TopLevelFileObject = IoGetCurrentIrpStackLocation(TopLevelIrp)->FileObject;
        if (0 != FileObject && 0 != TopLevelFileObject &&
            FileObject->FsContext == TopLevelFileObject->FsContext &&
            FspFileNodeIsValid(FileObject->FsContext))
        {
            DEBUGBREAK_EX(iorecu);

            FspIrpSetTopFlags(Irp, FspIrpFlags(TopLevelIrp));
        }
    }
}
