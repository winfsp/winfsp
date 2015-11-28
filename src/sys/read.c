/**
 * @file sys/read.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolRead(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_DRIVER_DISPATCH FspRead;
FSP_IOCOMPLETION_DISPATCH FspReadComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolRead)
#pragma alloc_text(PAGE, FspRead)
#pragma alloc_text(PAGE, FspReadComplete)
#endif

static NTSTATUS FspFsvolRead(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspRead(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_READ == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolRead(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}

VOID FspReadComplete(
    PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("", 0);
}
