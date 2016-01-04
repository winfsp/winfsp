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

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_CONTEXT *FsContext = FileObject->FsContext;
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

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
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
