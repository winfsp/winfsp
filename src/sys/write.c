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
FSP_IOCMPL_DISPATCH FspFsvolWriteComplete;
FSP_DRIVER_DISPATCH FspWrite;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolWrite)
#pragma alloc_text(PAGE, FspFsvolWriteCached)
#pragma alloc_text(PAGE, FspFsvolWriteCachedDeferred)
#pragma alloc_text(PAGE, FspFsvolWriteNonCached)
#pragma alloc_text(PAGE, FspFsvolWriteComplete)
#pragma alloc_text(PAGE, FspWrite)
#endif

static NTSTATUS FspFsvolWrite(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    BOOLEAN TopLevel = FspSetTopLevelIrp(Irp);

    /* is this an MDL complete request? */
    if (FlagOn(IrpSp->MinorFunction, IRP_MN_COMPLETE))
    {
        Result = FspCcMdlWriteComplete(IrpSp->FileObject,
            &IrpSp->Parameters.Write.ByteOffset, Irp->MdlAddress);
        Irp->MdlAddress = 0;
        goto exit;
    }

    /* do we have anything to write? */
    if (0 == IrpSp->Parameters.Write.Length)
    {
        Irp->IoStatus.Information = 0;
        Result = STATUS_SUCCESS;
        goto exit;
    }

    /* probe and lock the user buffer */
    if (0 == Irp->MdlAddress)
    {
        Result = FspLockUserBuffer(Irp->UserBuffer, IrpSp->Parameters.Write.Length,
            Irp->RequestorMode, IoReadAccess, &Irp->MdlAddress);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    if (!FlagOn(Irp->Flags, IRP_NOCACHE))
        Result = FspFsvolWriteCached(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));
    else
        Result = FspFsvolWriteNonCached(FsvolDeviceObject, Irp, IrpSp);

exit:
    FspResetTopLevelIrp(TopLevel);

    return Result;
}

static NTSTATUS FspFsvolWriteCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSCTL_TRANSACT_REQ *RequestWorkItem = FspIrpRequest(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
#if 0
    ULONG WriteKey = IrpSp->Parameters.Write.Key;
    LARGE_INTEGER WriteOffset = IrpSp->Parameters.Write.ByteOffset;
#endif
    ULONG WriteLength = IrpSp->Parameters.Write.Length;
#if 0
    BOOLEAN WriteToEof =
        FILE_WRITE_TO_END_OF_FILE == WriteOffset.LowPart && -1L == WriteOffset.HighPart;
#endif

    ASSERT(FileNode == FileDesc->FileNode);

    /* should we defer the write? */
    if (DEBUGRANDTEST(10, TRUE) ||
        !CcCanIWrite(FileObject, WriteLength, CanWait, 0 != RequestWorkItem))
    {
        Result = FspWqCreateIrpWorkItem(Irp, FspFsvolWriteCached, 0);
        if (NT_SUCCESS(Result))
        {
            IoMarkIrpPending(Irp);
            CcDeferWrite(FileObject, FspFsvolWriteCachedDeferred, Irp, 0, WriteLength,
                0 != RequestWorkItem);

            return STATUS_PENDING;
        }

        /* if we are unable to defer we will go ahead and (try to) service the IRP now! */
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}

static VOID FspFsvolWriteCachedDeferred(PVOID Context1, PVOID Context2)
{
    FspWqPostIrpWorkItem(Context1);
}

static NTSTATUS FspFsvolWriteNonCached(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspFsvolWriteComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC(
        "FileObject=%p, UserBuffer=%p, MdlAddress=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%ld",
        IrpSp->FileObject, Irp->UserBuffer, Irp->MdlAddress,
        IrpSp->Parameters.Write.Key,
        IrpSp->Parameters.Write.ByteOffset.HighPart, IrpSp->Parameters.Write.ByteOffset.LowPart,
        IrpSp->Parameters.Write.Length);
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
