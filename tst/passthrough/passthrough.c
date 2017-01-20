/**
 * @file passthrough.c
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

#include <winfsp/winfsp.h>

#define PROGNAME                        "passthrough"

#define info(format, ...)               FspServiceLog(EVENTLOG_INFORMATION_TYPE, format, __VA_ARGS__)
#define warn(format, ...)               FspServiceLog(EVENTLOG_WARNING_TYPE, format, __VA_ARGS__)
#define fail(format, ...)               FspServiceLog(EVENTLOG_ERROR_TYPE, format, __VA_ARGS__)

typedef struct
{
    FSP_FILE_SYSTEM *FileSystem;
    PWSTR Path;
} PTFS;

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS SetVolumeLabel_(FSP_FILE_SYSTEM *FileSystem,
    PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    return STATUS_INVALID_DEVICE_REQUEST;
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
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static VOID Cleanup(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR FileName, ULONG Flags)
{
}

static VOID Close(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext)
{
}

static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS Write(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS Flush(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS CanDelete(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR FileName)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS SetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    PWSTR Pattern,
    PULONG PBytesTransferred)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static FSP_FILE_SYSTEM_INTERFACE PtfsInterface =
{
    GetVolumeInfo,
    SetVolumeLabel_,
    GetSecurityByName,
    Create,
    Open,
    Overwrite,
    Cleanup,
    Close,
    Read,
    Write,
    Flush,
    GetFileInfo,
    SetBasicInfo,
    SetFileSize,
    CanDelete,
    Rename,
    GetSecurity,
    SetSecurity,
    ReadDirectory,
};

static VOID PtfsDelete(PTFS *Ptfs);

static NTSTATUS PtfsCreate(PWSTR Path, PWSTR VolumePrefix, PWSTR MountPoint, UINT32 DebugFlags,
    PTFS **PPtfs)
{
    WCHAR FullPath[MAX_PATH];
    ULONG Length;
    HANDLE Handle;
    FILETIME CreationTime;
    DWORD LastError;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    PTFS *Ptfs = 0;
    NTSTATUS Result;

    *PPtfs = 0;

    if (!GetFullPathNameW(Path, MAX_PATH, FullPath, 0))
        return FspNtStatusFromWin32(GetLastError());

    Handle = CreateFileW(
        FullPath, FILE_READ_ATTRIBUTES, 0, 0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return FspNtStatusFromWin32(GetLastError());
    LastError = GetFileTime(Handle, &CreationTime, 0, 0) ? GetLastError() : 0;
    CloseHandle(Handle);
    if (0 != LastError)
        return FspNtStatusFromWin32(LastError);

    /* from now on we must goto exit on failure */

    Ptfs = malloc(sizeof *Ptfs);
    if (0 == Ptfs)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memset(Ptfs, 0, sizeof *Ptfs);

    Length = (ULONG)(wcslen(FullPath) + 1) * sizeof(WCHAR);
    Ptfs->Path = malloc(Length);
    if (0 == Ptfs->Path)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memcpy(Ptfs->Path, FullPath, Length);

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.SectorSize = 4096;
    VolumeParams.SectorsPerAllocationUnit = 1;
    VolumeParams.VolumeCreationTime = ((PLARGE_INTEGER)&CreationTime)->QuadPart;
    VolumeParams.VolumeSerialNumber = 0;
    VolumeParams.FileInfoTimeout = 1000;
    VolumeParams.CaseSensitiveSearch = 0;
    VolumeParams.CasePreservedNames = 1;
    VolumeParams.UnicodeOnDisk = 1;
    VolumeParams.PersistentAcls = 1;
    VolumeParams.PostCleanupWhenModifiedOnly = 1;
    VolumeParams.UmFileContextIsUserContext2 = 1;
    if (0 != VolumePrefix)
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), VolumePrefix);
    wcscpy_s(VolumeParams.FileSystemName, sizeof VolumeParams.FileSystemName / sizeof(WCHAR),
        L"" PROGNAME);

    Result = FspFileSystemCreate(
        VolumeParams.Prefix[0] ? L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME,
        &VolumeParams,
        &PtfsInterface,
        &Ptfs->FileSystem);
    if (!NT_SUCCESS(Result))
        goto exit;
    Ptfs->FileSystem->UserContext = Ptfs;

    Result = FspFileSystemSetMountPoint(Ptfs->FileSystem, MountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    FspFileSystemSetDebugLog(Ptfs->FileSystem, DebugFlags);

    Result = STATUS_SUCCESS;

exit:
    if (NT_SUCCESS(Result))
        *PPtfs = Ptfs;
    else if (0 != Ptfs)
        PtfsDelete(Ptfs);

    return Result;
}

static VOID PtfsDelete(PTFS *Ptfs)
{
    if (0 != Ptfs->FileSystem)
        FspFileSystemDelete(Ptfs->FileSystem);

    if (0 != Ptfs->Path)
        free(Ptfs->Path);

    free(Ptfs);
}

static ULONG wcstol_deflt(wchar_t *w, ULONG deflt)
{
    wchar_t *endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
#define argtos(v)                       if (arge > ++argp) v = *argp; else goto usage
#define argtol(v)                       if (arge > ++argp) v = wcstol_deflt(*argp, v); else goto usage

    wchar_t **argp, **arge;
    PWSTR DebugLogFile = 0;
    ULONG DebugFlags = 0;
    PWSTR VolumePrefix = 0;
    PWSTR PassThrough = 0;
    PWSTR MountPoint = 0;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
    PTFS *Ptfs = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'd':
            argtol(DebugFlags);
            break;
        case L'D':
            argtos(DebugLogFile);
            break;
        case L'm':
            argtos(MountPoint);
            break;
        case L'p':
            argtos(PassThrough);
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

    if (0 == PassThrough || 0 == MountPoint)
        goto usage;

    if (0 != DebugLogFile)
    {
        if (0 == wcscmp(L"-", DebugLogFile))
            DebugLogHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        else
            DebugLogHandle = CreateFileW(
                DebugLogFile,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == DebugLogHandle)
        {
            fail(L"cannot open debug log file");
            goto usage;
        }

        FspDebugLogSetHandle(DebugLogHandle);
    }

    Result = PtfsCreate(PassThrough, VolumePrefix, MountPoint, DebugFlags, &Ptfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create file system");
        goto exit;
    }

    Result = FspFileSystemStartDispatcher(Ptfs->FileSystem, 0);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot start file system");
        goto exit;
    }

    MountPoint = FspFileSystemMountPoint(Ptfs->FileSystem);

    info(L"%s%s%s -p %s -m %s",
        L"" PROGNAME,
        0 != VolumePrefix && L'\0' != VolumePrefix[0] ? L" -u " : L"",
            0 != VolumePrefix && L'\0' != VolumePrefix[0] ? VolumePrefix : L"",
        PassThrough,
        MountPoint);

    Service->UserContext = Ptfs;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != Ptfs)
        PtfsDelete(Ptfs);

    return Result;

usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -d DebugFlags       [-1: enable all debug logs]\n"
        "    -D DebugLogFile     [file path; use - for stdout]\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -p Directory        [directory to expose as pass through file system]\n"
        "    -m MountPoint       [X:|*|directory]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;

#undef argtos
#undef argtol
}

static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    PTFS *Ptfs = Service->UserContext;

    FspFileSystemStopDispatcher(Ptfs->FileSystem);
    PtfsDelete(Ptfs);

    return STATUS_SUCCESS;
}

static NTSTATUS WinFspLoad(VOID)
{
#if defined(_WIN64)
#define FSP_DLLNAME                     "winfsp-x64.dll"
#else
#define FSP_DLLNAME                     "winfsp-x86.dll"
#endif
#define FSP_DLLPATH                     "bin\\" FSP_DLLNAME

    LONG WINAPI __HrLoadAllImportsForDll(CONST CHAR *);
    WCHAR PathBuf[MAX_PATH - (sizeof L"" FSP_DLLPATH / sizeof(WCHAR) - 1)];
    DWORD Size;
    LONG Result;
    HMODULE Module;

    Size = sizeof PathBuf;
    Result = RegGetValueW(
        HKEY_LOCAL_MACHINE, L"Software\\WinFsp", L"InstallDir",
        RRF_RT_REG_SZ | 0x00020000/*RRF_SUBKEY_WOW6432KEY*/,
        0, PathBuf, &Size);
    if (ERROR_SUCCESS != Result)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    RtlCopyMemory(PathBuf + (Size / sizeof(WCHAR) - 1), L"" FSP_DLLPATH, sizeof L"" FSP_DLLPATH);
    Module = LoadLibraryW(PathBuf);
    if (0 == Module)
        return STATUS_DLL_NOT_FOUND;

    Result = __HrLoadAllImportsForDll(FSP_DLLNAME);
    if (0 > Result)
        return STATUS_DELAY_LOAD_FAILED;

    return STATUS_SUCCESS;

#undef FSP_DLLNAME
#undef FSP_DLLPATH
}

int wmain(int argc, wchar_t **argv)
{
    if (!NT_SUCCESS(WinFspLoad()))
        return ERROR_DELAY_LOAD_FAILED;

    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}
