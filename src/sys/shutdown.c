/**
 * @file sys/shutdown.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspShutdown;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspShutdown)
#endif

NTSTATUS
FspShutdown(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    return STATUS_NOT_IMPLEMENTED;
}
