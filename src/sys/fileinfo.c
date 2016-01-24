/**
 * @file sys/fileinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryAllInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryAttributeTagInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryBasicInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryInternalInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryNameInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryNetworkOpenInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryPositionInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryStandardInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd);
static NTSTATUS FspFsvolQueryInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolQueryInformationComplete;
static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolSetInformationComplete;
FSP_DRIVER_DISPATCH FspQueryInformation;
FSP_DRIVER_DISPATCH FspSetInformation;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryAllInformation)
#pragma alloc_text(PAGE, FspFsvolQueryAttributeTagInformation)
#pragma alloc_text(PAGE, FspFsvolQueryBasicInformation)
#pragma alloc_text(PAGE, FspFsvolQueryInternalInformation)
#pragma alloc_text(PAGE, FspFsvolQueryNameInformation)
#pragma alloc_text(PAGE, FspFsvolQueryNetworkOpenInformation)
#pragma alloc_text(PAGE, FspFsvolQueryPositionInformation)
#pragma alloc_text(PAGE, FspFsvolQueryStandardInformation)
#pragma alloc_text(PAGE, FspFsvolQueryInformation)
#pragma alloc_text(PAGE, FspFsvolQueryInformationComplete)
#pragma alloc_text(PAGE, FspFsvolSetInformation)
#pragma alloc_text(PAGE, FspFsvolSetInformationComplete)
#pragma alloc_text(PAGE, FspQueryInformation)
#pragma alloc_text(PAGE, FspSetInformation)
#endif

#define GETFILEINFO()                   \
    FSP_FSCTL_FILE_INFO FileInfoBuf;    \
    if (0 == FileInfo)                  \
    {                                   \
        if (!FspFileNodeTryGetFileInfo((FSP_FILE_NODE *)FileObject->FsContext, &FileInfoBuf))\
            return FSP_STATUS_IOQ_POST; \
        FileInfo = &FileInfoBuf;        \
    }

static NTSTATUS FspFsvolQueryAllInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_ALL_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    PFILE_ALL_INFORMATION Info = (PFILE_ALL_INFORMATION)*PBuffer;

    if (0 == FileInfo)
    {
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FspFileNodeAcquireShared(FileNode, Main);

        *PBuffer = (PVOID)&Info->PositionInformation;
        FspFsvolQueryPositionInformation(FileObject, PBuffer, BufferEnd);

        *PBuffer = (PVOID)&Info->StandardInformation;
        FspFsvolQueryStandardInformation(FileObject, PBuffer, BufferEnd);

        FspFileNodeRelease(FileNode, Main);

        Info->EaInformation.EaSize = 0;

        *PBuffer = (PVOID)&Info->InternalInformation;
        FspFsvolQueryInternalInformation(FileObject, PBuffer, BufferEnd);
    }

    GETFILEINFO();

    *PBuffer = (PVOID)&Info->BasicInformation;
    FspFsvolQueryBasicInformation(FileObject, PBuffer, BufferEnd, FileInfo);

    *PBuffer = (PVOID)&Info->NameInformation;
    return FspFsvolQueryNameInformation(FileObject, PBuffer, BufferEnd);
}

static NTSTATUS FspFsvolQueryAttributeTagInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_ATTRIBUTE_TAG_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    GETFILEINFO();

    PFILE_ATTRIBUTE_TAG_INFORMATION Info = (PFILE_ATTRIBUTE_TAG_INFORMATION)*PBuffer;

    Info->FileAttributes = FileInfo->FileAttributes;
    Info->ReparseTag = FileInfo->ReparseTag;

    *PBuffer += sizeof(FILE_ATTRIBUTE_TAG_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryBasicInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_BASIC_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    GETFILEINFO();

    PFILE_BASIC_INFORMATION Info = (PFILE_BASIC_INFORMATION)*PBuffer;

    Info->CreationTime.QuadPart = FileInfo->CreationTime;
    Info->LastAccessTime.QuadPart = FileInfo->LastAccessTime;
    Info->LastWriteTime.QuadPart = FileInfo->LastWriteTime;
    Info->ChangeTime.QuadPart = FileInfo->ChangeTime;
    Info->FileAttributes = FileInfo->FileAttributes;

    *PBuffer += sizeof(FILE_BASIC_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryInternalInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_INTERNAL_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PFILE_INTERNAL_INFORMATION Info = (PFILE_INTERNAL_INFORMATION)*PBuffer;

    Info->IndexNumber.QuadPart = FileNode->IndexNumber;

    *PBuffer += sizeof(FILE_INTERNAL_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryNameInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_NAME_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    NTSTATUS Result = STATUS_SUCCESS;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    PFILE_NAME_INFORMATION Info = (PFILE_NAME_INFORMATION)*PBuffer;
    PUINT8 Buffer = (PUINT8)Info->FileName;
    ULONG CopyLength;

    Info->FileNameLength = FsvolDeviceExtension->VolumePrefix.Length + FileNode->FileName.Length;

    CopyLength = FsvolDeviceExtension->VolumePrefix.Length;
    if (Buffer + CopyLength > BufferEnd)
    {
        CopyLength = (ULONG)(BufferEnd - Buffer);
        Result = STATUS_BUFFER_OVERFLOW;
    }
    RtlCopyMemory(Buffer, FsvolDeviceExtension->VolumePrefix.Buffer, CopyLength);
    Buffer += CopyLength;

    CopyLength = FileNode->FileName.Length;
    if (Buffer + CopyLength > BufferEnd)
    {
        CopyLength = (ULONG)(BufferEnd - Buffer);
        Result = STATUS_BUFFER_OVERFLOW;
    }
    RtlCopyMemory(Buffer, FileNode->FileName.Buffer, CopyLength);
    Buffer += CopyLength;

    *PBuffer = Buffer;

    return Result;
}

static NTSTATUS FspFsvolQueryNetworkOpenInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd, const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_NETWORK_OPEN_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    PFILE_NETWORK_OPEN_INFORMATION Info = (PFILE_NETWORK_OPEN_INFORMATION)*PBuffer;

    if (0 == FileInfo)
    {
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FspFileNodeAcquireShared(FileNode, Main);

        Info->AllocationSize = FileNode->Header.AllocationSize;
        Info->EndOfFile = FileNode->Header.FileSize;

        FspFileNodeRelease(FileNode, Main);
    }

    GETFILEINFO();

    Info->CreationTime.QuadPart = FileInfo->CreationTime;
    Info->LastAccessTime.QuadPart = FileInfo->LastAccessTime;
    Info->LastWriteTime.QuadPart = FileInfo->LastWriteTime;
    Info->ChangeTime.QuadPart = FileInfo->ChangeTime;
    Info->FileAttributes = FileInfo->FileAttributes;

    *PBuffer += sizeof(FILE_BASIC_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryPositionInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_POSITION_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PFILE_POSITION_INFORMATION Info = (PFILE_POSITION_INFORMATION)*PBuffer;

    FspFileNodeAcquireShared(FileNode, Main);

    Info->CurrentByteOffset = FileObject->CurrentByteOffset;

    FspFileNodeRelease(FileNode, Main);

    *PBuffer += sizeof(FILE_POSITION_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryStandardInformation(PFILE_OBJECT FileObject,
    PUINT8 *PBuffer, PUINT8 BufferEnd)
{
    PAGED_CODE();

    if (*PBuffer + sizeof(FILE_STANDARD_INFORMATION) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PFILE_STANDARD_INFORMATION Info = (PFILE_STANDARD_INFORMATION)*PBuffer;

    FspFileNodeAcquireShared(FileNode, Main);

    Info->AllocationSize = FileNode->Header.AllocationSize;
    Info->EndOfFile = FileNode->Header.FileSize;
    Info->NumberOfLinks = 1;
    Info->DeletePending = FileObject->DeletePending;
    Info->Directory = FileNode->IsDirectory;

    FspFileNodeRelease(FileNode, Main);

    *PBuffer += sizeof(FILE_STANDARD_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PUINT8 SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    PUINT8 SystemBufferEnd = (PUINT8)SystemBuffer + IrpSp->Parameters.QueryFile.Length;

    switch (IrpSp->Parameters.QueryFile.FileInformationClass)
    {
    case FileAllInformation:
        Result = FspFsvolQueryAllInformation(FileObject, &SystemBuffer, SystemBufferEnd, 0);
        break;
    case FileAttributeTagInformation:
        Result = FspFsvolQueryAttributeTagInformation(FileObject, &SystemBuffer, SystemBufferEnd, 0);
        break;
    case FileBasicInformation:
        Result = FspFsvolQueryBasicInformation(FileObject, &SystemBuffer, SystemBufferEnd, 0);
        break;
    case FileCompressionInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no compression support */
        break;
    case FileEaInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no EA support currently */
        break;
    case FileHardLinkInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no hard link support */
        break;
    case FileInternalInformation:
        Result = FspFsvolQueryInternalInformation(FileObject, &SystemBuffer, SystemBufferEnd);
        break;
    case FileNameInformation:
        Result = FspFsvolQueryNameInformation(FileObject, &SystemBuffer, SystemBufferEnd);
        break;
    case FileNetworkOpenInformation:
        Result = FspFsvolQueryNetworkOpenInformation(FileObject, &SystemBuffer, SystemBufferEnd, 0);
        break;
    case FilePositionInformation:
        Result = FspFsvolQueryPositionInformation(FileObject, &SystemBuffer, SystemBufferEnd);
        break;
    case FileStandardInformation:
        Result = FspFsvolQueryStandardInformation(FileObject, &SystemBuffer, SystemBufferEnd);
        break;
    case FileStreamInformation:
        Result = STATUS_INVALID_PARAMETER;  /* !!!: no stream support yet! */
        break;
    default:
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    if (FSP_STATUS_IOQ_POST != Result)
    {
        Irp->IoStatus.Information = (UINT_PTR)((PUINT8)SystemBufferEnd - (PUINT8)SystemBuffer);
        return Result;
    }

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    BOOLEAN FileNameRequired = 0 != FsvolDeviceExtension->VolumeParams.FileNameRequired;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    Result = FspIopCreateRequest(Irp, FileNameRequired ? &FileNode->FileName : 0, 0, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactQueryInformationKind;
    Request->Req.QueryInformation.UserContext = FileNode->UserContext;
    Request->Req.QueryInformation.UserContext2 = FileDesc->UserContext2;

    return FSP_STATUS_IOQ_POST;
}

VOID FspFsvolQueryInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = Response->IoStatus.Information;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PUINT8 SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
    PUINT8 SystemBufferEnd = (PUINT8)SystemBuffer + IrpSp->Parameters.QueryFile.Length;

    FspFileNodeSetFileInfo(FileNode, &Response->Rsp.QueryInformation.FileInfo);

    switch (IrpSp->Parameters.QueryFile.FileInformationClass)
    {
    case FileAllInformation:
        Result = FspFsvolQueryAllInformation(FileObject, &SystemBuffer, SystemBufferEnd,
            &Response->Rsp.QueryInformation.FileInfo);
        break;
    case FileAttributeTagInformation:
        Result = FspFsvolQueryAttributeTagInformation(FileObject, &SystemBuffer, SystemBufferEnd,
            &Response->Rsp.QueryInformation.FileInfo);
        break;
    case FileBasicInformation:
        Result = FspFsvolQueryBasicInformation(FileObject, &SystemBuffer, SystemBufferEnd,
            &Response->Rsp.QueryInformation.FileInfo);
        break;
    case FileNetworkOpenInformation:
        Result = FspFsvolQueryNetworkOpenInformation(FileObject, &SystemBuffer, SystemBufferEnd,
            &Response->Rsp.QueryInformation.FileInfo);
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    ASSERT(FSP_STATUS_IOQ_POST != Result);

    Irp->IoStatus.Information = (UINT_PTR)((PUINT8)SystemBufferEnd - (PUINT8)SystemBuffer);

    FSP_LEAVE_IOC("%s, FileObject=%p",
        FileInformationClassSym(IrpSp->Parameters.QueryFile.FileInformationClass),
        IrpSp->FileObject);
}

static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

VOID FspFsvolSetInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s, FileObject=%p",
        FileInformationClassSym(IrpSp->Parameters.SetFile.FileInformationClass),
        IrpSp->FileObject);
}

NTSTATUS FspQueryInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQueryInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s, FileObject=%p",
        FileInformationClassSym(IrpSp->Parameters.QueryFile.FileInformationClass),
        IrpSp->FileObject);
}

NTSTATUS FspSetInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s, FileObject=%p",
        FileInformationClassSym(IrpSp->Parameters.SetFile.FileInformationClass),
        IrpSp->FileObject);
}
