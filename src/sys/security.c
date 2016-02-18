/**
 * @file sys/security.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolQuerySecurityComplete;
static NTSTATUS FspFsvolSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolSetSecurityComplete;
static FSP_IOP_REQUEST_FINI FspFsvolSecurityRequestFini;
FSP_DRIVER_DISPATCH FspQuerySecurity;
FSP_DRIVER_DISPATCH FspSetSecurity;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQuerySecurity)
#pragma alloc_text(PAGE, FspFsvolQuerySecurityComplete)
#pragma alloc_text(PAGE, FspFsvolSetSecurity)
#pragma alloc_text(PAGE, FspFsvolSetSecurityComplete)
#pragma alloc_text(PAGE, FspFsvolSecurityRequestFini)
#pragma alloc_text(PAGE, FspQuerySecurity)
#pragma alloc_text(PAGE, FspSetSecurity)
#endif

enum
{
    /* QuerySecurity */
    RequestFileNode                     = 0,
    RequestSecurityChangeNumber         = 1,

    /* SetSecurity */
    //RequestFileNode                   = 0,
};

static NTSTATUS FspFsvolQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    SECURITY_INFORMATION SecurityInformation = IrpSp->Parameters.QuerySecurity.SecurityInformation;
    PVOID Buffer = Irp->UserBuffer;
    ULONG Length = IrpSp->Parameters.QuerySecurity.Length;
    PVOID SecurityBuffer;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeAcquireShared(FileNode, Main);
    if (FspFileNodeReferenceSecurity(FileNode, &SecurityBuffer, 0))
    {
        FspFileNodeRelease(FileNode, Main);

        Result = FspQuerySecurityDescriptorInfo(SecurityInformation, Buffer, &Length, SecurityBuffer);
        FspFileNodeDereferenceSecurity(SecurityBuffer);

        Irp->IoStatus.Information = Length;
        return Result;
    }

    FspFileNodeAcquireShared(FileNode, Pgio);

    FSP_FSCTL_TRANSACT_REQ *Request;

    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolSecurityRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactQuerySecurityKind;
    Request->Req.QuerySecurity.UserContext = FileNode->UserContext;
    Request->Req.QuerySecurity.UserContext2 = FileDesc->UserContext2;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolQuerySecurityComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    SECURITY_INFORMATION SecurityInformation = IrpSp->Parameters.QuerySecurity.SecurityInformation;
    PVOID Buffer = Irp->UserBuffer;
    ULONG Length = IrpSp->Parameters.QuerySecurity.Length;
    PVOID SecurityBuffer = 0;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    BOOLEAN Success;

    if (0 != FspIopRequestContext(Request, RequestFileNode))
    {
        FspIopRequestContext(Request, RequestSecurityChangeNumber) = (PVOID)FileNode->SecurityChangeNumber;
        FspIopRequestContext(Request, RequestFileNode) = 0;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }

    Success = DEBUGRANDTEST(90, TRUE) && FspFileNodeTryAcquireExclusive(FileNode, Main);
    if (!Success)
    {
        FspIopRetryCompleteIrp(Irp, Response, &Result);
        FSP_RETURN();
    }

    Success = !FspFileNodeTrySetSecurity(FileNode,
        Response->Buffer, Response->Rsp.QuerySecurity.SecurityDescriptor.Size,
        (ULONG)(UINT_PTR)FspIopRequestContext(Request, RequestSecurityChangeNumber));
    Success = Success && FspFileNodeReferenceSecurity(FileNode, &SecurityBuffer, 0);
    FspFileNodeRelease(FileNode, Main);
    if (Success)
    {
        Result = FspQuerySecurityDescriptorInfo(SecurityInformation, Buffer, &Length, SecurityBuffer);
        FspFileNodeDereferenceSecurity(SecurityBuffer);
    }
    else
    {
        SecurityBuffer = (PVOID)Response->Buffer;
        Result = FspQuerySecurityDescriptorInfo(SecurityInformation, Buffer, &Length, SecurityBuffer);
    }

    Irp->IoStatus.Information = Length;

    FSP_LEAVE_IOC("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.QuerySecurity.SecurityInformation);
}

static NTSTATUS FspFsvolSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspFsvolSetSecurityComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.SetSecurity.SecurityInformation);
}

static VOID FspFsvolSecurityRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

NTSTATUS FspQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQuerySecurity(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.QuerySecurity.SecurityInformation);
}

NTSTATUS FspSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetSecurity(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.SetSecurity.SecurityInformation);
}
