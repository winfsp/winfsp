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
FSP_DRIVER_DISPATCH FspCleanup;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCleanup)
#pragma alloc_text(PAGE, FspFsvrtCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanupComplete)
#pragma alloc_text(PAGE, FspCleanup)
#endif

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
    LONG OpenCount;

    /* all handles on this FileObject are gone; close the FileObject */
    OpenCount = FspFileContextClose(FsContext);

    /* is the FsContext going away as well? */
    if (0 == OpenCount)
    {
        /* remove the FsContext from the volume device generic table */
        FspFsvolDeviceLockContextTable(FsvolDeviceObject);
        FspFsvolDeviceDeleteContext(FsvolDeviceObject, FsContext->UserContext, 0);
        FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);
    }

    /* create the user-mode file system request */
    Result = FspIopCreateRequest(Irp, FileNameRequired ? &FsContext->FileName : 0, 0, &Request);
    if (!NT_SUCCESS(Result))
    {
        /*
         * This really should NOT, but can theoretically happen. One way around it would be to
         * preallocate the Request at IRP_MJ_CREATE time. Unfortunately this becomes more
         * complicated because of the FileNameRequired functionality.
         */
#if DBG
        DEBUGLOG("FileObject=%p, UserContext=%llx, UserContext2=%llx: "
            "error: the user-mode file system handle will be leaked!",
            FileObject, UserContext, UserContext2);
#endif
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    /* populate the Cleanup request */
    Request->Kind = FspFsctlTransactCleanupKind;
    Request->Req.Cleanup.UserContext = UserContext;
    Request->Req.Cleanup.UserContext2 = UserContext2;

    return STATUS_PENDING;
}

VOID FspFsvolCleanupComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p", IrpSp->FileObject);
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
