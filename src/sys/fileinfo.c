/**
 * @file sys/fileinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspQueryInformation;
DRIVER_DISPATCH FspSetInformation;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspQueryInformation)
#pragma alloc_text(PAGE, FspSetInformation)
#endif

NTSTATUS
FspQueryInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_INFORMATION == IoGetCurrentIrpStackLocation(Irp)->MajorFunction);

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE("", 0);
}

NTSTATUS
FspSetInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER(PAGED_CODE());

    ASSERT(IRP_MJ_SET_INFORMATION == IoGetCurrentIrpStackLocation(Irp)->MajorFunction);

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE("", 0);
}
