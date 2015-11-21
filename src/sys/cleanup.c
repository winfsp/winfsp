/**
 * @file sys/cleanup.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspCleanup;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCleanup)
#endif

NTSTATUS
FspCleanup(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CLEANUP == IrpSp->MajorFunction);

    if (FspFsctlDeviceExtensionKind == FspDeviceExtension(DeviceObject)->Kind)
        FSP_RETURN(Irp->IoStatus.Information = 0);

    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
