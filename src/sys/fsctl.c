/**
 * @file sys/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static
NTSTATUS
FspFsctlCreateVolume(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION IrpSp);
static
NTSTATUS
FspFsctlDeleteVolume(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION IrpSp);
static
NTSTATUS
FspFsvolTransact(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION IrpSp);
static DRIVER_DISPATCH FspFsctlFileSystemControl;
static DRIVER_DISPATCH FspFsvrtFileSystemControl;
static DRIVER_DISPATCH FspFsvolFileSystemControl;
DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreateVolume)
#pragma alloc_text(PAGE, FspFsctlDeleteVolume)
#pragma alloc_text(PAGE, FspFsvolTransact)
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvrtFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFileSystemControl)
#endif

static
NTSTATUS
FspFsctlCreateVolume(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static
NTSTATUS
FspFsctlDeleteVolume(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static
NTSTATUS
FspFsvolTransact(PDEVICE_OBJECT DeviceObject, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static
NTSTATUS
FspFsctlFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_CREATE:
            Result = FspFsctlCreateVolume(DeviceObject, IrpSp);
            break;
        case FSP_FSCTL_DELETE:
            Result = FspFsctlDeleteVolume(DeviceObject, IrpSp);
            break;
        }
    }
    return Result;
}

static
NTSTATUS
FspFsvrtFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_TRANSACT:
            Result = FspFsvolTransact(DeviceObject, IrpSp);
            break;
        }
        break;
    }
    return Result;
}

static
NTSTATUS
FspFsvolFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        break;
    case IRP_MN_MOUNT_VOLUME:
        break;
    case IRP_MN_VERIFY_VOLUME:
        break;
    }
    return Result;
}

NTSTATUS
FspFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlFileSystemControl(DeviceObject, Irp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtFileSystemControl(DeviceObject, Irp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
