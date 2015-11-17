/**
 * @file sys/read.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspRead;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspRead)
#endif

NTSTATUS
FspRead(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}
