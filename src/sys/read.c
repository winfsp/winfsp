/**
 * @file sys/read.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolRead(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolReadCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
static NTSTATUS FspFsvolReadNonCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOPREP_DISPATCH FspFsvolReadPrepare;
FSP_IOCMPL_DISPATCH FspFsvolReadComplete;
static FSP_IOP_REQUEST_FINI FspFsvolReadNonCachedRequestFini;
FSP_DRIVER_DISPATCH FspRead;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolRead)
#pragma alloc_text(PAGE, FspFsvolReadCached)
#pragma alloc_text(PAGE, FspFsvolReadNonCached)
#pragma alloc_text(PAGE, FspFsvolReadPrepare)
#pragma alloc_text(PAGE, FspFsvolReadComplete)
#pragma alloc_text(PAGE, FspFsvolReadNonCachedRequestFini)
#pragma alloc_text(PAGE, FspRead)
#endif

enum
{
    /* ReadNonCached */
    RequestIrp                          = 0,
    RequestSafeMdl                      = 1,
    RequestAddress                      = 2,
    RequestProcess                      = 3,
};

static NTSTATUS FspFsvolRead(
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
        Result = FspCcMdlReadComplete(FileObject, Irp->MdlAddress);
        Irp->MdlAddress = 0;
        return Result;
    }

    /* only regular files can be read */
    if (FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /* do we have anything to read? */
    if (0 == IrpSp->Parameters.Read.Length)
    {
        Irp->IoStatus.Information = 0;
        return STATUS_SUCCESS;
    }

    /* are we doing cached or non-cached I/O? */
    if (FlagOn(FileObject->Flags, FO_CACHE_SUPPORTED) &&
        !FlagOn(Irp->Flags, IRP_PAGING_IO | IRP_NOCACHE))
        Result = FspFsvolReadCached(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));
    else
        Result = FspFsvolReadNonCached(FsvolDeviceObject, Irp, IrpSp);

    return Result;
}

static NTSTATUS FspFsvolReadCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    LARGE_INTEGER ReadOffset = IrpSp->Parameters.Read.ByteOffset;
    ULONG ReadLength = IrpSp->Parameters.Read.Length;
#if 0
    /* !!!: lock support! */
    ULONG ReadKey = IrpSp->Parameters.Read.Key;
#endif
    BOOLEAN SynchronousIo = BooleanFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);
    FSP_FSCTL_FILE_INFO FileInfo;
    CC_FILE_SIZES FileSizes;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    /* try to acquire the FileNode Main shared */
    Success = DEBUGTEST(90, TRUE) &&
        FspFileNodeTryAcquireSharedF(FileNode, FspFileNodeAcquireMain, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolReadCached, 0);

    /* initialize cache if not already initialized! */
    if (0 == FileObject->PrivateCacheMap)
    {
        ASSERT(FspTimeoutInfinity32 == FsvolDeviceExtension->VolumeParams.FileInfoTimeout);
        FspFileNodeGetFileInfo(FileNode, &FileInfo);
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

        Result = FspCcCopyRead(FileObject, &ReadOffset, ReadLength, CanWait, Buffer,
            &Irp->IoStatus);
        if (STATUS_PENDING == Result)
        {
            FspFileNodeRelease(FileNode, Main);
            return FspWqRepostIrpWorkItem(Irp, FspFsvolReadCached, 0);
        }
    }
    else
    {
        ASSERT(0 == Irp->MdlAddress);

        Result = FspCcMdlRead(FileObject, &ReadOffset, ReadLength, &Irp->MdlAddress,
            &Irp->IoStatus);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Main);
            return Result;
        }
    }

    /* update the current file offset if synchronous I/O */
    if (SynchronousIo)
        FileObject->CurrentByteOffset.QuadPart = ReadOffset.QuadPart + Irp->IoStatus.Information;

    FspFileNodeRelease(FileNode, Main);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolReadNonCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    LARGE_INTEGER ReadOffset = IrpSp->Parameters.Read.ByteOffset;
    ULONG ReadLength = IrpSp->Parameters.Read.Length;
    ULONG ReadKey = IrpSp->Parameters.Read.Key;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    /* no MDL requests on the non-cached path */
    if (FlagOn(IrpSp->MinorFunction, IRP_MN_MDL))
        return STATUS_INVALID_PARAMETER;

    /* probe and lock the user buffer */
    if (0 == Irp->MdlAddress)
    {
        Result = FspLockUserBuffer(Irp->UserBuffer, ReadLength,
            Irp->RequestorMode, IoReadAccess, &Irp->MdlAddress);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    /* create request */
    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolReadNonCachedRequestFini, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    Request->Kind = FspFsctlTransactReadKind;
    Request->Req.Read.UserContext = FileNode->UserContext;
    Request->Req.Read.UserContext2 = FileDesc->UserContext2;
    Request->Req.Read.Offset = ReadOffset.QuadPart;
    Request->Req.Read.Length = ReadLength;
    Request->Req.Read.Key = ReadKey;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolReadPrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    NTSTATUS Result;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_SAFE_MDL *SafeMdl = 0;
    PVOID Address;
    PEPROCESS Process;
    BOOLEAN Success;

    Success = DEBUGTEST(90, TRUE) && FspFileNodeTryAcquireShared(FileNode, Full);
    if (!Success)
    {
        FspIopRetryPrepareIrp(Irp, &Result);
        return Result;
    }

    /* create a "safe" MDL if necessary */
    if (!FspSafeMdlCheck(Irp->MdlAddress))
    {
        Result = FspSafeMdlCreate(Irp->MdlAddress, IoWriteAccess, &SafeMdl);
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

    Request->Req.Read.Address = (UINT64)(UINT_PTR)Address;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;
    FspIopRequestContext(Request, RequestSafeMdl) = SafeMdl;
    FspIopRequestContext(Request, RequestAddress) = Address;
    FspIopRequestContext(Request, RequestProcess) = Process;

    return STATUS_SUCCESS;
}

NTSTATUS FspFsvolReadComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    LARGE_INTEGER ReadOffset = IrpSp->Parameters.Read.ByteOffset;
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
        /* update the current file offset if synchronous I/O (and not paging I/O) */
        if (SynchronousIo)
            FileObject->CurrentByteOffset.QuadPart =
                ReadOffset.QuadPart + Response->IoStatus.Information;
    }

    Irp->IoStatus.Information = Response->IoStatus.Information;
    Result = STATUS_SUCCESS;

    FSP_LEAVE_IOC(
        "FileObject=%p, UserBuffer=%p, MdlAddress=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%ld",
        IrpSp->FileObject, Irp->UserBuffer, Irp->MdlAddress,
        IrpSp->Parameters.Read.Key,
        IrpSp->Parameters.Read.ByteOffset.HighPart, IrpSp->Parameters.Read.ByteOffset.LowPart,
        IrpSp->Parameters.Read.Length);
}

static VOID FspFsvolReadNonCachedRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
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

NTSTATUS FspRead(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolRead(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "FileObject=%p, UserBuffer=%p, MdlAddress=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%ld",
        IrpSp->FileObject, Irp->UserBuffer, Irp->MdlAddress,
        IrpSp->Parameters.Read.Key,
        IrpSp->Parameters.Read.ByteOffset.HighPart, IrpSp->Parameters.Read.ByteOffset.LowPart,
        IrpSp->Parameters.Read.Length);
}
