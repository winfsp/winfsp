/**
 * @file sys/flush.c
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

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolFlushBuffersComplete;
static FSP_IOP_REQUEST_FINI FspFsvolFlushBuffersRequestFini;
FSP_DRIVER_DISPATCH FspFlushBuffers;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolFlushBuffers)
#pragma alloc_text(PAGE, FspFsvolFlushBuffersComplete)
#pragma alloc_text(PAGE, FspFsvolFlushBuffersRequestFini)
#pragma alloc_text(PAGE, FspFlushBuffers)
#endif

enum
{
    /* FlushBuffers */
    RequestFileNode                     = 0,
    RequestFlushResult                  = 1,
};

static NTSTATUS FspFsvolFlushBuffers(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result, FlushResult;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FILE_NODE **FileNodes;
    ULONG FileNodeCount, Index;
    PIRP TopLevelIrp;
    IO_STATUS_BLOCK IoStatus;
    FSP_FSCTL_TRANSACT_REQ *Request;

    /*
     * A flush request on the volume (or the root directory according to FastFat)
     * is a request to flush the whole volume.
     */
    if (!FspFileNodeIsValid(FileNode) || FileNode->IsRootDirectory)
    {
        Result = FspFileNodeCopyOpenList(FsvolDeviceObject, &FileNodes, &FileNodeCount);
        if (!NT_SUCCESS(Result))
            return Result;

        /* reset the top-level IRP to avoid deadlock on the FileNodes' resources */
        TopLevelIrp = IoGetTopLevelIrp();
        IoSetTopLevelIrp(0);

        /*
         * Enumerate in reverse order so that files are flushed before containing directories.
         * This would be useful if we ever started flushing directories, but since we do not
         * it is not as important now.
         */
        FlushResult = STATUS_SUCCESS;
        for (Index = FileNodeCount - 1; FileNodeCount > Index; Index--)
            if (!FileNodes[Index]->IsDirectory)
            {
                Result = FspCcFlushCache(&FileNodes[Index]->NonPaged->SectionObjectPointers,
                    0, 0, &IoStatus);
                if (!NT_SUCCESS(Result) && NT_SUCCESS(FlushResult))
                    FlushResult = Result;
            }

        IoSetTopLevelIrp(TopLevelIrp);

        FspFileNodeDeleteList(FileNodes, FileNodeCount);

        Result = FspIopCreateRequest(Irp, 0, 0, &Request);
        if (!NT_SUCCESS(Result))
            return Result;

        /* a NULL UserContext indicates to user-mode to flush the whole volume! */
        Request->Kind = FspFsctlTransactFlushBuffersKind;
        //Request->Req.FlushBuffers.UserContext = 0;
        //Request->Req.FlushBuffers.UserContext2 = 0;

        FspIopRequestContext(Request, RequestFileNode) = 0;
        FspIopRequestContext(Request, RequestFlushResult) = (PVOID)(UINT_PTR)FlushResult;

        return FSP_STATUS_IOQ_POST;
    }
    else
    {
        ASSERT(FileNode == FileDesc->FileNode);

        /* cannot really flush directories but we will say we did it! */
        if (FileNode->IsDirectory)
            return STATUS_SUCCESS;

        FspFileNodeAcquireExclusive(FileNode, Full);

        Result = FspCcFlushCache(FileObject->SectionObjectPointer, 0, 0, &IoStatus);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Full);
            return Result;
        }

        Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolFlushBuffersRequestFini, &Request);
        if (!NT_SUCCESS(Result))
        {
            FspFileNodeRelease(FileNode, Full);
            return Result;
        }

        Request->Kind = FspFsctlTransactFlushBuffersKind;
        Request->Req.FlushBuffers.UserContext = FileNode->UserContext;
        Request->Req.FlushBuffers.UserContext2 = FileDesc->UserContext2;

        FspFileNodeSetOwner(FileNode, Full, Request);
        FspIopRequestContext(Request, RequestFileNode) = FileNode;
        FspIopRequestContext(Request, RequestFlushResult) = 0;

        return FSP_STATUS_IOQ_POST;
    }
}

NTSTATUS FspFsvolFlushBuffersComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    NTSTATUS FlushResult = (NTSTATUS)(UINT_PTR)FspIopRequestContext(Request, RequestFlushResult);

    Irp->IoStatus.Information = 0;
    if (!NT_SUCCESS(Response->IoStatus.Status))
        Result = Response->IoStatus.Status;
    else if (!NT_SUCCESS(FlushResult))
        Result = FlushResult;
    else
    {
        PFILE_OBJECT FileObject = IrpSp->FileObject;
        FSP_FILE_NODE *FileNode = FileObject->FsContext;

        /*
         * A flush request on the volume (or the root directory according to FastFat)
         * is a request to flush the whole volume.
         */
        if (!FspFileNodeIsValid(FileNode) || FileNode->IsRootDirectory)
            ;
        else
            FspFileNodeSetFileInfo(FileNode, FileObject, &Response->Rsp.FlushBuffers.FileInfo, TRUE);

        Result = STATUS_SUCCESS;
    }

    FSP_LEAVE_IOC("FileObject=%p",
        IrpSp->FileObject);
}

static VOID FspFsvolFlushBuffersRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

NTSTATUS FspFlushBuffers(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFlushBuffers(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p",
        IrpSp->FileObject);
}
