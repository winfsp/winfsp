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
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
FspSetInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}
