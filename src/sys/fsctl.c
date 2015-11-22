/**
 * @file sys/fsctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreateVolume)
#pragma alloc_text(PAGE, FspFsvolDeleteVolume)
#pragma alloc_text(PAGE, FspFsvolTransact)
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvrtFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFileSystemControl)
#endif

static NTSTATUS FspFsctlCreateVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    if (FSP_FSCTL_CREATE_BUFFER_SIZEMAX > IrpSp->Parameters.FileSystemControl.OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result;

    GUID Guid;
    Result = CreateGuid(&Guid);
    if (!NT_SUCCESS(Result))
        return Result;

    PDEVICE_OBJECT FsvrtDeviceObject;
    UNICODE_STRING DeviceSddl;
    UNICODE_STRING DeviceName;
    RtlInitUnicodeString(&DeviceSddl, L"" DEVICE_SDDL);
    RtlInitEmptyUnicodeString(&DeviceName, Irp->AssociatedIrp.SystemBuffer, FSP_FSCTL_CREATE_BUFFER_SIZEMAX);
    Result = RtlUnicodeStringPrintf(&DeviceName,
        L"\\Device\\Volume{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        Guid.Data1, Guid.Data2, Guid.Data3,
        Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
        Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7]);
    ASSERT(NT_SUCCESS(Result));
    Result = IoCreateDeviceSecure(DeviceObject->DriverObject,
        sizeof(FSP_FSVRT_DEVICE_EXTENSION), &DeviceName, DeviceObject->DeviceType, 0, FALSE,
        &DeviceSddl, 0,
        &FsvrtDeviceObject);
    if (!NT_SUCCESS(Result))
        return Result;
    FspDeviceExtension(FsvrtDeviceObject)->Kind = FspFsvrtDeviceExtensionKind;
    Irp->IoStatus.Information = DeviceName.Length + 1;

    return Result;
}

static NTSTATUS FspFsvolDeleteVolume(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolTransact(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_CREATE:
            Result = FspFsctlCreateVolume(DeviceObject, Irp, IrpSp);
            break;
        }
    }
    return Result;
}

static NTSTATUS FspFsvrtFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_DELETE:
            Result = FspFsvolDeleteVolume(DeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_TRANSACT:
            Result = FspFsvolTransact(DeviceObject, Irp, IrpSp);
            break;
        }
        break;
    }
    return Result;
}

static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        break;
    case IRP_MN_MOUNT_VOLUME:
        break;
    case IRP_MN_VERIFY_VOLUME:
        break;
    }
    return Result;
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
        FSP_RETURN(Result = FspFsctlFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "FileObject=%p%s%s",
        IrpSp->FileObject,
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ? ", " : "",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ?
            IoctlCodeSym(IrpSp->Parameters.FileSystemControl.FsControlCode) : "");
}
