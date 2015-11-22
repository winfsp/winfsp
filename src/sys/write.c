/**
 * @file sys/write.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsvolWrite;
DRIVER_DISPATCH FspWrite;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolWrite)
#pragma alloc_text(PAGE, FspWrite)
#endif

static
NTSTATUS
FspFsvolWrite(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspWrite(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_WRITE == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolWrite(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
