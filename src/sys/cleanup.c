/**
 * @file sys/cleanup.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolCleanupComplete;
static FSP_IOP_REQUEST_FINI FspFsvolCleanupRequestFini;
static VOID FspFsvolCleanupUninitialize(PVOID Context);
FSP_DRIVER_DISPATCH FspCleanup;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCleanup)
#pragma alloc_text(PAGE, FspFsvrtCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanupComplete)
#pragma alloc_text(PAGE, FspFsvolCleanupRequestFini)
#pragma alloc_text(PAGE, FspFsvolCleanupUninitialize)
#pragma alloc_text(PAGE, FspCleanup)
#endif

enum
{
    /* Cleanup */
    RequestIrp                          = 0,
};

typedef struct
{
    PFILE_OBJECT FileObject;
    LARGE_INTEGER TruncateSize, *PTruncateSize;
    WORK_QUEUE_ITEM WorkQueueItem;
} FSP_FSVOL_CLEANUP_UNINITIALIZE_WORK_ITEM;

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    if (0 != IrpSp->FileObject->FsContext2)
        FspVolumeDelete(DeviceObject, Irp, IrpSp);

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_SUCCESS;

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FSCTL_TRANSACT_REQ *Request;
    BOOLEAN DeletePending;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeCleanup(FileNode, FileObject, &DeletePending);
    if (DeletePending)
        FspFileNodeAcquireExclusive(FileNode, Full);
    else
        FspFileNodeAcquireShared(FileNode, Full);

    /* create the user-mode file system request; MustSucceed because IRP_MJ_CLEANUP cannot fail */
    FspIopCreateRequestMustSucceedEx(Irp, DeletePending ? &FileNode->FileName : 0, 0,
        FspFsvolCleanupRequestFini, &Request);
    Request->Kind = FspFsctlTransactCleanupKind;
    Request->Req.Cleanup.UserContext = FileNode->UserContext;
    Request->Req.Cleanup.UserContext2 = FileDesc->UserContext2;
    Request->Req.Cleanup.Delete = DeletePending;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;

    return FSP_STATUS_IOQ_POST_BEST_EFFORT;

    /*
     * Note that it is still possible for this request to not be delivered,
     * if the volume device Ioq is stopped. But such failures are benign
     * from our perspective, because they mean that the file system is going
     * away and should correctly tear things down.
     */
}

NTSTATUS FspFsvolCleanupComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p", IrpSp->FileObject);
}

static VOID FspFsvolCleanupRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    /*
     * Cleanup is rather unusual in that we are doing the cleanup post-processing
     * in RequestFini rather than in CleanupComplete. The reason for this is that
     * we want this processing to happen even in the (unlikely) event of the user-
     * mode file system going away, while our Request is queued (in which case the
     * Irp will get cancelled).
     */

    PAGED_CODE();

    PIRP Irp = Context[RequestIrp];
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSCTL_FILE_INFO FileInfo;
    LARGE_INTEGER TruncateSize = { 0 }, *PTruncateSize = 0;
    BOOLEAN DeletePending = 0 != Request->Req.Cleanup.Delete;
    BOOLEAN DeletedFromContextTable;
    BOOLEAN Success;

    FspFileNodeClose(FileNode, FileObject, &DeletedFromContextTable);
    if (DeletedFromContextTable)
    {
        if (DeletePending)
            PTruncateSize = &TruncateSize;
        else if (FileNode->TruncateOnClose)
        {
            /*
             * Even when FileInfoTimeout != Infinity,
             * this is the last size that the cache manager knows.
             */
            FspFileNodeGetFileInfo(FileNode, &FileInfo);

            TruncateSize.QuadPart = FileInfo.FileSize;
            PTruncateSize = &TruncateSize;
        }
    }

    if (DeletePending)
    {
        /* FileNode is Exclusive Full; release Pgio */
        FspFileNodeReleaseOwner(FileNode, Pgio, Request);

        /* FileNode is now Exclusive Main; owner is Request */
    }
    else
    {
        /* FileNode is Shared Full; reacquire as Exclusive Main for CcUnitializeCacheMap */
        FspFileNodeReleaseOwner(FileNode, Full, Request);

        Success = DEBUGTEST(90, TRUE) && FspFileNodeTryAcquireExclusive(FileNode, Main);
        if (!Success)
        {
            /* oh, maaan! we now have to do delayed uninitialize! */

            FSP_FSVOL_CLEANUP_UNINITIALIZE_WORK_ITEM *WorkItem;

            WorkItem = FspAllocatePoolMustSucceed(
                NonPagedPool, sizeof *WorkItem, FSP_ALLOC_INTERNAL_TAG);
            WorkItem->FileObject = FileObject;
            WorkItem->TruncateSize = TruncateSize;
            WorkItem->PTruncateSize = 0 != PTruncateSize ? &WorkItem->TruncateSize : 0;
            ExInitializeWorkItem(&WorkItem->WorkQueueItem, FspFsvolCleanupUninitialize, WorkItem);

            /* make sure that the file object (and corresponding device object) stay around! */
            ObReferenceObject(FileObject);

            ExQueueWorkItem(&WorkItem->WorkQueueItem, CriticalWorkQueue);

            return;
        }

        /* FileNode is now Exclusive Main; owner is current thread */
    }

    CcUninitializeCacheMap(FileObject, PTruncateSize, 0);

    /* this works correctly even if owner is current thread */
    FspFileNodeReleaseOwner(FileNode, Main, Request);
}

static VOID FspFsvolCleanupUninitialize(PVOID Context)
{
    PAGED_CODE();

    FSP_FSVOL_CLEANUP_UNINITIALIZE_WORK_ITEM *WorkItem = Context;
    PFILE_OBJECT FileObject = WorkItem->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    LARGE_INTEGER *PTruncateSize = WorkItem->PTruncateSize;

    FspFileNodeAcquireExclusive(FileNode, Main);

    CcUninitializeCacheMap(FileObject, PTruncateSize, 0);

    FspFileNodeRelease(FileNode, Main);

    ObDereferenceObject(FileObject);

    FspFree(WorkItem);
}

NTSTATUS FspCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCleanup(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCleanup(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCleanup(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
