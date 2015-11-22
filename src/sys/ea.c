/**
 * @file sys/ea.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsvolQueryEa;
static DRIVER_DISPATCH FspFsvolSetEa;
DRIVER_DISPATCH FspQueryEa;
DRIVER_DISPATCH FspSetEa;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryEa)
#pragma alloc_text(PAGE, FspFsvolSetEa)
#pragma alloc_text(PAGE, FspQueryEa)
#pragma alloc_text(PAGE, FspSetEa)
#endif

static
NTSTATUS
FspFsvolQueryEa(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static
NTSTATUS
FspFsvolSetEa(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspQueryEa(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_EA == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQueryEa(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}

NTSTATUS
FspSetEa(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_SET_EA == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetEa(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
