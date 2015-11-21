/**
 * @file sys/dirctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspDirectoryControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspDirectoryControl)
#endif

NTSTATUS
FspDirectoryControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_DIRECTORY_CONTROL == IrpSp->MajorFunction);

    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE_MJ("", 0);
}
