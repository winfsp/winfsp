/**
 * @file sys/dirctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryDirectoryCopy(
    PUNICODE_STRING DirectoryPattern, BOOLEAN CaseInsensitive,
    UINT64 DirectoryOffset, PUINT64 PDirectoryOffset,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    FSP_FSCTL_DIR_INFO **PDirInfo, ULONG DirInfoSize,
    PVOID DestBuf, PULONG PDestLen);
static NTSTATUS FspFsvolQueryDirectoryCopyCache(
    FSP_FILE_DESC *FileDesc, BOOLEAN ResetCache,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    FSP_FSCTL_DIR_INFO *DirInfo, ULONG DirInfoSize,
    PVOID DestBuf, PULONG PDestLen);
static NTSTATUS FspFsvolQueryDirectoryCopyInPlace(
    FSP_FILE_DESC *FileDesc,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    FSP_FSCTL_DIR_INFO *DirInfo, ULONG DirInfoSize,
    PVOID DestBuf, PULONG PDestLen);
static NTSTATUS FspFsvolQueryDirectoryRetry(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
static NTSTATUS FspFsvolQueryDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolNotifyChangeDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolDirectoryControl(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOPREP_DISPATCH FspFsvolDirectoryControlPrepare;
FSP_IOCMPL_DISPATCH FspFsvolDirectoryControlComplete;
static FSP_IOP_REQUEST_FINI FspFsvolQueryDirectoryRequestFini;
FSP_DRIVER_DISPATCH FspDirectoryControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryCopy)
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryCopyCache)
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryCopyInPlace)
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryRetry)
#pragma alloc_text(PAGE, FspFsvolQueryDirectory)
#pragma alloc_text(PAGE, FspFsvolNotifyChangeDirectory)
#pragma alloc_text(PAGE, FspFsvolDirectoryControl)
#pragma alloc_text(PAGE, FspFsvolDirectoryControlPrepare)
#pragma alloc_text(PAGE, FspFsvolDirectoryControlComplete)
#pragma alloc_text(PAGE, FspFsvolQueryDirectoryRequestFini)
#pragma alloc_text(PAGE, FspDirectoryControl)
#endif

#define FILE_INDEX_FROM_OFFSET(v)       ((ULONG)(v))
#define OFFSET_FROM_FILE_INDEX(v)       ((UINT64)(v))

enum
{
    /* QueryDirectory */
    RequestFileNode                     = 0,
    RequestMdl                          = 1,
    RequestAddress                      = 2,
    RequestProcess                      = 3,

    /* DirectoryControlComplete retry */
    RequestDirInfoChangeNumber          = 0,
};

static NTSTATUS FspFsvolQueryDirectoryCopy(
    PUNICODE_STRING DirectoryPattern, BOOLEAN CaseInsensitive,
    UINT64 DirectoryOffset, PUINT64 PDirectoryOffset,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    FSP_FSCTL_DIR_INFO **PDirInfo, ULONG DirInfoSize,
    PVOID DestBuf, PULONG PDestLen)
{
#define FILL_INFO_BASE(TYPE, ...)\
    do\
    {\
        TYPE InfoStruct = { 0 }, *Info = &InfoStruct;\
        Info->NextEntryOffset = 0;\
        Info->FileIndex = FILE_INDEX_FROM_OFFSET(DirInfo->NextOffset);\
        Info->FileNameLength = FileName.Length;\
        __VA_ARGS__\
        Info = DestBuf;\
        *Info = InfoStruct;\
        RtlCopyMemory(Info->FileName, DirInfo->FileNameBuf, FileName.Length);\
    } while (0,0)
#define FILL_INFO(TYPE, ...)\
    FILL_INFO_BASE(TYPE,\
        Info->CreationTime.QuadPart = DirInfo->FileInfo.CreationTime;\
        Info->LastAccessTime.QuadPart = DirInfo->FileInfo.LastAccessTime;\
        Info->LastWriteTime.QuadPart = DirInfo->FileInfo.LastWriteTime;\
        Info->ChangeTime.QuadPart = DirInfo->FileInfo.ChangeTime;\
        Info->EndOfFile.QuadPart = DirInfo->FileInfo.FileSize;\
        Info->AllocationSize.QuadPart = DirInfo->FileInfo.AllocationSize;\
        Info->FileAttributes = 0 != DirInfo->FileInfo.FileAttributes ?\
            DirInfo->FileInfo.FileAttributes : FILE_ATTRIBUTE_NORMAL;\
        __VA_ARGS__\
        )

    PAGED_CODE();

    BOOLEAN MatchAll = FspFileDescDirectoryPatternMatchAll == DirectoryPattern->Buffer;
    BOOLEAN DirectoryOffsetFound = FALSE;
    FSP_FSCTL_DIR_INFO *DirInfo = *PDirInfo;
    PUINT8 DirInfoEnd = (PUINT8)DirInfo + DirInfoSize;
    PUINT8 DestBufBgn = (PUINT8)DestBuf;
    PUINT8 DestBufEnd = (PUINT8)DestBuf + *PDestLen;
    PVOID PrevDestBuf = 0;
    ULONG BaseInfoLen;
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
        for (;
            (PUINT8)DirInfo + sizeof(DirInfo->Size) <= DirInfoEnd;
            DirInfo = (PVOID)((PUINT8)DirInfo + FSP_FSCTL_DEFAULT_ALIGN_UP(DirInfoSize)))
        {
            DirInfoSize = DirInfo->Size;

            if (sizeof(FSP_FSCTL_DIR_INFO) > DirInfoSize)
            {
                if (0 == *PDestLen)
                    return STATUS_NO_MORE_FILES;
                break;
            }

            if (0 != DirectoryOffset && !DirectoryOffsetFound)
            {
                DirectoryOffsetFound = DirInfo->NextOffset == DirectoryOffset;
                continue;
            }

            FileName.Length =
            FileName.MaximumLength = (USHORT)(DirInfoSize - sizeof(FSP_FSCTL_DIR_INFO));
            FileName.Buffer = DirInfo->FileNameBuf;

            if (MatchAll || FsRtlIsNameInExpression(DirectoryPattern, &FileName, CaseInsensitive, 0))
            {
                if ((PUINT8)DestBuf +
                    FSP_FSCTL_ALIGN_UP(BaseInfoLen + FileName.Length, sizeof(LONGLONG)) > DestBufEnd)
                {
                    if (0 == *PDestLen)
                    {
                        *PDestLen = BaseInfoLen + FileName.Length;
                        return STATUS_BUFFER_OVERFLOW;
                    }
                    break;
                }

                if (0 != PrevDestBuf)
                    *(PULONG)PrevDestBuf = (ULONG)((PUINT8)DestBuf - (PUINT8)PrevDestBuf);
                PrevDestBuf = DestBuf;

                *PDirectoryOffset = DirInfo->NextOffset;
                *PDestLen = (ULONG)((PUINT8)DestBuf + BaseInfoLen + FileName.Length - DestBufBgn);

                switch (FileInformationClass)
                {
                case FileDirectoryInformation:
                    FILL_INFO(FILE_DIRECTORY_INFORMATION,);
                    break;
                case FileFullDirectoryInformation:
                    FILL_INFO(FILE_FULL_DIR_INFORMATION,
                        Info->EaSize = 0;
                    );
                    break;
                case FileIdFullDirectoryInformation:
                    FILL_INFO(FILE_ID_FULL_DIR_INFORMATION,
                        Info->EaSize = 0;
                        Info->FileId.QuadPart = DirInfo->FileInfo.IndexNumber;
                    );
                    break;
                case FileNamesInformation:
                    FILL_INFO_BASE(FILE_NAMES_INFORMATION,);
                    break;
                case FileBothDirectoryInformation:
                    FILL_INFO(FILE_BOTH_DIR_INFORMATION,
                        Info->EaSize = 0;
                        Info->ShortNameLength = 0;
                        RtlZeroMemory(Info->ShortName, sizeof Info->ShortName);
                    );
                    break;
                case FileIdBothDirectoryInformation:
                    FILL_INFO(FILE_ID_BOTH_DIR_INFORMATION,
                        Info->EaSize = 0;
                        Info->ShortNameLength = 0;
                        RtlZeroMemory(Info->ShortName, sizeof Info->ShortName);
                        Info->FileId.QuadPart = DirInfo->FileInfo.IndexNumber;
                    );
                    break;
                default:
                    ASSERT(0);
                    break;
                }

                if (ReturnSingleEntry)
                    break;

                DestBuf = (PVOID)((PUINT8)DestBuf +
                    FSP_FSCTL_ALIGN_UP(BaseInfoLen + FileName.Length, sizeof(LONGLONG)));
            }
            else
                *PDirectoryOffset = DirInfo->NextOffset;
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        NTSTATUS Result = GetExceptionCode();
        if (STATUS_INSUFFICIENT_RESOURCES == Result)
            return Result;
        return FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
    }

    *PDirInfo = DirInfo;

    return STATUS_SUCCESS;

#undef FILL_INFO
#undef FILL_INFO_BASE
}

static NTSTATUS FspFsvolQueryDirectoryCopyCache(
    FSP_FILE_DESC *FileDesc, BOOLEAN ResetCache,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    FSP_FSCTL_DIR_INFO *DirInfo, ULONG DirInfoSize,
    PVOID DestBuf, PULONG PDestLen)
{
    /* FileNode/FileDesc assumed acquired exclusive (Main or Full) */

    PAGED_CODE();

    FSP_FILE_NODE *FileNode = FileDesc->FileNode;

    if (ResetCache || FileDesc->DirInfo != FileNode->NonPaged->DirInfo)
        FileDesc->DirInfoCacheHint = 0; /* reset the DirInfo hint if anything looks fishy! */

    FileDesc->DirInfo = FileNode->NonPaged->DirInfo;

    NTSTATUS Result;
    BOOLEAN CaseInsensitive = !FileDesc->CaseSensitive;
    PUNICODE_STRING DirectoryPattern = &FileDesc->DirectoryPattern;
    UINT64 DirectoryOffset = FileDesc->DirectoryOffset;
    PUINT8 DirInfoBgn = (PUINT8)DirInfo;
    PUINT8 DirInfoEnd = (PUINT8)DirInfo + DirInfoSize;

    DirInfo = (PVOID)(DirInfoBgn + FileDesc->DirInfoCacheHint);
    DirInfoSize = (ULONG)(DirInfoEnd - (PUINT8)DirInfo);

    Result = FspFsvolQueryDirectoryCopy(DirectoryPattern, CaseInsensitive,
        0 != FileDesc->DirInfoCacheHint ? 0 : DirectoryOffset, &DirectoryOffset,
        FileInformationClass, ReturnSingleEntry,
        &DirInfo, DirInfoSize,
        DestBuf, PDestLen);

    if (NT_SUCCESS(Result))
    {
        if (0 != *PDestLen)
            FileDesc->DirectoryHasSuchFile = TRUE;
        FileDesc->DirectoryOffset = DirectoryOffset;
        FileDesc->DirInfoCacheHint = (ULONG)((PUINT8)DirInfo - DirInfoBgn);
    }
    else if (STATUS_NO_MORE_FILES == Result && !FileDesc->DirectoryHasSuchFile)
        Result = STATUS_NO_SUCH_FILE;

    return Result;
}

static NTSTATUS FspFsvolQueryDirectoryCopyInPlace(
    FSP_FILE_DESC *FileDesc,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    FSP_FSCTL_DIR_INFO *DirInfo, ULONG DirInfoSize,
    PVOID DestBuf, PULONG PDestLen)
{
    /* FileNode/FileDesc assumed acquired exclusive (Main or Full) */

    PAGED_CODE();

    NTSTATUS Result;
    BOOLEAN CaseInsensitive = !FileDesc->CaseSensitive;
    PUNICODE_STRING DirectoryPattern = &FileDesc->DirectoryPattern;
    UINT64 DirectoryOffset = FileDesc->DirectoryOffset;

    ASSERT(DirInfo == DestBuf);
    static_assert(
        FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) >=
        FIELD_OFFSET(FILE_ID_BOTH_DIR_INFORMATION, FileName),
        "FSP_FSCTL_DIR_INFO must be bigger than FILE_ID_BOTH_DIR_INFORMATION");

    Result = FspFsvolQueryDirectoryCopy(DirectoryPattern, CaseInsensitive,
        0, &DirectoryOffset,
        FileInformationClass, ReturnSingleEntry,
        &DirInfo, DirInfoSize,
        DestBuf, PDestLen);

    if (NT_SUCCESS(Result))
    {
        if (0 != *PDestLen)
            FileDesc->DirectoryHasSuchFile = TRUE;
        FileDesc->DirectoryOffset = DirectoryOffset;
    }
    else if (STATUS_NO_MORE_FILES == Result && !FileDesc->DirectoryHasSuchFile)
        Result = STATUS_NO_SUCH_FILE;

    return Result;
}

static NTSTATUS FspFsvolQueryDirectoryRetry(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    BOOLEAN RestartScan = BooleanFlagOn(IrpSp->Flags, SL_RESTART_SCAN);
    BOOLEAN IndexSpecified = BooleanFlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED);
    BOOLEAN ReturnSingleEntry = BooleanFlagOn(IrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;
    PUNICODE_STRING FileName = IrpSp->Parameters.QueryDirectory.FileName;
    ULONG FileIndex = IrpSp->Parameters.QueryDirectory.FileIndex;
    PVOID Buffer = 0 != Irp->AssociatedIrp.SystemBuffer ?
        Irp->AssociatedIrp.SystemBuffer : Irp->UserBuffer;
    ULONG Length = IrpSp->Parameters.QueryDirectory.Length;
    ULONG SystemBufferLength;
    PVOID DirInfoBuffer;
    ULONG DirInfoSize;
    FSP_FSCTL_TRANSACT_REQ *Request;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    /* try to acquire the FileNode exclusive; Full because we may need to send a Request */
    Success = DEBUGTEST(90, TRUE) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolQueryDirectoryRetry, 0);

    /* set the DirectoryPattern in the FileDesc */
    Result = FspFileDescResetDirectoryPattern(FileDesc, FileName, RestartScan);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /* determine where to (re)start */
    if (IndexSpecified)
    {
        FileDesc->DirectoryHasSuchFile = FALSE;
        FileDesc->DirectoryOffset = OFFSET_FROM_FILE_INDEX(FileIndex);
    }
    else if (RestartScan)
    {
        FileDesc->DirectoryHasSuchFile = FALSE;
        FileDesc->DirectoryOffset = 0;
    }

    /* see if the required information is still in the cache and valid! */
    if (FspFileNodeReferenceDirInfo(FileNode, &DirInfoBuffer, &DirInfoSize))
    {
        SystemBufferLength = Length;

        Result = FspFsvolQueryDirectoryCopyCache(FileDesc,
            IndexSpecified || RestartScan,
            FileInformationClass, ReturnSingleEntry,
            DirInfoBuffer, DirInfoSize, Buffer, &Length);

        FspFileNodeDereferenceDirInfo(DirInfoBuffer);

        if (!NT_SUCCESS(Result) || 0 != Length)
        {
            FspFileNodeRelease(FileNode, Full);
            Irp->IoStatus.Information = Length;
            return Result;
        }
    }
    else
        SystemBufferLength = 0 != FsvolDeviceExtension->VolumeParams.FileInfoTimeout ?
            FspFsvolDeviceDirInfoCacheItemSizeMax : Length;

    FspFileNodeConvertExclusiveToShared(FileNode, Full);

    /* buffer the user buffer! */
    if (SystemBufferLength > FspFsvolDeviceDirInfoCacheItemSizeMax)
        SystemBufferLength = FspFsvolDeviceDirInfoCacheItemSizeMax;
    else if (SystemBufferLength < sizeof(FSP_FSCTL_DIR_INFO) +
        FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR))
        SystemBufferLength = sizeof(FSP_FSCTL_DIR_INFO) +
            FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR);
    Result = FspBufferUserBuffer(Irp,
        FSP_FSCTL_ALIGN_UP(SystemBufferLength, PAGE_SIZE), IoWriteAccess);
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
    Request->Req.QueryDirectory.Offset = FileDesc->DirectoryOffset;
    Request->Req.QueryDirectory.Length = SystemBufferLength;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

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
    ULONG Length = IrpSp->Parameters.QueryDirectory.Length;
    ULONG BaseInfoLen;

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
    if (BaseInfoLen >= Length)
        return STATUS_BUFFER_TOO_SMALL;

    Result = FspFsvolQueryDirectoryRetry(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));

    return Result;
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

    NTSTATUS Result;
    PMDL Mdl = 0;
    PVOID Address;
    PEPROCESS Process;

    Mdl = IoAllocateMdl(
        Irp->AssociatedIrp.SystemBuffer,
        Request->Req.QueryDirectory.Length,
        FALSE, FALSE, 0);
    if (0 == Mdl)
        return STATUS_INSUFFICIENT_RESOURCES;

    MmBuildMdlForNonPagedPool(Mdl);

    /* map the MDL into user-mode */
    Result = FspMapLockedPagesInUserMode(Mdl, &Address);
    if (!NT_SUCCESS(Result))
    {
        if (0 != Mdl)
            IoFreeMdl(Mdl);

        return Result;
    }

    /* get a pointer to the current process so that we can unmap the address later */
    Process = PsGetCurrentProcess();
    ObReferenceObject(Process);

    Request->Req.QueryDirectory.Address = (UINT64)(UINT_PTR)Address;

    FspIopRequestContext(Request, RequestMdl) = Mdl;
    FspIopRequestContext(Request, RequestAddress) = Address;
    FspIopRequestContext(Request, RequestProcess) = Process;

    return STATUS_SUCCESS;
}

NTSTATUS FspFsvolDirectoryControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    BOOLEAN ReturnSingleEntry = BooleanFlagOn(IrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.QueryDirectory.Length;
    ULONG DirInfoChangeNumber;
    PVOID DirInfoBuffer;
    ULONG DirInfoSize;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);
    ASSERT(Request->Req.QueryDirectory.Offset == FileDesc->DirectoryOffset);

    if (0 == Response->IoStatus.Information)
    {
        Result = !FileDesc->DirectoryHasSuchFile ?
            STATUS_NO_SUCH_FILE : STATUS_NO_MORE_FILES;
        FSP_RETURN();
    }

    if (FspFsctlTransactQueryDirectoryKind == Request->Kind)
    {
        DirInfoChangeNumber = FileNode->DirInfoChangeNumber;
        Request->Kind = FspFsctlTransactReservedKind;
        FspIopResetRequest(Request, 0);
        FspIopRequestContext(Request, RequestDirInfoChangeNumber) = (PVOID)DirInfoChangeNumber;
    }
    else
        DirInfoChangeNumber =
            (ULONG)(UINT_PTR)FspIopRequestContext(Request, RequestDirInfoChangeNumber);

    /* acquire FileNode exclusive Full (because we may need to go back to user-mode) */
    Success = DEBUGTEST(90, TRUE) && FspFileNodeTryAcquireExclusive(FileNode, Full);
    if (!Success)
    {
        FspIopRetryCompleteIrp(Irp, Response, &Result);
        FSP_RETURN();
    }

    if (0 == FileDesc->DirectoryOffset &&
        FspFileNodeTrySetDirInfo(FileNode,
            Irp->AssociatedIrp.SystemBuffer,
            (ULONG)Response->IoStatus.Information,
            DirInfoChangeNumber) &&
        FspFileNodeReferenceDirInfo(FileNode, &DirInfoBuffer, &DirInfoSize))
    {
        Result = FspFsvolQueryDirectoryCopyCache(FileDesc,
            TRUE,
            FileInformationClass, ReturnSingleEntry,
            DirInfoBuffer, DirInfoSize, Buffer, &Length);

        FspFileNodeDereferenceDirInfo(DirInfoBuffer);
    }
    else
    {
        DirInfoBuffer = Irp->AssociatedIrp.SystemBuffer;
        DirInfoSize = (ULONG)Response->IoStatus.Information;
        Result = FspFsvolQueryDirectoryCopyInPlace(FileDesc,
            FileInformationClass, ReturnSingleEntry,
            DirInfoBuffer, DirInfoSize, Buffer, &Length);
    }

    if (NT_SUCCESS(Result) && 0 == Length)
    {
        /*
         * Looks like we have to go back to user-mode!
         */

        PDEVICE_OBJECT FsvolDeviceObject = IrpSp->DeviceObject;
        FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
            FspFsvolDeviceExtension(FsvolDeviceObject);

        FspFileNodeConvertExclusiveToShared(FileNode, Full);

        Request->Kind = FspFsctlTransactQueryDirectoryKind;
        FspIopResetRequest(Request, FspFsvolQueryDirectoryRequestFini);

        Request->Req.QueryDirectory.Address = 0;
        Request->Req.QueryDirectory.Offset = FileDesc->DirectoryOffset;

        FspFileNodeSetOwner(FileNode, Full, Request);
        FspIopRequestContext(Request, RequestFileNode) = FileNode;

        FspIoqPostIrp(FsvolDeviceExtension->Ioq, Irp, &Result);
    }
    else
    {
        FspFileNodeRelease(FileNode, Full);

        Irp->IoStatus.Information = Length;
    }

    FSP_LEAVE_IOC("%s%sFileObject=%p",
        IRP_MN_QUERY_DIRECTORY == IrpSp->MinorFunction ?
            FileInformationClassSym(IrpSp->Parameters.QueryDirectory.FileInformationClass) : "",
        IRP_MN_QUERY_DIRECTORY == IrpSp->MinorFunction ? ", " : "",
        IrpSp->FileObject);
}

static VOID FspFsvolQueryDirectoryRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];
    PMDL Mdl = Context[RequestMdl];
    PVOID Address = Context[RequestAddress];
    PEPROCESS Process = Context[RequestProcess];

    if (0 != Address)
    {
        KAPC_STATE ApcState;
        BOOLEAN Attach;

        ASSERT(0 != Process);
        Attach = Process != PsGetCurrentProcess();

        if (Attach)
            KeStackAttachProcess(Process, &ApcState);
        MmUnmapLockedPages(Address, Mdl);
        if (Attach)
            KeUnstackDetachProcess(&ApcState);

        ObDereferenceObject(Process);
    }

    if (0 != Mdl)
        IoFreeMdl(Mdl);

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

    FSP_LEAVE_MJ("%s%sFileObject=%p",
        IRP_MN_QUERY_DIRECTORY == IrpSp->MinorFunction ?
            FileInformationClassSym(IrpSp->Parameters.QueryDirectory.FileInformationClass) : "",
        IRP_MN_QUERY_DIRECTORY == IrpSp->MinorFunction ? ", " : "",
        IrpSp->FileObject);
}
