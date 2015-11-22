/**
 * @file sys/fileinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsvolQueryInformation;
static DRIVER_DISPATCH FspFsvolSetInformation;
DRIVER_DISPATCH FspQueryInformation;
DRIVER_DISPATCH FspSetInformation;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryInformation)
#pragma alloc_text(PAGE, FspFsvolSetInformation)
#pragma alloc_text(PAGE, FspQueryInformation)
#pragma alloc_text(PAGE, FspSetInformation)
#endif

static
NTSTATUS
FspFsvolQueryInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static
NTSTATUS
FspFsvolSetInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspQueryInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_INFORMATION == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQueryInformation(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}

NTSTATUS
FspSetInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_SET_INFORMATION == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetInformation(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
