/**
 * @file sys/dirctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolQueryDirectoryRetry(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
static NTSTATUS FspFsvolQueryDirectoryCopy(
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING QueryFileName, BOOLEAN CaseInsensitive,
    PVOID DestBuf, PULONG PDestLen,
    FSP_FSCTL_DIR_INFO *DirInfo, ULONG DirInfoSize);
static NTSTATUS FspFsvolNotifyChangeDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolDirectoryControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOPREP_DISPATCH FspFsvolDirectoryControlPrepare;
FSP_IOCMPL_DISPATCH FspFsvolDirectoryControlComplete;
static FSP_IOP_REQUEST_FINI FspFsvolQueryDirectoryRequestFini;
FSP_DRIVER_DISPATCH FspDirectoryControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryDirectory)
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryRetry)
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryCopy)
#pragma alloc_text(PAGE, FspFsvolNotifyChangeDirectory)
#pragma alloc_text(PAGE, FspFsvolDirectoryControl)
#pragma alloc_text(PAGE, FspFsvolDirectoryControlPrepare)
#pragma alloc_text(PAGE, FspFsvolDirectoryControlComplete)
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryRequestFini)
#pragma alloc_text(PAGE, FspDirectoryControl)
#endif

enum
{
    /* QueryDirectory */
    RequestFileNode                     = 0,
    RequestDirectoryChangeNumber        = 1,
};

static NTSTATUS FspFsvolQueryDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;
    PUNICODE_STRING FileName = IrpSp->Parameters.QueryDirectory.FileName;

    /* only directory files can be queried */
    if (!FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /* check that FileName is valid (if supplied) */
    if (0 != FileName && !FspUnicodePathIsValid(FileName, FALSE))
        return STATUS_INVALID_PARAMETER;

    /* is this an allowed file information class? */
    switch (FileInformationClass)
    {
    case FileDirectoryInformation:
    case FileFullDirectoryInformation:
    case FileIdFullDirectoryInformation:
    case FileNamesInformation:
    case FileBothDirectoryInformation:
    case FileIdBothDirectoryInformation:
        break;
    default:
        return STATUS_INVALID_INFO_CLASS;
    }

    Result = FspFsvolQueryDirectoryRetry(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));

    return Result;
}

static NTSTATUS FspFsvolQueryDirectoryRetry(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    BOOLEAN RestartScan = BooleanFlagOn(Irp->Flags, SL_RESTART_SCAN);
    BOOLEAN IndexSpecified = BooleanFlagOn(Irp->Flags, SL_INDEX_SPECIFIED);
    BOOLEAN ReturnSingleEntry = BooleanFlagOn(Irp->Flags, SL_RETURN_SINGLE_ENTRY);
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;
    PUNICODE_STRING FileName = IrpSp->Parameters.QueryDirectory.FileName;
    PVOID Buffer = 0 != Irp->AssociatedIrp.SystemBuffer ?
        Irp->AssociatedIrp.SystemBuffer : Irp->UserBuffer;
    ULONG Length = IrpSp->Parameters.QueryDirectory.Length;
    PVOID DirInfoBuffer;
    ULONG DirInfoSize;
    FSP_FSCTL_TRANSACT_REQ *Request;
    BOOLEAN InitialQuery;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    /* try to acquire the FileNode exclusive; Full because we may need to send a Request */
    Success = DEBUGTEST(90, TRUE) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolQueryDirectoryRetry, 0);

    InitialQuery = 0 == FileDesc->QueryFileName.Buffer;

    /* set the QueryFileName in the FileDesc */
    Result = FspFileDescResetQueryFileName(FileDesc, FileName, RestartScan);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /* determine where to (re)start */
    if (IndexSpecified)
        FileDesc->QueryOffset = (UINT64)IrpSp->Parameters.QueryDirectory.FileIndex << 32;
    else if (RestartScan)
        FileDesc->QueryOffset = 0;

    FspFileNodeConvertExclusiveToShared(FileNode, Full);

    /* see if the required information is still in the cache and valid! */
    if (FspFileNodeReferenceDirInfo(FileNode, &DirInfoBuffer, &DirInfoSize))
    {
        FspFileNodeRelease(FileNode, Full);

        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
        BOOLEAN CaseInsensitive = 0 == FsvolDeviceExtension->VolumeParams.CaseSensitiveSearch;

        Result = FspFsvolQueryDirectoryCopy(FileInformationClass, ReturnSingleEntry,
            &FileDesc->QueryFileName, CaseInsensitive,
            Buffer, &Length, DirInfoBuffer, DirInfoSize);
        FspFileNodeDereferenceDirInfo(DirInfoBuffer);

        if (NT_SUCCESS(Result) && 0 == Length)
            Result = InitialQuery ? STATUS_NO_SUCH_FILE : STATUS_NO_MORE_FILES;

        Irp->IoStatus.Information = Length;
        return Result;
    }

    /* buffer the user buffer! */
    Result = FspBufferUserBuffer(Irp, Length, IoWriteAccess);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /* create request */
    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolQueryDirectoryRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactQueryDirectoryKind;
    Request->Req.QueryDirectory.UserContext = FileNode->UserContext;
    Request->Req.QueryDirectory.UserContext2 = FileDesc->UserContext2;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

static NTSTATUS FspFsvolQueryDirectoryCopy(
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING QueryFileName, BOOLEAN CaseInsensitive,
    PVOID DestBuf, PULONG PDestLen,
    FSP_FSCTL_DIR_INFO *DirInfo, ULONG DirInfoSize)
{
#define FILL_INFO(Info, DirInfo)\
    Info->NextEntryOffset = 0;\
    Info->FileIndex = DirInfo->NextOffset >> 32;\
    Info->CreationTime.QuadPart = DirInfo->FileInfo.CreationTime;\
    Info->LastAccessTime.QuadPart = DirInfo->FileInfo.LastAccessTime;\
    Info->LastWriteTime.QuadPart = DirInfo->FileInfo.LastWriteTime;\
    Info->ChangeTime.QuadPart = DirInfo->FileInfo.ChangeTime;\
    Info->EndOfFile.QuadPart = DirInfo->FileInfo.FileSize;\
    Info->AllocationSize.QuadPart = DirInfo->FileInfo.AllocationSize;\
    Info->FileAttributes = 0 != DirInfo->FileInfo.FileAttributes ?\
        DirInfo->FileInfo.FileAttributes : FILE_ATTRIBUTE_NORMAL;\
    Info->FileNameLength = FileNameLen;\
    RtlCopyMemory(Info->FileName, DirInfo->FileNameBuf, FileNameLen)

    PAGED_CODE();

    BOOLEAN MatchAll = FspFileDescQueryFileNameMatchAll == QueryFileName->Buffer;
    PVOID PrevDestBuf = 0;
    PUINT8 DestBufBgn = (PUINT8)DestBuf;
    PUINT8 DestBufEnd = (PUINT8)DestBuf + *PDestLen;
    PUINT8 DirInfoEnd = (PUINT8)DirInfo + DirInfoSize;
    ULONG BaseInfoLen, FileNameLen;
    UNICODE_STRING FileName;

    *PDestLen = 0;

    switch (FileInformationClass)
    {
    case FileDirectoryInformation:
        BaseInfoLen = FIELD_OFFSET(FILE_DIRECTORY_INFORMATION, FileName);
        break;
    case FileFullDirectoryInformation:
        BaseInfoLen = FIELD_OFFSET(FILE_FULL_DIR_INFORMATION, FileName);
        break;
    case FileIdFullDirectoryInformation:
        BaseInfoLen = FIELD_OFFSET(FILE_ID_FULL_DIR_INFORMATION, FileName);
        break;
    case FileNamesInformation:
        BaseInfoLen = FIELD_OFFSET(FILE_NAMES_INFORMATION, FileName);
        break;
    case FileBothDirectoryInformation:
        BaseInfoLen = FIELD_OFFSET(FILE_BOTH_DIR_INFORMATION, FileName);
        break;
    case FileIdBothDirectoryInformation:
        BaseInfoLen = FIELD_OFFSET(FILE_ID_BOTH_DIR_INFORMATION, FileName);
        break;
    default:
        return STATUS_INVALID_INFO_CLASS;
    }

    try
    {
        for (;;)
        {
            if ((PUINT8)DirInfo + sizeof(DirInfo->Size) > DirInfoEnd ||
                sizeof(FSP_FSCTL_DIR_INFO) > DirInfo->Size)
                break;

            FileNameLen = DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO);
            FileName.Length = FileName.MaximumLength = (USHORT)FileNameLen;
            FileName.Buffer = DirInfo->FileNameBuf;

            if (MatchAll || FsRtlIsNameInExpression(QueryFileName, &FileName, CaseInsensitive, 0))
            {
                if ((PUINT8)DestBuf + FSP_FSCTL_ALIGN_UP(BaseInfoLen + FileNameLen, sizeof(LONGLONG)) >
                    DestBufEnd)
                {
                    if (DestBufBgn == DestBuf)
                    {
                        *PDestLen = BaseInfoLen + FileNameLen;
                        return STATUS_BUFFER_OVERFLOW;
                    }
                    break;
                }

                if (0 != PrevDestBuf)
                    *(PULONG)PrevDestBuf = (ULONG)((PUINT8)DestBuf - DestBufBgn);
                PrevDestBuf = DestBuf;

                switch (FileInformationClass)
                {
                case FileDirectoryInformation:
                    {
                        FILE_DIRECTORY_INFORMATION *Info = DestBuf;

                        FILL_INFO(Info, DirInfo);
                    }
                    break;
                case FileFullDirectoryInformation:
                    {
                        FILE_FULL_DIR_INFORMATION *Info = DestBuf;

                        FILL_INFO(Info, DirInfo);
                        Info->EaSize = 0;
                    }
                    break;
                case FileIdFullDirectoryInformation:
                    {
                        FILE_ID_FULL_DIR_INFORMATION *Info = DestBuf;

                        FILL_INFO(Info, DirInfo);
                        Info->EaSize = 0;
                        Info->FileId.QuadPart = DirInfo->FileInfo.IndexNumber;
                    }
                    break;
                case FileNamesInformation:
                    {
                        FILE_NAMES_INFORMATION *Info = DestBuf;

                        Info->NextEntryOffset = 0;
                        Info->FileIndex = DirInfo->NextOffset >> 32;
                        Info->FileNameLength = FileNameLen;
                        RtlCopyMemory(Info->FileName, DirInfo->FileNameBuf, FileNameLen);
                    }
                    break;
                case FileBothDirectoryInformation:
                    {
                        FILE_BOTH_DIR_INFORMATION *Info = DestBuf;

                        FILL_INFO(Info, DirInfo);
                        Info->EaSize = 0;
                        Info->ShortNameLength = 0;
                        RtlZeroMemory(Info->ShortName, sizeof Info->ShortName);
                    }
                    break;
                case FileIdBothDirectoryInformation:
                    {
                        FILE_ID_BOTH_DIR_INFORMATION *Info = DestBuf;

                        FILL_INFO(Info, DirInfo);
                        Info->EaSize = 0;
                        Info->ShortNameLength = 0;
                        RtlZeroMemory(Info->ShortName, sizeof Info->ShortName);
                        Info->FileId.QuadPart = DirInfo->FileInfo.IndexNumber;
                    }
                    break;
                default:
                    ASSERT(0);
                    return STATUS_INVALID_INFO_CLASS;
                }

                *PDestLen = (ULONG)((PUINT8)DestBuf + BaseInfoLen + FileNameLen - DestBufBgn);

                if (ReturnSingleEntry)
                    break;

                DestBuf = (PVOID)((PUINT8)DestBuf +
                    FSP_FSCTL_ALIGN_UP(BaseInfoLen + FileNameLen, sizeof(LONGLONG)));
            }

            DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfo->Size));
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        NTSTATUS Result = GetExceptionCode();
        if (STATUS_INSUFFICIENT_RESOURCES == Result)
            return Result;
        return FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
    }

    return STATUS_SUCCESS;
#undef FILL_INFO
}

static NTSTATUS FspFsvolNotifyChangeDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolDirectoryControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->MinorFunction)
    {
    case IRP_MN_QUERY_DIRECTORY:
        Result = FspFsvolQueryDirectory(FsvolDeviceObject, Irp, IrpSp);
        break;
    case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
        Result = FspFsvolNotifyChangeDirectory(FsvolDeviceObject, Irp, IrpSp);
        break;
    }

    return Result;
}

NTSTATUS FspFsvolDirectoryControlPrepare(
    PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    PAGED_CODE();

    return STATUS_SUCCESS;
}

NTSTATUS FspFsvolDirectoryControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s", "");
}

static VOID FspFsvolQueryDirectoryRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

NTSTATUS FspDirectoryControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolDirectoryControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s", "");
}
