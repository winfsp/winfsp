/**
 * @file sys/dirctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsvolDirectoryControl;
DRIVER_DISPATCH FspDirectoryControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolDirectoryControl)
#pragma alloc_text(PAGE, FspDirectoryControl)
#endif

static
NTSTATUS
FspFsvolDirectoryControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspDirectoryControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_DIRECTORY_CONTROL == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolDirectoryControl(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
