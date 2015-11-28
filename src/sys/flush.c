/**
 * @file sys/flush.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_DRIVER_DISPATCH FspFlushBuffers;
FSP_IOPROC_DISPATCH FspFlushBuffersComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolFlushBuffers)
#pragma alloc_text(PAGE, FspFlushBuffers)
#pragma alloc_text(PAGE, FspFlushBuffersComplete)
#endif

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_FLUSH_BUFFERS == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFlushBuffers(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s", "");
}

VOID FspFlushBuffersComplete(
    PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s", "");
}
