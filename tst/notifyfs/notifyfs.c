/**
 * @file notifyfs.c
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

#include <winfsp/winfsp.h>
#include <sddl.h>

#define DEBUGFLAGS                      0
//#define DEBUGFLAGS                      -1

#define PROGNAME                        "notifyfs"
#define ALLOCATION_UNIT                 4096

#define info(format, ...)               FspServiceLog(EVENTLOG_INFORMATION_TYPE, format, __VA_ARGS__)
#define warn(format, ...)               FspServiceLog(EVENTLOG_WARNING_TYPE, format, __VA_ARGS__)
#define fail(format, ...)               FspServiceLog(EVENTLOG_ERROR_TYPE, format, __VA_ARGS__)

typedef struct
{
    FSP_FILE_SYSTEM *FileSystem;
    PTP_TIMER Timer;
    UINT32 Ticks;
} NOTIFYFS;

static PSECURITY_DESCRIPTOR DefaultSecurity;
static ULONG DefaultSecuritySize;

static UINT32 CountFromTicks(UINT32 Ticks)
{
    /*
     * The formula below produces the periodic sequence:
     *     0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
     *     0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
     *     ...
     */
    UINT32 div10 = (Ticks % 20) / 10;
    UINT32 mod10 = Ticks % 10;
    UINT32 mdv10 = 1 - div10;
    UINT32 mmd10 = 10 - mod10;
    return mdv10 * mod10 + div10 * mmd10;
}

static UINT32 FileCount(NOTIFYFS *Notifyfs)
{
    UINT32 Ticks = InterlockedOr(&Notifyfs->Ticks, 0);
    return CountFromTicks(Ticks);
}

static UINT32 FileLookup(NOTIFYFS *Notifyfs, PWSTR FileName)
{
    FileName++;
    PWSTR Endp;
    UINT32 Count = FileCount(Notifyfs);
    UINT32 Index = wcstoul(FileName, &Endp, 10);
    if ('\0' != *Endp || (FileName != Endp && (0 == Index || Index > Count)))
        return -1;  /* not found */
    if (FileName == Endp)
        return 0;   /* root */
    return Index;   /* regular file named 1, 2, ..., Count */
}

static UINT32 FileContents(UINT32 Index, WCHAR P[32])
{
    WCHAR Buffer[32];
    if (0 == P)
        P = Buffer;
    if (0 == Index)
        P[0] = '\0';
    else
        wsprintfW(P, L"%u\n", (unsigned)Index);
    return lstrlenW(P);
}

static VOID FillFileInfo(UINT32 Index, FSP_FSCTL_FILE_INFO *FileInfo)
{
    FILETIME SystemTime;

    GetSystemTimeAsFileTime(&SystemTime);

    memset(FileInfo, 0, sizeof FileInfo);
    FileInfo->FileAttributes = 0 == Index ? FILE_ATTRIBUTE_DIRECTORY : 0;
    FileInfo->FileSize = FileContents(Index, 0);
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1)
        / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime =
    FileInfo->LastAccessTime =
    FileInfo->LastWriteTime =
    FileInfo->ChangeTime = *(PUINT64)&SystemTime;
}

static BOOLEAN AddDirInfo(PWSTR FileName, UINT32 Index,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    union
    {
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_DIR_INFO D;
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.D;

    memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    if (0 != FileName)
        lstrcpyW(DirInfo->FileNameBuf, FileName);
    else
        wsprintfW(DirInfo->FileNameBuf, L"%u", (unsigned)Index);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(DirInfo->FileNameBuf) * sizeof(WCHAR));
    FillFileInfo(Index, &DirInfo->FileInfo);

    return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
}

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    memset(VolumeInfo, 0, sizeof *VolumeInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    NOTIFYFS *Notifyfs = (NOTIFYFS *)FileSystem->UserContext;
    UINT32 Index;

    Index = FileLookup(Notifyfs, FileName);
    if (-1 == Index)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (0 != PFileAttributes)
        *PFileAttributes = 0 == Index ? FILE_ATTRIBUTE_DIRECTORY : 0;

    if (0 != PSecurityDescriptorSize)
    {
        if (DefaultSecuritySize > *PSecurityDescriptorSize)
        {
            *PSecurityDescriptorSize = DefaultSecuritySize;
            return STATUS_BUFFER_OVERFLOW;
        }

        *PSecurityDescriptorSize = DefaultSecuritySize;
        if (0 != SecurityDescriptor)
            memcpy(SecurityDescriptor, DefaultSecurity, DefaultSecuritySize);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS Create(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
    NOTIFYFS *Notifyfs = (NOTIFYFS *)FileSystem->UserContext;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT *FullContext = (PVOID)PFileContext;
    UINT32 Index;

    Index = FileLookup(Notifyfs, FileName);
    if (-1 == Index)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    FullContext->UserContext = Index;
    FullContext->UserContext2 = 0;

    FillFileInfo(Index, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    NOTIFYFS *Notifyfs = (NOTIFYFS *)FileSystem->UserContext;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT *FullContext = FileContext;
    UINT32 Index = (UINT32)FullContext->UserContext;
    UINT64 EndOffset;
    WCHAR ContentBuf[32];
    UINT32 ContentLen;

    ContentLen = FileContents(Index, ContentBuf);

    if (Offset >= ContentLen)
        return STATUS_END_OF_FILE;

    EndOffset = Offset + Length;
    if (EndOffset > ContentLen)
        EndOffset = ContentLen;

    memcpy(Buffer, (PUINT8)ContentBuf + Offset, (size_t)(EndOffset - Offset));

    *PBytesTransferred = (ULONG)(EndOffset - Offset);

    return STATUS_SUCCESS;
}

static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    NOTIFYFS *Notifyfs = (NOTIFYFS *)FileSystem->UserContext;
    FSP_FSCTL_TRANSACT_FULL_CONTEXT *FullContext = FileContext;
    UINT32 Index = (UINT32)FullContext->UserContext;

    FillFileInfo(Index, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    NOTIFYFS *Notifyfs = (NOTIFYFS *)FileSystem->UserContext;
    UINT32 Count = FileCount(Notifyfs);
    UINT32 Index;

    Index = 0 == Marker ? 1 : wcstoul(Marker, 0, 10) + 1;
    for (; Count >= Index; Index++)
        if (!AddDirInfo(0, Index, Buffer, Length, PBytesTransferred))
            break;
    FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

    return STATUS_SUCCESS;
}

static FSP_FILE_SYSTEM_INTERFACE NotifyfsInterface =
{
    .GetVolumeInfo = GetVolumeInfo,
    .GetSecurityByName = GetSecurityByName,
    .Create = Create,
    .Open = Open,
    .Overwrite = Overwrite,
    .Read = Read,
    .GetFileInfo = GetFileInfo,
    .ReadDirectory = ReadDirectory,
};

static VOID CALLBACK NotifyfsTick(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
{
    NOTIFYFS *Notifyfs = Context;
    UINT32 Ticks = InterlockedIncrement(&Notifyfs->Ticks);
    UINT32 OldCount = CountFromTicks(Ticks - 1);
    UINT32 NewCount = CountFromTicks(Ticks);
    union
    {
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_NOTIFY_INFO, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_NOTIFY_INFO V;
    } NotifyInfoBuf;
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo = &NotifyInfoBuf.V;

    memset(NotifyInfo, 0, sizeof NotifyInfo);
    if (OldCount < NewCount)
    {
        wsprintfW(NotifyInfo->FileNameBuf, L"\\%u", (unsigned)NewCount);
        NotifyInfo->Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) +
            wcslen(NotifyInfo->FileNameBuf) * sizeof(WCHAR));
        NotifyInfo->Action = FILE_ACTION_ADDED;
        NotifyInfo->Filter = FILE_NOTIFY_CHANGE_FILE_NAME;
        FspDebugLog("CREATE \\%u\n", (unsigned)NewCount);
    }
    else if (OldCount > NewCount)
    {
        wsprintfW(NotifyInfo->FileNameBuf, L"\\%u", (unsigned)OldCount);
        NotifyInfo->Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) +
            wcslen(NotifyInfo->FileNameBuf) * sizeof(WCHAR));
        NotifyInfo->Action = FILE_ACTION_REMOVED;
        NotifyInfo->Filter = FILE_NOTIFY_CHANGE_FILE_NAME;
        FspDebugLog("REMOVE \\%u\n", (unsigned)OldCount);
    }

    if (OldCount != NewCount)
    {
        if (STATUS_SUCCESS == FspFileSystemNotifyBegin(Notifyfs->FileSystem, 500))
        {
            FspFileSystemNotify(Notifyfs->FileSystem, NotifyInfo, NotifyInfo->Size);
            FspFileSystemNotifyEnd(Notifyfs->FileSystem);
        }
    }
}

static VOID NotifyfsDelete(NOTIFYFS *Notifyfs);

static NTSTATUS NotifyfsCreate(PWSTR VolumePrefix, PWSTR MountPoint, NOTIFYFS **PNotifyfs)
{
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    NOTIFYFS *Notifyfs = 0;
    INT64 TimerDue;
    NTSTATUS Result;

    *PNotifyfs = 0;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)", SDDL_REVISION_1,
        &DefaultSecurity, &DefaultSecuritySize))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Notifyfs = malloc(sizeof *Notifyfs);
    if (0 == Notifyfs)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memset(Notifyfs, 0, sizeof *Notifyfs);

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.SectorSize = ALLOCATION_UNIT;
    VolumeParams.SectorsPerAllocationUnit = 1;
    VolumeParams.VolumeCreationTime = 0;
    VolumeParams.VolumeSerialNumber = 0;
    VolumeParams.FileInfoTimeout = 1000;
    VolumeParams.CaseSensitiveSearch = 0;
    VolumeParams.CasePreservedNames = 1;
    VolumeParams.UnicodeOnDisk = 1;
    VolumeParams.PersistentAcls = 0;
    VolumeParams.PostCleanupWhenModifiedOnly = 1;
    VolumeParams.UmFileContextIsFullContext = 1;
    if (0 != VolumePrefix)
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), VolumePrefix);
    wcscpy_s(VolumeParams.FileSystemName, sizeof VolumeParams.FileSystemName / sizeof(WCHAR),
        L"" PROGNAME);

    Result = FspFileSystemCreate(
        VolumeParams.Prefix[0] ? L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME,
        &VolumeParams,
        &NotifyfsInterface,
        &Notifyfs->FileSystem);
    if (!NT_SUCCESS(Result))
        goto exit;
    Notifyfs->FileSystem->UserContext = Notifyfs;

    FspFileSystemSetDebugLog(Notifyfs->FileSystem, DEBUGFLAGS);

    Result = FspFileSystemSetMountPoint(Notifyfs->FileSystem, MountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    Notifyfs->Timer = CreateThreadpoolTimer(NotifyfsTick, Notifyfs, 0);
    if (0 == Notifyfs->Timer)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    TimerDue = -1000;
    SetThreadpoolTimer(Notifyfs->Timer, (PVOID)&TimerDue, 1000, 0);

    Result = STATUS_SUCCESS;

exit:
    if (NT_SUCCESS(Result))
        *PNotifyfs = Notifyfs;
    else if (0 != Notifyfs)
        NotifyfsDelete(Notifyfs);

    return Result;
}

static VOID NotifyfsDelete(NOTIFYFS *Notifyfs)
{
    if (0 != Notifyfs->Timer)
    {
        SetThreadpoolTimer(Notifyfs->Timer, 0, 0, 0);
        WaitForThreadpoolTimerCallbacks(Notifyfs->Timer, TRUE);
        CloseThreadpoolTimer(Notifyfs->Timer);
    }

    if (0 != Notifyfs->FileSystem)
        FspFileSystemDelete(Notifyfs->FileSystem);

    free(Notifyfs);
}

static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
#define argtos(v)                       if (arge > ++argp) v = *argp; else goto usage

    wchar_t **argp, **arge;
    PWSTR VolumePrefix = 0;
    PWSTR MountPoint = 0;
    NOTIFYFS *Notifyfs = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'm':
            argtos(MountPoint);
            break;
        case L'u':
            argtos(VolumePrefix);
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (0 == MountPoint)
        goto usage;

    FspDebugLogSetHandle(GetStdHandle(STD_ERROR_HANDLE));

    Result = NotifyfsCreate(VolumePrefix, MountPoint, &Notifyfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create file system");
        goto exit;
    }

    Result = FspFileSystemStartDispatcher(Notifyfs->FileSystem, 0);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot start file system");
        goto exit;
    }

    MountPoint = FspFileSystemMountPoint(Notifyfs->FileSystem);

    info(L"%s%s%s -m %s",
        L"" PROGNAME,
        0 != VolumePrefix && L'\0' != VolumePrefix[0] ? L" -u " : L"",
            0 != VolumePrefix && L'\0' != VolumePrefix[0] ? VolumePrefix : L"",
        MountPoint);

    Service->UserContext = Notifyfs;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != Notifyfs)
        NotifyfsDelete(Notifyfs);

    return Result;

usage:;
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -m MountPoint       [X:|*|directory]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;

#undef argtos
}

static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    NOTIFYFS *Notifyfs = Service->UserContext;

    FspFileSystemStopDispatcher(Notifyfs->FileSystem);
    NotifyfsDelete(Notifyfs);

    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    if (!NT_SUCCESS(FspLoad(0)))
        return ERROR_DELAY_LOAD_FAILED;

    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}
