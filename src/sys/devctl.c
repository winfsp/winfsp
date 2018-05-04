/**
 * @file sys/devctl.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
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

static NTSTATUS FspFsvolDeviceControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolDeviceControlComplete;
static FSP_IOP_REQUEST_FINI FspFsvolDeviceControlRequestFini;
FSP_DRIVER_DISPATCH FspDeviceControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolDeviceControl)
#pragma alloc_text(PAGE, FspFsvolDeviceControlComplete)
#pragma alloc_text(PAGE, FspFsvolDeviceControlRequestFini)
#pragma alloc_text(PAGE, FspDeviceControl)
#endif

enum
{
    RequestFileNode                     = 0,
};

static NTSTATUS FspFsvolDeviceControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    ULONG IoControlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;

    /* do we support DeviceControl? */
    if (!FsvolDeviceExtension->VolumeParams.DeviceControl)
        return STATUS_INVALID_DEVICE_REQUEST;

    /* do not forward IRP's originating in the kernel! */
    if (KernelMode == Irp->RequestorMode)
        return STATUS_INVALID_DEVICE_REQUEST;

    /* only allow custom devices and METHOD_BUFFERED */
    if (0 == (DEVICE_TYPE_FROM_CTL_CODE(IoControlCode) & 0x8000) ||
        METHOD_BUFFERED != METHOD_FROM_CTL_CODE(IoControlCode))
        return STATUS_INVALID_DEVICE_REQUEST;

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(FileObject->FsContext))
        return STATUS_INVALID_PARAMETER;

    NTSTATUS Result;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    PVOID InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG InputBufferLength = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    if (FSP_FSCTL_DEVICECONTROL_SIZEMAX < InputBufferLength ||
        FSP_FSCTL_DEVICECONTROL_SIZEMAX < OutputBufferLength)
        return STATUS_INVALID_BUFFER_SIZE;

    Result = FspIopCreateRequestEx(Irp, 0, InputBufferLength,
        FspFsvolDeviceControlRequestFini, &Request);
    if (!NT_SUCCESS(Result))
        return Result;

    FspFileNodeAcquireShared(FileNode, Full);

    Request->Kind = FspFsctlTransactDeviceControlKind;
    Request->Req.DeviceControl.UserContext = FileNode->UserContext;
    Request->Req.DeviceControl.UserContext2 = FileDesc->UserContext2;
    Request->Req.DeviceControl.IoControlCode = IoControlCode;
    Request->Req.DeviceControl.Buffer.Offset = 0;
    Request->Req.DeviceControl.Buffer.Size = (UINT16)InputBufferLength;
    Request->Req.DeviceControl.OutputLength = OutputBufferLength;
    RtlCopyMemory(Request->Buffer, InputBuffer, InputBufferLength);

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolDeviceControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PVOID OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (Response->Buffer + Response->Rsp.DeviceControl.Buffer.Offset +
        Response->Rsp.DeviceControl.Buffer.Size > (PUINT8)Response + Response->Size)
        FSP_RETURN(Result = STATUS_INTERNAL_ERROR);

    if (OutputBufferLength >= Response->Rsp.DeviceControl.Buffer.Size)
        OutputBufferLength = Response->Rsp.DeviceControl.Buffer.Size;
    else
        Result = STATUS_BUFFER_OVERFLOW;

    RtlCopyMemory(OutputBuffer, Response->Buffer + Response->Rsp.DeviceControl.Buffer.Offset,
        OutputBufferLength);

    Irp->IoStatus.Information = OutputBufferLength;

    FSP_LEAVE_IOC(
        "%s, FileObject=%p",
        IoctlCodeSym(IrpSp->Parameters.DeviceIoControl.IoControlCode),
        IrpSp->FileObject);
}

static VOID FspFsvolDeviceControlRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

NTSTATUS FspDeviceControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolDeviceControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "%s, FileObject=%p",
        IoctlCodeSym(IrpSp->Parameters.DeviceIoControl.IoControlCode),
        IrpSp->FileObject);
}
