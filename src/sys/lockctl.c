/**
 * @file sys/lockctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolLockControlComplete;
FSP_DRIVER_DISPATCH FspLockControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolLockControl)
#pragma alloc_text(PAGE, FspFsvolLockControlComplete)
#pragma alloc_text(PAGE, FspLockControl)
#endif

static NTSTATUS FspFsvolLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspFsvolLockControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s", "");
}

NTSTATUS FspLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolLockControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s", "");
}
