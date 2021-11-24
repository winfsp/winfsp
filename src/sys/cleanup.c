/**
 * @file sys/cleanup.c
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

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolCleanupComplete;
static FSP_IOP_REQUEST_FINI FspFsvolCleanupRequestFini;
FSP_DRIVER_DISPATCH FspCleanup;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCleanup)
#pragma alloc_text(PAGE, FspFsvrtCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanupComplete)
#pragma alloc_text(PAGE, FspFsvolCleanupRequestFini)
#pragma alloc_text(PAGE, FspCleanup)
#endif

enum
{
    /* Cleanup */
    RequestIrp                          = 0,
};

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    if (0 != IrpSp->FileObject->FsContext2)
        FspVolumeDelete(DeviceObject, Irp, IrpSp);

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    Irp->IoStatus.Information = 0;
    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_SUCCESS;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FSCTL_TRANSACT_REQ *Request;
    ULONG CleanupFlags;
    BOOLEAN Delete, SetAllocationSize, FileModified;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeAcquireExclusive(FileNode, Main);

    FspFileNodeCleanup(FileNode, FileObject, &CleanupFlags);
    Delete = CleanupFlags & 1;
    SetAllocationSize = !!(CleanupFlags & 2);
    FileModified = BooleanFlagOn(FileObject->Flags, FO_FILE_MODIFIED);

    /* if this is a directory inform the FSRTL Notify mechanism */
    if (FileNode->IsDirectory)
    {
        if (Delete)
            FspNotifyDeletePending(
                FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList, FileNode);

        FspNotifyCleanup(
            FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList, FileDesc);
    }

    /* remove any locks for this file object */
    FspFileNodeUnlockAll(FileNode, FileObject, IoGetRequestorProcess(Irp));

    /* create the user-mode file system request; MustSucceed because IRP_MJ_CLEANUP cannot fail */
    FspIopCreateRequestMustSucceedEx(Irp, Delete ? &FileNode->FileName : 0, 0,
        FspFsvolCleanupRequestFini, &Request);
    Request->Kind = FspFsctlTransactCleanupKind;
    Request->Req.Cleanup.UserContext = FileNode->UserContext;
    Request->Req.Cleanup.UserContext2 = FileDesc->UserContext2;
    Request->Req.Cleanup.Delete = Delete;
    Request->Req.Cleanup.SetAllocationSize = SetAllocationSize;
    Request->Req.Cleanup.SetArchiveBit = (FileModified || FileDesc->DidSetSecurity) &&
        !FileDesc->DidSetFileAttributes;
    Request->Req.Cleanup.SetLastAccessTime = !FileDesc->DidSetLastAccessTime;
    Request->Req.Cleanup.SetLastWriteTime = FileModified && !FileDesc->DidSetLastWriteTime;
    Request->Req.Cleanup.SetChangeTime = (FileModified || FileDesc->DidSetMetadata) &&
        !FileDesc->DidSetChangeTime;

    FspFileNodeAcquireExclusive(FileNode, Pgio);

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;

    FspFileNodeCleanupFlush(FileNode, FileObject);

    if (Request->Req.Cleanup.Delete ||
        Request->Req.Cleanup.SetAllocationSize ||
        Request->Req.Cleanup.SetArchiveBit ||
        Request->Req.Cleanup.SetLastWriteTime ||
        Request->Req.Cleanup.SetChangeTime ||
        !FsvolDeviceExtension->VolumeParams.PostCleanupWhenModifiedOnly)
        /*
         * Note that it is still possible for this request to not be delivered,
         * if the volume device Ioq is stopped. But such failures are benign
         * from our perspective, because they mean that the file system is going
         * away and should correctly tear things down.
         */
        return FSP_STATUS_IOQ_POST_BEST_EFFORT;
    else
    {
        if (FileDesc->DidSetMetadata)
        {
            if (0 == FileNode->MainFileNode)
                FspFileNodeInvalidateParentDirInfo(FileNode);
            else
                FspFileNodeInvalidateStreamInfo(FileNode);
        }

        return STATUS_SUCCESS; /* FspFsvolCleanupRequestFini will take care of the rest! */
    }
}

NTSTATUS FspFsvolCleanupComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    ULONG NotifyFilter, NotifyAction;

    ASSERT(FileNode == FileDesc->FileNode);

    /* send the appropriate notification; also invalidate dirinfo/etc. caches */
    if (Request->Req.Cleanup.Delete)
    {
        NotifyFilter = FileNode->IsDirectory ?
            FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME;
        NotifyAction = FILE_ACTION_REMOVED;
    }
    else if (FileNode->PosixDelete)
    {
        NotifyFilter = 0;
        NotifyAction = 0;
    }
    else
    {
        /* send notification for any metadata changes */
        NotifyFilter = 0;
#if 0
        /* do not send notification when resetting the allocation size */
        if (Request->Req.Cleanup.SetAllocationSize)
            NotifyFilter |= FILE_NOTIFY_CHANGE_SIZE;
#endif
        if (Request->Req.Cleanup.SetArchiveBit)
            NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
#if 0
        /* do not send notification for implicit LastAccessTime changes */
        if (Request->Req.Cleanup.SetLastAccessTime)
            NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
#endif
        if (Request->Req.Cleanup.SetLastWriteTime)
            NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
        NotifyAction = FILE_ACTION_MODIFIED;
    }

    if (0 != NotifyFilter)
        FspFileNodeNotifyChange(FileNode, NotifyFilter, NotifyAction, TRUE);
    else
    {
        if (FileDesc->DidSetMetadata)
        {
            if (0 == FileNode->MainFileNode)
                FspFileNodeInvalidateParentDirInfo(FileNode);
            else
                FspFileNodeInvalidateStreamInfo(FileNode);
        }
    }

    FSP_LEAVE_IOC("FileObject=%p", IrpSp->FileObject);
}

static VOID FspFsvolCleanupRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    /*
     * Cleanup is rather unusual in that we are doing the cleanup post-processing
     * in RequestFini rather than in CleanupComplete. The reason for this is that
     * we want this processing to happen even in the (unlikely) event of the user-
     * mode file system going away, while our Request is queued (in which case the
     * Irp will get cancelled).
     */

    PAGED_CODE();

    PIRP Irp = Context[RequestIrp];
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    HANDLE MainFileHandle;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeReleaseOwner(FileNode, Pgio, Request);

    FspFileNodeCleanupComplete(FileNode, FileObject, !!Request->Req.Cleanup.Delete);
    if (!FileNode->IsDirectory)
        FspFileNodeOplockCheck(FileNode, Irp);
    SetFlag(FileObject->Flags, FO_CLEANUP_COMPLETE);

    MainFileHandle = FileDesc->MainFileHandle;
    FileDesc->MainFileHandle = 0;

    FspFileNodeReleaseOwner(FileNode, Main, Request);

    FspMainFileClose(MainFileHandle, 0);
}

NTSTATUS FspCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCleanup(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCleanup(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCleanup(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
