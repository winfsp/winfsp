/**
 * @file sys/fsctl.c
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

static NTSTATUS FspFsctlFileSystemControl(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControlReparsePoint(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN IsWrite);
static NTSTATUS FspFsvolFileSystemControlReparsePointComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response,
    BOOLEAN IsWrite);
static NTSTATUS FspFsvolFileSystemControlOplock(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static IO_COMPLETION_ROUTINE FspFsvolFileSystemControlOplockCompletion;
static WORKER_THREAD_ROUTINE FspFsvolFileSystemControlOplockCompletionWork;
static NTSTATUS FspFsvolFileSystemControlQueryPersistentVolumeState(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControlGetStatistics(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControlGetRetrievalPointers(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolFileSystemControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolFileSystemControlComplete;
static FSP_IOP_REQUEST_FINI FspFsvolFileSystemControlRequestFini;
FSP_DRIVER_DISPATCH FspFileSystemControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlFileSystemControl)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlReparsePoint)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlReparsePointComplete)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlOplock)
// !#pragma alloc_text(PAGE, FspFsvolFileSystemControlOplockCompletion)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlOplockCompletionWork)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlQueryPersistentVolumeState)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlGetStatistics)
#pragma alloc_text(PAGE, FspFsvolFileSystemControlGetRetrievalPointers)
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
        case FSP_FSCTL_MOUNTDEV:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeMakeMountdev(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_VOLUME_NAME:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeGetName(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_VOLUME_LIST:
            Result = FspVolumeGetNameList(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_TRANSACT:
        case FSP_FSCTL_TRANSACT_BATCH:
        case FSP_FSCTL_TRANSACT_INTERNAL:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeTransact(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_STOP:
        case FSP_FSCTL_STOP0:
            /* Fix GitHub issue #369
             *
             * The original WinFsp protocol for shutting down a file system was to issue
             * an FSP_FSCTL_STOP control code to the fsctl device. This would set the IOQ
             * to the "stopped" state and would also cancel all active IRP's. Cancelation
             * of IRP's would sometimes free buffers that may have still been in use by
             * the user mode file system threads; hence access violation.
             *
             * To fix this problem a new control code FSP_FSCTL_STOP0 is introduced. The
             * new file system shutdown protocol is backwards compatible with the original
             * one and works as follows:
             *
             * - First the file system process issues an FSP_FSCTL_STOP0 control code which
             * sets the IOQ to the "stopped" state but does NOT cancel IRP's.
             *
             * - Then the file system process waits for its dispatcher threads to complete
             * (see FspFileSystemStopDispatcher).
             *
             * - Finally the file system process issues an FSP_FSCTL_STOP control code
             * which stops the (already stopped) IOQ and cancels all IRP's.
             */
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeStop(FsctlDeviceObject, Irp, IrpSp);
            break;
        case FSP_FSCTL_NOTIFY:
            if (0 != IrpSp->FileObject->FsContext2)
                Result = FspVolumeNotify(FsctlDeviceObject, Irp, IrpSp);
            break;
        default:
            if (CTL_CODE(0, 0xC00, 0, 0) ==
                (IrpSp->Parameters.FileSystemControl.FsControlCode & CTL_CODE(0, 0xC00, 0, 0)))
            {
                if (0 != IrpSp->FileObject->FsContext2)
                    Result = FspVolumeTransactFsext(FsctlDeviceObject, Irp, IrpSp);
            }
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
        return STATUS_INVALID_PARAMETER;

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
        ASSERT(
            FSCTL_SET_REPARSE_POINT == FsControlCode ||
            FSCTL_DELETE_REPARSE_POINT == FsControlCode);

        if (0 == InputBuffer || 0 == InputBufferLength)
            return STATUS_INVALID_BUFFER_SIZE;

        if (0 != OutputBufferLength)
            return STATUS_INVALID_PARAMETER;

        ReparseData = (PREPARSE_DATA_BUFFER)InputBuffer;

        if (FSCTL_SET_REPARSE_POINT == FsControlCode)
        {
            if (FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMAX - (FileNode->FileName.Length + sizeof(WCHAR)) <
                    InputBufferLength)
                return STATUS_IO_REPARSE_DATA_INVALID;

            Result = FsRtlValidateReparsePointBuffer(InputBufferLength, InputBuffer);
            if (!NT_SUCCESS(Result))
                return Result;
        }
        else
        {
            if ((ULONG)FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer) != InputBufferLength &&
                (ULONG)FIELD_OFFSET(REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer) != InputBufferLength)
                return STATUS_IO_REPARSE_DATA_INVALID;

            if (0 != ReparseData->ReparseDataLength)
                return STATUS_IO_REPARSE_DATA_INVALID;

            if (IO_REPARSE_TAG_RESERVED_ZERO == ReparseData->ReparseTag ||
                IO_REPARSE_TAG_RESERVED_ONE == ReparseData->ReparseTag)
                return STATUS_IO_REPARSE_TAG_INVALID;

            if (!IsReparseTagMicrosoft(ReparseData->ReparseTag) &&
                (ULONG)FIELD_OFFSET(REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer) != InputBufferLength)
                return STATUS_IO_REPARSE_DATA_INVALID;
        }

        /* NTFS seems to require one of these rights to allow FSCTL_{SET,DELETE}_REPARSE_POINT */
        if (!FlagOn(FileDesc->GrantedAccess,
            FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES))
            return STATUS_ACCESS_DENIED;

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
                                FspFsvolDeviceVolumePrefixInString(FsvolDeviceObject, &TargetObjectName))
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
        ASSERT(FSCTL_GET_REPARSE_POINT == FsControlCode);

        if (0 != InputBufferLength)
            return STATUS_INVALID_PARAMETER;

        if (0 == OutputBuffer || 0 == OutputBufferLength)
            return STATUS_INVALID_USER_BUFFER;

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
    {
        PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
        PFILE_OBJECT FileObject = IrpSp->FileObject;
        FSP_FILE_NODE *FileNode = FileObject->FsContext;
        FSP_FILE_DESC *FileDesc = IrpSp->FileObject->FsContext2;

        ASSERT(FileNode == FileDesc->FileNode);

        FspFileNodeInvalidateFileInfo(FileNode);

        FileDesc->DidSetReparsePoint = TRUE;
        FileDesc->DidSetMetadata = TRUE;

        return STATUS_SUCCESS;
    }

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

typedef struct
{
    PDEVICE_OBJECT FsvolDeviceObject;
    WORK_QUEUE_ITEM WorkItem;
} FSP_FSVOL_FILESYSTEM_CONTROL_OPLOCK_COMPLETION_CONTEXT;

static NTSTATUS FspFsvolFileSystemControlOplock(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    PFILE_OBJECT FileObject = IrpSp->FileObject;

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    ULONG FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;
    PVOID InputBuffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    ULONG OplockCount;
    FSP_FSVOL_FILESYSTEM_CONTROL_OPLOCK_COMPLETION_CONTEXT *CompletionContext;

    if (FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /*
     * As per FastFat:
     *
     * We grab the Fcb exclusively for oplock requests, shared for oplock
     * break acknowledgement.
     */

    switch (FsControlCode)
    {
    case FSCTL_REQUEST_OPLOCK:
        if (sizeof(REQUEST_OPLOCK_INPUT_BUFFER) > InputBufferLength ||
            sizeof(REQUEST_OPLOCK_OUTPUT_BUFFER) > OutputBufferLength)
            return STATUS_BUFFER_TOO_SMALL;
        if (FlagOn(((PREQUEST_OPLOCK_INPUT_BUFFER)InputBuffer)->Flags,
            REQUEST_OPLOCK_INPUT_FLAG_REQUEST))
            goto exclusive;
        if (FlagOn(((PREQUEST_OPLOCK_INPUT_BUFFER)InputBuffer)->Flags,
            REQUEST_OPLOCK_INPUT_FLAG_ACK))
            goto shared;

        /* one of REQUEST_OPLOCK_INPUT_FLAG_REQUEST or REQUEST_OPLOCK_INPUT_FLAG_ACK required */
        return STATUS_INVALID_PARAMETER;

    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:
    case FSCTL_REQUEST_BATCH_OPLOCK:
    case FSCTL_REQUEST_FILTER_OPLOCK:
    exclusive:
        FspFileNodeAcquireExclusive(FileNode, Main);
        if (!FsRtlOplockIsSharedRequest(Irp))
        {
            FspFsvolDeviceLockContextTable(FsvolDeviceObject);
            OplockCount = FileNode->HandleCount;
            FspFsvolDeviceUnlockContextTable(FsvolDeviceObject);
        }
        else
            OplockCount = FsRtlAreThereCurrentOrInProgressFileLocks(&FileNode->FileLock);
        break;

    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
    case FSCTL_OPLOCK_BREAK_NOTIFY:
    case FSCTL_OPLOCK_BREAK_ACK_NO_2:
    shared:
        FspFileNodeAcquireShared(FileNode, Main);
        OplockCount = 0;
        break;

    default:
        ASSERT(0);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    /*
     * The FileNode is acquired exclusive or shared.
     * Make sure to release it before exiting!
     */

    if (FSCTL_REQUEST_FILTER_OPLOCK == FsControlCode ||
        FSCTL_REQUEST_BATCH_OPLOCK == FsControlCode ||
        (FSCTL_REQUEST_OPLOCK == FsControlCode &&
            FlagOn(((PREQUEST_OPLOCK_INPUT_BUFFER)InputBuffer)->RequestedOplockLevel,
                OPLOCK_LEVEL_CACHE_HANDLE)))
    {
        BOOLEAN DeletePending;

        DeletePending = 0 != FileNode->DeletePending;
        MemoryBarrier();
        if (DeletePending)
        {
            Result = STATUS_DELETE_PENDING;
            goto unlock_exit;
        }
    }

    /*
     * This IRP will be completed by the FSRTL package and therefore
     * we will have no chance to do our normal IRP completion processing.
     * Hook the IRP completion and perform the IRP completion processing
     * there.
     */

    CompletionContext = FspAllocNonPaged(sizeof *CompletionContext);
    if (0 == CompletionContext)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto unlock_exit;
    }
    CompletionContext->FsvolDeviceObject = FsvolDeviceObject;
    ExInitializeWorkItem(&CompletionContext->WorkItem,
        FspFsvolFileSystemControlOplockCompletionWork, CompletionContext);

    Result = FspIrpHook(Irp, FspFsvolFileSystemControlOplockCompletion, CompletionContext);
    if (!NT_SUCCESS(Result))
    {
        FspFree(CompletionContext);
        goto unlock_exit;
    }

    /*
     * FspOplockFsctrl takes ownership of the IRP under all circumstances.
     */

    IoSetTopLevelIrp(0);

    Result = FspFileNodeOplockFsctl(FileNode, Irp, OplockCount);

    FspFileNodeRelease(FileNode, Main);

    return Result | FSP_STATUS_IGNORE_BIT;

unlock_exit:
    FspFileNodeRelease(FileNode, Main);

    return Result;
}

static NTSTATUS FspFsvolFileSystemControlOplockCompletion(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    // !PAGED_CODE();

    FSP_FSVOL_FILESYSTEM_CONTROL_OPLOCK_COMPLETION_CONTEXT *CompletionContext =
        FspIrpHookContext(Context);
    ExQueueWorkItem(&CompletionContext->WorkItem, DelayedWorkQueue);

    return FspIrpHookNext(DeviceObject, Irp, Context);
}

static VOID FspFsvolFileSystemControlOplockCompletionWork(PVOID Context)
{
    PAGED_CODE();

    FSP_FSVOL_FILESYSTEM_CONTROL_OPLOCK_COMPLETION_CONTEXT *CompletionContext = Context;
    FspDeviceDereference(CompletionContext->FsvolDeviceObject);
    FspFree(CompletionContext);
}

static NTSTATUS FspFsvolFileSystemControlQueryPersistentVolumeState(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    PFILE_FS_PERSISTENT_VOLUME_INFORMATION Info;

    if (0 == Buffer)
        return STATUS_INVALID_PARAMETER;

    if (sizeof(FILE_FS_PERSISTENT_VOLUME_INFORMATION) > InputBufferLength ||
        sizeof(FILE_FS_PERSISTENT_VOLUME_INFORMATION) > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    Info = Buffer;
    if (1 != Info->Version ||
        !FlagOn(Info->FlagMask, PERSISTENT_VOLUME_STATE_SHORT_NAME_CREATION_DISABLED))
        return STATUS_INVALID_PARAMETER;

    RtlZeroMemory(Info, sizeof(FILE_FS_PERSISTENT_VOLUME_INFORMATION));
    Info->VolumeFlags = PERSISTENT_VOLUME_STATE_SHORT_NAME_CREATION_DISABLED;

    Irp->IoStatus.Information = sizeof(FILE_FS_PERSISTENT_VOLUME_INFORMATION);

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolFileSystemControlGetStatistics(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

    Result = FspStatisticsCopy(FsvolDeviceExtension->Statistics, Buffer, &Length);

    Irp->IoStatus.Information = Length;

    return Result;
}

static NTSTATUS FspFsvolFileSystemControlGetRetrievalPointers(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    /*
     * FSCTL_GET_RETRIEVAL_POINTERS is normally used for defragmentation support,
     * which WinFsp does NOT support. However some tools (notably IFSTEST) use it
     * to determine whether files are "resident" or "non-resident" which is an NTFS
     * concept. To support such tools we respond in a manner that indicates that
     * WinFsp files are always non-resident.
     */

    PAGED_CODE();

    PFILE_OBJECT FileObject = IrpSp->FileObject;

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PSTARTING_VCN_INPUT_BUFFER InputBuffer = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
    PRETRIEVAL_POINTERS_BUFFER OutputBuffer = Irp->UserBuffer;
    ULONG InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    ULONG OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    STARTING_VCN_INPUT_BUFFER StartingVcn;
    RETRIEVAL_POINTERS_BUFFER RetrievalPointers;
    UINT64 AllocationUnit;

    if (0 == InputBuffer || 0 == OutputBuffer)
        return STATUS_INVALID_PARAMETER;

    if (sizeof(STARTING_VCN_INPUT_BUFFER) > InputBufferLength ||
        sizeof(RETRIEVAL_POINTERS_BUFFER) > OutputBufferLength)
        return STATUS_BUFFER_TOO_SMALL;

    if (UserMode == Irp->RequestorMode)
    {
        try
        {
            ProbeForRead(InputBuffer, InputBufferLength, sizeof(UCHAR)/*FastFat*/);
            StartingVcn = *InputBuffer;
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else
        StartingVcn = *InputBuffer;

    RetrievalPointers.ExtentCount = 1;
    RetrievalPointers.StartingVcn.QuadPart = 0;
    RetrievalPointers.Extents[0].NextVcn.QuadPart = 0;
    RetrievalPointers.Extents[0].Lcn.QuadPart = -1LL;

    AllocationUnit = FsvolDeviceExtension->VolumeParams.SectorSize *
        FsvolDeviceExtension->VolumeParams.SectorsPerAllocationUnit;

    FspFileNodeAcquireShared(FileNode, Main);
    RetrievalPointers.Extents[0].NextVcn.QuadPart =
        FileNode->Header.AllocationSize.QuadPart / AllocationUnit;
    FspFileNodeRelease(FileNode, Main);

    if (StartingVcn.StartingVcn.QuadPart > RetrievalPointers.Extents[0].NextVcn.QuadPart)
        return STATUS_END_OF_FILE;

    if (UserMode == Irp->RequestorMode)
    {
        try
        {
            ProbeForWrite(OutputBuffer, OutputBufferLength, sizeof(UCHAR)/*FastFat*/);
            *OutputBuffer = RetrievalPointers;
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }
    else
        *OutputBuffer = RetrievalPointers;

    Irp->IoStatus.Information = sizeof RetrievalPointers;

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
        case FSP_FSCTL_QUERY_WINFSP:
            Irp->IoStatus.Information = 0;
            Result = STATUS_SUCCESS;
            break;
        case FSCTL_GET_REPARSE_POINT:
            Result = FspFsvolFileSystemControlReparsePoint(FsvolDeviceObject, Irp, IrpSp, FALSE);
            break;
        case FSCTL_SET_REPARSE_POINT:
        case FSCTL_DELETE_REPARSE_POINT:
            Result = FspFsvolFileSystemControlReparsePoint(FsvolDeviceObject, Irp, IrpSp, TRUE);
            break;
        case FSCTL_REQUEST_OPLOCK_LEVEL_1:
        case FSCTL_REQUEST_OPLOCK_LEVEL_2:
        case FSCTL_REQUEST_BATCH_OPLOCK:
        case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
        case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
        case FSCTL_OPLOCK_BREAK_NOTIFY:
        case FSCTL_OPLOCK_BREAK_ACK_NO_2:
        case FSCTL_REQUEST_FILTER_OPLOCK:
        case FSCTL_REQUEST_OPLOCK:
            Result = FspFsvolFileSystemControlOplock(FsvolDeviceObject, Irp, IrpSp);
            break;
        case FSCTL_QUERY_PERSISTENT_VOLUME_STATE:
            Result = FspFsvolFileSystemControlQueryPersistentVolumeState(FsvolDeviceObject, Irp, IrpSp);
            break;
        case FSCTL_FILESYSTEM_GET_STATISTICS:
            Result = FspFsvolFileSystemControlGetStatistics(FsvolDeviceObject, Irp, IrpSp);
            break;
        case FSCTL_GET_RETRIEVAL_POINTERS:
            Result = FspFsvolFileSystemControlGetRetrievalPointers(FsvolDeviceObject, Irp, IrpSp);
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
