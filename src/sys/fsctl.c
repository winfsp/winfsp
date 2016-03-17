/**
 * @file sys/fsctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolFileSystemControlComplete;
FSP_DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlComplete)
#pragma alloc_text(PAGE, FspFileSystemControl)
#endif

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_VOLUME_NAME:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeGetName(DeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_TRANSACT:
        case FSP_FSCTL_TRANSACT_BATCH:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeTransact(DeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_STOP:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeStop(DeviceObject, Irp, IrpSp);
            break;
        }
        break;
    case IRP_MN_MOUNT_VOLUME:
        Result = FspVolumeMount(DeviceObject, Irp, IrpSp);
        break;
    }

    return Result;
}

static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_WORK:
        case FSP_FSCTL_WORK_BEST_EFFORT:
            Result = FspVolumeWork(DeviceObject, Irp, IrpSp);
            break;
        }
        break;
    }

    return Result;
}

NTSTATUS FspFsvolFileSystemControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC(
        "%s%sFileObject=%p",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ?
            IoctlCodeSym(IrpSp->Parameters.FileSystemControl.FsControlCode) : "",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ? ", " : "",
        IrpSp->FileObject);
}

NTSTATUS FspFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlFileSystemControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "%s%sFileObject=%p",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ?
            IoctlCodeSym(IrpSp->Parameters.FileSystemControl.FsControlCode) : "",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ? ", " : "",
        IrpSp->FileObject);
}
