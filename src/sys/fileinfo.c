/**
 * @file sys/fileinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryAllInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryAttributeTagInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryBasicInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryInternalInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd);
static NTSTATUS FspFsvolQueryNameInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd);
static NTSTATUS FspFsvolQueryNetworkOpenInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryPositionInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd);
static NTSTATUS FspFsvolQueryStandardInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo);
static NTSTATUS FspFsvolQueryInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolQueryInformationComplete;
static NTSTATUS FspFsvolSetAllocationInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetBasicInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetDispositionInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetEndOfFileInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length, BOOLEAN AdvanceOnly,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetPositionInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetRenameInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolSetInformationComplete;
static FSP_IOP_REQUEST_FINI FspFsvolInformationRequestFini;
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
#pragma alloc_text(PAGE, FspFsvolSetAllocationInformation)
#pragma alloc_text(PAGE, FspFsvolSetBasicInformation)
#pragma alloc_text(PAGE, FspFsvolSetDispositionInformation)
#pragma alloc_text(PAGE, FspFsvolSetEndOfFileInformation)
#pragma alloc_text(PAGE, FspFsvolSetPositionInformation)
#pragma alloc_text(PAGE, FspFsvolSetRenameInformation)
#pragma alloc_text(PAGE, FspFsvolSetInformation)
#pragma alloc_text(PAGE, FspFsvolSetInformationComplete)
#pragma alloc_text(PAGE, FspFsvolInformationRequestFini)
#pragma alloc_text(PAGE, FspQueryInformation)
#pragma alloc_text(PAGE, FspSetInformation)
#endif

enum
{
    RequestFileNode                     = 0,
    RequestInfoChangeNumber             = 1,
};

static NTSTATUS FspFsvolQueryAllInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    PFILE_ALL_INFORMATION Info = (PFILE_ALL_INFORMATION)*PBuffer;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    if (0 == FileInfo)
    {
        if ((PVOID)(Info + 1) > BufferEnd)
            return STATUS_BUFFER_TOO_SMALL;

        return STATUS_SUCCESS;
    }

    Info->BasicInformation.CreationTime.QuadPart = FileInfo->CreationTime;
    Info->BasicInformation.LastAccessTime.QuadPart = FileInfo->LastAccessTime;
    Info->BasicInformation.LastWriteTime.QuadPart = FileInfo->LastWriteTime;
    Info->BasicInformation.ChangeTime.QuadPart = FileInfo->ChangeTime;
    Info->BasicInformation.FileAttributes = 0 != FileInfo->FileAttributes ?
        FileInfo->FileAttributes : FILE_ATTRIBUTE_NORMAL;

    Info->StandardInformation.AllocationSize.QuadPart = FileInfo->AllocationSize;
    Info->StandardInformation.EndOfFile.QuadPart = FileInfo->FileSize;
    Info->StandardInformation.NumberOfLinks = 1;
    Info->StandardInformation.DeletePending = FileObject->DeletePending;
    Info->StandardInformation.Directory = FileNode->IsDirectory;

    Info->InternalInformation.IndexNumber.QuadPart = FileNode->IndexNumber;

    Info->EaInformation.EaSize = 0;

    Info->PositionInformation.CurrentByteOffset = FileObject->CurrentByteOffset;

    *PBuffer = (PVOID)&Info->NameInformation;
    return FspFsvolQueryNameInformation(FileObject, PBuffer, BufferEnd);
}

static NTSTATUS FspFsvolQueryAttributeTagInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    PFILE_ATTRIBUTE_TAG_INFORMATION Info = (PFILE_ATTRIBUTE_TAG_INFORMATION)*PBuffer;

    if (0 == FileInfo)
    {
        if ((PVOID)(Info + 1) > BufferEnd)
            return STATUS_BUFFER_TOO_SMALL;

        return STATUS_SUCCESS;
    }

    Info->FileAttributes = 0 != FileInfo->FileAttributes ?
        FileInfo->FileAttributes : FILE_ATTRIBUTE_NORMAL;
    Info->ReparseTag = FileInfo->ReparseTag;

    *PBuffer = (PVOID)(Info + 1);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryBasicInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    PFILE_BASIC_INFORMATION Info = (PFILE_BASIC_INFORMATION)*PBuffer;

    if (0 == FileInfo)
    {
        if ((PVOID)(Info + 1) > BufferEnd)
            return STATUS_BUFFER_TOO_SMALL;

        return STATUS_SUCCESS;
    }

    Info->CreationTime.QuadPart = FileInfo->CreationTime;
    Info->LastAccessTime.QuadPart = FileInfo->LastAccessTime;
    Info->LastWriteTime.QuadPart = FileInfo->LastWriteTime;
    Info->ChangeTime.QuadPart = FileInfo->ChangeTime;
    Info->FileAttributes = 0 != FileInfo->FileAttributes ?
        FileInfo->FileAttributes : FILE_ATTRIBUTE_NORMAL;

    *PBuffer = (PVOID)(Info + 1);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryInternalInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd)
{
    PAGED_CODE();

    PFILE_INTERNAL_INFORMATION Info = (PFILE_INTERNAL_INFORMATION)*PBuffer;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    if ((PVOID)(Info + 1) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    Info->IndexNumber.QuadPart = FileNode->IndexNumber;

    *PBuffer = (PVOID)(Info + 1);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryNameInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    PFILE_NAME_INFORMATION Info = (PFILE_NAME_INFORMATION)*PBuffer;
    PUINT8 Buffer = (PUINT8)Info->FileName;
    ULONG CopyLength;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);

    if ((PVOID)(Info + 1) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    Info->FileNameLength = FsvolDeviceExtension->VolumePrefix.Length + FileNode->FileName.Length;

    CopyLength = FsvolDeviceExtension->VolumePrefix.Length;
    if (Buffer + CopyLength > (PUINT8)BufferEnd)
    {
        CopyLength = (ULONG)((PUINT8)BufferEnd - Buffer);
        Result = STATUS_BUFFER_OVERFLOW;
    }
    RtlCopyMemory(Buffer, FsvolDeviceExtension->VolumePrefix.Buffer, CopyLength);
    Buffer += CopyLength;

    CopyLength = FileNode->FileName.Length;
    if (Buffer + CopyLength > (PUINT8)BufferEnd)
    {
        CopyLength = (ULONG)((PUINT8)BufferEnd - Buffer);
        Result = STATUS_BUFFER_OVERFLOW;
    }
    RtlCopyMemory(Buffer, FileNode->FileName.Buffer, CopyLength);
    Buffer += CopyLength;

    *PBuffer = Buffer;

    return Result;
}

static NTSTATUS FspFsvolQueryNetworkOpenInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    PFILE_NETWORK_OPEN_INFORMATION Info = (PFILE_NETWORK_OPEN_INFORMATION)*PBuffer;

    if (0 == FileInfo)
    {
        if ((PVOID)(Info + 1) > BufferEnd)
            return STATUS_BUFFER_TOO_SMALL;

        return STATUS_SUCCESS;
    }

    Info->AllocationSize.QuadPart = FileInfo->AllocationSize;
    Info->EndOfFile.QuadPart = FileInfo->FileSize;
    Info->CreationTime.QuadPart = FileInfo->CreationTime;
    Info->LastAccessTime.QuadPart = FileInfo->LastAccessTime;
    Info->LastWriteTime.QuadPart = FileInfo->LastWriteTime;
    Info->ChangeTime.QuadPart = FileInfo->ChangeTime;
    Info->FileAttributes = 0 != FileInfo->FileAttributes ?
        FileInfo->FileAttributes : FILE_ATTRIBUTE_NORMAL;

    *PBuffer = (PVOID)(Info + 1);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryPositionInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd)
{
    PAGED_CODE();

    PFILE_POSITION_INFORMATION Info = (PFILE_POSITION_INFORMATION)*PBuffer;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    if ((PVOID)(Info + 1) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    FspFileNodeAcquireShared(FileNode, Main);

    Info->CurrentByteOffset = FileObject->CurrentByteOffset;

    FspFileNodeRelease(FileNode, Main);

    *PBuffer = (PVOID)(Info + 1);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryStandardInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd,
    const FSP_FSCTL_FILE_INFO *FileInfo)
{
    PAGED_CODE();

    PFILE_STANDARD_INFORMATION Info = (PFILE_STANDARD_INFORMATION)*PBuffer;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    if (0 == FileInfo)
    {
        if ((PVOID)(Info + 1) > BufferEnd)
            return STATUS_BUFFER_TOO_SMALL;

        return STATUS_SUCCESS;
    }

    Info->AllocationSize.QuadPart = FileInfo->AllocationSize;
    Info->EndOfFile.QuadPart = FileInfo->FileSize;
    Info->NumberOfLinks = 1;
    Info->DeletePending = FileObject->DeletePending;
    Info->Directory = FileNode->IsDirectory;

    *PBuffer = (PVOID)(Info + 1);

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
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryFile.FileInformationClass;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    PVOID BufferEnd = (PUINT8)Buffer + IrpSp->Parameters.QueryFile.Length;
    FSP_FSCTL_FILE_INFO FileInfoBuf;

    ASSERT(FileNode == FileDesc->FileNode);

    switch (FileInformationClass)
    {
    case FileAllInformation:
        Result = FspFsvolQueryAllInformation(FileObject, &Buffer, BufferEnd, 0);
        break;
    case FileAttributeTagInformation:
        Result = FspFsvolQueryAttributeTagInformation(FileObject, &Buffer, BufferEnd, 0);
        break;
    case FileBasicInformation:
        Result = FspFsvolQueryBasicInformation(FileObject, &Buffer, BufferEnd, 0);
        break;
    case FileCompressionInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no compression support */
        return Result;
    case FileEaInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no EA support currently */
        return Result;
    case FileHardLinkInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no hard link support */
        return Result;
    case FileInternalInformation:
        Result = FspFsvolQueryInternalInformation(FileObject, &Buffer, BufferEnd);
        return Result;
    case FileNameInformation:
        Result = FspFsvolQueryNameInformation(FileObject, &Buffer, BufferEnd);
        return Result;
    case FileNetworkOpenInformation:
        Result = FspFsvolQueryNetworkOpenInformation(FileObject, &Buffer, BufferEnd, 0);
        break;
    case FilePositionInformation:
        Result = FspFsvolQueryPositionInformation(FileObject, &Buffer, BufferEnd);
        return Result;
    case FileStandardInformation:
        Result = FspFsvolQueryStandardInformation(FileObject, &Buffer, BufferEnd, 0);
        break;
    case FileStreamInformation:
        Result = STATUS_INVALID_PARAMETER;  /* !!!: no stream support yet! */
        return Result;
    default:
        Result = STATUS_INVALID_PARAMETER;
        return Result;
    }

    if (!NT_SUCCESS(Result))
        return Result;

    FspFileNodeAcquireShared(FileNode, Main);
    if (FspFileNodeTryGetFileInfo(FileNode, &FileInfoBuf))
    {
        FspFileNodeRelease(FileNode, Main);
        switch (FileInformationClass)
        {
        case FileAllInformation:
            Result = FspFsvolQueryAllInformation(FileObject, &Buffer, BufferEnd, &FileInfoBuf);
            break;
        case FileAttributeTagInformation:
            Result = FspFsvolQueryAttributeTagInformation(FileObject, &Buffer, BufferEnd, &FileInfoBuf);
            break;
        case FileBasicInformation:
            Result = FspFsvolQueryBasicInformation(FileObject, &Buffer, BufferEnd, &FileInfoBuf);
            break;
        case FileNetworkOpenInformation:
            Result = FspFsvolQueryNetworkOpenInformation(FileObject, &Buffer, BufferEnd, &FileInfoBuf);
            break;
        case FileStandardInformation:
            Result = FspFsvolQueryStandardInformation(FileObject, &Buffer, BufferEnd, &FileInfoBuf);
            break;
        default:
            ASSERT(0);
            Result = STATUS_INVALID_PARAMETER;
            break;
        }

        Irp->IoStatus.Information = (UINT_PTR)((PUINT8)Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);
        return Result;
    }

    FspFileNodeAcquireShared(FileNode, Pgio);

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    BOOLEAN FileNameRequired = 0 != FsvolDeviceExtension->VolumeParams.FileNameRequired;
    FSP_FSCTL_TRANSACT_REQ *Request;

    Result = FspIopCreateRequestEx(Irp, FileNameRequired ? &FileNode->FileName : 0, 0,
        FspFsvolInformationRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactQueryInformationKind;
    Request->Req.QueryInformation.UserContext = FileNode->UserContext;
    Request->Req.QueryInformation.UserContext2 = FileDesc->UserContext2;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolQueryInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryFile.FileInformationClass;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    PVOID BufferEnd = (PUINT8)Buffer + IrpSp->Parameters.QueryFile.Length;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    FSP_FSCTL_FILE_INFO FileInfoBuf;
    const FSP_FSCTL_FILE_INFO *FileInfo;

    if (0 != FspIopRequestContext(Request, RequestFileNode))
    {
        FspIopRequestContext(Request, RequestInfoChangeNumber) = (PVOID)FileNode->InfoChangeNumber;
        FspIopRequestContext(Request, RequestFileNode) = 0;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }

    if (!FspFileNodeTryAcquireExclusive(FileNode, Main))
        FspIopRetryCompleteIrp(Irp, Response, &Result);

    if (!FspFileNodeTrySetFileInfo(FileNode, FileObject, &Response->Rsp.QueryInformation.FileInfo,
        (ULONG)(UINT_PTR)FspIopRequestContext(Request, RequestInfoChangeNumber)))
    {
        FspFileNodeGetFileInfo(FileNode, &FileInfoBuf);
        FileInfo = &FileInfoBuf;
    }
    else
        FileInfo = &Response->Rsp.QueryInformation.FileInfo;

    FspFileNodeRelease(FileNode, Main);

    switch (FileInformationClass)
    {
    case FileAllInformation:
        Result = FspFsvolQueryAllInformation(FileObject, &Buffer, BufferEnd, FileInfo);
        break;
    case FileAttributeTagInformation:
        Result = FspFsvolQueryAttributeTagInformation(FileObject, &Buffer, BufferEnd, FileInfo);
        break;
    case FileBasicInformation:
        Result = FspFsvolQueryBasicInformation(FileObject, &Buffer, BufferEnd, FileInfo);
        break;
    case FileNetworkOpenInformation:
        Result = FspFsvolQueryNetworkOpenInformation(FileObject, &Buffer, BufferEnd, FileInfo);
        break;
    case FileStandardInformation:
        Result = FspFsvolQueryStandardInformation(FileObject, &Buffer, BufferEnd, FileInfo);
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    Irp->IoStatus.Information = (UINT_PTR)((PUINT8)Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);

    FSP_LEAVE_IOC("%s, FileObject=%p",
        FileInformationClassSym(IrpSp->Parameters.QueryFile.FileInformationClass),
        IrpSp->FileObject);
}

#if 0
static NTSTATUS FspFsvolSetAllocationInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    if (0 != Request)
    {
        if (sizeof(FILE_ALLOCATION_INFORMATION) > Length)
            return STATUS_INVALID_PARAMETER;

        PFILE_ALLOCATION_INFORMATION Info = (PFILE_ALLOCATION_INFORMATION)Buffer;
        FSP_FILE_NODE *FileNode = FileObject->FsContext;
        BOOLEAN Success;

        Request->Req.SetInformation.Info.Allocation.AllocationSize = Info->AllocationSize.QuadPart;

        FspFileNodeAcquireExclusive(FileNode, Full);

        Success = MmCanFileBeTruncated(FileObject->SectionObjectPointer, &Info->AllocationSize);
        if (!Success)
        {
            FspFileNodeRelease(FileNode, Full);

            return STATUS_USER_MAPPED_FILE;
        }

        FspIopRequestContext(Request, RequestFileNode) = FileNode;
        FspIopRequestContext(Request, RequestAcquireFlags) = (PVOID)FspFileNodeAcquireFull;

        return FSP_STATUS_IOQ_POST;
    }
    else
    {
        FspIopRequestContext(Request, RequestFileNode) = 0;
        FspIopRequestContext(Request, RequestAcquireFlags) = 0;

        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FileNode->Header.AllocationSize.QuadPart = Response->Rsp.SetInformation.FileInfo.AllocationSize;
        FileNode->Header.FileSize.QuadPart = Response->Rsp.SetInformation.FileInfo.FileSize;
        FileNode->CcStatus = FspCcSetFileSizes(
            FileObject, (PCC_FILE_SIZES)&FileNode->Header.AllocationSize);

        FspFileNodeRelease(FileNode, Full);

        return STATUS_SUCCESS;
    }
}

static NTSTATUS FspFsvolSetBasicInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolSetDispositionInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolSetEndOfFileInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length, BOOLEAN AdvanceOnly,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolSetPositionInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolSetRenameInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}
#endif

static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

#if 0
    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    BOOLEAN FileNameRequired = 0 != FsvolDeviceExtension->VolumeParams.FileNameRequired;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.QueryFile.Length;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    Result = FspIopCreateRequestEx(Irp, FileNameRequired ? &FileNode->FileName : 0, 0,
        FspFsvolInformationRequestFini, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactSetInformationKind;
    Request->Req.SetInformation.UserContext = FileNode->UserContext;
    Request->Req.SetInformation.UserContext2 = FileDesc->UserContext2;
    Request->Req.SetInformation.FileInformationClass = FileInformationClass;

    switch (FileInformationClass)
    {
    case FileAllocationInformation:
        Result = FspFsvolSetAllocationInformation(FileObject, Buffer, Length, Request, 0);
        break;
    case FileBasicInformation:
        Result = FspFsvolSetBasicInformation(FileObject, Buffer, Length, Request, 0);
        break;
    case FileDispositionInformation:
        Result = FspFsvolSetDispositionInformation(FileObject, Buffer, Length, Request, 0);
        break;
    case FileEndOfFileInformation:
        Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length,
            IrpSp->Parameters.SetFile.AdvanceOnly, Request, 0);
        break;
    case FileLinkInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no hard link support */
        break;
    case FilePositionInformation:
        Result = FspFsvolSetPositionInformation(FileObject, Buffer, Length, Request, 0);
        break;
    case FileRenameInformation:
        Result = FspFsvolSetRenameInformation(FileObject, Buffer, Length, Request, 0);
        break;
    case FileValidDataLengthInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no ValidDataLength support */
        break;
    default:
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    return Result;
#else
    return STATUS_INVALID_DEVICE_REQUEST;
#endif
}

NTSTATUS FspFsvolSetInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

#if 0
    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = Response->IoStatus.Information;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.QueryFile.Length;

    FspFileNodeSetFileInfo(FileNode, &Response->Rsp.SetInformation.FileInfo);

    switch (FileInformationClass)
    {
    case FileAllocationInformation:
        Result = FspFsvolSetAllocationInformation(FileObject, Buffer, Length, 0, Response);
        break;
    case FileBasicInformation:
        Result = FspFsvolSetBasicInformation(FileObject, Buffer, Length, 0, Response);
        break;
    case FileDispositionInformation:
        Result = FspFsvolSetDispositionInformation(FileObject, Buffer, Length, 0, Response);
        break;
    case FileEndOfFileInformation:
        Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length,
            IrpSp->Parameters.SetFile.AdvanceOnly, 0, Response);
        break;
    case FileLinkInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no hard link support */
        break;
    case FilePositionInformation:
        Result = FspFsvolSetPositionInformation(FileObject, Buffer, Length, 0, Response);
        break;
    case FileRenameInformation:
        Result = FspFsvolSetRenameInformation(FileObject, Buffer, Length, 0, Response);
        break;
    case FileValidDataLengthInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no ValidDataLength support */
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    ASSERT(FSP_STATUS_IOQ_POST != Result);

    Irp->IoStatus.Information = 0;
#endif

    FSP_LEAVE_IOC("%s, FileObject=%p",
        FileInformationClassSym(IrpSp->Parameters.SetFile.FileInformationClass),
        IrpSp->FileObject);
}

static VOID FspFsvolInformationRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[3])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
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
