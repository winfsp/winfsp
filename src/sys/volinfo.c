/**
 * @file sys/volinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryVolumeAttributeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryVolumeDeviceInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryVolumeFullSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
static NTSTATUS FspFsvolQueryVolumeSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
static NTSTATUS FspFsvolQueryVolumeFsVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
static NTSTATUS FspFsvolQueryVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolQueryVolumeInformationComplete;
static NTSTATUS FspFsvolSetVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolSetVolumeInformationComplete;
FSP_DRIVER_DISPATCH FspQueryVolumeInformation;
FSP_DRIVER_DISPATCH FspSetVolumeInformation;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryVolumeAttributeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeDeviceInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeFullSizeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeSizeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeFsVolumeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeInformationComplete)
#pragma alloc_text(PAGE, FspFsvolSetVolumeInformation)
#pragma alloc_text(PAGE, FspFsvolSetVolumeInformationComplete)
#pragma alloc_text(PAGE, FspQueryVolumeInformation)
#pragma alloc_text(PAGE, FspSetVolumeInformation)
#endif

static NTSTATUS FspFsvolQueryVolumeAttributeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolQueryVolumeDeviceInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolQueryVolumeFullSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolQueryVolumeSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolQueryVolumeFsVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolQueryVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    PUINT8 Buffer = Irp->AssociatedIrp.SystemBuffer;
    PUINT8 BufferEnd = Buffer + IrpSp->Parameters.QueryFile.Length;

    switch (IrpSp->Parameters.QueryVolume.FsInformationClass)
    {
    case FileFsAttributeInformation:
        Result = FspFsvolQueryVolumeAttributeInformation(FsvolDeviceObject, &Buffer, BufferEnd);
        break;
    case FileFsDeviceInformation:
        Result = FspFsvolQueryVolumeDeviceInformation(FsvolDeviceObject, &Buffer, BufferEnd);
        break;
    case FileFsFullSizeInformation:
        Result = FspFsvolQueryVolumeFullSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd, 0);
        break;
    case FileFsSizeInformation:
        Result = FspFsvolQueryVolumeSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd, 0);
        break;
    case FileFsVolumeInformation:
        Result = FspFsvolQueryVolumeFsVolumeInformation(FsvolDeviceObject, &Buffer, BufferEnd, 0);
        break;
    default:
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    if (FSP_STATUS_IOQ_POST != Result)
    {
        Irp->IoStatus.Information = (UINT_PTR)(Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);
        return Result;
    }

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    BOOLEAN FileNameRequired = 0 != FsvolDeviceExtension->VolumeParams.FileNameRequired;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSCTL_TRANSACT_REQ *Request;

    Result = FspIopCreateRequest(Irp, FileNameRequired ? &FileNode->FileName : 0, 0, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactQueryVolumeInformationKind;

    return FSP_STATUS_IOQ_POST;
}

VOID FspFsvolQueryVolumeInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = Response->IoStatus.Information;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->DeviceObject;
    PUINT8 Buffer = Irp->AssociatedIrp.SystemBuffer;
    PUINT8 BufferEnd = Buffer + IrpSp->Parameters.QueryFile.Length;

    //FspVolumeSetSizeInfo(FsvolDeviceObject,
    //    &Response->Rsp.QueryVolumeInformation.TotalAllocationUnits);

    switch (IrpSp->Parameters.QueryVolume.FsInformationClass)
    {
    case FileFsFullSizeInformation:
        Result = FspFsvolQueryVolumeFullSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd,
            &Response->Rsp.QueryVolumeInformation.VolumeInfo);
        break;
    case FileFsSizeInformation:
        Result = FspFsvolQueryVolumeSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd,
            &Response->Rsp.QueryVolumeInformation.VolumeInfo);
        break;
    case FileFsVolumeInformation:
        Result = FspFsvolQueryVolumeFsVolumeInformation(FsvolDeviceObject, &Buffer, BufferEnd,
            &Response->Rsp.QueryVolumeInformation.VolumeInfo);
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    ASSERT(FSP_STATUS_IOQ_POST != Result);

    Irp->IoStatus.Information = (UINT_PTR)(Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);

    FSP_LEAVE_IOC("%s",
        FsInformationClassSym(IrpSp->Parameters.QueryVolume.FsInformationClass));
}

static NTSTATUS FspFsvolSetVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

VOID FspFsvolSetVolumeInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s",
        FsInformationClassSym(IrpSp->Parameters.SetVolume.FsInformationClass));
}

NTSTATUS FspQueryVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQueryVolumeInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s",
        FsInformationClassSym(IrpSp->Parameters.QueryVolume.FsInformationClass));
}

NTSTATUS FspSetVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetVolumeInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s",
        FsInformationClassSym(IrpSp->Parameters.SetVolume.FsInformationClass));
}
