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

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static NTSTATUS FspFsvrtClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static NTSTATUS FspFsvolClose(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;

    if (!FspDeviceRetain(FsvrtDeviceObject))
        return STATUS_CANCELLED;
    try
    {
        FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =
            FspFsvrtDeviceExtension(FsvrtDeviceObject);
        PFILE_OBJECT FileObject = IrpSp->FileObject;
        FSP_FILE_CONTEXT *FsContext = FileObject->FsContext;
        UINT64 UserContext = FsContext->UserContext;
        UINT64 UserContext2 = (UINT_PTR)FileObject->FsContext2;
        BOOLEAN FileNameRequired = 0 != FsvrtDeviceExtension->VolumeParams.FileNameRequired;
        FSP_FSCTL_TRANSACT_REQ *Request;

        /* dereference the FsContext (and delete if no more references) */
        FspFileContextRelease(FsContext);

        /* create the user-mode file system request */
        Result = FspIopCreateRequest(0, FileNameRequired ? &FsContext->FileName : 0, 0, &Request);
        if (!NT_SUCCESS(Result))
            goto leak_exit;

        /*
         * The new request is associated with our IRP and will be deleted during its completion.
         */

        /* populate the Close request */
        Request->Kind = FspFsctlTransactCloseKind;
        Request->Req.Close.UserContext = UserContext;
        Request->Req.Close.UserContext2 = UserContext2;

        /* post as a work request; this allows us to complete our own IRP and return immediately! */
        if (!FspIopPostWorkRequest(DeviceObject, Request))
            /* no need to delete the request here as FspIopPostWorkRequest() will do so in all cases */
            goto leak_exit;

        Result = STATUS_SUCCESS;

        goto exit;

    leak_exit:;
#if DBG
        DEBUGLOG("FileObject=%p[%p:\"%wZ\"], UserContext=%llx, UserContext2=%p: "
            "error: the user-mode file system handle will be leaked!",
            IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName,
            UserContext, UserContext2);
#endif
        /* IRP_MJ_CLOSE cannot really fail :-\ */
        Result = STATUS_SUCCESS;

    exit:;
    }
    finally
    {
        FspDeviceRelease(FsvrtDeviceObject);
    }

    return Result;
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

    ASSERT(IRP_MJ_CLOSE == IrpSp->MajorFunction);

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
