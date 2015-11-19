/**
 * @file sys/create.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

DRIVER_DISPATCH FspCreate;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCreate)
#endif

NTSTATUS
FspCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CREATE == IrpSp->MajorFunction);

    if (FspFileSystemDeviceExtensionKind == FspDeviceExtension(DeviceObject)->Kind)
    {
        Result = STATUS_SUCCESS;
        Irp->IoStatus.Status = Result;
        Irp->IoStatus.Information = FILE_OPENED;
        IoCompleteRequest(Irp, FSP_IO_INCREMENT);
        FSP_RETURN();
    }

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    Result = STATUS_INVALID_DEVICE_REQUEST;

    FSP_LEAVE_MJ("", 0);
}
