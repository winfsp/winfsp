/**
 * @file sys/create.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static DRIVER_DISPATCH FspFsctlCreate;
static DRIVER_DISPATCH FspFsvrtCreate;
static DRIVER_DISPATCH FspFsvolCreate;
DRIVER_DISPATCH FspCreate;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreate)
#pragma alloc_text(PAGE, FspFsvrtCreate)
#pragma alloc_text(PAGE, FspFsvolCreate)
#pragma alloc_text(PAGE, FspCreate)
#endif

static
NTSTATUS
FspFsctlCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;
    return Result;
}

static
NTSTATUS
FspFsvrtCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;
    return Result;
}

static
NTSTATUS
FspFsvolCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CREATE == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCreate(DeviceObject, Irp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCreate(DeviceObject, Irp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCreate(DeviceObject, Irp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "FileObject=%p[%p:\"%wZ\"], "
        "DesiredAccess=%#lx, "
        "ShareAccess=%#x, "
        "Options=%#lx, "
        "FileAttributes=%#x, "
        "AllocationSize=%#lx:%#lx, "
        "Ea=%p, EaLength=%ld",
        IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName,
        IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
        IrpSp->Parameters.Create.ShareAccess,
        IrpSp->Parameters.Create.Options,
        IrpSp->Parameters.Create.FileAttributes,
        Irp->Overlay.AllocationSize.HighPart, Irp->Overlay.AllocationSize.LowPart,
        Irp->AssociatedIrp.SystemBuffer, IrpSp->Parameters.Create.EaLength);
}
