/**
 * @file sys/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsctlFileSystemControl;
static DRIVER_DISPATCH FspFsvrtFileSystemControl;
static DRIVER_DISPATCH FspFsvolFileSystemControl;
DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvrtFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFileSystemControl)
#endif

static
NTSTATUS
FspFsctlFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static
NTSTATUS
FspFsvrtFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static
NTSTATUS
FspFsvolFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspFileSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_FILE_SYSTEM_CONTROL == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlFileSystemControl(DeviceObject, Irp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtFileSystemControl(DeviceObject, Irp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}
