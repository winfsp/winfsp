/**
 * @file sys/close.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspClose;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspClose)
#endif

NTSTATUS
FspClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CLOSE == IrpSp->MajorFunction);

    if (FspFileSystemDeviceExtensionKind == FspDeviceExtension(DeviceObject)->Kind)
        FSP_RETURN(Irp->IoStatus.Information = 0);

    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
