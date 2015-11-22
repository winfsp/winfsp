/**
 * @file sys/cleanup.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsctlCleanup;
static DRIVER_DISPATCH FspFsvrtCleanup;
static DRIVER_DISPATCH FspFsvolCleanup;
DRIVER_DISPATCH FspCleanup;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCleanup)
#pragma alloc_text(PAGE, FspFsvrtCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanup)
#pragma alloc_text(PAGE, FspCleanup)
#endif

static
NTSTATUS
FspFsctlCleanup(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static
NTSTATUS
FspFsvrtCleanup(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static
NTSTATUS
FspFsvolCleanup(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspCleanup(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CLEANUP == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCleanup(DeviceObject, Irp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCleanup(DeviceObject, Irp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCleanup(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
