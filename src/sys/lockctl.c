/**
 * @file sys/lockctl.c
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

static NTSTATUS FspFsvolLockControlRetry(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
static NTSTATUS FspFsvolLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolLockControlComplete;
FSP_DRIVER_DISPATCH FspLockControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolLockControlRetry)
#pragma alloc_text(PAGE, FspFsvolLockControl)
#pragma alloc_text(PAGE, FspFsvolLockControlComplete)
#pragma alloc_text(PAGE, FspLockControl)
#endif

static NTSTATUS FspFsvolLockControlRetry(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    BOOLEAN Success;

    /* try to acquire the FileNode shared Main */
    Success = DEBUGTEST(90) &&
        FspFileNodeTryAcquireSharedF(FileNode, FspFileNodeAcquireMain, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolLockControlRetry, 0);

    /* perform oplock check; we are only implementing Win7 behavior */
    Result = FspFileNodeOplockCheckAsync(
        FileNode, FspFileNodeAcquireMain, FspFsvolLockControlRetry,
        Irp);
    if (STATUS_PENDING == Result)
        return Result;
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Main);
        return Result;
    }

    ULONG IrpFlags = FspIrpFlags(Irp);
    IoSetTopLevelIrp(0);

    /* let the FSRTL package handle this one! */
    Result = FspFileNodeProcessLockIrp(FileNode, Irp);

    FspFileNodeReleaseF(FileNode, IrpFlags);

    return Result | FSP_STATUS_IGNORE_BIT;
}

static NTSTATUS FspFsvolLockControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;

    /* only regular files can be locked */
    if (FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    Result = FspFsvolLockControlRetry(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));

    return Result;
}

NTSTATUS FspFsvolLockControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%lld",
        IrpSp->FileObject,
        IrpSp->Parameters.LockControl.Key,
        IrpSp->Parameters.LockControl.ByteOffset.HighPart,
        IrpSp->Parameters.LockControl.ByteOffset.LowPart,
        IrpSp->Parameters.LockControl.Length->QuadPart);
}

NTSTATUS FspLockControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolLockControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p, "
        "Key=%#lx, ByteOffset=%#lx:%#lx, Length=%lld",
        IrpSp->FileObject,
        IrpSp->Parameters.LockControl.Key,
        IrpSp->Parameters.LockControl.ByteOffset.HighPart,
        IrpSp->Parameters.LockControl.ByteOffset.LowPart,
        IrpSp->Parameters.LockControl.Length->QuadPart);
}
