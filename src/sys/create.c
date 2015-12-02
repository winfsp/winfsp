/**
 * @file sys/create.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_DRIVER_DISPATCH FspCreate;
FSP_IOCMPL_DISPATCH FspCreateComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCreate)
#pragma alloc_text(PAGE, FspFsvrtCreate)
#pragma alloc_text(PAGE, FspFsvolCreate)
#pragma alloc_text(PAGE, FspCreate)
#pragma alloc_text(PAGE, FspCreateComplete)
#endif

static NTSTATUS FspFsctlCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;
    return Result;
}

static NTSTATUS FspFsvrtCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = FILE_OPENED;
    return Result;
}

static NTSTATUS FspFsvolCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =
        FspFsvrtDeviceExtension(FsvolDeviceExtension->FsvrtDeviceObject);

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFILE_OBJECT RelatedFileObject = FileObject->RelatedFileObject;
    UNICODE_STRING FileName = FileObject->FileName;
    ULONG Flags = IrpSp->Flags;
    KPROCESSOR_MODE RequestorMode = FlagOn(Flags, SL_FORCE_ACCESS_CHECK) ? UserMode : Irp->RequestorMode;
    PACCESS_STATE AccessState = IrpSp->Parameters.Create.SecurityContext->AccessState;
    ACCESS_MASK DesiredAccess = IrpSp->Parameters.Create.SecurityContext->DesiredAccess;
    USHORT ShareAccess = IrpSp->Parameters.Create.ShareAccess;
    ULONG CreateDisposition = IrpSp->Parameters.Create.Options >> 24;
    ULONG CreateOptions = IrpSp->Parameters.Create.Options & 0xffffff;
    USHORT FileAttributes = IrpSp->Parameters.Create.FileAttributes;
    LARGE_INTEGER AllocationSize = Irp->Overlay.AllocationSize;
    PFILE_FULL_EA_INFORMATION EaBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG EaLength = IrpSp->Parameters.Create.EaLength;
    BOOLEAN HasTraversePrivilege = BooleanFlagOn(AccessState->Flags, TOKEN_HAS_TRAVERSE_PRIVILEGE);

    /* cannot open the volume object */
    if (0 == RelatedFileObject && 0 == FileName.Length)
        return STATUS_ACCESS_DENIED; // need error code like UNIX EPERM (STATUS_NOT_SUPPORTED?)

    /* cannot open paging file */
    if (FlagOn(Flags, SL_OPEN_PAGING_FILE))
        return STATUS_ACCESS_DENIED;

    /* cannot open files by fileid */
    if (FlagOn(CreateOptions, FILE_OPEN_BY_FILE_ID))
        return STATUS_NOT_IMPLEMENTED;

    /* do we support EA? */
    if (0 != EaBuffer && !FsvrtDeviceExtension->VolumeParams.EaSupported)
        return STATUS_EAS_NOT_SUPPORTED;

    /* according to fastfat, filenames that begin with two backslashes are ok */
    if (sizeof(WCHAR) * 2 <= FileName.Length &&
        L'\\' == FileName.Buffer[1] && L'\\' == FileName.Buffer[0])
    {
        FileName.Length -= sizeof(WCHAR);
        FileName.MaximumLength -= sizeof(WCHAR);
        FileName.Buffer++;

        if (sizeof(WCHAR) * 2 <= FileName.Length &&
            L'\\' == FileName.Buffer[1] && L'\\' == FileName.Buffer[0])
            return STATUS_OBJECT_NAME_INVALID;
    }

    return STATUS_PENDING;
}

NTSTATUS FspCreate(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CREATE == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCreate(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCreate(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCreate(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "FileObject=%p[%p:\"%wZ\"], "
        "Flags=%x, "
        "DesiredAccess=%#lx, "
        "ShareAccess=%#x, "
        "Options=%#lx, "
        "FileAttributes=%#x, "
        "AllocationSize=%#lx:%#lx, "
        "Ea=%p, EaLength=%ld",
        IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName,
        IrpSp->Flags,
        IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
        IrpSp->Parameters.Create.ShareAccess,
        IrpSp->Parameters.Create.Options,
        IrpSp->Parameters.Create.FileAttributes,
        Irp->Overlay.AllocationSize.HighPart, Irp->Overlay.AllocationSize.LowPart,
        Irp->AssociatedIrp.SystemBuffer, IrpSp->Parameters.Create.EaLength);
}

VOID FspCreateComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC(
        "FileObject=%p[%p:\"%wZ\"]",
        IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName);
}
