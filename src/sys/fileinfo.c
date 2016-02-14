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
static FSP_IOP_REQUEST_FINI FspFsvolQueryInformationRequestFini;
static NTSTATUS FspFsvolSetAllocationInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetBasicInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetEndOfFileInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length, BOOLEAN AdvanceOnly,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetPositionInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length);
static NTSTATUS FspFsvolSetDispositionInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolSetDispositionInformationSuccess(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetRenameInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolSetRenameInformationSuccess(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);
static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolSetInformationComplete;
static FSP_IOP_REQUEST_FINI FspFsvolSetInformationRequestFini;
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
#pragma alloc_text(PAGE, FspFsvolQueryInformationRequestFini)
#pragma alloc_text(PAGE, FspFsvolSetAllocationInformation)
#pragma alloc_text(PAGE, FspFsvolSetBasicInformation)
#pragma alloc_text(PAGE, FspFsvolSetEndOfFileInformation)
#pragma alloc_text(PAGE, FspFsvolSetPositionInformation)
#pragma alloc_text(PAGE, FspFsvolSetDispositionInformation)
#pragma alloc_text(PAGE, FspFsvolSetDispositionInformationSuccess)
#pragma alloc_text(PAGE, FspFsvolSetRenameInformation)
#pragma alloc_text(PAGE, FspFsvolSetRenameInformationSuccess)
#pragma alloc_text(PAGE, FspFsvolSetInformation)
#pragma alloc_text(PAGE, FspFsvolSetInformationComplete)
#pragma alloc_text(PAGE, FspFsvolSetInformationRequestFini)
#pragma alloc_text(PAGE, FspQueryInformation)
#pragma alloc_text(PAGE, FspSetInformation)
#endif

enum
{
    /* QueryInformation */
    RequestFileNode                     = 0,
    RequestInfoChangeNumber             = 1,
    RequestAllInformationResult         = 2,
    RequestAllInformationBuffer         = 3,

    /* SetInformation */
    //RequestFileNode                   = 0,
    RequestDeviceObject                 = 1,
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

        *PBuffer = (PVOID)&Info->NameInformation;
        return FspFsvolQueryNameInformation(FileObject, PBuffer, BufferEnd);
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

    return STATUS_SUCCESS;
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
    PDEVICE_OBJECT FsvolDeviceObject = FileNode->FsvolDeviceObject;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);

    if ((PVOID)(Info + 1) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    FspFsvolDeviceFileRenameAcquireShared(FsvolDeviceObject);

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

    FspFsvolDeviceFileRenameRelease(FsvolDeviceObject);

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
    NTSTATUS AllInformationResult = STATUS_INVALID_PARAMETER;
    PVOID AllInformationBuffer = 0;

    ASSERT(FileNode == FileDesc->FileNode);

    switch (FileInformationClass)
    {
    case FileAllInformation:
        Result = FspFsvolQueryAllInformation(FileObject, &Buffer, BufferEnd, 0);
        AllInformationResult = Result;
        AllInformationBuffer = Buffer;
        if (STATUS_BUFFER_OVERFLOW == Result)
            Result = STATUS_SUCCESS;
        Buffer = Irp->AssociatedIrp.SystemBuffer;
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
        Irp->IoStatus.Information = (UINT_PTR)((PUINT8)Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);
        return Result;
    case FileNameInformation:
    case FileNormalizedNameInformation:
        Result = FspFsvolQueryNameInformation(FileObject, &Buffer, BufferEnd);
        Irp->IoStatus.Information = (UINT_PTR)((PUINT8)Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);
        return Result;
    case FileNetworkOpenInformation:
        Result = FspFsvolQueryNetworkOpenInformation(FileObject, &Buffer, BufferEnd, 0);
        break;
    case FilePositionInformation:
        Result = FspFsvolQueryPositionInformation(FileObject, &Buffer, BufferEnd);
        Irp->IoStatus.Information = (UINT_PTR)((PUINT8)Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);
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
            ASSERT(NT_SUCCESS(Result));
            Result = AllInformationResult;
            Buffer = AllInformationBuffer;
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

    FSP_FSCTL_TRANSACT_REQ *Request;

    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolQueryInformationRequestFini, &Request);
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
    FspIopRequestContext(Request, RequestAllInformationResult) = (PVOID)(ULONG)AllInformationResult;
    FspIopRequestContext(Request, RequestAllInformationBuffer) = AllInformationBuffer;

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
    BOOLEAN Success;

    if (0 != FspIopRequestContext(Request, RequestFileNode))
    {
        FspIopRequestContext(Request, RequestInfoChangeNumber) = (PVOID)FileNode->InfoChangeNumber;
        FspIopRequestContext(Request, RequestFileNode) = 0;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }

    Success = DEBUGRANDTEST(90, TRUE) && FspFileNodeTryAcquireExclusive(FileNode, Main);
    if (!Success)
    {
        FspIopRetryCompleteIrp(Irp, Response, &Result);

        return Result;
    }

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
        ASSERT(NT_SUCCESS(Result));
        Result = (NTSTATUS)(UINT_PTR)FspIopRequestContext(Request, RequestAllInformationResult);
        Buffer = FspIopRequestContext(Request, RequestAllInformationBuffer);
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

static VOID FspFsvolQueryInformationRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

static NTSTATUS FspFsvolSetAllocationInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    if (0 == Request)
    {
        if (sizeof(FILE_ALLOCATION_INFORMATION) > Length)
            return STATUS_INVALID_PARAMETER;
    }
    else if (0 == Response)
    {
        PFILE_ALLOCATION_INFORMATION Info = (PFILE_ALLOCATION_INFORMATION)Buffer;
        FSP_FILE_NODE *FileNode = FileObject->FsContext;
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
            FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
        UINT64 AllocationSize = Info->AllocationSize.QuadPart;
        UINT64 AllocationUnit;
        BOOLEAN Success;

        AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
            FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;
        AllocationSize = (AllocationSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
        Request->Req.SetInformation.Info.Allocation.AllocationSize = AllocationSize;

        Success = MmCanFileBeTruncated(FileObject->SectionObjectPointer, &Info->AllocationSize);
        if (!Success)
            return STATUS_USER_MAPPED_FILE;
    }
    else
    {
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.SetInformation.FileInfo);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetBasicInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    if (0 == Request)
    {
        if (sizeof(FILE_BASIC_INFORMATION) > Length)
            return STATUS_INVALID_PARAMETER;
    }
    else if (0 == Response)
    {
        PFILE_BASIC_INFORMATION Info = (PFILE_BASIC_INFORMATION)Buffer;
        FSP_FILE_NODE *FileNode = FileObject->FsContext;
        UINT32 FileAttributes = Info->FileAttributes;

        if (0 == FileAttributes)
            FileAttributes = ((UINT32)-1);
        else
        {
            ClearFlag(FileAttributes, FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY);
            if (FileNode->IsDirectory)
                SetFlag(FileAttributes, FILE_ATTRIBUTE_DIRECTORY);
        }

        Request->Req.SetInformation.Info.Basic.FileAttributes = FileAttributes;
        Request->Req.SetInformation.Info.Basic.CreationTime = Info->CreationTime.QuadPart;
        Request->Req.SetInformation.Info.Basic.LastAccessTime = Info->LastAccessTime.QuadPart;
        Request->Req.SetInformation.Info.Basic.LastWriteTime = Info->LastWriteTime.QuadPart;
    }
    else
    {
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.SetInformation.FileInfo);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetEndOfFileInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length, BOOLEAN AdvanceOnly,
    FSP_FSCTL_TRANSACT_REQ *Request, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    if (0 == Request)
    {
        if (sizeof(FILE_END_OF_FILE_INFORMATION) > Length)
            return STATUS_INVALID_PARAMETER;
    }
    else if (0 == Response)
    {
        PFILE_END_OF_FILE_INFORMATION Info = (PFILE_END_OF_FILE_INFORMATION)Buffer;
        BOOLEAN Success;

        Request->Req.SetInformation.Info.EndOfFile.FileSize = Info->EndOfFile.QuadPart;
        Request->Req.SetInformation.Info.EndOfFile.AdvanceOnly = AdvanceOnly;

        // !!!: REVISIT after better understanding relationship between AllocationSize and FileSize
        Success = MmCanFileBeTruncated(FileObject->SectionObjectPointer, &Info->EndOfFile);
        if (!Success)
            return STATUS_USER_MAPPED_FILE;
    }
    else
    {
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.SetInformation.FileInfo);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetPositionInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length)
{
    PAGED_CODE();

    if (sizeof(FILE_POSITION_INFORMATION) > Length)
        return STATUS_INVALID_PARAMETER;

    PFILE_POSITION_INFORMATION Info = (PFILE_POSITION_INFORMATION)Buffer;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    FspFileNodeAcquireExclusive(FileNode, Main);

    FileObject->CurrentByteOffset = Info->CurrentByteOffset;

    FspFileNodeRelease(FileNode, Main);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetDispositionInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFILE_DISPOSITION_INFORMATION Info = (PFILE_DISPOSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.SetFile.Length;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FSCTL_TRANSACT_REQ *Request;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    if (sizeof(FILE_DISPOSITION_INFORMATION) > Length)
        return STATUS_INVALID_PARAMETER;
    if (FileNode->IsRootDirectory)
        /* cannot delete root directory */
        return STATUS_CANNOT_DELETE;

    FspFsvolDeviceFileRenameAcquireShared(FsvolDeviceObject);
    FspFileNodeAcquireExclusive(FileNode, Full);

    if (Info->DeleteFile)
    {
        /* make sure no process is mapping the file as an image */
        Success = MmFlushImageSection(FileObject->SectionObjectPointer, MmFlushForDelete);
        if (!Success)
        {
            Result = STATUS_CANNOT_DELETE;
            goto unlock_exit;
        }
    }

    Result = FspIopCreateRequestEx(Irp, &FileNode->FileName, 0,
        FspFsvolSetInformationRequestFini, &Request);
    if (!NT_SUCCESS(Result))
        goto unlock_exit;

    Request->Kind = FspFsctlTransactSetInformationKind;
    Request->Req.SetInformation.UserContext = FileNode->UserContext;
    Request->Req.SetInformation.UserContext2 = FileDesc->UserContext2;
    Request->Req.SetInformation.FileInformationClass = FileDispositionInformation;
    Request->Req.SetInformation.Info.Disposition.Delete = Info->DeleteFile;

    FspFsvolDeviceFileRenameSetOwner(FsvolDeviceObject, Request);
    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;
    FspIopRequestContext(Request, RequestDeviceObject) = FsvolDeviceObject;

    return FSP_STATUS_IOQ_POST;

unlock_exit:
    FspFileNodeRelease(FileNode, Full);
    FspFsvolDeviceFileRenameRelease(FsvolDeviceObject);

    return Result;
}

static NTSTATUS FspFsvolSetDispositionInformationSuccess(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->DeviceObject;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFILE_DISPOSITION_INFORMATION Info = (PFILE_DISPOSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    FileNode->DeletePending = Info->DeleteFile;
    FileObject->DeletePending = Info->DeleteFile;

    FspIopRequestContext(Request, RequestFileNode) = 0;
    FspIopRequestContext(Request, RequestDeviceObject) = 0;
    FspFileNodeReleaseOwner(FileNode, Full, Request);
    FspFsvolDeviceFileRenameReleaseOwner(FsvolDeviceObject, Request);

    Irp->IoStatus.Information = 0;

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetRenameInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFILE_OBJECT TargetFileObject = IrpSp->Parameters.SetFile.FileObject;
    PFILE_RENAME_INFORMATION Info = (PFILE_RENAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.SetFile.Length;
    BOOLEAN ReplaceIfExists = IrpSp->Parameters.SetFile.ReplaceIfExists;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FILE_NODE *TargetFileNode = 0 != TargetFileObject ?
        TargetFileObject->FsContext : 0;
    FSP_FSCTL_TRANSACT_REQ *Request;
    UNICODE_STRING Remain, Suffix;
    UNICODE_STRING NewFileName;
    PUINT8 NewFileNameBuffer;
    BOOLEAN AppendBackslash;

    ASSERT(FileNode == FileDesc->FileNode);

    if (sizeof(FILE_RENAME_INFORMATION) > Length)
        return STATUS_INVALID_PARAMETER;
    if (sizeof(WCHAR) > Info->FileNameLength)
        return STATUS_INVALID_PARAMETER;
    if (FileNode->IsRootDirectory)
        /* cannot rename root directory */
        return STATUS_INVALID_PARAMETER;
    if (!FspUnicodePathIsValid(&FileNode->FileName, FALSE))
        /* cannot rename streams (WinFsp limitation) */
        return STATUS_INVALID_PARAMETER;

    if (0 != TargetFileNode)
    {
        if (!FspFileNodeIsValid(TargetFileNode))
            return STATUS_INVALID_PARAMETER;

        ASSERT(TargetFileNode->IsDirectory);
    }

    FspFsvolDeviceFileRenameAcquireExclusive(FsvolDeviceObject);
    FspFileNodeAcquireExclusive(FileNode, Full);

    if (0 != TargetFileNode)
        Remain = TargetFileNode->FileName;
    else
        FspUnicodePathSuffix(&FileNode->FileName, &Remain, &Suffix);

    Suffix.Length = Suffix.MaximumLength = (USHORT)Info->FileNameLength;
    Suffix.Buffer = Info->FileName;
    if (L'\\' == Suffix.Buffer[0])
        FspUnicodePathSuffix(&Suffix, &NewFileName, &Suffix);

    if (!FspUnicodePathIsValid(&Remain, FALSE) || !FspUnicodePathIsValid(&Suffix, FALSE))
    {
        /* cannot rename streams (WinFsp limitation) */
        Result = STATUS_INVALID_PARAMETER;
        goto unlock_exit;
    }

    AppendBackslash = sizeof(WCHAR) < Remain.Length;
    NewFileName.Length = NewFileName.MaximumLength =
        Remain.Length + AppendBackslash * sizeof(WCHAR) + Suffix.Length;

    Result = FspIopCreateRequestEx(Irp, &FileNode->FileName,
        NewFileName.Length + sizeof(WCHAR),
        FspFsvolSetInformationRequestFini, &Request);
    if (!NT_SUCCESS(Result))
        goto unlock_exit;

    NewFileNameBuffer = Request->Buffer + Request->FileName.Size;
    NewFileName.Buffer = (PVOID)NewFileNameBuffer;

    RtlCopyMemory(NewFileNameBuffer, Remain.Buffer, Remain.Length);
    *(PWSTR)(NewFileNameBuffer + Remain.Length) = L'\\';
    RtlCopyMemory(NewFileNameBuffer + Remain.Length + AppendBackslash * sizeof(WCHAR),
        Suffix.Buffer, Suffix.Length);
    *(PWSTR)(NewFileNameBuffer + NewFileName.Length) = L'\0';

    Request->Kind = FspFsctlTransactSetInformationKind;
    Request->Req.SetInformation.UserContext = FileNode->UserContext;
    Request->Req.SetInformation.UserContext2 = FileDesc->UserContext2;
    Request->Req.SetInformation.FileInformationClass = FileRenameInformation;
    Request->Req.SetInformation.Info.Rename.NewFileName.Offset = Request->FileName.Size;
    Request->Req.SetInformation.Info.Rename.NewFileName.Size = NewFileName.Length + sizeof(WCHAR);
    Request->Req.SetInformation.Info.Rename.ReplaceIfExists = ReplaceIfExists;

    FspFsvolDeviceFileRenameSetOwner(FsvolDeviceObject, Request);
    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;
    FspIopRequestContext(Request, RequestDeviceObject) = FsvolDeviceObject;

    /*
     * Special rules for renaming open files:
     * -   A file cannot be renamed if it has any open handles,
     *     unless it is only open because of a batch opportunistic lock (oplock)
     *     and the batch oplock can be broken immediately.
     * -   A file cannot be renamed if a file with the same name exists
     *     and has open handles (except in the batch-oplock case described earlier).
     * -   A directory cannot be renamed if it or any of its subdirectories contains a file
     *     that has open handles (except in the batch-oplock case described earlier).
     */

    Result = STATUS_SUCCESS;
    FspFsvolDeviceLockContextTable(FsvolDeviceObject);
    if (1 < FileNode->OpenCount ||
        (FileNode->IsDirectory &&
            0 != FspFsvolDeviceLookupDescendantContextByName(FsvolDeviceObject, &FileNode->FileName, TRUE)) ||
        0 != FspFsvolDeviceLookupDescendantContextByName(FsvolDeviceObject, &NewFileName, FALSE))
        Result = STATUS_ACCESS_DENIED;
    FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);
    if (!NT_SUCCESS(Result))
        return Result;

    return FSP_STATUS_IOQ_POST;

unlock_exit:
    FspFileNodeRelease(FileNode, Full);
    FspFsvolDeviceFileRenameRelease(FsvolDeviceObject);

    return Result;
}

static NTSTATUS FspFsvolSetRenameInformationSuccess(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PDEVICE_OBJECT FsvolDeviceObject = IrpSp->DeviceObject;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    UNICODE_STRING NewFileName;

    NewFileName.Length = NewFileName.MaximumLength =
        Request->Req.SetInformation.Info.Rename.NewFileName.Size - sizeof(WCHAR);
    NewFileName.Buffer = FspAllocMustSucceed(NewFileName.Length);
    RtlCopyMemory(NewFileName.Buffer, Request->Buffer + Request->FileName.Size, NewFileName.Length);

    FspFileNodeRename(FileNode, &NewFileName);

    FspIopRequestContext(Request, RequestFileNode) = 0;
    FspIopRequestContext(Request, RequestDeviceObject) = 0;
    FspFileNodeReleaseOwner(FileNode, Full, Request);
    FspFsvolDeviceFileRenameReleaseOwner(FsvolDeviceObject, Request);

    Irp->IoStatus.Information = 0;

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;

    /* special case FileDispositionInformation/FileRenameInformation */
    if (FileDispositionInformation == FileInformationClass)
        return FspFsvolSetDispositionInformation(FsvolDeviceObject, Irp, IrpSp);
    if (FileRenameInformation == FileInformationClass)
        return FspFsvolSetRenameInformation(FsvolDeviceObject, Irp, IrpSp);

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.SetFile.Length;

    switch (FileInformationClass)
    {
    case FileAllocationInformation:
        Result = FspFsvolSetAllocationInformation(FileObject, Buffer, Length, 0, 0);
        break;
    case FileBasicInformation:
        Result = FspFsvolSetBasicInformation(FileObject, Buffer, Length, 0, 0);
        break;
    case FileEndOfFileInformation:
        Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length,
            IrpSp->Parameters.SetFile.AdvanceOnly, 0, 0);
        break;
    case FileLinkInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no hard link support */
        return Result;
    case FilePositionInformation:
        Result = FspFsvolSetPositionInformation(FileObject, Buffer, Length);
        return Result;
    case FileValidDataLengthInformation:
        Result = STATUS_INVALID_PARAMETER;  /* no ValidDataLength support */
        return Result;
    default:
        Result = STATUS_INVALID_PARAMETER;
        return Result;
    }

    if (!NT_SUCCESS(Result))
        return Result;

    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolSetInformationRequestFini, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactSetInformationKind;
    Request->Req.SetInformation.UserContext = FileNode->UserContext;
    Request->Req.SetInformation.UserContext2 = FileDesc->UserContext2;
    Request->Req.SetInformation.FileInformationClass = FileInformationClass;

    FspFileNodeAcquireExclusive(FileNode, Full);
    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    switch (FileInformationClass)
    {
    case FileAllocationInformation:
        Result = FspFsvolSetAllocationInformation(FileObject, Buffer, Length, Request, 0);
        break;
    case FileBasicInformation:
        Result = FspFsvolSetBasicInformation(FileObject, Buffer, Length, Request, 0);
        break;
    case FileEndOfFileInformation:
        Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length,
            IrpSp->Parameters.SetFile.AdvanceOnly, Request, 0);
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

NTSTATUS FspFsvolSetInformationComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.SetFile.FileInformationClass;

    /* special case FileDispositionInformation/FileRenameInformation */
    if (FileDispositionInformation == FileInformationClass)
        FSP_RETURN(Result = FspFsvolSetDispositionInformationSuccess(Irp, Response));
    if (FileRenameInformation == FileInformationClass)
        FSP_RETURN(Result = FspFsvolSetRenameInformationSuccess(Irp, Response));

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.SetFile.Length;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    switch (FileInformationClass)
    {
    case FileAllocationInformation:
        Result = FspFsvolSetAllocationInformation(FileObject, Buffer, Length, Request, Response);
        break;
    case FileBasicInformation:
        Result = FspFsvolSetBasicInformation(FileObject, Buffer, Length, Request, Response);
        break;
    case FileEndOfFileInformation:
        Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length,
            IrpSp->Parameters.SetFile.AdvanceOnly, Request, Response);
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    FspIopRequestContext(Request, RequestFileNode) = 0;
    FspFileNodeReleaseOwner(FileNode, Full, Request);

    Irp->IoStatus.Information = 0;

    FSP_LEAVE_IOC("%s, FileObject=%p",
        FileInformationClassSym(IrpSp->Parameters.SetFile.FileInformationClass),
        IrpSp->FileObject);
}

static VOID FspFsvolSetInformationRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];
    PDEVICE_OBJECT FsvolDeviceObject = Context[RequestDeviceObject];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);

    if (0 != FsvolDeviceObject)
        FspFsvolDeviceFileRenameReleaseOwner(FsvolDeviceObject, Request);
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
