/**
 * @file sys/flush.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolFlushBuffersComplete;
FSP_DRIVER_DISPATCH FspFlushBuffers;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolFlushBuffers)
#pragma alloc_text(PAGE, FspFsvolFlushBuffersComplete)
#pragma alloc_text(PAGE, FspFlushBuffers)
#endif

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspFsvolFlushBuffersComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s", "");
}

NTSTATUS FspFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFlushBuffers(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s", "");
}
