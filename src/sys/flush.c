/**
 * @file sys/flush.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspFlushBuffers;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFlushBuffers)
#endif

NTSTATUS
FspFlushBuffers(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    return STATUS_NOT_IMPLEMENTED;
}
