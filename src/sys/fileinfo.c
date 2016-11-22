/**
 * @file sys/fileinfo.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
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
static NTSTATUS FspFsvolQueryEaInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd);
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
static NTSTATUS FspFsvolQueryStreamInformationCopy(
    FSP_FSCTL_STREAM_INFO *StreamInfoBuffer, ULONG StreamInfoBufferSize,
    PVOID DestBuf, PULONG PDestLen);
static NTSTATUS FspFsvolQueryStreamInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolQueryStreamInformationSuccess(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);
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
    PVOID Buffer, ULONG Length,
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
FSP_IOPREP_DISPATCH FspFsvolSetInformationPrepare;
FSP_IOCMPL_DISPATCH FspFsvolSetInformationComplete;
static FSP_IOP_REQUEST_FINI FspFsvolSetInformationRequestFini;
FSP_DRIVER_DISPATCH FspQueryInformation;
FSP_DRIVER_DISPATCH FspSetInformation;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryAllInformation)
#pragma alloc_text(PAGE, FspFsvolQueryAttributeTagInformation)
#pragma alloc_text(PAGE, FspFsvolQueryBasicInformation)
#pragma alloc_text(PAGE, FspFsvolQueryEaInformation)
#pragma alloc_text(PAGE, FspFsvolQueryInternalInformation)
#pragma alloc_text(PAGE, FspFsvolQueryNameInformation)
#pragma alloc_text(PAGE, FspFsvolQueryNetworkOpenInformation)
#pragma alloc_text(PAGE, FspFsvolQueryPositionInformation)
#pragma alloc_text(PAGE, FspFsvolQueryStandardInformation)
#pragma alloc_text(PAGE, FspFsvolQueryStreamInformationCopy)
#pragma alloc_text(PAGE, FspFsvolQueryStreamInformation)
#pragma alloc_text(PAGE, FspFsvolQueryStreamInformationSuccess)
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
#pragma alloc_text(PAGE, FspFsvolSetInformationPrepare)
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
    /* Rename */
    RequestAccessToken                  = 2,
    RequestProcess                      = 3,
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

static NTSTATUS FspFsvolQueryEaInformation(PFILE_OBJECT FileObject,
    PVOID *PBuffer, PVOID BufferEnd)
{
    PAGED_CODE();

    PFILE_EA_INFORMATION Info = (PFILE_EA_INFORMATION)*PBuffer;

    if ((PVOID)(Info + 1) > BufferEnd)
        return STATUS_BUFFER_TOO_SMALL;

    /*
     * No EA support currently. We must nevertheless respond to this query
     * or SRV2 gets unhappy. Just tell them that we have 0 EA's.
     */
    Info->EaSize = 0;

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

    FspFileNodeAcquireShared(FileNode, Main);

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

    FspFileNodeRelease(FileNode, Main);

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

static NTSTATUS FspFsvolQueryStreamInformationCopy(
    FSP_FSCTL_STREAM_INFO *StreamInfo, ULONG StreamInfoSize,
    PVOID DestBuf, PULONG PDestLen)
{
#define STREAM_TYPE                     ":$DATA"
#define STREAM_TYPE_LENGTH              (sizeof L"" STREAM_TYPE - sizeof(WCHAR))
#define STREAM_EXTRA_LENGTH             (sizeof L":" STREAM_TYPE - sizeof(WCHAR))
#define FILL_INFO()\
    do\
    {\
        FILE_STREAM_INFORMATION InfoStruct = { 0 }, *Info = &InfoStruct;\
        Info->NextEntryOffset = 0;\
        Info->StreamNameLength = StreamNameLength;\
        Info->StreamSize.QuadPart = StreamInfo->StreamSize;\
        Info->StreamAllocationSize.QuadPart = StreamInfo->StreamAllocationSize;\
        Info = DestBuf;\
        RtlCopyMemory(Info, &InfoStruct, FIELD_OFFSET(FILE_STREAM_INFORMATION, StreamName));\
        {\
            PWSTR StreamName = Info->StreamName;\
            ULONG Length[3];\
            Length[0] = 0 < CopyLength ? sizeof(WCHAR) : 0;\
            Length[1] = Info->StreamNameLength - STREAM_EXTRA_LENGTH < CopyLength - Length[0] ?\
                Info->StreamNameLength - STREAM_EXTRA_LENGTH : CopyLength - Length[0];\
            Length[2] = CopyLength - Length[0] - Length[1];\
            ASSERT(\
                sizeof(WCHAR) >= Length[0] &&\
                Info->StreamNameLength - STREAM_EXTRA_LENGTH >= Length[1] &&\
                STREAM_TYPE_LENGTH >= Length[2]);\
            RtlCopyMemory(StreamName, L":", Length[0]);\
            StreamName += Length[0] / sizeof(WCHAR);\
            RtlCopyMemory(StreamName, StreamInfo->StreamNameBuf, Length[1]);\
            StreamName += Length[1] / sizeof(WCHAR);\
            RtlCopyMemory(StreamName, L"" STREAM_TYPE, Length[2]);\
        }\
    } while (0,0)

    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    PUINT8 StreamInfoEnd = (PUINT8)StreamInfo + StreamInfoSize;
    PUINT8 DestBufBgn = (PUINT8)DestBuf;
    PUINT8 DestBufEnd = (PUINT8)DestBuf + *PDestLen;
    PVOID PrevDestBuf = 0;
    ULONG StreamNameLength, CopyLength;
    ULONG BaseInfoLen = FIELD_OFFSET(FILE_STREAM_INFORMATION, StreamName);

    *PDestLen = 0;

    for (;
        NT_SUCCESS(Result) && (PUINT8)StreamInfo + sizeof(StreamInfo->Size) <= StreamInfoEnd;
        StreamInfo = (PVOID)((PUINT8)StreamInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(StreamInfoSize)))
    {
        StreamInfoSize = StreamInfo->Size;

        if (sizeof(FSP_FSCTL_STREAM_INFO) > StreamInfoSize)
            break;

        StreamNameLength = StreamInfoSize - sizeof(FSP_FSCTL_STREAM_INFO) + STREAM_EXTRA_LENGTH;

        /* CopyLength is the same as StreamNameLength except on STATUS_BUFFER_OVERFLOW */
        CopyLength = StreamNameLength;

        /* do we have enough space for this stream info? */
        if ((PUINT8)DestBuf + BaseInfoLen + CopyLength > DestBufEnd)
        {
            /* can we even copy the base info? */
            if ((PUINT8)DestBuf + BaseInfoLen > DestBufEnd)
            {
                if (0 == *PDestLen)
                    /* if we haven't copied anything yet, buffer is too small */
                    return STATUS_BUFFER_TOO_SMALL;
                else
                    /* *PDestLen contains bytes copied so far */
                    return STATUS_BUFFER_OVERFLOW;
            }

            /* copy as much of the stream name as we can and return STATUS_BUFFER_OVERFLOW */
            CopyLength = (ULONG)(DestBufEnd - ((PUINT8)DestBuf + BaseInfoLen));
            Result = STATUS_BUFFER_OVERFLOW;
        }

        /* fill in NextEntryOffset */
        if (0 != PrevDestBuf)
            *(PULONG)PrevDestBuf = (ULONG)((PUINT8)DestBuf - (PUINT8)PrevDestBuf);
        PrevDestBuf = DestBuf;

        /* update bytes copied */
        *PDestLen = (ULONG)((PUINT8)DestBuf + BaseInfoLen + CopyLength - DestBufBgn);

        FILL_INFO();

        /* advance DestBuf; make sure to align properly! */
        DestBuf = (PVOID)((PUINT8)DestBuf +
            FSP_FSCTL_ALIGN_UP(BaseInfoLen + CopyLength, sizeof(LONGLONG)));
    }

    /* our code flow should allow only these two status codes here */
    ASSERT(STATUS_SUCCESS == Result || STATUS_BUFFER_OVERFLOW == Result);

    return Result;

#undef FILL_INFO
#undef STREAM_EXTRA_LENGTH
#undef STREAM_TYPE_LENGTH
#undef STREAM_TYPE
}

static NTSTATUS FspFsvolQueryStreamInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    if (!FspFsvolDeviceExtension(FsvolDeviceObject)->VolumeParams.NamedStreams)
        return STATUS_INVALID_PARAMETER;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.QueryFile.Length;
    PVOID StreamInfoBuffer;
    ULONG StreamInfoBufferSize;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeAcquireShared(FileNode, Main);
    if (FspFileNodeReferenceStreamInfo(FileNode, &StreamInfoBuffer, &StreamInfoBufferSize))
    {
        FspFileNodeRelease(FileNode, Main);

        Result = FspFsvolQueryStreamInformationCopy(
            StreamInfoBuffer, StreamInfoBufferSize, Buffer, &Length);

        FspFileNodeDereferenceStreamInfo(StreamInfoBuffer);

        Irp->IoStatus.Information = Length;
        return Result;
    }

    FspFileNodeAcquireShared(FileNode, Pgio);

    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolQueryInformationRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactQueryStreamInformationKind;
    Request->Req.QueryStreamInformation.UserContext = FileNode->UserContext;
    Request->Req.QueryStreamInformation.UserContext2 = FileDesc->UserContext2;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

static NTSTATUS FspFsvolQueryStreamInformationSuccess(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    if (!FspFsvolDeviceExtension(IoGetCurrentIrpStackLocation(Irp)->DeviceObject)->
        VolumeParams.NamedStreams)
        return STATUS_INVALID_PARAMETER;

    NTSTATUS Result;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.QueryFile.Length;
    PVOID StreamInfoBuffer = 0;
    ULONG StreamInfoBufferSize = 0;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    BOOLEAN Success;

    if (0 != FspIopRequestContext(Request, RequestFileNode))
    {
        /* check that the stream info we got back is valid */
        if (Response->Buffer + Response->Rsp.QueryStreamInformation.Buffer.Size >
            (PUINT8)Response + Response->Size)
        {
            Irp->IoStatus.Information = 0;
            return STATUS_INTERNAL_ERROR;
        }

        FspIopRequestContext(Request, RequestInfoChangeNumber) = (PVOID)
            FspFileNodeStreamInfoChangeNumber(FileNode);
        FspIopRequestContext(Request, RequestFileNode) = 0;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }

    Success = DEBUGTEST(90) && FspFileNodeTryAcquireExclusive(FileNode, Main);
    if (!Success)
    {
        FspIopRetryCompleteIrp(Irp, Response, &Result);
        return Result;
    }

    Success = !FspFileNodeTrySetStreamInfo(FileNode,
        Response->Buffer, Response->Rsp.QueryStreamInformation.Buffer.Size,
        (ULONG)(UINT_PTR)FspIopRequestContext(Request, RequestInfoChangeNumber));
    Success = Success &&
        FspFileNodeReferenceStreamInfo(FileNode, &StreamInfoBuffer, &StreamInfoBufferSize);
    FspFileNodeRelease(FileNode, Main);
    if (Success)
    {
        Result = FspFsvolQueryStreamInformationCopy(
            StreamInfoBuffer, StreamInfoBufferSize, Buffer, &Length);
        FspFileNodeDereferenceStreamInfo(StreamInfoBuffer);
    }
    else
    {
        StreamInfoBuffer = (PVOID)Response->Buffer;
        StreamInfoBufferSize = Response->Rsp.QueryStreamInformation.Buffer.Size;
        Result = FspFsvolQueryStreamInformationCopy(
            StreamInfoBuffer, StreamInfoBufferSize, Buffer, &Length);
    }

    Irp->IoStatus.Information = Length;

    return Result;
}

static NTSTATUS FspFsvolQueryInformation(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryFile.FileInformationClass;

    /* special case FileStreamInformation */
    if (FileStreamInformation == FileInformationClass)
        return FspFsvolQueryStreamInformation(FsvolDeviceObject, Irp, IrpSp);

    NTSTATUS Result;
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
        Result = FspFsvolQueryEaInformation(FileObject, &Buffer, BufferEnd);
        Irp->IoStatus.Information = (UINT_PTR)((PUINT8)Buffer - (PUINT8)Irp->AssociatedIrp.SystemBuffer);
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

    /* special case FileStreamInformation */
    if (FileStreamInformation == FileInformationClass)
        FSP_RETURN(Result = FspFsvolQueryStreamInformationSuccess(Irp, Response));

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
        FspIopRequestContext(Request, RequestInfoChangeNumber) = (PVOID)
            FspFileNodeFileInfoChangeNumber(FileNode);
        FspIopRequestContext(Request, RequestFileNode) = 0;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }

    Success = DEBUGTEST(90) && FspFileNodeTryAcquireExclusive(FileNode, Main);
    if (!Success)
    {
        FspIopRetryCompleteIrp(Irp, Response, &Result);
        FSP_RETURN();
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
        FSP_FSCTL_FILE_INFO FileInfo;
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
            FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
        LARGE_INTEGER AllocationSize = Info->AllocationSize;
        UINT64 AllocationUnit;
        BOOLEAN Success;

        AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
            FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;
        AllocationSize.QuadPart = (AllocationSize.QuadPart + AllocationUnit - 1) /
            AllocationUnit * AllocationUnit;
        Request->Req.SetInformation.Info.Allocation.AllocationSize = AllocationSize.QuadPart;

        /*
         * Even when the FileInfo is expired, this is the best guess for a file size
         * without asking the user-mode file system.
         */
        FspFileNodeGetFileInfo(FileNode, &FileInfo);

        /* are we truncating? */
        if ((UINT64)AllocationSize.QuadPart < FileInfo.FileSize)
        {
            /* see what the MM thinks about all this */
            Success = MmCanFileBeTruncated(FileObject->SectionObjectPointer, &AllocationSize);
            if (!Success)
                return STATUS_USER_MAPPED_FILE;
        }
    }
    else
    {
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.SetInformation.FileInfo);
        FileNode->TruncateOnClose = TRUE;

        /* mark the file object as modified */
        SetFlag(FileObject->Flags, FO_FILE_MODIFIED);

        FspFileNodeNotifyChange(FileNode, FILE_NOTIFY_CHANGE_SIZE, FILE_ACTION_MODIFIED);
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
        ULONG NotifyFilter = 0;

        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.SetInformation.FileInfo);

        if ((UINT32)-1 != Request->Req.SetInformation.Info.Basic.FileAttributes)
            NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
        if (0 != Request->Req.SetInformation.Info.Basic.CreationTime)
            NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
        if (0 != Request->Req.SetInformation.Info.Basic.LastAccessTime)
            NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
        if (0 != Request->Req.SetInformation.Info.Basic.LastWriteTime)
            NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
        FspFileNodeNotifyChange(FileNode, NotifyFilter, FILE_ACTION_MODIFIED);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolSetEndOfFileInformation(PFILE_OBJECT FileObject,
    PVOID Buffer, ULONG Length,
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
        FSP_FILE_NODE *FileNode = FileObject->FsContext;
        FSP_FSCTL_FILE_INFO FileInfo;
        BOOLEAN Success;

        Request->Req.SetInformation.Info.EndOfFile.FileSize = Info->EndOfFile.QuadPart;

        /*
         * Even when the FileInfo is expired, this is the best guess for a file size
         * without asking the user-mode file system.
         */
        FspFileNodeGetFileInfo(FileNode, &FileInfo);

        /* are we truncating? */
        if ((UINT64)Info->EndOfFile.QuadPart < FileInfo.FileSize)
        {
            /* see what the MM thinks about all this */
            Success = MmCanFileBeTruncated(FileObject->SectionObjectPointer, &Info->EndOfFile);
            if (!Success)
                return STATUS_USER_MAPPED_FILE;
        }
    }
    else
    {
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.SetInformation.FileInfo);
        FileNode->TruncateOnClose = TRUE;

        /* mark the file object as modified -- FastFat does this only for Allocation though! */
        SetFlag(FileObject->Flags, FO_FILE_MODIFIED);

        FspFileNodeNotifyChange(FileNode, FILE_NOTIFY_CHANGE_SIZE, FILE_ACTION_MODIFIED);
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

retry:
    FspFileNodeAcquireExclusive(FileNode, Full);

    if (Info->DeleteFile)
    {
        /*
         * Perform oplock check.
         *
         * It is ok to block our thread during receipt of the SetInformation IRP.
         * However we cannot acquire the FileNode exclusive and wait for oplock
         * breaks to complete, because oplock break processing acquires the FileNode
         * shared.
         *
         * Instead we initiate the oplock breaks and then check if any are in progress.
         * If that is the case we release the FileNode and wait for the oplock breaks
         * to complete. Once they are complete we retry the whole thing.
         */
        Result = FspFileNodeOplockCheckEx(FileNode, Irp,
            OPLOCK_FLAG_COMPLETE_IF_OPLOCKED);
        if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result ||
            DEBUGTEST_EX(NT_SUCCESS(Result), 10, FALSE))
        {
            FspFileNodeRelease(FileNode, Full);
            Result = FspFileNodeOplockCheck(FileNode, Irp);
            if (!NT_SUCCESS(Result))
                return Result;
            goto retry;
        }
        if (!NT_SUCCESS(Result))
            goto unlock_exit;

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

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;

unlock_exit:
    FspFileNodeRelease(FileNode, Full);

    return Result;
}

static NTSTATUS FspFsvolSetDispositionInformationSuccess(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    PFILE_DISPOSITION_INFORMATION Info = (PFILE_DISPOSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    FileNode->DeletePending = Info->DeleteFile;
    FileObject->DeletePending = Info->DeleteFile;

    /* fastfat does this, although it seems unnecessary */
#if 1
    if (FileNode->IsDirectory && Info->DeleteFile)
    {
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
            FspFsvolDeviceExtension(IrpSp->DeviceObject);
        FspNotifyDeletePending(
            FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList, FileNode);
    }
#endif

    FspIopRequestContext(Request, RequestFileNode) = 0;
    FspFileNodeReleaseOwner(FileNode, Full, Request);

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
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FILE_NODE *TargetFileNode = 0 != TargetFileObject ?
        TargetFileObject->FsContext : 0;
    FSP_FSCTL_TRANSACT_REQ *Request = 0;
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
    if (!FspFileNameIsValid(&FileNode->FileName, 0, 0))
        /* cannot rename streams (WinFsp limitation) */
        return STATUS_INVALID_PARAMETER;

    if (0 != TargetFileNode)
    {
        if (!FspFileNodeIsValid(TargetFileNode))
            return STATUS_INVALID_PARAMETER;

        ASSERT(TargetFileNode->IsDirectory);
    }

    FspFsvolDeviceFileRenameAcquireExclusive(FsvolDeviceObject);
retry:
    FspFileNodeAcquireExclusive(FileNode, Full);

    if (0 == Request)
    {
        if (0 != TargetFileNode)
            Remain = TargetFileNode->FileName;
        else
            FspFileNameSuffix(&FileNode->FileName, &Remain, &Suffix);

        Suffix.Length = (USHORT)Info->FileNameLength;
        Suffix.Buffer = Info->FileName;
        /* if there is a backslash anywhere in the NewFileName get its suffix */
        for (PWSTR P = Suffix.Buffer, EndP = P + Suffix.Length / sizeof(WCHAR); EndP > P; P++)
            if (L'\\' == *P)
            {
                Suffix.Length = (USHORT)((EndP - P - 1) * sizeof(WCHAR));
                Suffix.Buffer = P + 1;
            }
        Suffix.MaximumLength = Suffix.Length;

        if (!FspFileNameIsValid(&Remain, 0, 0) || !FspFileNameIsValid(&Suffix, 0, 0))
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
    }

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

    Result = FspFileNodeRenameCheck(FsvolDeviceObject, Irp,
        FileNode, FspFileNodeAcquireFull,
        &FileNode->FileName, TRUE);
    /* FspFileNodeRenameCheck releases FileNode with STATUS_OPLOCK_BREAK_IN_PROGRESS or failure */
    if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result)
        goto retry;
    if (!NT_SUCCESS(Result))
    {
        Result = STATUS_ACCESS_DENIED;
        goto rename_unlock_exit;
    }

    if (0 != FspFileNameCompare(&FileNode->FileName, &NewFileName, !FileDesc->CaseSensitive, 0))
    {
        Result = FspFileNodeRenameCheck(FsvolDeviceObject, Irp,
            FileNode, FspFileNodeAcquireFull,
            &NewFileName, FALSE);
        /* FspFileNodeRenameCheck releases FileNode with STATUS_OPLOCK_BREAK_IN_PROGRESS or failure */
        if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result)
            goto retry;
        if (!NT_SUCCESS(Result))
        {
            Result = STATUS_ACCESS_DENIED;
            goto rename_unlock_exit;
        }
    }
    else
    {
        /*
         * If the new file name is *exactly* the same (including case) as the old one,
         * there is no need to go to the user mode file system. Just return STATUS_SUCCESS.
         * Our RequestFini will do any cleanup necessary.
         *
         * This check needs to be done *after* the open handle test above. This is what FASTFAT
         * and NTFS do.
         */
        if (0 == FspFileNameCompare(&FileNode->FileName, &NewFileName, FALSE, 0))
        {
            Result = STATUS_SUCCESS;
            goto unlock_exit;
        }
    }

    FspFsvolDeviceFileRenameSetOwner(FsvolDeviceObject, Request);
    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;
    FspIopRequestContext(Request, RequestDeviceObject) = FsvolDeviceObject;

    return FSP_STATUS_IOQ_POST;

unlock_exit:
    FspFileNodeRelease(FileNode, Full);
rename_unlock_exit:
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

    /* fastfat has some really arcane rules on rename notifications; simplify! */
    FspFileNodeNotifyChange(FileNode,
        FileNode->IsDirectory ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME,
        FILE_ACTION_RENAMED_OLD_NAME);

    NewFileName.Length = NewFileName.MaximumLength =
        Request->Req.SetInformation.Info.Rename.NewFileName.Size - sizeof(WCHAR);
    NewFileName.Buffer = (PVOID)
        (Request->Buffer + Request->Req.SetInformation.Info.Rename.NewFileName.Offset);
    FspFileNodeRename(FileNode, &NewFileName);

    /* fastfat has some really arcane rules on rename notifications; simplify! */
    FspFileNodeNotifyChange(FileNode,
        FileNode->IsDirectory ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME,
        FILE_ACTION_RENAMED_NEW_NAME);

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
        if (IrpSp->Parameters.SetFile.AdvanceOnly)
            /* we do not support ValidDataLength currently! */
            Result = STATUS_INVALID_PARAMETER;
        else
            Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length, 0, 0);
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

retry:
    FspFileNodeAcquireExclusive(FileNode, Full);

    if (FileAllocationInformation == FileInformationClass ||
        FileEndOfFileInformation == FileInformationClass)
    {
        /*
         * Perform oplock check.
         *
         * It is ok to block our thread during receipt of the SetInformation IRP.
         * However we cannot acquire the FileNode exclusive and wait for oplock
         * breaks to complete, because oplock break processing acquires the FileNode
         * shared.
         *
         * Instead we initiate the oplock breaks and then check if any are in progress.
         * If that is the case we release the FileNode and wait for the oplock breaks
         * to complete. Once they are complete we retry the whole thing.
         */
        Result = FspFileNodeOplockCheckEx(FileNode, Irp,
            OPLOCK_FLAG_COMPLETE_IF_OPLOCKED);
        if (STATUS_OPLOCK_BREAK_IN_PROGRESS == Result ||
            DEBUGTEST_EX(NT_SUCCESS(Result), 10, FALSE))
        {
            FspFileNodeRelease(FileNode, Full);
            Result = FspFileNodeOplockCheck(FileNode, Irp);
            if (!NT_SUCCESS(Result))
                return Result;
            goto retry;
        }
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Full);
            return Result;
        }
    }

    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolSetInformationRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactSetInformationKind;
    Request->Req.SetInformation.UserContext = FileNode->UserContext;
    Request->Req.SetInformation.UserContext2 = FileDesc->UserContext2;
    Request->Req.SetInformation.FileInformationClass = FileInformationClass;

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
        Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length, Request, 0);
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

NTSTATUS FspFsvolSetInformationPrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    if (FileRenameInformation != IrpSp->Parameters.SetFile.FileInformationClass ||
        !IrpSp->Parameters.SetFile.ReplaceIfExists)
        return STATUS_SUCCESS;

    NTSTATUS Result;
    SECURITY_SUBJECT_CONTEXT SecuritySubjectContext;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
    SECURITY_CLIENT_CONTEXT SecurityClientContext;
    HANDLE UserModeAccessToken;
    PEPROCESS Process;

    /* duplicate the subject context access token into an impersonation token */
    SecurityQualityOfService.Length = sizeof SecurityQualityOfService;
    SecurityQualityOfService.ImpersonationLevel = SecurityIdentification;
    SecurityQualityOfService.ContextTrackingMode = SECURITY_STATIC_TRACKING;
    SecurityQualityOfService.EffectiveOnly = FALSE;
    SeCaptureSubjectContext(&SecuritySubjectContext);
    SeLockSubjectContext(&SecuritySubjectContext);
    Result = SeCreateClientSecurityFromSubjectContext(&SecuritySubjectContext,
        &SecurityQualityOfService, FALSE, &SecurityClientContext);
    SeUnlockSubjectContext(&SecuritySubjectContext);
    SeReleaseSubjectContext(&SecuritySubjectContext);
    if (!NT_SUCCESS(Result))
        return Result;

    ASSERT(TokenImpersonation == SeTokenType(SecurityClientContext.ClientToken));

    /* get a user-mode handle to the impersonation token */
    Result = ObOpenObjectByPointer(SecurityClientContext.ClientToken,
        0, 0, TOKEN_QUERY, *SeTokenObjectType, UserMode, &UserModeAccessToken);
    SeDeleteClientSecurity(&SecurityClientContext);
    if (!NT_SUCCESS(Result))
        return Result;

    /* get a pointer to the current process so that we can close the impersonation token later */
    Process = PsGetCurrentProcess();
    ObReferenceObject(Process);

    /* send the user-mode handle to the user-mode file system */
    FspIopRequestContext(Request, RequestAccessToken) = UserModeAccessToken;
    FspIopRequestContext(Request, RequestProcess) = Process;
    Request->Req.SetInformation.Info.Rename.AccessToken = (UINT_PTR)UserModeAccessToken;

    return STATUS_SUCCESS;
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
        Result = FspFsvolSetEndOfFileInformation(FileObject, Buffer, Length, Request, Response);
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
    HANDLE AccessToken = Context[RequestAccessToken];
    PEPROCESS Process = Context[RequestProcess];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);

    if (0 != FsvolDeviceObject)
        FspFsvolDeviceFileRenameReleaseOwner(FsvolDeviceObject, Request);

    if (0 != AccessToken)
    {
        KAPC_STATE ApcState;
        BOOLEAN Attach;

        ASSERT(0 != Process);
        Attach = Process != PsGetCurrentProcess();

        if (Attach)
            KeStackAttachProcess(Process, &ApcState);
#if DBG
        NTSTATUS Result0;
        Result0 = ObCloseHandle(AccessToken, UserMode);
        if (!NT_SUCCESS(Result0))
            DEBUGLOG("ObCloseHandle() = %s", NtStatusSym(Result0));
#else
        ObCloseHandle(AccessToken, UserMode);
#endif
        if (Attach)
            KeUnstackDetachProcess(&ApcState);

        ObDereferenceObject(Process);
    }
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
