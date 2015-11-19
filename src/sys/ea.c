/**
 * @file sys/ea.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspQueryEa;
DRIVER_DISPATCH FspSetEa;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspQueryEa)
#pragma alloc_text(PAGE, FspSetEa)
#endif

NTSTATUS
FspQueryEa(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_EA == IoGetCurrentIrpStackLocation(Irp)->MajorFunction);

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE("", 0);
}

NTSTATUS
FspSetEa(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER(PAGED_CODE());

    ASSERT(IRP_MJ_SET_EA == IoGetCurrentIrpStackLocation(Irp)->MajorFunction);

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE("", 0);
}
