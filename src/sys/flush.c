/**
 * @file sys/flush.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolFlushBuffersComplete;
static FSP_IOP_REQUEST_FINI FspFsvolFlushBuffersRequestFini;
FSP_DRIVER_DISPATCH FspFlushBuffers;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolFlushBuffers)
#pragma alloc_text(PAGE, FspFsvolFlushBuffersComplete)
#pragma alloc_text(PAGE, FspFsvolFlushBuffersRequestFini)
#pragma alloc_text(PAGE, FspFlushBuffers)
#endif

enum
{
    /* FlushBuffers */
    RequestFileNode                     = 0,
};

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSCTL_TRANSACT_REQ *Request;

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
    {
        /* indicate that we are flushing the whole volume! */
        Result = FspIopCreateRequest(Irp, 0, 0, &Request);
        if (!NT_SUCCESS(Result))
            return Result;

        Request->Kind = FspFsctlTransactFlushBuffersKind;

        return FSP_STATUS_IOQ_POST;
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    IO_STATUS_BLOCK IoStatus;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeAcquireExclusive(FileNode, Full);

    Result = FspCcFlushCache(FileObject->SectionObjectPointer, 0, 0, &IoStatus);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolFlushBuffersRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactFlushBuffersKind;
    Request->Req.FlushBuffers.UserContext = FileNode->UserContext;
    Request->Req.FlushBuffers.UserContext2 = FileDesc->UserContext2;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolFlushBuffersComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p",
        IrpSp->FileObject);
}

static VOID FspFsvolFlushBuffersRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

NTSTATUS FspFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFlushBuffers(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p",
        IrpSp->FileObject);
}
