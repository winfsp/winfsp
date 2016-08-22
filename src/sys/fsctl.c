/**
 * @file sys/fsctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControlReparsePoint(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN IsWrite);
static NTSTATUS FspFsvolFileSystemControlReparsePointComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response,
    BOOLEAN IsWrite);
static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolFileSystemControlComplete;
static FSP_IOP_REQUEST_FINI FspFsvolFileSystemControlRequestFini;
FSP_DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlReparsePoint)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlReparsePointComplete)
#pragma alloc_text(PAGE, FspFsvolFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlComplete)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlRequestFini)
#pragma alloc_text(PAGE, FspFileSystemControl)
#endif

enum
{
    RequestFileNode                     = 0,
};

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_VOLUME_NAME:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeGetName(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_VOLUME_LIST:
            Result = FspVolumeGetNameList(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_TRANSACT:
        case FSP_FSCTL_TRANSACT_BATCH:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeTransact(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_STOP:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeStop(FsctlDeviceObject, Irp, IrpSp);
            break;
        }
        break;
    case IRP_MN_MOUNT_VOLUME:
        Result = FspVolumeMount(FsctlDeviceObject, Irp, IrpSp);
        break;
    }

    return Result;
}

static NTSTATUS FspFsvolFileSystemControlReparsePoint(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN IsWrite)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    ULONG FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;
    PVOID InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    PVOID OutputBuffer = Irp->UserBuffer;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PREPARSE_DATA_BUFFER ReparseData;
    PWSTR PathBuffer;
    BOOLEAN TargetOnFileSystem = FALSE;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    /* do we support reparse points? */
    if (!FsvolDeviceExtension->VolumeParams.ReparsePoints)
        return STATUS_INVALID_DEVICE_REQUEST;

    if (IsWrite)
    {
        if (0 == InputBuffer || 0 == InputBufferLength ||
            FSP_FSCTL_TRANSACT_REQ_SIZEMAX - FIELD_OFFSET(FSP_FSCTL_TRANSACT_REQ, Buffer) -
                (FileNode->FileName.Length + sizeof(WCHAR)) < InputBufferLength)
            return STATUS_INVALID_PARAMETER;

        Result = FsRtlValidateReparsePointBuffer(InputBufferLength, InputBuffer);
        if (!NT_SUCCESS(Result))
            return Result;

        ReparseData = (PREPARSE_DATA_BUFFER)InputBuffer;

        if (IO_REPARSE_TAG_SYMLINK == ReparseData->ReparseTag)
        {
            /* NTFS severely limits symbolic links; we will not do that unless our file system asks */
            if (FsvolDeviceExtension->VolumeParams.ReparsePointsAccessCheck)
            {
                if (KernelMode != Irp->RequestorMode &&
                    SeSinglePrivilegeCheck(RtlConvertLongToLuid(SE_CREATE_SYMBOLIC_LINK_PRIVILEGE),
                        UserMode))
                    return STATUS_ACCESS_DENIED;
            }

            /* determine if target resides on same device as link (convenience for user mode) */
            PathBuffer = ReparseData->SymbolicLinkReparseBuffer.PathBuffer +
                ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR);
            if (ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength > 4 * sizeof(WCHAR) &&
                '\\' == PathBuffer[0] &&
                '?'  == PathBuffer[1] &&
                '?'  == PathBuffer[2] &&
                '\\' == PathBuffer[3])
            {
                UNICODE_STRING TargetDeviceName;
                PFILE_OBJECT TargetDeviceFile;
                PDEVICE_OBJECT TargetDeviceObject;

                RtlInitEmptyUnicodeString(&TargetDeviceName,
                    PathBuffer,
                    ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength);

                /* the first path component is assumed to be the device name */
                TargetDeviceName.Length = 4 * sizeof(WCHAR);
                while (TargetDeviceName.Length < TargetDeviceName.MaximumLength &&
                    '\\' != TargetDeviceName.Buffer[TargetDeviceName.Length / sizeof(WCHAR)])
                    TargetDeviceName.Length += sizeof(WCHAR);

                Result = IoGetDeviceObjectPointer(&TargetDeviceName,
                    FILE_READ_ATTRIBUTES, &TargetDeviceFile, &TargetDeviceObject);
                if (!NT_SUCCESS(Result))
                    goto target_check_exit;

                TargetOnFileSystem = IoGetRelatedDeviceObject(FileObject) == TargetDeviceObject;
                ObDereferenceObject(TargetDeviceFile);

            target_check_exit:
                ;
            }
        }
    }
    else
    {
        if (0 != InputBuffer || 0 != InputBufferLength ||
            0 == OutputBuffer || 0 == OutputBufferLength)
            return STATUS_INVALID_PARAMETER;

        Result = FspBufferUserBuffer(Irp, OutputBufferLength, IoWriteAccess);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    if (IsWrite)
        FspFileNodeAcquireExclusive(FileNode, Full);
    else
        FspFileNodeAcquireShared(FileNode, Full);

    Result = FspIopCreateRequestEx(Irp, &FileNode->FileName, InputBufferLength,
        FspFsvolFileSystemControlRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactFileSystemControlKind;
    Request->Req.FileSystemControl.UserContext = FileNode->UserContext;
    Request->Req.FileSystemControl.UserContext2 = FileDesc->UserContext2;
    Request->Req.FileSystemControl.FsControlCode = FsControlCode;
    if (IsWrite)
    {
        Request->Req.FileSystemControl.Buffer.Offset = Request->FileName.Size;
        Request->Req.FileSystemControl.Buffer.Size = (UINT16)InputBufferLength;
        RtlCopyMemory(Request->Buffer + Request->Req.FileSystemControl.Buffer.Offset,
            InputBuffer, InputBufferLength);

        Request->Req.FileSystemControl.TargetOnFileSystem = TargetOnFileSystem;
    }

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

static NTSTATUS FspFsvolFileSystemControlReparsePointComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response,
    BOOLEAN IsWrite)
{
    PAGED_CODE();

    if (!IsWrite)
        return STATUS_SUCCESS;

    NTSTATUS Result;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PVOID OutputBuffer = Irp->AssociatedIrp.SystemBuffer; /* see FspBufferUserBuffer call */
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

    if (Response->Buffer + Response->Rsp.FileSystemControl.Buffer.Offset +
        Response->Rsp.FileSystemControl.Buffer.Size > (PUINT8)Response + Response->Size)
        return STATUS_IO_REPARSE_DATA_INVALID;

    Result = FsRtlValidateReparsePointBuffer(Response->Rsp.FileSystemControl.Buffer.Size,
        (PVOID)(Response->Buffer + Response->Rsp.FileSystemControl.Buffer.Offset));
    if (!NT_SUCCESS(Result))
        return Result;

    if (Response->Rsp.FileSystemControl.Buffer.Size > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    RtlCopyMemory(OutputBuffer, Response->Buffer + Response->Rsp.FileSystemControl.Buffer.Offset,
        Response->Rsp.FileSystemControl.Buffer.Size);

    Irp->IoStatus.Information = Response->Rsp.FileSystemControl.Buffer.Size;

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSP_FSCTL_WORK:
        case FSP_FSCTL_WORK_BEST_EFFORT:
            Result = FspVolumeWork(FsvolDeviceObject, Irp, IrpSp);
            break;
        case FSCTL_GET_REPARSE_POINT:
            Result = FspFsvolFileSystemControlReparsePoint(FsvolDeviceObject, Irp, IrpSp, FALSE);
            break;
        case FSCTL_SET_REPARSE_POINT:
        case FSCTL_DELETE_REPARSE_POINT:
            Result = FspFsvolFileSystemControlReparsePoint(FsvolDeviceObject, Irp, IrpSp, TRUE);
            break;
        }
        break;
    }

    return Result;
}

NTSTATUS FspFsvolFileSystemControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
        switch (IrpSp->Parameters.FileSystemControl.FsControlCode)
        {
        case FSCTL_GET_REPARSE_POINT:
            Result = FspFsvolFileSystemControlReparsePointComplete(Irp, Response, FALSE);
            break;
        case FSCTL_SET_REPARSE_POINT:
        case FSCTL_DELETE_REPARSE_POINT:
            Result = FspFsvolFileSystemControlReparsePointComplete(Irp, Response, TRUE);
            break;
        default:
            ASSERT(0);
            Result = STATUS_INVALID_PARAMETER;
            break;
        }
        break;
    default:
        ASSERT(0);
        Result = STATUS_INVALID_PARAMETER;
        break;
    }

    FspIopRequestContext(Request, RequestFileNode) = 0;
    FspFileNodeReleaseOwner(FileNode, Full, Request);

    FSP_LEAVE_IOC(
        "%s%sFileObject=%p",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ?
            IoctlCodeSym(IrpSp->Parameters.FileSystemControl.FsControlCode) : "",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ? ", " : "",
        IrpSp->FileObject);
}

static VOID FspFsvolFileSystemControlRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

NTSTATUS FspFileSystemControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolFileSystemControl(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlFileSystemControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ(
        "%s%sFileObject=%p",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ?
            IoctlCodeSym(IrpSp->Parameters.FileSystemControl.FsControlCode) : "",
        IRP_MN_USER_FS_REQUEST == IrpSp->MinorFunction ? ", " : "",
        IrpSp->FileObject);
}
