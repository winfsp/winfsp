/**
 * @file sys/volinfo.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryFsAttributeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryFsDeviceInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryFsFullSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
static NTSTATUS FspFsvolQueryFsSectorSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
static NTSTATUS FspFsvolQueryFsSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
static NTSTATUS FspFsvolQueryFsVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
static NTSTATUS FspFsvolQueryVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolQueryVolumeInformationComplete;
static NTSTATUS FspFsvolSetFsLabelInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PVOID Buffer, ULONG Length, PULONG PRequestExtraSize,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolSetVolumeInformationComplete;
FSP_DRIVER_DISPATCH FspQueryVolumeInformation;
FSP_DRIVER_DISPATCH FspSetVolumeInformation;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryFsAttributeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryFsDeviceInformation)
#pragma alloc_text(PAGE, FspFsvolQueryFsFullSizeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryFsSectorSizeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryFsSizeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryFsVolumeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeInformation)
#pragma alloc_text(PAGE, FspFsvolQueryVolumeInformationComplete)
#pragma alloc_text(PAGE, FspFsvolSetFsLabelInformation)
#pragma alloc_text(PAGE, FspFsvolSetVolumeInformation)
#pragma alloc_text(PAGE, FspFsvolSetVolumeInformationComplete)
#pragma alloc_text(PAGE, FspQueryVolumeInformation)
#pragma alloc_text(PAGE, FspSetVolumeInformation)
#endif

#define GETVOLUMEINFO()                 \
    FSP_FSCTL_VOLUME_INFO VolumeInfoBuf;\
    if (0 == VolumeInfo)                \
    {                                   \
        if (!FspFsvolDeviceTryGetVolumeInfo(FsvolDeviceObject, &VolumeInfoBuf))\
            return FSP_STATUS_IOQ_POST; \
        VolumeInfo = &VolumeInfoBuf;    \
    }

static NTSTATUS FspFsvolQueryFsAttributeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_FS_ATTRIBUTE_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result = STATUS_SUCCESS;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_FS_ATTRIBUTE_INFORMATION Info = (PFILE_FS_ATTRIBUTE_INFORMATION)*PBuffer;
    PUINT8 Buffer = (PUINT8)Info->FileSystemName;
    ULONG CopyLength;
    UNICODE_STRING FileSystemName;
    WCHAR FileSystemNameBuf[16 + FSP_FSCTL_VOLUME_FSNAME_SIZE / sizeof(WCHAR)];

    ASSERT(sizeof FileSystemNameBuf >= sizeof L"" DRIVER_NAME + FSP_FSCTL_VOLUME_FSNAME_SIZE);

    Info->FileSystemAttributes =
        (FsvolDeviceExtension->VolumeParams.CaseSensitiveSearch ? FILE_CASE_SENSITIVE_SEARCH : 0) |
        (FsvolDeviceExtension->VolumeParams.CasePreservedNames ? FILE_CASE_PRESERVED_NAMES : 0) |
        (FsvolDeviceExtension->VolumeParams.UnicodeOnDisk ? FILE_UNICODE_ON_DISK : 0) |
        (FsvolDeviceExtension->VolumeParams.PersistentAcls ? FILE_PERSISTENT_ACLS : 0) |
        (FsvolDeviceExtension->VolumeParams.ReparsePoints ? FILE_SUPPORTS_REPARSE_POINTS : 0) |
        (FsvolDeviceExtension->VolumeParams.NamedStreams ? FILE_NAMED_STREAMS : 0) |
        //(FsvolDeviceExtension->VolumeParams.HardLinks ? FILE_SUPPORTS_HARD_LINKS : 0) |
        (FsvolDeviceExtension->VolumeParams.ExtendedAttributes ? FILE_SUPPORTS_EXTENDED_ATTRIBUTES : 0) |
        (FsvolDeviceExtension->VolumeParams.ReadOnlyVolume ? FILE_READ_ONLY_VOLUME : 0) |
        (FsvolDeviceExtension->VolumeParams.SupportsPosixUnlinkRename ? FILE_SUPPORTS_POSIX_UNLINK_RENAME : 0);
    Info->MaximumComponentNameLength = FsvolDeviceExtension->VolumeParams.MaxComponentLength;

    RtlInitUnicodeString(&FileSystemName, FsvolDeviceExtension->VolumeParams.FileSystemName);

    if (0 == FileSystemName.Length ||
        (L'-' == FileSystemName.Buffer[0] ||
            L'/' == FileSystemName.Buffer[0] ||
            L'\\' == FileSystemName.Buffer[0]))
    {
        CopyLength = sizeof L"" DRIVER_NAME - sizeof(WCHAR);
        RtlCopyMemory(FileSystemNameBuf, L"" DRIVER_NAME, CopyLength);
        RtlCopyMemory(FileSystemNameBuf + CopyLength / sizeof(WCHAR), FileSystemName.Buffer,
            FileSystemName.Length);
        CopyLength += FileSystemName.Length;
    }
    else
    {
        CopyLength = FileSystemName.Length;
        RtlCopyMemory(FileSystemNameBuf, FileSystemName.Buffer, CopyLength);
    }

    Info->FileSystemNameLength = CopyLength;
    if (Buffer + CopyLength > BufferEnd)
    {
        CopyLength = (ULONG)(BufferEnd - Buffer);
        Result = STATUS_BUFFER_OVERFLOW;
    }
    RtlCopyMemory(Buffer, FileSystemNameBuf, CopyLength);
    Buffer += CopyLength;

    *PBuffer = Buffer;

    return Result;
}

static NTSTATUS FspFsvolQueryFsDeviceInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_FS_DEVICE_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    PFILE_FS_DEVICE_INFORMATION Info = (PFILE_FS_DEVICE_INFORMATION)*PBuffer;

    /*
     * The following value MUST be FILE_DEVICE_DISK or GetFileType fails,
     * which has all sorts of interesting consequences (like cmd.exe failing
     * to redirect to a file when under a network file system).
     *
     * See also (which explicitly says to use FILE_DEVICE_DISK for our case):
     * https://msdn.microsoft.com/en-us/library/cc232109.aspx
     */
    Info->DeviceType = FILE_DEVICE_DISK;
    Info->Characteristics = FsvolDeviceObject->Characteristics;

    *PBuffer += sizeof(FILE_FS_DEVICE_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryFsFullSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_FS_FULL_SIZE_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    GETVOLUMEINFO();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_FS_FULL_SIZE_INFORMATION Info = (PFILE_FS_FULL_SIZE_INFORMATION)*PBuffer;
    UINT64 AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
        FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;

    Info->TotalAllocationUnits.QuadPart = VolumeInfo->TotalSize / AllocationUnit;
    Info->CallerAvailableAllocationUnits.QuadPart =
    Info->ActualAvailableAllocationUnits.QuadPart = VolumeInfo->FreeSize / AllocationUnit;
    Info->SectorsPerAllocationUnit = FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;
    Info->BytesPerSector = FsvolDeviceExtension->VolumeParams.SectorSize;

    *PBuffer += sizeof(FILE_FS_FULL_SIZE_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryFsSectorSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_FS_SECTOR_SIZE_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_FS_SECTOR_SIZE_INFORMATION Info = (PFILE_FS_SECTOR_SIZE_INFORMATION)*PBuffer;

    Info->LogicalBytesPerSector =
        Info->PhysicalBytesPerSectorForAtomicity =
        Info->PhysicalBytesPerSectorForPerformance =
        Info->FileSystemEffectivePhysicalBytesPerSectorForAtomicity =
            FsvolDeviceExtension->VolumeParams.SectorSize;
    Info->Flags =
        SSINFO_FLAGS_ALIGNED_DEVICE |
        SSINFO_FLAGS_PARTITION_ALIGNED_ON_DEVICE |
        SSINFO_FLAGS_NO_SEEK_PENALTY;
    Info->ByteOffsetForSectorAlignment = 0;
    Info->ByteOffsetForPartitionAlignment = 0;

    *PBuffer += sizeof(FILE_FS_SECTOR_SIZE_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryFsSizeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_FS_SIZE_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    GETVOLUMEINFO();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_FS_SIZE_INFORMATION Info = (PFILE_FS_SIZE_INFORMATION)*PBuffer;
    UINT64 AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
        FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;

    Info->TotalAllocationUnits.QuadPart = VolumeInfo->TotalSize / AllocationUnit;
    Info->AvailableAllocationUnits.QuadPart = VolumeInfo->FreeSize / AllocationUnit;
    Info->SectorsPerAllocationUnit = FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;
    Info->BytesPerSector = FsvolDeviceExtension->VolumeParams.SectorSize;

    *PBuffer += sizeof(FILE_FS_SIZE_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryFsVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PUINT8 *PBuffer, PUINT8 BufferEnd,
    const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_FS_VOLUME_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    GETVOLUMEINFO();

    NTSTATUS Result = STATUS_SUCCESS;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_FS_VOLUME_INFORMATION Info = (PFILE_FS_VOLUME_INFORMATION)*PBuffer;
    PUINT8 Buffer = (PUINT8)Info->VolumeLabel;
    ULONG CopyLength;

    Info->VolumeCreationTime.QuadPart = FsvolDeviceExtension->VolumeParams.VolumeCreationTime;
    Info->VolumeSerialNumber = FsvolDeviceExtension->VolumeParams.VolumeSerialNumber;
    Info->VolumeLabelLength = VolumeInfo->VolumeLabelLength;
    Info->SupportsObjects = FALSE;

    CopyLength = VolumeInfo->VolumeLabelLength;
    if (Buffer + CopyLength > BufferEnd)
    {
        CopyLength = (ULONG)(BufferEnd - Buffer);
        Result = STATUS_BUFFER_OVERFLOW;
    }
    RtlCopyMemory(Buffer, VolumeInfo->VolumeLabel, CopyLength);
    Buffer += CopyLength;

    *PBuffer = Buffer;

    return Result;
}

static NTSTATUS FspFsvolQueryVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

#if defined(FSP_CFG_REJECT_EARLY_IRP)
    if (!FspFsvolDeviceReadyToAcceptIrp(FsvolDeviceObject))
        return STATUS_CANCELLED;
#endif

    NTSTATUS Result;
    PUINT8 Buffer = Irp->AssociatedIrp.SystemBuffer;
    PUINT8 BufferEnd = Buffer + IrpSp->Parameters.QueryVolume.Length;

    switch (IrpSp->Parameters.QueryVolume.FsInformationClass)
    {
    case FileFsAttributeInformation:
        Result = FspFsvolQueryFsAttributeInformation(FsvolDeviceObject, &Buffer, BufferEnd);
        break;
    case FileFsDeviceInformation:
        Result = FspFsvolQueryFsDeviceInformation(FsvolDeviceObject, &Buffer, BufferEnd);
        break;
    case FileFsFullSizeInformation:
        Result = FspFsvolQueryFsFullSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd, 0);
        break;
    case FileFsSectorSizeInformation:
        Result = FspFsvolQueryFsSectorSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd, 0);
        break;
    case FileFsSizeInformation:
        Result = FspFsvolQueryFsSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd, 0);
        break;
    case FileFsVolumeInformation:
        Result = FspFsvolQueryFsVolumeInformation(FsvolDeviceObject, &Buffer, BufferEnd, 0);
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

    FSP_FSCTL_TRANSACT_REQ *Request;

    Result = FspIopCreateRequest(Irp, 0, 0, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactQueryVolumeInformationKind;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolQueryVolumeInformationComplete(
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
    PUINT8 BufferEnd = Buffer + IrpSp->Parameters.QueryVolume.Length;

    FspFsvolDeviceSetVolumeInfo(FsvolDeviceObject, &Response->Rsp.QueryVolumeInformation.VolumeInfo);

    switch (IrpSp->Parameters.QueryVolume.FsInformationClass)
    {
    case FileFsFullSizeInformation:
        Result = FspFsvolQueryFsFullSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd,
            &Response->Rsp.QueryVolumeInformation.VolumeInfo);
        break;
    case FileFsSizeInformation:
        Result = FspFsvolQueryFsSizeInformation(FsvolDeviceObject, &Buffer, BufferEnd,
            &Response->Rsp.QueryVolumeInformation.VolumeInfo);
        break;
    case FileFsVolumeInformation:
        Result = FspFsvolQueryFsVolumeInformation(FsvolDeviceObject, &Buffer, BufferEnd,
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

static NTSTATUS FspFsvolSetFsLabelInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PVOID Buffer, ULONG Length, PULONG PRequestExtraSize,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    if (0 == Request)
    {
        if (sizeof(FILE_FS_LABEL_INFORMATION) > Length)
            return STATUS_INVALID_PARAMETER;

        PFILE_FS_LABEL_INFORMATION Info = (PFILE_FS_LABEL_INFORMATION)Buffer;

        *PRequestExtraSize = Info->VolumeLabelLength + sizeof(WCHAR);
    }
    else if (0 == Response)
    {
        PFILE_FS_LABEL_INFORMATION Info = (PFILE_FS_LABEL_INFORMATION)Buffer;

        Request->Req.SetVolumeInformation.Info.Label.VolumeLabel.Offset = 0;
        Request->Req.SetVolumeInformation.Info.Label.VolumeLabel.Size =
            (UINT16)(Info->VolumeLabelLength + sizeof(WCHAR));
        RtlCopyMemory(Request->Buffer, Info->VolumeLabel, Info->VolumeLabelLength);
        ((PWSTR)Request->Buffer)[Info->VolumeLabelLength / sizeof(WCHAR)] = L'\0';
    }
    else
        FspFsvolDeviceSetVolumeInfo(FsvolDeviceObject, &Response->Rsp.SetVolumeInformation.VolumeInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetVolumeInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

#if defined(FSP_CFG_REJECT_EARLY_IRP)
    if (!FspFsvolDeviceReadyToAcceptIrp(FsvolDeviceObject))
        return STATUS_CANCELLED;
#endif

    NTSTATUS Result;
    FS_INFORMATION_CLASS FsInformationClass = IrpSp->Parameters.SetVolume.FsInformationClass;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.SetVolume.Length;
    ULONG RequestExtraSize = 0;

    switch (FsInformationClass)
    {
    case FileFsLabelInformation:
        Result = FspFsvolSetFsLabelInformation(FsvolDeviceObject, Buffer, Length, &RequestExtraSize, 0, 0);
        break;
    default:
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    if (!NT_SUCCESS(Result))
        return Result;

    FSP_FSCTL_TRANSACT_REQ *Request;

    Result = FspIopCreateRequest(Irp, 0, RequestExtraSize, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactSetVolumeInformationKind;
    Request->Req.SetVolumeInformation.FsInformationClass = FsInformationClass;

    switch (FsInformationClass)
    {
    case FileFsLabelInformation:
        Result = FspFsvolSetFsLabelInformation(FsvolDeviceObject, Buffer, Length, 0, Request, 0);
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    if (!NT_SUCCESS(Result))
        return Result;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolSetVolumeInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    FS_INFORMATION_CLASS FsInformationClass = IrpSp->Parameters.SetVolume.FsInformationClass;
    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->DeviceObject;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.SetFile.Length;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    switch (FsInformationClass)
    {
    case FileFsLabelInformation:
        Result = FspFsvolSetFsLabelInformation(FsvolDeviceObject, Buffer, Length, 0, Request, Response);
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    Irp->IoStatus.Information = 0;

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
