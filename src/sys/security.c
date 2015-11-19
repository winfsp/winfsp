/**
 * @file sys/security.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspQuerySecurity;
DRIVER_DISPATCH FspSetSecurity;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspQuerySecurity)
#pragma alloc_text(PAGE, FspSetSecurity)
#endif

NTSTATUS
FspQuerySecurity(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_SECURITY == IoGetCurrentIrpStackLocation(Irp)->MajorFunction);

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE("", 0);
}

NTSTATUS
FspSetSecurity(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER(PAGED_CODE());

    ASSERT(IRP_MJ_SET_SECURITY == IoGetCurrentIrpStackLocation(Irp)->MajorFunction);

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE("", 0);
}
