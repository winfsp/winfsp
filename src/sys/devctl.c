/**
 * @file sys/devctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsvolDeviceControl;
DRIVER_DISPATCH FspDeviceControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolDeviceControl)
#pragma alloc_text(PAGE, FspDeviceControl)
#endif

static
NTSTATUS
FspFsvolDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_DEVICE_CONTROL == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolDeviceControl(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
