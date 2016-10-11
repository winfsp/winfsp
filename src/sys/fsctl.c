/**
 * @file sys/fsctl.c
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

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;

    /* do we support reparse points? */
    if (!FsvolDeviceExtension->VolumeParams.ReparsePoints)
        return STATUS_INVALID_DEVICE_REQUEST;

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    ULONG FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;
    PVOID InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    PVOID OutputBuffer = Irp->UserBuffer;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PREPARSE_DATA_BUFFER ReparseData;
    PWSTR ReparseTargetPath;
    USHORT ReparseTargetPathLength;
    UINT16 TargetOnFileSystem = 0;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    if (IsWrite)
    {
        if (0 == InputBuffer || 0 == InputBufferLength ||
            FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMAX - (FileNode->FileName.Length + sizeof(WCHAR)) <
                InputBufferLength)
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
                    !SeSinglePrivilegeCheck(RtlConvertLongToLuid(SE_CREATE_SYMBOLIC_LINK_PRIVILEGE),
                        UserMode))
                    return STATUS_PRIVILEGE_NOT_HELD;
            }

            ReparseTargetPath = ReparseData->SymbolicLinkReparseBuffer.PathBuffer +
                ReparseData->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR);
            ReparseTargetPathLength = ReparseData->SymbolicLinkReparseBuffer.SubstituteNameLength;

            /* is this an absolute path? determine if target resides on same device as link */
            if (!FlagOn(ReparseData->SymbolicLinkReparseBuffer.Flags, SYMLINK_FLAG_RELATIVE) &&
                ReparseTargetPathLength >= sizeof(WCHAR) && L'\\' == ReparseTargetPath[0])
            {
                UNICODE_STRING TargetObjectName;
                PDEVICE_OBJECT TargetDeviceObject;
                PFILE_OBJECT TargetFileObject;
                ULONG TargetFileNameIndex;
                ULONG32 TargetProviderId;
                FSRTL_MUP_PROVIDER_INFO_LEVEL_1 ProviderInfo;
                ULONG ProviderInfoSize;

                TargetObjectName.Length = TargetObjectName.MaximumLength = ReparseTargetPathLength;
                TargetObjectName.Buffer = ReparseTargetPath;

                /* get a pointer to the target device */
                Result = FspGetDeviceObjectPointer(&TargetObjectName, FILE_READ_DATA,
                    &TargetFileNameIndex, &TargetFileObject, &TargetDeviceObject);
                if (!NT_SUCCESS(Result))
                    goto target_check_exit;

                /* is the target device the same as ours? */
                if (TargetFileNameIndex < ReparseTargetPathLength &&
                    IoGetRelatedDeviceObject(FileObject) == TargetDeviceObject)
                {
                    if (0 == FsvolDeviceExtension->VolumePrefix.Length)
                        /* not going thru MUP: DONE! */
                        TargetOnFileSystem = (UINT16)TargetFileNameIndex;
                    else
                    {
                        /* going thru MUP cases: \Device\Volume{GUID} and \??\UNC\{VolumePrefix} */
                        ProviderInfoSize = sizeof ProviderInfo;
                        Result = FsRtlMupGetProviderInfoFromFileObject(TargetFileObject, 1,
                            &ProviderInfo, &ProviderInfoSize);
                        if (NT_SUCCESS(Result))
                        {
                            /* case \Device\Volume{GUID}: is the targer provider id same as ours? */

                            TargetProviderId = ProviderInfo.ProviderId;

                            ProviderInfoSize = sizeof ProviderInfo;
                            Result = FsRtlMupGetProviderInfoFromFileObject(FileObject, 1,
                                &ProviderInfo, &ProviderInfoSize);
                            if (!NT_SUCCESS(Result))
                                goto target_check_exit;

                            if (ProviderInfo.ProviderId == TargetProviderId)
                                TargetOnFileSystem = (UINT16)TargetFileNameIndex;
                        }
                        else
                        {
                            /* case \??\UNC\{VolumePrefix}: is the target volume prefix same as ours? */

                            TargetObjectName.Length = TargetObjectName.MaximumLength =
                                FsvolDeviceExtension->VolumePrefix.Length;
                            TargetObjectName.Buffer = ReparseTargetPath +
                                TargetFileNameIndex / sizeof(WCHAR);

                            TargetFileNameIndex += FsvolDeviceExtension->VolumePrefix.Length;

                            if (TargetFileNameIndex < ReparseTargetPathLength &&
                                RtlEqualUnicodeString(&FsvolDeviceExtension->VolumePrefix,
                                    &TargetObjectName,
                                    FSP_VOLUME_PREFIX_CASE_INS))
                                TargetOnFileSystem = (UINT16)TargetFileNameIndex;
                        }
                    }
                }

                ObDereferenceObject(TargetFileObject);

            target_check_exit:
                ;
            }
        }

        FspFileNodeAcquireExclusive(FileNode, Full);
    }
    else
    {
        if (0 == OutputBuffer || 0 == OutputBufferLength)
            return STATUS_INVALID_PARAMETER;

        /*
         * NtFsControlFile (IopXxxControlFile) will setup Irp->AssociatedIrp.SystemBuffer
         * with enough space for either InputBufferLength or OutputBufferLength. There is
         * no need to call FspBufferUserBuffer ourselves.
         */

        FspFileNodeAcquireShared(FileNode, Full);
    }

    Result = FspIopCreateRequestEx(Irp, &FileNode->FileName, IsWrite ? InputBufferLength : 0,
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

    if (IsWrite)
        return STATUS_SUCCESS;

    NTSTATUS Result;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    PVOID OutputBuffer = Irp->AssociatedIrp.SystemBuffer;
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

    /* exit now if we do not have a FileObject (FSP_FSCTL_WORK*) */
    if (0 == IrpSp->FileObject)
        FSP_RETURN();

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    Result = STATUS_INVALID_DEVICE_REQUEST;
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
        }
        break;
    }

    ASSERT(STATUS_INVALID_DEVICE_REQUEST != Result);

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
