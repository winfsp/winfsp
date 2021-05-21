/**
 * @file sys/write.c
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
    RequestCookie                       = 1,
    RequestSafeMdl                      = 1,
    RequestAddress                      = 2,
    RequestProcess                      = 3,
};
FSP_FSCTL_STATIC_ASSERT(RequestCookie == RequestSafeMdl, "");

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

    /* perform oplock check */
    Result = FspFileNodeOplockCheckAsync(
        FileNode, FspFileNodeAcquireMain, FspFsvolWriteCached,
        Irp);
    if (STATUS_PENDING == Result)
        return Result;
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Main);
        return Result;
    }

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
    if (WriteToEndOfFile)
        WriteOffset.QuadPart = FileInfo.FileSize;
    WriteEndOffset = WriteOffset.QuadPart + WriteLength;
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

        /* send EndOfFileInformation IRP; this will also set TruncateOnClose, etc. */
        EndOfFileInformation.EndOfFile.QuadPart = WriteEndOffset;
        Result = FspSendSetInformationIrp(FsvolDeviceObject/* bypass filters */, FileObject,
            FileEndOfFileInformation, &EndOfFileInformation, sizeof EndOfFileInformation);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Main);
            return Result;
        }

        /* double-check that the cache still exists in case CcSetFileSizes failed */
        if (0 == FileObject->SectionObjectPointer->SharedCacheMap)
        {
            FspFileNodeRelease(FileNode, Main);
            return STATUS_INSUFFICIENT_RESOURCES; // or STATUS_SECTION_TOO_BIG?
        }
    }

    /* double-check that the end offset is <= than the file size and fail if not */
    if (WriteEndOffset > (UINT64)CcGetFileSizePointer(FileObject)->QuadPart)
    {
        FspFileNodeRelease(FileNode, Main);
        return STATUS_INTERNAL_ERROR;
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
        Result = FspCcPrepareMdlWrite(FileObject, &WriteOffset, WriteLength, &Irp->MdlAddress,
            &Irp->IoStatus);
        if (!NT_SUCCESS(Result))
            goto cleanup;
        ASSERT(STATUS_PENDING != Result);
    }

    /* update the current file offset if synchronous I/O */
    if (SynchronousIo)
        FileObject->CurrentByteOffset.QuadPart = WriteEndOffset;

    /* mark the file object as modified (if not paging I/O) */
    SetFlag(FileObject->Flags, FO_FILE_MODIFIED);

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
    // !PAGED_CODE();

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

    /* probe and lock the user buffer */
    Result = FspLockUserBuffer(Irp, WriteLength, IoReadAccess);
    if (!NT_SUCCESS(Result))
        return Result;

    /* acquire FileNode exclusive Full */
    Success = DEBUGTEST(90) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolWriteNonCached, 0);

    /* perform oplock check */
    if (!PagingIo)
    {
        Result = FspFileNodeOplockCheckAsync(
            FileNode, FspFileNodeAcquireFull, FspFsvolWriteNonCached,
            Irp);
        if (STATUS_PENDING == Result)
            return Result;
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Full);
            return Result;
        }
    }

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

    Request = FspIrpRequest(Irp);
    if (0 == Request)
    {
        /* create request */
        Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolWriteNonCachedRequestFini, &Request);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Full);
            return Result;
        }
    }
    else
    {
        /* reuse existing request */
        ASSERT(Request->Size == sizeof *Request);
        ASSERT(Request->Hint == (UINT_PTR)Irp);
        FspIopResetRequest(Request, FspFsvolWriteNonCachedRequestFini);
        RtlZeroMemory(&Request->Req,
            sizeof *Request - FIELD_OFFSET(FSP_FSCTL_TRANSACT_REQ, Req));
    }


    Request->Kind = FspFsctlTransactWriteKind;
    Request->Req.Write.UserContext = FileNode->UserContext;
    Request->Req.Write.UserContext2 = FileDesc->UserContext2;
    Request->Req.Write.Offset = WriteOffset.QuadPart;
    Request->Req.Write.Length = WriteLength;
    Request->Req.Write.Key = WriteKey;
    Request->Req.Write.ConstrainedIo = !!PagingIo;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;

    FSP_STATISTICS *Statistics = FspFsvolDeviceStatistics(FsvolDeviceObject);
    if (PagingIo)
    {
        FspStatisticsInc(Statistics, Base.UserFileWrites);
        FspStatisticsAdd(Statistics, Base.UserFileWriteBytes, WriteLength);
        FspStatisticsInc(Statistics, Base.UserDiskWrites);
    }
    else
    {
        FspStatisticsInc(Statistics, Specific.NonCachedWrites);
        FspStatisticsAdd(Statistics, Specific.NonCachedWriteBytes, WriteLength);
        FspStatisticsInc(Statistics, Specific.NonCachedDiskWrites);
    }

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolWritePrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    if (FspWriteIrpShouldUseProcessBuffer(Irp, Request->Req.Write.Length))
    {
        NTSTATUS Result;
        PVOID Cookie;
        PVOID Address;
        PEPROCESS Process;
        PVOID SystemAddress = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

        if (0 == SystemAddress)
            return STATUS_INSUFFICIENT_RESOURCES; /* something is seriously screwy! */

        Result = FspProcessBufferAcquire(Request->Req.Write.Length, &Cookie, &Address);
        if (!NT_SUCCESS(Result))
            return Result;

        ASSERT(0 != Address);
        try
        {
            RtlCopyMemory(Address, SystemAddress, Request->Req.Write.Length);
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            Result = GetExceptionCode();
            Result = FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;

            FspProcessBufferRelease(Cookie, Address);

            return Result;
        }

        /* get a pointer to the current process so that we can release the buffer later */
        Process = PsGetCurrentProcess();
        ObReferenceObject(Process);

        Request->Req.Write.Address = (UINT64)(UINT_PTR)Address;

        FspIopRequestContext(Request, RequestCookie) = (PVOID)((UINT_PTR)Cookie | 1);
        FspIopRequestContext(Request, RequestAddress) = Address;
        FspIopRequestContext(Request, RequestProcess) = Process;

        return STATUS_SUCCESS;
    }
    else
    {
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
        Result = FspMapLockedPagesInUserMode(
            0 != SafeMdl ? SafeMdl->Mdl : Irp->MdlAddress, &Address, FspMvMdlMappingNoWrite);
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

    if (Response->IoStatus.Information > Request->Req.Write.Length)
        FSP_RETURN(Result = STATUS_INTERNAL_ERROR);

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
        FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.Write.FileInfo, TRUE);

        if (OriginalFileSize != Response->Rsp.Write.FileInfo.FileSize)
            FspFileNodeNotifyChange(FileNode, FILE_NOTIFY_CHANGE_SIZE, FILE_ACTION_MODIFIED, FALSE);

        /* update the current file offset if synchronous I/O (and not paging I/O) */
        if (SynchronousIo && !PagingIo)
            FileObject->CurrentByteOffset.QuadPart = WriteToEndOfFile ?
                Response->Rsp.Write.FileInfo.FileSize :
                WriteOffset.QuadPart + Response->IoStatus.Information;

        /* mark the file object as modified (if not paging I/O) */
        if (!PagingIo)
            SetFlag(FileObject->Flags, FO_FILE_MODIFIED);

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

    if ((UINT_PTR)Context[RequestCookie] & 1)
    {
        PVOID Cookie = (PVOID)((UINT_PTR)Context[RequestCookie] & ~1);
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
            FspProcessBufferRelease(Cookie, Address);
            if (Attach)
                KeUnstackDetachProcess(&ApcState);

            ObDereferenceObject(Process);
        }
    }
    else
    {
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
    }

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
