/**
 * @file sys/close.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsctlClose;
static DRIVER_DISPATCH FspFsvrtClose;
static DRIVER_DISPATCH FspFsvolClose;
DRIVER_DISPATCH FspClose;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlClose)
#pragma alloc_text(PAGE, FspFsvrtClose)
#pragma alloc_text(PAGE, FspFsvolClose)
#pragma alloc_text(PAGE, FspClose)
#endif

static
NTSTATUS
FspFsctlClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static
NTSTATUS
FspFsvrtClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static
NTSTATUS
FspFsvolClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CLOSE == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolClose(DeviceObject, Irp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtClose(DeviceObject, Irp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlClose(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
