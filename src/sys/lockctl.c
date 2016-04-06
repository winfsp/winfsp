/**
 * @file sys/lockctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    PBOOLEAN PIrpRelinquished);
FSP_IOCMPL_DISPATCH FspFsvolLockControlComplete;
FSP_DRIVER_DISPATCH FspLockControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolLockControl)
#pragma alloc_text(PAGE, FspFsvolLockControlComplete)
#pragma alloc_text(PAGE, FspLockControl)
#endif

static NTSTATUS FspFsvolLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    PBOOLEAN PIrpRelinquished)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    /* only regular files can be locked */
    if (FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /* let the FSRTL package handle this one! */
    Result = FspFileNodeProcessLockIrp(FileNode, Irp, PIrpRelinquished);

    return Result;
}

NTSTATUS FspFsvolLockControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%ld",
        IrpSp->FileObject,
        IrpSp->Parameters.LockControl.Key,
        IrpSp->Parameters.LockControl.ByteOffset.HighPart,
        IrpSp->Parameters.LockControl.ByteOffset.LowPart,
        IrpSp->Parameters.LockControl.Length);
}

NTSTATUS FspLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    BOOLEAN IrpRelinquished = FALSE;

    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolLockControl(DeviceObject, Irp, IrpSp, &IrpRelinquished));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ_COND(!IrpRelinquished && STATUS_PENDING != Result,
        "FileObject=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%ld",
        IrpSp->FileObject,
        IrpSp->Parameters.LockControl.Key,
        IrpSp->Parameters.LockControl.ByteOffset.HighPart,
        IrpSp->Parameters.LockControl.ByteOffset.LowPart,
        IrpSp->Parameters.LockControl.Length);
}
