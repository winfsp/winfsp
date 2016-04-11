/**
 * @file sys/write.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolWrite(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolWriteCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
static VOID FspFsvolWriteCachedDeferred(PVOID Context1, PVOID Context2);
static NTSTATUS FspFsvolWriteNonCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
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
        Result = FspFsvolWriteNonCached(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));

    return Result;
}

static NTSTATUS FspFsvolWriteCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    /* assert: must be top-level IRP */
    ASSERT(0 == FspIrpTopFlags(Irp));

    NTSTATUS Result;
    BOOLEAN Retrying = 0 != FspIrpRequest(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    LARGE_INTEGER WriteOffset = IrpSp->Parameters.Write.ByteOffset;
    ULONG WriteLength = IrpSp->Parameters.Write.Length;
    BOOLEAN WriteToEndOfFile =
        FILE_WRITE_TO_END_OF_FILE == WriteOffset.LowPart && -1L == WriteOffset.HighPart;
    BOOLEAN SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);
    FSP_FSCTL_FILE_INFO FileInfo;
    CC_FILE_SIZES FileSizes;
    FILE_END_OF_FILE_INFORMATION EndOfFileInformation;
    UINT64 WriteEndOffset;
    BOOLEAN ExtendingFile;
    BOOLEAN Success;

    /* should we defer the write? */
    Success = DEBUGTEST(90) && CcCanIWrite(FileObject, WriteLength, CanWait, Retrying);
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
    Success = DEBUGTEST(90) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireMain, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteCached, 0);

    /* check the file locks */
    if (!FsRtlCheckLockForWriteAccess(&FileNode->FileLock, Irp))
    {
        FspFileNodeRelease(FileNode, Main);
        return STATUS_FILE_LOCK_CONFLICT;
    }

    /* compute new file size */
    ASSERT(FspTimeoutInfinity32 ==
        FspFsvolDeviceExtension(FsvolDeviceObject)->VolumeParams.FileInfoTimeout);
    FspFileNodeGetFileInfo(FileNode, &FileInfo);
    WriteEndOffset = WriteToEndOfFile ?
        FileInfo.FileSize + WriteLength : WriteOffset.QuadPart + WriteLength;
    ExtendingFile = FileInfo.FileSize < WriteEndOffset;
    if (ExtendingFile && !CanWait)
    {
        /* need CanWait==TRUE for FspSendSetInformationIrp */
        FspFileNodeRelease(FileNode, Main);
        return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteCached, 0);
    }

    /* initialize cache if not already initialized! */
    if (0 == FileObject->PrivateCacheMap)
    {
        FileSizes.AllocationSize.QuadPart = FileInfo.AllocationSize;
        FileSizes.FileSize.QuadPart = FileInfo.FileSize;
        FileSizes.ValidDataLength.QuadPart = MAXLONGLONG;

        Result = FspCcInitializeCacheMap(FileObject, &FileSizes, FALSE,
            &FspCacheManagerCallbacks, FileNode);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Main);
            return Result;
        }
    }

    /* are we extending the file? */
    if (ExtendingFile)
    {
        ASSERT(CanWait);

        EndOfFileInformation.EndOfFile.QuadPart = WriteEndOffset;
        Result = FspSendSetInformationIrp(FsvolDeviceObject/* bypass filters */, FileObject,
            FileEndOfFileInformation, &EndOfFileInformation, sizeof EndOfFileInformation);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Main);
            return Result;
        }
    }

    /*
     * From this point forward we must jump to the CLEANUP label on failure.
     */

    /* are we using the copy or MDL interface? */
    if (!FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
    {
        PVOID Buffer;

        Buffer = 0 == Irp->MdlAddress ?
            Irp->UserBuffer : MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
        if (0 == Buffer)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto cleanup;
        }

        Result = FspCcCopyWrite(FileObject, &WriteOffset, WriteLength, CanWait, Buffer);
        if (!NT_SUCCESS(Result) || STATUS_PENDING == Result)
            goto cleanup;

        Irp->IoStatus.Information = WriteLength;
    }
    else
    {
        ASSERT(0 == Irp->MdlAddress);

        Result = FspCcPrepareMdlWrite(FileObject, &WriteOffset, WriteLength, &Irp->MdlAddress,
            &Irp->IoStatus);
        if (!NT_SUCCESS(Result))
            goto cleanup;
        ASSERT(STATUS_PENDING != Result);
    }

    /* update the current file offset if synchronous I/O */
    if (SynchronousIo)
        FileObject->CurrentByteOffset.QuadPart = WriteEndOffset;

    FspFileNodeRelease(FileNode, Main);

    return STATUS_SUCCESS;

cleanup:
    FspFileNodeRelease(FileNode, Main);

    if (STATUS_PENDING == Result)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteCached, 0);

    return Result;
}

static VOID FspFsvolWriteCachedDeferred(PVOID Context1, PVOID Context2)
{
    FspWqPostIrpWorkItem(Context1);
}

static NTSTATUS FspFsvolWriteNonCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    /* assert: either a top-level IRP or Paging I/O */
    ASSERT(0 == FspIrpTopFlags(Irp) || FlagOn(Irp->Flags, IRP_PAGING_IO));

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
    FSP_FSCTL_FILE_INFO FileInfo;
    FSP_FSCTL_TRANSACT_REQ *Request;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    /* no MDL requests on the non-cached path */
    if (FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
        return STATUS_INVALID_PARAMETER;

    /* paging I/O cannot change the file size */
    if (PagingIo && WriteToEndOfFile)
        return STATUS_INVALID_PARAMETER;

    /* stop CcWriteBehind from calling me! */
    if (FspIoqStopped(FspFsvolDeviceExtension(FsvolDeviceObject)->Ioq))
        return FspFsvolDeviceStoppedStatus(FsvolDeviceObject);

    /* if we are called by the lazy writer we must constrain writes */
    if (FlagOn(FspIrpTopFlags(Irp), FspFileNodeAcquireMain) &&  /* if TopLevelIrp has acquired Main */
        FileNode->Tls.LazyWriteThread == PsGetCurrentThread())  /* and this is a lazy writer thread */
    {
        ASSERT(PagingIo);
        ASSERT(FspTimeoutInfinity32 ==
            FspFsvolDeviceExtension(FsvolDeviceObject)->VolumeParams.FileInfoTimeout);

        FspFileNodeGetFileInfo(FileNode, &FileInfo);

        if ((UINT64)WriteOffset.QuadPart >= FileInfo.FileSize)
            return STATUS_SUCCESS;

        if (WriteLength > (ULONG)(FileInfo.FileSize - WriteOffset.QuadPart))
            WriteLength = (ULONG)(FileInfo.FileSize - WriteOffset.QuadPart);
    }

    /* probe and lock the user buffer */
    Result = FspLockUserBuffer(Irp, WriteLength, IoReadAccess);
    if (!NT_SUCCESS(Result))
        return Result;

    /* acquire FileNode exclusive Full */
    Success = DEBUGTEST(90) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteNonCached, 0);

    /* check the file locks */
    if (!PagingIo && !FsRtlCheckLockForWriteAccess(&FileNode->FileLock, Irp))
    {
        FspFileNodeRelease(FileNode, Full);
        return STATUS_FILE_LOCK_CONFLICT;
    }

    /* if this is a non-cached transfer on a cached file then flush and purge the file */
    if (!PagingIo && 0 != FileObject->SectionObjectPointer->DataSectionObject)
    {
        if (!CanWait)
        {
            FspFileNodeRelease(FileNode, Full);
            return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteNonCached, 0);
        }

        Result = FspFileNodeFlushAndPurgeCache(FileNode,
            IrpSp->Parameters.Write.ByteOffset.QuadPart,
            IrpSp->Parameters.Write.Length,
            TRUE);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Full);
            return Result;
        }
    }

    /* delete any work item if present! */
    FspIrpDeleteRequest(Irp);

    /* create request */
    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolWriteNonCachedRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactWriteKind;
    Request->Req.Write.UserContext = FileNode->UserContext;
    Request->Req.Write.UserContext2 = FileDesc->UserContext2;
    Request->Req.Write.Offset = WriteOffset.QuadPart;
    Request->Req.Write.Length = WriteLength;
    Request->Req.Write.Key = WriteKey;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolWritePrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_SAFE_MDL *SafeMdl = 0;
    PVOID Address;
    PEPROCESS Process;

    /* create a "safe" MDL if necessary */
    if (!FspSafeMdlCheck(Irp->MdlAddress))
    {
        Result = FspSafeMdlCreate(Irp->MdlAddress, IoReadAccess, &SafeMdl);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    /* map the MDL into user-mode */
    Result = FspMapLockedPagesInUserMode(0 != SafeMdl ? SafeMdl->Mdl : Irp->MdlAddress, &Address);
    if (!NT_SUCCESS(Result))
    {
        if (0 != SafeMdl)
            FspSafeMdlDelete(SafeMdl);

        return Result;
    }

    /* get a pointer to the current process so that we can unmap the address later */
    Process = PsGetCurrentProcess();
    ObReferenceObject(Process);

    Request->Req.Write.Address = (UINT64)(UINT_PTR)Address;

    FspIopRequestContext(Request, RequestSafeMdl) = SafeMdl;
    FspIopRequestContext(Request, RequestAddress) = Address;
    FspIopRequestContext(Request, RequestProcess) = Process;

    return STATUS_SUCCESS;
}

NTSTATUS FspFsvolWriteComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    LARGE_INTEGER WriteOffset = IrpSp->Parameters.Write.ByteOffset;
    BOOLEAN WriteToEndOfFile =
        FILE_WRITE_TO_END_OF_FILE == WriteOffset.LowPart && -1L == WriteOffset.HighPart;
    BOOLEAN PagingIo = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
    BOOLEAN SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);

    /* if we are top-level */
    if (0 == FspIrpTopFlags(Irp))
    {
        UINT64 OriginalFileSize = FileNode->Header.FileSize.QuadPart;

        /* update file info */
        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.Write.FileInfo);

        if (OriginalFileSize != Response->Rsp.Write.FileInfo.FileSize)
            FspFileNodeNotifyChange(FileNode, FILE_NOTIFY_CHANGE_SIZE, FILE_ACTION_MODIFIED);

        /* update the current file offset if synchronous I/O (and not paging I/O) */
        if (SynchronousIo && !PagingIo)
            FileObject->CurrentByteOffset.QuadPart = WriteToEndOfFile ?
                Response->Rsp.Write.FileInfo.FileSize :
                WriteOffset.QuadPart + Response->IoStatus.Information;

        FspIopResetRequest(Request, 0);
    }
    else
    {
        ASSERT(PagingIo);
        FspIopResetRequest(Request, 0);
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
