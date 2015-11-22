/**
 * @file sys/flush.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsvolFlushBuffers;
DRIVER_DISPATCH FspFlushBuffers;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolFlushBuffers)
#pragma alloc_text(PAGE, FspFlushBuffers)
#endif

static
NTSTATUS
FspFsvolFlushBuffers(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspFlushBuffers(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_FLUSH_BUFFERS == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFlushBuffers(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
