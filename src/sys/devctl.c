/**
 * @file sys/devctl.c
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

static NTSTATUS FspFsvrtDeviceControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static BOOLEAN FspFsvrtDeviceControlStorageQuery(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    PNTSTATUS PResult);
static NTSTATUS FspFsvolDeviceControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolDeviceControlComplete;
static FSP_IOP_REQUEST_FINI FspFsvolDeviceControlRequestFini;
FSP_DRIVER_DISPATCH FspDeviceControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvrtDeviceControl)
#pragma alloc_text(PAGE, FspFsvrtDeviceControlStorageQuery)
#pragma alloc_text(PAGE, FspFsvolDeviceControl)
#pragma alloc_text(PAGE, FspFsvolDeviceControlComplete)
#pragma alloc_text(PAGE, FspFsvolDeviceControlRequestFini)
#pragma alloc_text(PAGE, FspDeviceControl)
#endif

enum
{
    RequestFileNode                     = 0,
};

static NTSTATUS FspFsvrtDeviceControl(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;

    if (FspFsvrtDeviceControlStorageQuery(FsvrtDeviceObject, Irp, IrpSp, &Result))
        return Result;

    if (FspMountdevDeviceControl(FsvrtDeviceObject, Irp, IrpSp, &Result))
        return Result;

    /*
     * Fix GitHub issue #177. All credit for the investigation of this issue
     * and the suggested steps to reproduce and work around the problem goes
     * to GitHub user @thinkport.
     *
     * When Windows attempts to mount a new volume it iterates over all disk file
     * systems registered with IoRegisterFileSystem. Foreign (i.e. non-WinFsp) file
     * systems would in some cases attempt to mount our Fsvrt volume device by
     * sending it unknown IOCTL codes, which would then be failed with
     * STATUS_INVALID_DEVICE_REQUEST. Unfortunately the file systems would then
     * report this error code to the I/O Manager, which would cause it to abort the
     * mounting process completely and thus WinFsp would never get a chance to
     * mount its own volume device!
     */
    return STATUS_UNRECOGNIZED_VOLUME;
}

static BOOLEAN FspFsvrtDeviceControlStorageQuery(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    PNTSTATUS PResult)
{
    PAGED_CODE();

    /*
     * SQL Server insists on sending us storage level IOCTL's even though
     * WinFsp file systems are not on top of a real disk. So bite the bullet
     * and implement some of those storage IOCTL's to make SQL Server happy.
     */

    if (IOCTL_STORAGE_QUERY_PROPERTY != IrpSp->Parameters.DeviceIoControl.IoControlCode ||
        sizeof(STORAGE_PROPERTY_QUERY) > IrpSp->Parameters.DeviceIoControl.InputBufferLength)
        return FALSE;

    PSTORAGE_PROPERTY_QUERY Query = Irp->AssociatedIrp.SystemBuffer;
    switch (Query->PropertyId)
    {
    case StorageAccessAlignmentProperty:
        if (PropertyExistsQuery == Query->QueryType)
            *PResult = STATUS_SUCCESS;
        else if (PropertyStandardQuery == Query->QueryType)
        {
            FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension = FspFsvrtDeviceExtension(FsvrtDeviceObject);
            PSTORAGE_ACCESS_ALIGNMENT_DESCRIPTOR Descriptor = Irp->AssociatedIrp.SystemBuffer;
            ULONG OutputBufferLength = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;

            if (sizeof(STORAGE_DESCRIPTOR_HEADER) > OutputBufferLength)
                *PResult = STATUS_BUFFER_TOO_SMALL;
            else if (sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR) > OutputBufferLength)
            {
                Descriptor->Version = sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR);
                Descriptor->Size = sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR);
                Irp->IoStatus.Information = sizeof(STORAGE_DESCRIPTOR_HEADER);
                *PResult = STATUS_SUCCESS;
            }
            else
            {
                RtlZeroMemory(Descriptor, sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR));
                Descriptor->Version = sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR);
                Descriptor->Size = sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR);
                Descriptor->BytesPerLogicalSector = FsvrtDeviceExtension->SectorSize;
                Descriptor->BytesPerPhysicalSector = FsvrtDeviceExtension->SectorSize;
                Irp->IoStatus.Information = sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR);
                *PResult = STATUS_SUCCESS;
            }
        }
        else
            *PResult = STATUS_NOT_SUPPORTED;
        return TRUE;
    }

    return FALSE;
}

static NTSTATUS FspFsvolDeviceControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    ULONG IoControlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    NTSTATUS Result;

    /*
     * Possibly forward the IOCTL request to the user mode file system. The rules are:
     *
     * - File system must support DeviceControl.
     * - Only IOCTL with custom devices (see DEVICE_TYPE_FROM_CTL_CODE) and
     *   METHOD_BUFFERED will be forwarded.
     */

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
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtDeviceControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "%s, FileObject=%p",
        IoctlCodeSym(IrpSp->Parameters.DeviceIoControl.IoControlCode),
        IrpSp->FileObject);
}
