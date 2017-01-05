/**
 * @file sys/dirctl.c
 *
 * @copyright 2015-2017 Bill Zissimopoulos
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

/*
 * NOTE:
 *
 * FspIopCompleteIrpEx does some special processing for IRP_MJ_DIRECTORY_CONTROL /
 * IRP_MN_QUERY_DIRECTORY IRP's that come from SRV2. If the processing of this IRP
 * changes substantially (in particular if we eliminate our use of
 * Irp->AssociatedIrp.SystemBuffer) we should also revisit FspIopCompleteIrpEx.
 */

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
static IO_COMPLETION_ROUTINE FspFsvolNotifyChangeDirectoryCompletion;
static WORKER_THREAD_ROUTINE FspFsvolNotifyChangeDirectoryCompletionWork;
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
// !#pragma alloc_text(PAGE, FspFsvolNotifyChangeDirectoryCompletion)
#pragma alloc_text(PAGE, FspFsvolNotifyChangeDirectoryCompletionWork)
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
    RequestIrp                          = 0,
    RequestMdl                          = 1,
    RequestAddress                      = 2,
    RequestProcess                      = 3,

    /* QueryDirectoryRetry */
    RequestSystemBufferLength           = 0,

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
        RtlCopyMemory(Info, &InfoStruct, FIELD_OFFSET(TYPE, FileName));\
        RtlMoveMemory(Info->FileName, DirInfo->FileNameBuf, CopyLength);\
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

    NTSTATUS Result = STATUS_SUCCESS;
    BOOLEAN MatchAll = FspFileDescDirectoryPatternMatchAll == DirectoryPattern->Buffer, Match;
    BOOLEAN Loop = TRUE, DirectoryOffsetFound = FALSE;
    FSP_FSCTL_DIR_INFO *DirInfo = *PDirInfo;
    PUINT8 DirInfoEnd = (PUINT8)DirInfo + DirInfoSize;
    PUINT8 DestBufBgn = (PUINT8)DestBuf;
    PUINT8 DestBufEnd = (PUINT8)DestBuf + *PDestLen;
    PVOID PrevDestBuf = 0;
    ULONG BaseInfoLen, CopyLength;
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
            Loop && (PUINT8)DirInfo + sizeof(DirInfo->Size) <= DirInfoEnd;
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

            /* CopyLength is the same as FileName.Length except on STATUS_BUFFER_OVERFLOW */
            CopyLength = FileName.Length;

            Match = MatchAll;
            if (!Match)
            {
                Result = FspFileNameInExpression(DirectoryPattern, &FileName, CaseInsensitive, 0, &Match);
                if (!NT_SUCCESS(Result))
                    return Result;
            }

            if (Match)
            {
                if ((PUINT8)DestBuf + BaseInfoLen + CopyLength > DestBufEnd)
                {
                    /* if we have already copied something exit the loop */
                    if (0 != *PDestLen)
                        break;

                    if ((PUINT8)DestBuf + BaseInfoLen > DestBufEnd)
                        /* buffer is too small, can't copy anything */
                        return STATUS_BUFFER_TOO_SMALL;
                    else
                    {
                        /* copy as much of the file name as we can and return STATUS_BUFFER_OVERFLOW */
                        CopyLength = (USHORT)(DestBufEnd - ((PUINT8)DestBuf + BaseInfoLen));
                        Result = STATUS_BUFFER_OVERFLOW;
                        Loop = FALSE;
                    }
                }

                if (0 != PrevDestBuf)
                    *(PULONG)PrevDestBuf = (ULONG)((PUINT8)DestBuf - (PUINT8)PrevDestBuf);
                PrevDestBuf = DestBuf;

                *PDirectoryOffset = DirInfo->NextOffset;
                *PDestLen = (ULONG)((PUINT8)DestBuf + BaseInfoLen + CopyLength - DestBufBgn);

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

                DestBuf = (PVOID)((PUINT8)DestBuf +
                    FSP_FSCTL_ALIGN_UP(BaseInfoLen + CopyLength, sizeof(LONGLONG)));

                if (ReturnSingleEntry)
                    /* cannot just break, *PDirInfo must be advanced */
                    Loop = FALSE;
            }
            else
                *PDirectoryOffset = DirInfo->NextOffset;
        }
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
        if (STATUS_INSUFFICIENT_RESOURCES == Result)
            return Result;
        return FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
    }

    /* our code flow should allow only these two status codes here */
    ASSERT(STATUS_SUCCESS == Result || STATUS_BUFFER_OVERFLOW == Result);

    *PDirInfo = DirInfo;

    return Result;

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
    FSP_FSCTL_STATIC_ASSERT(
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

static inline NTSTATUS FspFsvolQueryDirectoryBufferUserBuffer(
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension, PIRP Irp, PULONG PLength)
{
    if (0 != Irp->AssociatedIrp.SystemBuffer)
        return STATUS_SUCCESS;

    NTSTATUS Result;
    ULONG Length = *PLength;

    if (Length > FspFsvolDeviceDirInfoCacheItemSizeMax)
        Length = FspFsvolDeviceDirInfoCacheItemSizeMax;
    else if (Length < sizeof(FSP_FSCTL_DIR_INFO) +
        FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR))
        Length = sizeof(FSP_FSCTL_DIR_INFO) +
            FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR);

    Result = FspBufferUserBuffer(Irp, FSP_FSCTL_ALIGN_UP(Length, PAGE_SIZE), IoWriteAccess);
    if (!NT_SUCCESS(Result))
        return Result;

    *PLength = Length;

    return STATUS_SUCCESS;
}

static NTSTATUS FspFsvolQueryDirectoryRetry(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait)
{
    /*
     * The SystemBufferLength contains the length of the SystemBuffer that we
     * are going to allocate (in FspFsvolQueryDirectoryBufferUserBuffer). This
     * buffer is going to be used as the IRP SystemBuffer and it will also be
     * mapped into the user mode file system process (in
     * FspFsvolDirectoryControlPrepare) so that the file system can fill in the
     * buffer.
     *
     * The SystemBufferLength is not the actual length that we are going to use
     * when completing the IRP. This will be computed at IRP completion time
     * (using FspFsvolQueryDirectoryCopy). Instead the SystemBufferLength is
     * the size that we want the user mode file system to see and it may be
     * different from the requested length for the following reasons:
     *
     *   - If the FileInfoTimeout is non-zero, then the directory maintains a
     *     DirInfo meta cache that can be used to fulfill IRP requests without
     *     reaching out to user mode. In this case we want the SystemBufferLength
     *     to be FspFsvolDeviceDirInfoCacheItemSizeMax so that we read up to the
     *     cache size maximum.
     *
     *   - If the requested DirectoryPattern (stored in FileDesc) is not the "*"
     *     (MatchAll) pattern, then we want to read as many entries as possible
     *     from the user mode file system to avoid multiple roundtrips to user
     *     mode when doing file name matching. In this case we set again the
     *     SystemBufferLength to be FspFsvolDeviceDirInfoCacheItemSizeMax. This
     *     is an important optimization and without it QueryDirectory is *very*
     *     slow without the DirInfo meta cache (i.e. when FileInfoTimeout is 0).
     *
     *   - If the requsted DirectoryPattern is the MatchAll pattern then we set
     *     the SystemBufferLength to the requested (IRP) length as it is actually
     *     counter-productive to try to read more than we need.
     */
#define GetSystemBufferLengthMaybeCached()\
    (0 != FsvolDeviceExtension->VolumeParams.FileInfoTimeout && 0 == FileDesc->DirectoryOffset) ||\
    FspFileDescDirectoryPatternMatchAll != FileDesc->DirectoryPattern.Buffer ?\
        FspFsvolDeviceDirInfoCacheItemSizeMax : Length
#define GetSystemBufferLengthNonCached()\
    FspFileDescDirectoryPatternMatchAll != FileDesc->DirectoryPattern.Buffer ?\
        FspFsvolDeviceDirInfoCacheItemSizeMax : Length
#define GetSystemBufferLengthBestGuess()\
    FspFsvolDeviceDirInfoCacheItemSizeMax

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
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    SystemBufferLength = 0 != Request ?
        (ULONG)(UINT_PTR)FspIopRequestContext(Request, RequestSystemBufferLength) : 0;

    /* try to acquire the FileNode exclusive; Full because we may need to send a Request */
    Success = DEBUGTEST(90) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, CanWait);
    if (!Success)
    {
        if (0 == SystemBufferLength)
            SystemBufferLength = GetSystemBufferLengthBestGuess();

        Result = FspFsvolQueryDirectoryBufferUserBuffer(
            FsvolDeviceExtension, Irp, &SystemBufferLength);
        if (!NT_SUCCESS(Result))
            return Result;

        Result = FspWqCreateIrpWorkItem(Irp, FspFsvolQueryDirectoryRetry, 0);
        if (!NT_SUCCESS(Result))
            return Result;

        Request = FspIrpRequest(Irp);
        FspIopRequestContext(Request, RequestSystemBufferLength) =
            (PVOID)(UINT_PTR)SystemBufferLength;

        FspWqPostIrpWorkItem(Irp);

        return STATUS_PENDING;
    }

    /* if we have been retried reset our work item now! */
    if (0 != Request)
    {
        FspIrpDeleteRequest(Irp);
        Request = 0;
    }

    /* set the DirectoryPattern in the FileDesc */
    Result = FspFileDescResetDirectoryPattern(FileDesc, FileName,
        RestartScan && 0 != FileName && 0 != FileName->Length);
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
        if (0 == SystemBufferLength)
            SystemBufferLength = GetSystemBufferLengthNonCached();

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
    {
        if (0 == SystemBufferLength)
            SystemBufferLength = GetSystemBufferLengthMaybeCached();
    }

    FspFileNodeConvertExclusiveToShared(FileNode, Full);

    /* buffer the user buffer! */
    Result = FspFsvolQueryDirectoryBufferUserBuffer(
        FsvolDeviceExtension, Irp, &SystemBufferLength);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /* create request */
    Result = FspIopCreateRequestEx(Irp, 0,
        FspFileDescDirectoryPatternMatchAll != FileDesc->DirectoryPattern.Buffer ?
            FileDesc->DirectoryPattern.Length + sizeof(WCHAR) : 0,
        FspFsvolQueryDirectoryRequestFini, &Request);
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
    Request->Req.QueryDirectory.CaseSensitive = FileDesc->CaseSensitive;

    if (FspFileDescDirectoryPatternMatchAll != FileDesc->DirectoryPattern.Buffer)
    {
        Request->Req.QueryDirectory.Pattern.Offset = Request->FileName.Size;
        Request->Req.QueryDirectory.Pattern.Size =
            FileDesc->DirectoryPattern.Length + sizeof(WCHAR);
        RtlCopyMemory(Request->Buffer + Request->FileName.Size,
            FileDesc->DirectoryPattern.Buffer, FileDesc->DirectoryPattern.Length);
        *(PWSTR)(Request->Buffer + Request->FileName.Size + FileDesc->DirectoryPattern.Length) =
            L'\0';
    }

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestIrp) = Irp;

    return FSP_STATUS_IOQ_POST;

#undef GetSystemBufferLengthBestGuess
#undef GetSystemBufferLengthNonCached
#undef GetSystemBufferLengthMaybeCached
}

static NTSTATUS FspFsvolQueryDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FILE_INFORMATION_CLASS FileInformationClass = IrpSp->Parameters.QueryDirectory.FileInformationClass;
    PUNICODE_STRING FileName = IrpSp->Parameters.QueryDirectory.FileName;
    ULONG Length = IrpSp->Parameters.QueryDirectory.Length;
    ULONG BaseInfoLen;

    /* SystemBuffer must be NULL as we are going to be using it! */
    if (0 != Irp->AssociatedIrp.SystemBuffer)
    {
        ASSERT(0);
        return STATUS_INVALID_PARAMETER;
    }

    /* only directory files can be queried */
    if (!FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /* check that FileName is valid (if supplied) */
    if (0 != FileName &&
        !FspFileNameIsValidPattern(FileName, FsvolDeviceExtension->VolumeParams.MaxComponentLength))
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

typedef struct
{
    PDEVICE_OBJECT FsvolDeviceObject;
    WORK_QUEUE_ITEM WorkItem;
} FSP_FSVOL_NOTIFY_CHANGE_DIRECTORY_COMPLETION_CONTEXT;

static NTSTATUS FspFsvolNotifyChangeDirectory(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
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
    ULONG CompletionFilter = IrpSp->Parameters.NotifyDirectory.CompletionFilter;
    BOOLEAN WatchTree = BooleanFlagOn(IrpSp->Flags, SL_WATCH_TREE);
    BOOLEAN DeletePending;
    FSP_FSVOL_NOTIFY_CHANGE_DIRECTORY_COMPLETION_CONTEXT *CompletionContext;

    ASSERT(FileNode == FileDesc->FileNode);

    /* only directory files can be watched */
    if (!FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /* only processes with traverse privilege can watch trees! */
    if (WatchTree && !FileDesc->HasTraversePrivilege)
        return STATUS_ACCESS_DENIED;

    /* stop now if the directory is pending deletion */
    DeletePending = 0 != FileNode->DeletePending;
    MemoryBarrier();
    if (DeletePending)
        return STATUS_DELETE_PENDING;

    /*
     * This IRP will be completed by the FSRTL package and therefore
     * we will have no chance to do our normal IRP completion processing.
     * Hook the IRP completion and perform the IRP completion processing
     * there.
     */

    CompletionContext = FspAllocNonPaged(sizeof *CompletionContext);
    if (0 == CompletionContext)
        return STATUS_INSUFFICIENT_RESOURCES;
    CompletionContext->FsvolDeviceObject = FsvolDeviceObject;
    ExInitializeWorkItem(&CompletionContext->WorkItem,
        FspFsvolNotifyChangeDirectoryCompletionWork, CompletionContext);

    Result = FspIrpHook(Irp, FspFsvolNotifyChangeDirectoryCompletion, CompletionContext);
    if (!NT_SUCCESS(Result))
    {
        FspFree(CompletionContext);
        return Result;
    }

    /*
     * It is possible for FspNotifyChangeDirectory to complete the IRP immediately.
     * In this case trying to access the IRP (to get its IrpFlags) in FspFileNodeRelease
     * can lead to a bugcheck. For this reason we set the TopLevelIrp to NULL here.
     *
     * IRP_MN_NOTIFY_CHANGE_DIRECTORY does not need the TopLevelIrp functionality,
     * because it cannot be used recursively (I believe -- famous last words).
     */
    PIRP TopLevelIrp = IoGetTopLevelIrp();
    IoSetTopLevelIrp(0);

    FspFileNodeAcquireExclusive(FileNode, Main);

    Result = FspNotifyChangeDirectory(
        FsvolDeviceExtension->NotifySync,
        &FsvolDeviceExtension->NotifyList,
        FileDesc,
        &FileNode->FileName,
        WatchTree,
        CompletionFilter,
        Irp);

    FspFileNodeRelease(FileNode, Main);

    if (!NT_SUCCESS(Result))
    {
        /* set back the top level IRP just in case! */
        IoSetTopLevelIrp(TopLevelIrp);

        FspIrpHookReset(Irp);
        FspFree(CompletionContext);
        return Result;
    }

    return STATUS_PENDING;
}

static NTSTATUS FspFsvolNotifyChangeDirectoryCompletion(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    // !PAGED_CODE();

    FSP_FSVOL_NOTIFY_CHANGE_DIRECTORY_COMPLETION_CONTEXT *CompletionContext =
        FspIrpHookContext(Context);
    ExQueueWorkItem(&CompletionContext->WorkItem, DelayedWorkQueue);

    return FspIrpHookNext(DeviceObject, Irp, Context);
}

static VOID FspFsvolNotifyChangeDirectoryCompletionWork(PVOID Context)
{
    PAGED_CODE();

    FSP_FSVOL_NOTIFY_CHANGE_DIRECTORY_COMPLETION_CONTEXT *CompletionContext = Context;
    FspDeviceDereference(CompletionContext->FsvolDeviceObject);
    FspFree(CompletionContext);
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
    Result = FspMapLockedPagesInUserMode(Mdl, &Address, 0);
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

    if (Response->IoStatus.Information > Request->Req.QueryDirectory.Length)
        FSP_RETURN(Result = STATUS_INTERNAL_ERROR);

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
        DirInfoChangeNumber = FspFileNodeDirInfoChangeNumber(FileNode);
        Request->Kind = FspFsctlTransactReservedKind;
        FspIopResetRequest(Request, 0);
        FspIopRequestContext(Request, RequestDirInfoChangeNumber) = (PVOID)DirInfoChangeNumber;
    }
    else
        DirInfoChangeNumber =
            (ULONG)(UINT_PTR)FspIopRequestContext(Request, RequestDirInfoChangeNumber);

    /* acquire FileNode exclusive Full (because we may need to go back to user-mode) */
    Success = DEBUGTEST(90) && FspFileNodeTryAcquireExclusive(FileNode, Full);
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
        FspIopRequestContext(Request, RequestIrp) = Irp;

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

    PIRP Irp = Context[RequestIrp];
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

    if (0 != Irp)
    {
        PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
        FSP_FILE_NODE *FileNode = IrpSp->FileObject->FsContext;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }
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
