/**
 * @file sys/dirctl.c
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

static NTSTATUS FspFsvolQueryDirectoryCopy(
    PUNICODE_STRING DirectoryPattern, BOOLEAN CaseInsensitive,
    PUNICODE_STRING DirectoryMarker, PUNICODE_STRING DirectoryMarkerOut,
    PUINT64 DirectoryMarkerAsNextOffset,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    BOOLEAN ReturnEaSize,
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

enum
{
    /* QueryDirectory */
    RequestFileNode                     = 0,
    RequestCookie                       = 1,
    RequestAddress                      = 2,
    RequestProcess                      = 3,
};

enum
{
    FspFsvolQueryDirectoryLengthMax     =
        FspFsvolDeviceDirInfoCacheItemSizeMax - FspMetaCacheItemHeaderSize,
};

static NTSTATUS FspFsvolQueryDirectoryCopy(
    PUNICODE_STRING DirectoryPattern, BOOLEAN CaseInsensitive,
    PUNICODE_STRING DirectoryMarker, PUNICODE_STRING DirectoryMarkerOut,
    PUINT64 DirectoryMarkerAsNextOffset,
    FILE_INFORMATION_CLASS FileInformationClass, BOOLEAN ReturnSingleEntry,
    BOOLEAN ReturnEaSize,
    FSP_FSCTL_DIR_INFO **PDirInfo, ULONG DirInfoSize,
    PVOID DestBuf, PULONG PDestLen)
{
#define FILL_INFO_BASE(TYPE, ...)\
    do\
    {\
        TYPE InfoStruct = { 0 }, *Info = &InfoStruct;\
        Info->NextEntryOffset = 0;\
        Info->FileIndex = 0;\
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
#define FILL_INFO_EASIZE()\
    if (!FlagOn(DirInfo->FileInfo.FileAttributes, FILE_ATTRIBUTE_REPARSE_POINT))\
    {\
        Info->EaSize = ReturnEaSize ? DirInfo->FileInfo.EaSize : 0;\
        /* magic computations are courtesy of NTFS */\
        if (0 != Info->EaSize)\
            Info->EaSize += 4;\
    }\
    else\
        /* Fix GitHub issue #380: turns out that the EaSize field is also used for the reparse tag */\
        Info->EaSize = DirInfo->FileInfo.ReparseTag

    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    BOOLEAN MatchAll = FspFileDescDirectoryPatternMatchAll == DirectoryPattern->Buffer, Match;
    BOOLEAN Loop = TRUE, DirectoryMarkerFound = FALSE;
    FSP_FSCTL_DIR_INFO *DirInfo = *PDirInfo;
    PUINT8 DirInfoEnd = (PUINT8)DirInfo + DirInfoSize;
    PUINT8 DestBufBgn = (PUINT8)DestBuf;
    PUINT8 DestBufEnd = (PUINT8)DestBuf + *PDestLen;
    PVOID PrevDestBuf = 0;
    ULONG BaseInfoLen, CopyLength;
    UNICODE_STRING FileName;
    UINT64 DirectoryNextOffset;

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

            FileName.Length =
            FileName.MaximumLength = (USHORT)(DirInfoSize - sizeof(FSP_FSCTL_DIR_INFO));
            FileName.Buffer = DirInfo->FileNameBuf;

            DirectoryNextOffset = DirInfo->NextOffset;

            if (0 != DirectoryMarker && 0 != DirectoryMarker->Buffer &&
                !DirectoryMarkerFound)
            {
                if (0 == DirectoryMarkerAsNextOffset)
                {
                    DirectoryMarkerFound = 0 == FspFileNameCompare(
                        &FileName, DirectoryMarker, CaseInsensitive, 0);
                }
                else
                {
                    ASSERT(sizeof(UINT64) == DirectoryMarker->Length);
                    DirectoryMarkerFound = DirectoryNextOffset == *(PUINT64)DirectoryMarker->Buffer;
                }
            }

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

                *PDestLen = (ULONG)((PUINT8)DestBuf + BaseInfoLen + CopyLength - DestBufBgn);

                switch (FileInformationClass)
                {
                case FileDirectoryInformation:
                    FILL_INFO(FILE_DIRECTORY_INFORMATION,);
                    break;
                case FileFullDirectoryInformation:
                    FILL_INFO(FILE_FULL_DIR_INFORMATION,
                        FILL_INFO_EASIZE();
                    );
                    break;
                case FileIdFullDirectoryInformation:
                    FILL_INFO(FILE_ID_FULL_DIR_INFORMATION,
                        FILL_INFO_EASIZE();
                        Info->FileId.QuadPart = DirInfo->FileInfo.IndexNumber;
                    );
                    break;
                case FileNamesInformation:
                    FILL_INFO_BASE(FILE_NAMES_INFORMATION,);
                    break;
                case FileBothDirectoryInformation:
                    FILL_INFO(FILE_BOTH_DIR_INFORMATION,
                        FILL_INFO_EASIZE();
                        Info->ShortNameLength = 0;
                        RtlZeroMemory(Info->ShortName, sizeof Info->ShortName);
                    );
                    break;
                case FileIdBothDirectoryInformation:
                    FILL_INFO(FILE_ID_BOTH_DIR_INFORMATION,
                        FILL_INFO_EASIZE();
                        Info->ShortNameLength = 0;
                        RtlZeroMemory(Info->ShortName, sizeof Info->ShortName);
                        Info->FileId.QuadPart = DirInfo->FileInfo.IndexNumber;
                    );
                    break;
                default:
                    ASSERT(0);
                    break;
                }

                if (0 == DirectoryMarkerAsNextOffset)
                {
                    DirectoryMarkerOut->Length = DirectoryMarkerOut->MaximumLength = FileName.Length;
                    DirectoryMarkerOut->Buffer = (PVOID)((PUINT8)DestBuf + BaseInfoLen);
                }
                else
                {
                    DirectoryMarkerOut->Length = DirectoryMarkerOut->MaximumLength = sizeof(UINT64);
                    DirectoryMarkerOut->Buffer = (PVOID)DirectoryMarkerAsNextOffset;
                    *DirectoryMarkerAsNextOffset = DirectoryNextOffset;
                }

                DestBuf = (PVOID)((PUINT8)DestBuf +
                    FSP_FSCTL_ALIGN_UP(BaseInfoLen + CopyLength, sizeof(LONGLONG)));

                if (ReturnSingleEntry)
                    /* cannot just break, *PDirInfo must be advanced */
                    Loop = FALSE;
            }
            else
            {
                if (0 == DirectoryMarkerAsNextOffset)
                    *DirectoryMarkerOut = FileName;
                else
                {
                    DirectoryMarkerOut->Length = DirectoryMarkerOut->MaximumLength = sizeof(UINT64);
                    DirectoryMarkerOut->Buffer = (PVOID)DirectoryMarkerAsNextOffset;
                    *DirectoryMarkerAsNextOffset = DirectoryNextOffset;
                }
            }
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

#undef FILL_INFO_EASIZE
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
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    BOOLEAN CaseInsensitive = !FileDesc->CaseSensitive;
    PUNICODE_STRING DirectoryPattern = &FileDesc->DirectoryPattern;
    UNICODE_STRING DirectoryMarker = FileDesc->DirectoryMarker;
    UINT64 DirectoryMarkerAsNextOffset = 0;
    PUINT8 DirInfoBgn = (PUINT8)DirInfo;
    PUINT8 DirInfoEnd = (PUINT8)DirInfo + DirInfoSize;

    DirInfo = (PVOID)(DirInfoBgn + FileDesc->DirInfoCacheHint);
    DirInfoSize = (ULONG)(DirInfoEnd - (PUINT8)DirInfo);

    Result = FspFsvolQueryDirectoryCopy(DirectoryPattern, CaseInsensitive,
        0 != FileDesc->DirInfoCacheHint ? 0 : &FileDesc->DirectoryMarker, &DirectoryMarker,
        FsvolDeviceExtension->VolumeParams.DirectoryMarkerAsNextOffset ?
            &DirectoryMarkerAsNextOffset : 0,
        FileInformationClass, ReturnSingleEntry,
        !!FsvolDeviceExtension->VolumeParams.ExtendedAttributes,
        &DirInfo, DirInfoSize,
        DestBuf, PDestLen);

    if (NT_SUCCESS(Result))
    {
        Result = FspFileDescSetDirectoryMarker(FileDesc, &DirectoryMarker);
        if (NT_SUCCESS(Result))
        {
            if (0 != *PDestLen)
                FileDesc->DirectoryHasSuchFile = TRUE;
            FileDesc->DirInfoCacheHint = (ULONG)((PUINT8)DirInfo - DirInfoBgn);
        }
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
    FSP_FILE_NODE *FileNode = FileDesc->FileNode;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        FspFsvolDeviceExtension(FileNode->FsvolDeviceObject);
    BOOLEAN CaseInsensitive = !FileDesc->CaseSensitive;
    PUNICODE_STRING DirectoryPattern = &FileDesc->DirectoryPattern;
    UNICODE_STRING DirectoryMarker = FileDesc->DirectoryMarker;
    UINT64 DirectoryMarkerAsNextOffset = 0;

    FSP_FSCTL_STATIC_ASSERT(
        FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) >=
        FIELD_OFFSET(FILE_ID_BOTH_DIR_INFORMATION, FileName),
        "FSP_FSCTL_DIR_INFO must be bigger than FILE_ID_BOTH_DIR_INFORMATION");

    Result = FspFsvolQueryDirectoryCopy(DirectoryPattern, CaseInsensitive,
        0, &DirectoryMarker,
        FsvolDeviceExtension->VolumeParams.DirectoryMarkerAsNextOffset ?
            &DirectoryMarkerAsNextOffset : 0,
        FileInformationClass, ReturnSingleEntry,
        !!FsvolDeviceExtension->VolumeParams.ExtendedAttributes,
        &DirInfo, DirInfoSize,
        DestBuf, PDestLen);

    if (NT_SUCCESS(Result))
    {
        Result = FspFileDescSetDirectoryMarker(FileDesc, &DirectoryMarker);
        if (NT_SUCCESS(Result))
        {
            if (0 != *PDestLen)
                FileDesc->DirectoryHasSuchFile = TRUE;
        }
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
    ULONG BaseInfoLen;
    PUNICODE_STRING FileName = IrpSp->Parameters.QueryDirectory.FileName;
    //ULONG FileIndex = IrpSp->Parameters.QueryDirectory.FileIndex;
    PVOID Buffer = 0 == Irp->MdlAddress ?
        Irp->UserBuffer : MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    ULONG Length = IrpSp->Parameters.QueryDirectory.Length;
    ULONG QueryDirectoryLength, QueryDirectoryLengthMin;
    PVOID DirInfoBuffer;
    ULONG DirInfoSize;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    BOOLEAN PassQueryDirectoryPattern, PatternIsWild, PatternIsFileName;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    if (0 == Buffer)
        return 0 == Irp->MdlAddress ? STATUS_INVALID_PARAMETER : STATUS_INSUFFICIENT_RESOURCES;

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

    /* try to acquire the FileNode exclusive; Full because we may need to send a Request */
    Success = DEBUGTEST(90) &&
        FspFileNodeTryAcquireExclusiveF(FileNode, FspFileNodeAcquireFull, CanWait);
    if (!Success)
        return FspWqRepostIrpWorkItem(Irp, FspFsvolQueryDirectoryRetry, 0);

    /* if we have been retried reset our work item now! */
    if (0 != Request)
    {
        FspIrpDeleteRequest(Irp);
        Request = 0;
    }

    /* reset the FileDesc */
    Result = FspFileDescResetDirectory(FileDesc, FileName, RestartScan, IndexSpecified);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /* if the pattern does not contain wildcards and we already returned a result, bail now! */
    PatternIsWild = FsRtlDoesNameContainWildCards(&FileDesc->DirectoryPattern);
    if (!PatternIsWild && FileDesc->DirectoryHasSuchFile)
    {
        FspFileNodeRelease(FileNode, Full);
        return STATUS_NO_MORE_FILES;
    }

    /* see if the required information is still in the cache and valid! */
    if (FspFileNodeReferenceDirInfo(FileNode, &DirInfoBuffer, &DirInfoSize))
    {
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

        /* reset Length! */
        Length = IrpSp->Parameters.QueryDirectory.Length;
    }

    FspFileNodeConvertExclusiveToShared(FileNode, Full);

    /* special handling when pattern is filename */
    PatternIsFileName = FsvolDeviceExtension->VolumeParams.PassQueryDirectoryFileName &&
        !PatternIsWild;
    PassQueryDirectoryPattern = PatternIsFileName ||
        (FsvolDeviceExtension->VolumeParams.PassQueryDirectoryPattern &&
            FspFileDescDirectoryPatternMatchAll != FileDesc->DirectoryPattern.Buffer);

    /* probe and lock the user buffer */
    Result = FspLockUserBuffer(Irp, Length, IoWriteAccess);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /* create request */
    Result = FspIopCreateRequestEx(Irp, 0,
        (PassQueryDirectoryPattern ? FileDesc->DirectoryPattern.Length + sizeof(WCHAR) : 0) +
        (FsvolDeviceExtension->VolumeParams.MaxComponentLength + 1) * sizeof(WCHAR),
        FspFsvolQueryDirectoryRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    /*
     * Compute QueryDirectoryLength
     *
     * How much data to request from the file system varies according to the following matrix:
     *
     * Pattern                  | NoCache | Cache+1st | Cache+2nd
     * -------------------------+---------+-----------+----------
     * Full Wild                | Ratio   | Maximum   | Ratio
     * Partial Wild w/o PassQDP | Maximum | Maximum   | Maximum
     * Partial Wild w/  PassQDP | Ratio   | Ratio     | Ratio
     * File Name w/o PassQDF    | Maximum | Maximum   | Maximum
     * File Name w/  PassQDF    | Minimum | Minimum   | Minimum
     *
     * NoCache means DirInfo caching disabled. Cache+1st means DirInfo caching enabled, but
     * cache not primed. Cache+2nd means DirInfo caching enabled, cache is primed, but missed.
     * [If there is no cache miss, there is no need to send the request to the file system.]
     *
     * Maximum means to request the maximum size allowed by the FSD. Minimum means the size that
     * is guaranteed to contain at least one entry. Ratio means to compute how many directory
     * entries to request from the file system based on an estimate of how many entries the FSD
     * is supposed to deliver.
     *
     * The Ratio computation is as follows: Assume that N is the size of the average file name,
     * taken to be 24 * sizeof(WCHAR). Let M be the number of entries that can fit in the passed
     * buffer:
     *
     * M := Length / (BaseInfoLen + N) = QueryDirectoryLength / (sizeof(FSP_FSCTL_DIR_INFO) + N)
     * => QueryDirectoryLength = Length * (sizeof(FSP_FSCTL_DIR_INFO) + N) / (BaseInfoLen + N)
     */

    QueryDirectoryLengthMin = sizeof(FSP_FSCTL_DIR_INFO) +
        FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR);
    QueryDirectoryLengthMin = FSP_FSCTL_ALIGN_UP(QueryDirectoryLengthMin, 8);
    ASSERT(QueryDirectoryLengthMin < FspFsvolQueryDirectoryLengthMax);
    if (0 != FsvolDeviceExtension->VolumeParams.DirInfoTimeout &&
        0 == FileDesc->DirectoryMarker.Buffer)
    {
        if (PatternIsFileName)
            QueryDirectoryLength = QueryDirectoryLengthMin;
        else if (PassQueryDirectoryPattern)
        {
            QueryDirectoryLength = Length *
                (sizeof(FSP_FSCTL_DIR_INFO) + 24 * sizeof(WCHAR)) / (BaseInfoLen + 24 * sizeof(WCHAR));
            QueryDirectoryLength = FSP_FSCTL_ALIGN_UP(QueryDirectoryLength, 8);
            if (QueryDirectoryLength < QueryDirectoryLengthMin)
                QueryDirectoryLength = QueryDirectoryLengthMin;
            else if (QueryDirectoryLength > FspFsvolQueryDirectoryLengthMax)
                QueryDirectoryLength = FspFsvolQueryDirectoryLengthMax;
        }
        else
            QueryDirectoryLength = FspFsvolQueryDirectoryLengthMax;
    }
    else
    {
        if (PatternIsFileName)
            QueryDirectoryLength = QueryDirectoryLengthMin;
        else if (PassQueryDirectoryPattern ||
            FspFileDescDirectoryPatternMatchAll == FileDesc->DirectoryPattern.Buffer)
        {
            QueryDirectoryLength = Length *
                (sizeof(FSP_FSCTL_DIR_INFO) + 24 * sizeof(WCHAR)) / (BaseInfoLen + 24 * sizeof(WCHAR));
            QueryDirectoryLength = FSP_FSCTL_ALIGN_UP(QueryDirectoryLength, 8);
            if (QueryDirectoryLength < QueryDirectoryLengthMin)
                QueryDirectoryLength = QueryDirectoryLengthMin;
            else if (QueryDirectoryLength > FspFsvolQueryDirectoryLengthMax)
                QueryDirectoryLength = FspFsvolQueryDirectoryLengthMax;
        }
        else
            QueryDirectoryLength = FspFsvolQueryDirectoryLengthMax;
    }

    Request->Kind = FspFsctlTransactQueryDirectoryKind;
    Request->Req.QueryDirectory.UserContext = FileNode->UserContext;
    Request->Req.QueryDirectory.UserContext2 = FileDesc->UserContext2;
    Request->Req.QueryDirectory.Length = QueryDirectoryLength;
    Request->Req.QueryDirectory.CaseSensitive = FileDesc->CaseSensitive;

    if (PassQueryDirectoryPattern)
    {
        Request->Req.QueryDirectory.PatternIsFileName = PatternIsFileName;
        Request->Req.QueryDirectory.Pattern.Offset =
            Request->FileName.Size;
        Request->Req.QueryDirectory.Pattern.Size =
            FileDesc->DirectoryPattern.Length + sizeof(WCHAR);
        RtlCopyMemory(Request->Buffer + Request->Req.QueryDirectory.Pattern.Offset,
            FileDesc->DirectoryPattern.Buffer, FileDesc->DirectoryPattern.Length);
        *(PWSTR)(Request->Buffer +
            Request->Req.QueryDirectory.Pattern.Offset +
            FileDesc->DirectoryPattern.Length) = L'\0';
    }

    if (0 != FileDesc->DirectoryMarker.Buffer)
    {
        ASSERT(
            FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR) >=
            FileDesc->DirectoryMarker.Length);

        Request->Req.QueryDirectory.Marker.Offset =
            Request->FileName.Size + Request->Req.QueryDirectory.Pattern.Size;
        Request->Req.QueryDirectory.Marker.Size =
            FileDesc->DirectoryMarker.Length + sizeof(WCHAR);
        RtlCopyMemory(Request->Buffer + Request->Req.QueryDirectory.Marker.Offset,
            FileDesc->DirectoryMarker.Buffer, FileDesc->DirectoryMarker.Length);
        *(PWSTR)(Request->Buffer +
            Request->Req.QueryDirectory.Marker.Offset +
            FileDesc->DirectoryMarker.Length) = L'\0';
    }

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

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(FsvolDeviceObject);
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    PUNICODE_STRING FileName = IrpSp->Parameters.QueryDirectory.FileName;

    /* only directory files can be queried */
    if (!FileNode->IsDirectory)
        return STATUS_INVALID_PARAMETER;

    /* check that FileName is valid (if supplied) */
    if (0 != FileName &&
        !FspFileNameIsValidPattern(FileName, FsvolDeviceExtension->VolumeParams.MaxComponentLength))
        return STATUS_INVALID_PARAMETER;

    return FspFsvolQueryDirectoryRetry(FsvolDeviceObject, Irp, IrpSp, IoIsOperationSynchronous(Irp));
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
    PVOID Cookie;
    PVOID Address;
    PEPROCESS Process;

    Result = FspProcessBufferAcquire(Request->Req.QueryDirectory.Length, &Cookie, &Address);
    if (!NT_SUCCESS(Result))
        return Result;

    /* get a pointer to the current process so that we can release the buffer later */
    Process = PsGetCurrentProcess();
    ObReferenceObject(Process);

    Request->Req.QueryDirectory.Address = (UINT64)(UINT_PTR)Address;

    FspIopRequestContext(Request, RequestCookie) = (PVOID)((UINT_PTR)Cookie | 1);
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
    PVOID Buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
    ULONG Length = IrpSp->Parameters.QueryDirectory.Length;
    PVOID DirInfoBuffer;
    ULONG DirInfoSize;
    BOOLEAN Success;

    ASSERT(FileNode == FileDesc->FileNode);

    if (0 == Response->IoStatus.Information)
    {
        Result = !FileDesc->DirectoryHasSuchFile ?
            STATUS_NO_SUCH_FILE : STATUS_NO_MORE_FILES;
        FSP_RETURN();
    }

    if (0 != FspIopRequestContext(Request, RequestFileNode))
    {
        FspIopRequestContext(Request, FspIopRequestExtraContext) = (PVOID)
            FspFileNodeDirInfoChangeNumber(FileNode);
        FspIopRequestContext(Request, RequestFileNode) = 0;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }

    /* acquire FileNode exclusive Full (because we may need to go back to user-mode) */
    Success = DEBUGTEST(90) && FspFileNodeTryAcquireExclusive(FileNode, Full);
    if (!Success)
    {
        FspIopRetryCompleteIrp(Irp, Response, &Result);
        FSP_RETURN();
    }

    if (0 == Request->Req.QueryDirectory.Pattern.Size &&
        0 == Request->Req.QueryDirectory.Marker.Size &&
        FspFileNodeTrySetDirInfo(FileNode,
            (PVOID)(UINT_PTR)Request->Req.QueryDirectory.Address,
            (ULONG)Response->IoStatus.Information,
            (ULONG)(UINT_PTR)FspIopRequestContext(Request, FspIopRequestExtraContext)) &&
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
        DirInfoBuffer = (PVOID)(UINT_PTR)Request->Req.QueryDirectory.Address;
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

        FspIopResetRequest(Request, FspFsvolQueryDirectoryRequestFini);

        Request->Req.QueryDirectory.Address = 0;
        Request->Req.QueryDirectory.Marker.Offset = 0;
        Request->Req.QueryDirectory.Marker.Size = 0;
        if (0 != FileDesc->DirectoryMarker.Buffer)
        {
            ASSERT(
                FsvolDeviceExtension->VolumeParams.MaxComponentLength * sizeof(WCHAR) >=
                FileDesc->DirectoryMarker.Length);

            Request->Req.QueryDirectory.Marker.Offset =
                Request->FileName.Size + Request->Req.QueryDirectory.Pattern.Size;
            Request->Req.QueryDirectory.Marker.Size =
                FileDesc->DirectoryMarker.Length + sizeof(WCHAR);
            RtlCopyMemory(Request->Buffer + Request->Req.QueryDirectory.Marker.Offset,
                FileDesc->DirectoryMarker.Buffer, FileDesc->DirectoryMarker.Length);
            *(PWSTR)(Request->Buffer +
                Request->Req.QueryDirectory.Marker.Offset +
                FileDesc->DirectoryMarker.Length) = L'\0';
        }

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
    PVOID Cookie = (PVOID)((UINT_PTR)Context[RequestCookie] & ~1);
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
        FspProcessBufferRelease(Cookie, Address);
        if (Attach)
            KeUnstackDetachProcess(&ApcState);

        ObDereferenceObject(Process);
    }

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
