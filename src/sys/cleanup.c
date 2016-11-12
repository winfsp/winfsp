/**
 * @file sys/cleanup.c
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
    BOOLEAN DeletePending;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeAcquireExclusive(FileNode, Main);

    FspFileNodeCleanup(FileNode, FileObject, &DeletePending);

    /* if this is a directory inform the FSRTL Notify mechanism */
    if (FileNode->IsDirectory)
    {
        if (DeletePending)
            FspNotifyDeletePending(
                FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList, FileNode);

        FspNotifyCleanup(
            FsvolDeviceExtension->NotifySync, &FsvolDeviceExtension->NotifyList, FileDesc);
    }

    /* remove any locks for this file object */
    FspFileNodeUnlockAll(FileNode, FileObject, IoGetRequestorProcess(Irp));

    /* create the user-mode file system request; MustSucceed because IRP_MJ_CLEANUP cannot fail */
    FspIopCreateRequestMustSucceedEx(Irp, DeletePending ? &FileNode->FileName : 0, 0,
        FspFsvolCleanupRequestFini, &Request);
    Request->Kind = FspFsctlTransactCleanupKind;
    Request->Req.Cleanup.UserContext = FileNode->UserContext;
    Request->Req.Cleanup.UserContext2 = FileDesc->UserContext2;
    Request->Req.Cleanup.Delete = DeletePending;

    FspFileNodeAcquireExclusive(FileNode, Pgio);

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;

    if (DeletePending || !FsvolDeviceExtension->VolumeParams.PostCleanupOnDeleteOnly)
        /*
         * Note that it is still possible for this request to not be delivered,
         * if the volume device Ioq is stopped. But such failures are benign
         * from our perspective, because they mean that the file system is going
         * away and should correctly tear things down.
         */
        return FSP_STATUS_IOQ_POST_BEST_EFFORT;
    else
    {
        /* if the file is being resized invalidate the volume info */
        if (FileNode->TruncateOnClose)
            FspFsvolDeviceInvalidateVolumeInfo(IrpSp->DeviceObject);

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

    /* if the file is being deleted do a change notification */
    if (Request->Req.Cleanup.Delete)
        FspFileNodeNotifyChange(FileNode,
            FileNode->IsDirectory ? FILE_NOTIFY_CHANGE_DIR_NAME : FILE_NOTIFY_CHANGE_FILE_NAME,
            FILE_ACTION_REMOVED);
    /* if the file is being resized invalidate the volume info */
    else if (FileNode->TruncateOnClose)
        FspFsvolDeviceInvalidateVolumeInfo(IrpSp->DeviceObject);

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

    FspFileNodeCleanupComplete(FileNode, FileObject);
    if (!FileNode->IsDirectory)
        FspCheckOplock(FspFileNodeAddrOfOplock(FileNode), Irp, 0, 0, 0);
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
