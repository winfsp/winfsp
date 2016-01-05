/**
 * @file sys/close.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolCloseComplete;
FSP_DRIVER_DISPATCH FspClose;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlClose)
#pragma alloc_text(PAGE, FspFsvrtClose)
#pragma alloc_text(PAGE, FspFsvolClose)
#pragma alloc_text(PAGE, FspFsvolCloseComplete)
#pragma alloc_text(PAGE, FspClose)
#endif

static NTSTATUS FspFsctlClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvrtClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolClose(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileContextIsValid(IrpSp->FileObject->FsContext))
        return STATUS_SUCCESS;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    BOOLEAN FileNameRequired = 0 != FsvolDeviceExtension->VolumeParams.FileNameRequired;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_CONTEXT *FsContext = FileObject->FsContext;
    UINT64 UserContext = FsContext->UserContext;
    UINT64 UserContext2 = (UINT_PTR)FileObject->FsContext2;
    FSP_FSCTL_TRANSACT_REQ *Request;

    /* dereference the FsContext (and delete if no more references) */
    FspFileContextRelease(FsContext);

    /* create the user-mode file system request; MustSucceed because IRP_MJ_CLOSE cannot fail */
    Result = FspIopCreateRequestMustSucceed(Irp,
        FileNameRequired ? &FsContext->FileName : 0, 0, &Request);

    /* populate the Close request */
    Request->Kind = FspFsctlTransactCloseKind;
    Request->Req.Close.UserContext = UserContext;
    Request->Req.Close.UserContext2 = UserContext2;

    /*
     * Post as a MustSucceed work request. This allows us to complete our own IRP
     * and return immediately.
     */
    FspIopPostWorkRequestMustSucceed(FsvolDeviceObject, Request);

    /*
     * Note that it is still possible for this request to not be delivered,
     * if the volume device Ioq is stopped. But such failures are benign
     * from our perspective, because they mean that the file system is going
     * away and should correctly tear things down.
     */
    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

VOID FspFsvolCloseComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p", IrpSp->FileObject);
}

NTSTATUS FspClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolClose(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtClose(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlClose(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
