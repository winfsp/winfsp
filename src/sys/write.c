/**
 * @file sys/write.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolWrite(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolWriteCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
static VOID FspFsvolWriteCachedDeferred(PVOID Context1, PVOID Context2);
static NTSTATUS FspFsvolWriteNonCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOPREP_DISPATCH FspFsvolWritePrepare;
FSP_IOCMPL_DISPATCH FspFsvolWriteComplete;
static FSP_IOP_REQUEST_FINI FspFsvolWriteNonCachedRequestFini;
FSP_DRIVER_DISPATCH FspWrite;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolWrite)
#pragma alloc_text(PAGE, FspFsvolWriteCached)
#pragma alloc_text(PAGE, FspFsvolWriteCachedDeferred)
#pragma alloc_text(PAGE, FspFsvolWriteNonCached)
#pragma alloc_text(PAGE, FspFsvolWritePrepare)
#pragma alloc_text(PAGE, FspFsvolWriteComplete)
#pragma alloc_text(PAGE, FspFsvolWriteNonCachedRequestFini)
#pragma alloc_text(PAGE, FspWrite)
#endif

enum
{
    /* WriteNonCached */
    RequestIrp                          = 0,
    RequestSafeMdl                      = 1,
    RequestAddress                      = 2,
    RequestProcess                      = 3,
};

static NTSTATUS FspFsvolWrite(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    /* is this an MDL complete request? */
    if (FlagOn(IrpSp->MinorFunction, IRP_MN_COMPLETE))
    {
        Result = FspCcMdlWriteComplete(FileObject,
            &IrpSp->Parameters.Write.ByteOffset, Irp->MdlAddress);
        Irp->MdlAddress = 0;
        return Result;
    }

    /* only regular files can be written */
    if (FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /* do we have anything to write? */
    if (0 == IrpSp->Parameters.Write.Length)
    {
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    /* are we doing cached or non-cached I/O? */
    if (FlagOn(FileObject->Flags, FO_CACHE_SUPPORTED) &&
        !FlagOn(Irp->Flags, IRP_PAGING_IO | IRP_NOCACHE))
        Result = FspFsvolWriteCached(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));
    else
        Result = FspFsvolWriteNonCached(FsvolDeviceObject, Irp, IrpSp);

    return Result;
}

static NTSTATUS FspFsvolWriteCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    NTSTATUS Result;
    BOOLEAN Retrying = 0 != FspIrpRequest(Irp);
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    LARGE_INTEGER WriteOffset = IrpSp->Parameters.Write.ByteOffset;
    ULONG WriteLength = IrpSp->Parameters.Write.Length;
#if 0
    /* !!!: lock support! */
    ULONG WriteKey = IrpSp->Parameters.Write.Key;
#endif
    BOOLEAN WriteToEndOfFile =
        FILE_WRITE_TO_END_OF_FILE == WriteOffset.LowPart && -1L == WriteOffset.HighPart;
    BOOLEAN SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);
    FSP_FSCTL_FILE_INFO FileInfo;
    CC_FILE_SIZES FileSizes;
    UINT64 WriteEndOffset;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    /* should we defer the write? */
    Success = DEBUGTEST(90, TRUE) && CcCanIWrite(FileObject, WriteLength, CanWait, Retrying);
    if (!Success)
    {
        Result = FspWqCreateIrpWorkItem(Irp, FspFsvolWriteCached, 0);
        if (NT_SUCCESS(Result))
        {
            IoMarkIrpPending(Irp);
            CcDeferWrite(FileObject, FspFsvolWriteCachedDeferred, Irp, 0, WriteLength, Retrying);

            return STATUS_PENDING;
        }

        /* if we are unable to defer we will go ahead and (try to) service the IRP now! */
    }

    /* try to acquire the FileNode Main exclusive */
    Success = DEBUGTEST(90, TRUE) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireMain, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteCached, 0);

    /* compute new file size and allocation size */
    ASSERT(FspTimeoutInfinity32 == FsvolDeviceExtension->VolumeParams.FileInfoTimeout);
    FspFileNodeGetFileInfo(FileNode, &FileInfo);
    FileSizes.AllocationSize.QuadPart = FileInfo.AllocationSize;
    FileSizes.FileSize.QuadPart = FileInfo.FileSize;
    FileSizes.ValidDataLength.QuadPart = MAXLONGLONG;
    WriteEndOffset = WriteToEndOfFile ?
        FileInfo.FileSize + WriteLength : WriteOffset.QuadPart + WriteLength;
    if (FileInfo.FileSize < WriteEndOffset)
    {
        /* file is being extended */
        FileSizes.FileSize.QuadPart = WriteEndOffset;
        if (FileSizes.FileSize.QuadPart > FileSizes.AllocationSize.QuadPart)
        {
            UINT64 AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
                FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;
            FileSizes.AllocationSize.QuadPart = (FileSizes.FileSize.QuadPart + AllocationUnit - 1)
                / AllocationUnit * AllocationUnit;
        }
    }

    /* initialize cache if not already initialized! */
    if (0 == FileObject->PrivateCacheMap)
    {
        Result = FspCcInitializeCacheMap(FileObject, &FileSizes, FALSE,
            &FspCacheManagerCallbacks, FileNode);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Main);
            return Result;
        }
    }
    else if (FileInfo.FileSize < WriteEndOffset)
    {
        /* file is being extended */
        Result = FspCcSetFileSizes(FileObject, &FileSizes);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Main);
            return Result;
        }
    }

    /* are we using the copy or MDL interface? */
    if (!FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
    {
        PVOID Buffer;

        Buffer = 0 == Irp->MdlAddress ?
            Irp->UserBuffer : MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (0 == Buffer)
        {
            FspFileNodeRelease(FileNode, Main);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Result = FspCcCopyWrite(FileObject, &WriteOffset, WriteLength, CanWait || Retrying, Buffer);
        if (STATUS_PENDING == Result)
        {
            FspFileNodeRelease(FileNode, Main);
            return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteCached, 0);
        }

        Irp->IoStatus.Information = WriteLength;
    }
    else
    {
        ASSERT(0 == Irp->MdlAddress);

        Result = FspCcPrepareMdlWrite(FileObject, &WriteOffset, WriteLength,
            &Irp->MdlAddress, &Irp->IoStatus);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Main);
            return Result;
        }
    }

    /* update the current file offset if synchronous I/O */
    if (SynchronousIo)
        FileObject->CurrentByteOffset.QuadPart = WriteEndOffset;

    FspFileNodeRelease(FileNode, Main);

    return STATUS_SUCCESS;
}

static VOID FspFsvolWriteCachedDeferred(PVOID Context1, PVOID Context2)
{
    FspWqPostIrpWorkItem(Context1);
}

static NTSTATUS FspFsvolWriteNonCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    LARGE_INTEGER WriteOffset = IrpSp->Parameters.Write.ByteOffset;
    ULONG WriteLength = IrpSp->Parameters.Write.Length;
    ULONG WriteKey = IrpSp->Parameters.Write.Key;
    BOOLEAN WriteToEndOfFile =
        FILE_WRITE_TO_END_OF_FILE == WriteOffset.LowPart && -1L == WriteOffset.HighPart;
    BOOLEAN PagingIo = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    /* no MDL requests on the non-cached path */
    if (FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
        return STATUS_INVALID_PARAMETER;

    /* paging I/O cannot change the file size */
    if (PagingIo && WriteToEndOfFile)
        return STATUS_INVALID_PARAMETER;

    /* if non-cached I/O check the offset/length alignment */
    /*
     * We are going to avoid doing this test, because we don't really need to restrict
     * ourselves for non-cached I/O, but also because we do not always know the correct
     * file size for our alignment test. The file size is needed, because the alignment
     * test is:
     *
     *     if WriteOffset is sector aligned
     *     and (WriteLength is sector aligned or WriteOffset + WriteLength >= FileSize)
     *
     * This means that the user-mode file system must be able to deal with variable size
     * I/O, but this was the case anyway because of the following part of the test:
     *
     *     WriteOffset + WriteLength >= FileSize
     *
     * In any case the user-mode file system can enforce this rule if it wants!
     */
#if 0
    if (!PagingIo)
    {
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
            FspFsvolDeviceExtension(FsvolDeviceObject);
        if (0 != WriteOffset.QuadPart % FsvolDeviceExtension->VolumeParams.SectorSize ||
            0 != WriteLength % FsvolDeviceExtension->VolumeParams.SectorSize)
            return STATUS_NOT_IMPLEMENTED; /* FastFat does this! */
    }
#endif

    /* probe and lock the user buffer */
    if (0 == Irp->MdlAddress)
    {
        Result = FspLockUserBuffer(Irp->UserBuffer, IrpSp->Parameters.Write.Length,
            Irp->RequestorMode, IoReadAccess, &Irp->MdlAddress);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    /* create request */
    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolWriteNonCachedRequestFini, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactWriteKind;
    Request->Req.Write.UserContext = FileNode->UserContext;
    Request->Req.Write.UserContext2 = FileDesc->UserContext2;
    Request->Req.Write.Offset = WriteOffset.QuadPart;
    Request->Req.Write.Length = WriteLength;
    Request->Req.Write.Key = WriteKey;
    Request->Req.Write.Append = WriteToEndOfFile;
    Request->Req.Write.PagingIo = PagingIo;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolWritePrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    NTSTATUS Result;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    BOOLEAN PagingIo = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    FSP_SAFE_MDL *SafeMdl = 0;
    PVOID Address;
    PEPROCESS Process;
    BOOLEAN Success;

    Success = DEBUGTEST(90, TRUE) && FspFileNodeTryAcquireExclusive(FileNode, Full);
    if (!Success)
    {
        FspIopRetryPrepareIrp(Irp, &Result);
        return Result;
    }

    /* if this is a non-cached transfer on a cached file then flush and purge the file */
    if (!PagingIo && 0 != FileObject->SectionObjectPointer->DataSectionObject)
    {
        LARGE_INTEGER FlushOffset = IrpSp->Parameters.Write.ByteOffset;
        PLARGE_INTEGER PFlushOffset = &FlushOffset;
        ULONG FlushLength = IrpSp->Parameters.Write.Length;
        FSP_FSCTL_FILE_INFO FileInfo;
        IO_STATUS_BLOCK IoStatus = { 0 };

        if (FILE_WRITE_TO_END_OF_FILE == FlushOffset.LowPart && -1L == FlushOffset.HighPart)
        {
            if (FspFileNodeTryGetFileInfo(FileNode, &FileInfo))
                FlushOffset.QuadPart = FileInfo.FileSize;
            else
                PFlushOffset = 0; /* we don't know how big the file is, so flush it all! */
        }

        CcFlushCache(FileObject->SectionObjectPointer, PFlushOffset, FlushLength, &IoStatus);
        if (!NT_SUCCESS(IoStatus.Status))
        {
            FspFileNodeRelease(FileNode, Full);
            return IoStatus.Status;
        }

        CcPurgeCacheSection(FileObject->SectionObjectPointer, PFlushOffset, FlushLength, FALSE);
    }

    /* create a "safe" MDL if necessary */
    if (!FspSafeMdlCheck(Irp->MdlAddress))
    {
        Result = FspSafeMdlCreate(Irp->MdlAddress, IoReadAccess, &SafeMdl);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Full);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    /* map the MDL into user-mode */
    Result = FspMapLockedPagesInUserMode(0 != SafeMdl ? SafeMdl->Mdl : Irp->MdlAddress, &Address);
    if (!NT_SUCCESS(Result))
    {
        if (0 != SafeMdl)
            FspSafeMdlDelete(SafeMdl);

        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /* get a pointer to the current process so that we can unmap the address later */
    Process = PsGetCurrentProcess();
    ObReferenceObject(Process);

    Request->Req.Write.Address = (UINT64)(UINT_PTR)Address;

    FspFileNodeSetOwner(FileNode, Pgio, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;
    FspIopRequestContext(Request, RequestSafeMdl) = SafeMdl;
    FspIopRequestContext(Request, RequestAddress) = Address;
    FspIopRequestContext(Request, RequestProcess) = Process;

    return STATUS_SUCCESS;
}

NTSTATUS FspFsvolWriteComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    LARGE_INTEGER WriteOffset = IrpSp->Parameters.Write.ByteOffset;
    BOOLEAN WriteToEndOfFile =
        FILE_WRITE_TO_END_OF_FILE == WriteOffset.LowPart && -1L == WriteOffset.HighPart;
    BOOLEAN PagingIo = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    BOOLEAN SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    if (!PagingIo)
    {
        /* update file info */
        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.Write.FileInfo);

        /* update the current file offset if synchronous I/O (and not paging I/O) */
        if (SynchronousIo)
            FileObject->CurrentByteOffset.QuadPart = WriteToEndOfFile ?
                Response->Rsp.Write.FileInfo.FileSize :
                WriteOffset.QuadPart + Response->IoStatus.Information;
    }

    Irp->IoStatus.Information = Response->IoStatus.Information;
    Result = STATUS_SUCCESS;

    FSP_LEAVE_IOC(
        "FileObject=%p, UserBuffer=%p, MdlAddress=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%ld",
        IrpSp->FileObject, Irp->UserBuffer, Irp->MdlAddress,
        IrpSp->Parameters.Write.Key,
        IrpSp->Parameters.Write.ByteOffset.HighPart, IrpSp->Parameters.Write.ByteOffset.LowPart,
        IrpSp->Parameters.Write.Length);
}

static VOID FspFsvolWriteNonCachedRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    PIRP Irp = Context[RequestIrp];
    FSP_SAFE_MDL *SafeMdl = Context[RequestSafeMdl];
    PVOID Address = Context[RequestAddress];
    PEPROCESS Process = Context[RequestProcess];

    if (0 != Address)
    {
        KAPC_STATE ApcState;
        BOOLEAN Attach;

        ASSERT(0 != Process);
        Attach = Process != PsGetCurrentProcess();

        if (Attach)
            KeStackAttachProcess(Process, &ApcState);
        MmUnmapLockedPages(Address, 0 != SafeMdl ? SafeMdl->Mdl : Irp->MdlAddress);
        if (Attach)
            KeUnstackDetachProcess(&ApcState);

        ObDereferenceObject(Process);
    }

    if (0 != SafeMdl)
        FspSafeMdlDelete(SafeMdl);

    if (0 != Irp)
    {
        PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
        FSP_FILE_NODE *FileNode = IrpSp->FileObject->FsContext;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }
}

NTSTATUS FspWrite(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolWrite(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "FileObject=%p, UserBuffer=%p, MdlAddress=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%ld",
        IrpSp->FileObject, Irp->UserBuffer, Irp->MdlAddress,
        IrpSp->Parameters.Write.Key,
        IrpSp->Parameters.Write.ByteOffset.HighPart, IrpSp->Parameters.Write.ByteOffset.LowPart,
        IrpSp->Parameters.Write.Length);
}
