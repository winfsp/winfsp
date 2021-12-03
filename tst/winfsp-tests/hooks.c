/**
 * @file hooks.c
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

#define WINFSP_TESTS_NO_HOOKS
#include "winfsp-tests.h"
#include <winfsp/winfsp.h>

#define FILENAMEBUF_SIZE                1024

static VOID PrepareFileName(PCWSTR FileName, PWSTR FileNameBuf)
{
    static WCHAR DevicePrefix[] =
        L"\\\\?\\GLOBALROOT\\Device\\Volume{01234567-0123-0123-0101-010101010101}";
    static WCHAR MemfsSharePrefix[] =
        L"\\\\memfs\\share";
    static const int TogglePercent = 25;
    PWSTR P, EndP;
    size_t L1, L2, L3;

    wcscpy_s(FileNameBuf, FILENAMEBUF_SIZE, FileName);

    if (OptCaseRandomize)
    {
        if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1] &&
            L'?' == FileNameBuf[2] && L'\\' == FileNameBuf[3] &&
            testalpha(FileNameBuf[4]) && L':' == FileNameBuf[5] && L'\\' == FileNameBuf[6])
            P = FileNameBuf + 6;
        else if (0 == wcsncmp(FileNameBuf, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
            P = FileNameBuf + wcslen(DevicePrefix);
        else if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1])
            P = FileNameBuf + 2;
        else if (testalpha(FileNameBuf[0]) && L':' == FileNameBuf[1] && L'\\' == FileNameBuf[2])
            P = FileNameBuf + 2;
        else
            P = FileNameBuf;

        for (EndP = P + wcslen(P); EndP > P; P++)
            if (testalpha(*P) && myrand() <= (TogglePercent) * 0x7fff / 100)
                *P = togglealpha(*P);
    }

    if (OptMountPoint && memfs_running)
    {
        if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1] &&
            L'?' == FileNameBuf[2] && L'\\' == FileNameBuf[3] &&
            testalpha(FileNameBuf[4]) && L':' == FileNameBuf[5] && L'\\' == FileNameBuf[6])
            ABORT("--mountpoint not supported with NTFS");
        else if (0 == wcsncmp(FileNameBuf, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
            P = FileNameBuf + wcslen(DevicePrefix);
        else if (0 == mywcscmp(
            FileNameBuf, (int)wcslen(MemfsSharePrefix), MemfsSharePrefix, (int)wcslen(MemfsSharePrefix)))
            P = FileNameBuf + wcslen(MemfsSharePrefix);
        else if (testalpha(FileNameBuf[0]) && L':' == FileNameBuf[1] && L'\\' == FileNameBuf[2])
            ABORT("--mountpoint not supported with NTFS");
        else
            P = FileNameBuf;

        L1 = wcslen(P) + 1;
        L2 = wcslen(OptMountPoint);
        memmove(FileNameBuf + FILENAMEBUF_SIZE - L1, P, L1 * sizeof(WCHAR));
        memmove(FileNameBuf, OptMountPoint, L2 * sizeof(WCHAR));
        memmove(FileNameBuf + L2, FileNameBuf + FILENAMEBUF_SIZE - L1, L1 * sizeof(WCHAR));
    }

    if (OptShareName && memfs_running)
    {
        if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1] &&
            L'?' == FileNameBuf[2] && L'\\' == FileNameBuf[3] &&
            testalpha(FileNameBuf[4]) && L':' == FileNameBuf[5] && L'\\' == FileNameBuf[6])
            /* NTFS testing can only been done when the whole drive is being shared */
            P = FileNameBuf + 6;
        else if (0 == wcsncmp(FileNameBuf, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
            P = FileNameBuf + wcslen(DevicePrefix);
        else if (0 == mywcscmp(
            FileNameBuf, (int)wcslen(MemfsSharePrefix), MemfsSharePrefix, (int)wcslen(MemfsSharePrefix)))
            P = FileNameBuf + wcslen(MemfsSharePrefix);
        else if (testalpha(FileNameBuf[0]) && L':' == FileNameBuf[1] && L'\\' == FileNameBuf[2])
            /* NTFS testing can only been done when the whole drive is being shared */
            P = FileNameBuf + 2;
        else
            P = FileNameBuf;

        L1 = wcslen(P) + 1;
        L2 = wcslen(OptShareComputer);
        L3 = wcslen(OptShareName);
        memmove(FileNameBuf + FILENAMEBUF_SIZE - L1, P, L1 * sizeof(WCHAR));
        memmove(FileNameBuf, OptShareComputer, L2 * sizeof(WCHAR));
        memmove(FileNameBuf + L2, OptShareName, L3 * sizeof(WCHAR));
        memmove(FileNameBuf + L2 + L3, FileNameBuf + FILENAMEBUF_SIZE - L1, L1 * sizeof(WCHAR));
    }
}

typedef struct
{
    HANDLE VolumeHandle;
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo;
} MAYBE_NOTIFY_DATA;

static DWORD WINAPI MaybeNotifyRoutine(PVOID Context)
{
    MAYBE_NOTIFY_DATA *NotifyData = Context;

    /*
     * The supplied VolumeHandle may be invalid or refer to the wrong object.
     * This is ok because:
     *
     * - If the VolumeHandle is invalid, Windows will catch it and will fail the operation.
     * - If the VolumeHandle refers to the wrong object, the FspFsctlNotify "should" fail
     * because of an unknown DeviceIoControl code.
     * - If the VolumeHandle refers to the wrong file system, it is still ok if we send an
     * extraneous notify.
     */

    FspFsctlNotify(NotifyData->VolumeHandle,
        NotifyData->NotifyInfo, NotifyData->NotifyInfo->Size);

    HeapFree(GetProcessHeap(), 0, NotifyData->NotifyInfo);
    HeapFree(GetProcessHeap(), 0, NotifyData);

    return 0;
}

static VOID MaybeNotify(PWSTR FileName, ULONG Filter, ULONG Action)
{
    static WCHAR DevicePrefix[] =
        L"\\\\?\\GLOBALROOT\\Device\\Volume{01234567-0123-0123-0101-010101010101}";
    static WCHAR MemfsSharePrefix[] =
        L"\\\\memfs\\share";
    MAYBE_NOTIFY_DATA *NotifyData;
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo;
    size_t L;

    if (!OptNotify || OptExternal || 0 == memfs_handle)
        return;

    if (0 == wcsncmp(FileName, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
        FileName += wcslen(DevicePrefix);
    else if (0 == mywcscmp(
        FileName, (int)wcslen(MemfsSharePrefix), MemfsSharePrefix, (int)wcslen(MemfsSharePrefix)))
        FileName += wcslen(MemfsSharePrefix);
    else
        return;

    L = wcslen(FileName);
    NotifyData = HeapAlloc(GetProcessHeap(), 0, sizeof *NotifyData);
    NotifyInfo = HeapAlloc(GetProcessHeap(), 0, sizeof *NotifyInfo + L * sizeof(WCHAR));
    if (0 == NotifyData || 0 == NotifyInfo)
        ABORT("cannot malloc notify data");

    NotifyInfo->Size = (UINT16)(sizeof *NotifyInfo + L * sizeof(WCHAR));
    NotifyInfo->Filter = Filter;
    NotifyInfo->Action = Action;
    memcpy(NotifyInfo->FileNameBuf, FileName, L * sizeof(WCHAR));

    NotifyData->VolumeHandle = memfs_handle;
    NotifyData->NotifyInfo = NotifyInfo;

    if (!QueueUserWorkItem(MaybeNotifyRoutine, NotifyData, 0))
        ABORT("cannot queue notify data");
}

typedef struct
{
    HANDLE File;
    HANDLE Wait;
    OVERLAPPED Overlapped;
} OPLOCK_BREAK_WAIT_DATA;

static VOID CALLBACK OplockBreakWait(PVOID Context, BOOLEAN Timeout)
{
    OPLOCK_BREAK_WAIT_DATA *Data = Context;

    UnregisterWaitEx(Data->Wait, 0);
    CloseHandle(Data->File);
    HeapFree(GetProcessHeap(), 0, Data);
}

static VOID MaybeRequestOplock(PCWSTR FileName)
{
    HANDLE File;
    OPLOCK_BREAK_WAIT_DATA *Data;
    DWORD FsControlCode;
    BOOL Success;
    DWORD BytesTransferred;

    if (0 == OptOplock)
        return;

    File = CreateFileW(
        FileName,
        FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        0);
    if (INVALID_HANDLE_VALUE == File)
        return;

    Data = HeapAlloc(GetProcessHeap(), 0, sizeof *Data);
    if (0 == Data)
        ABORT("cannot malloc filter oplock data");
    memset(Data, 0, sizeof *Data);

    switch (OptOplock)
    {
    case 'B':
        FsControlCode = FSCTL_REQUEST_BATCH_OPLOCK;
        break;
    case 'F':
        FsControlCode = FSCTL_REQUEST_FILTER_OPLOCK;
        break;
    default:
        FsControlCode = FSCTL_REQUEST_FILTER_OPLOCK;
        break;
    }

    Success = DeviceIoControl(File, FsControlCode, 0, 0, 0, 0, &BytesTransferred,
        &Data->Overlapped);
    if (!Success && ERROR_IO_PENDING == GetLastError())
    {
        Data->File = File;
        if (!RegisterWaitForSingleObject(&Data->Wait, File,
            OplockBreakWait, Data, INFINITE, WT_EXECUTEONLYONCE))
            ABORT("cannot register wait for filter oplock");
    }
    else
    {
        CloseHandle(File);
        HeapFree(GetProcessHeap(), 0, Data);
    }
}

static VOID MaybeAdjustTraversePrivilege(BOOL Enable)
{
    if (OptNoTraverseToken)
    {
        TOKEN_PRIVILEGES Privileges;
        DWORD LastError = GetLastError();

        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = Enable ? SE_PRIVILEGE_ENABLED : 0;
        Privileges.Privileges[0].Luid = OptNoTraverseLuid;
        if (!AdjustTokenPrivileges(OptNoTraverseToken, FALSE, &Privileges, 0, 0, 0))
            ABORT("cannot adjust traverse privilege");

        SetLastError(LastError);
    }
}

HANDLE WINAPI HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    HANDLE Handle;

    PrepareFileName(lpFileName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeRequestOplock(FileNameBuf);

    MaybeAdjustTraversePrivilege(FALSE);
    Handle = (OptResilient ? ResilientCreateFileW : CreateFileW)(
        FileNameBuf,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);
    MaybeAdjustTraversePrivilege(TRUE);
    return Handle;
}

BOOL WINAPI HookCloseHandle(
    HANDLE hObject)
{
    return (OptResilient ? ResilientCloseHandle : CloseHandle)(
        hObject);
}

BOOL WINAPI HookSetFileAttributesW(
    LPCWSTR lpFileName,
    DWORD dwFileAttributes)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpFileName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = SetFileAttributesW(FileNameBuf, dwFileAttributes);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookCreateDirectoryW(
    LPCWSTR lpPathName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpPathName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = CreateDirectoryW(FileNameBuf, lpSecurityAttributes);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookDeleteFileW(
    LPCWSTR lpFileName)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpFileName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeRequestOplock(FileNameBuf);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = (OptResilient ? ResilientDeleteFileW : DeleteFileW)(
        FileNameBuf);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookRemoveDirectoryW(
    LPCWSTR lpPathName)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpPathName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = (OptResilient ? ResilientRemoveDirectoryW : RemoveDirectoryW)(
        FileNameBuf);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookMoveFileExW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    DWORD dwFlags)
{
    WCHAR OldFileNameBuf[FILENAMEBUF_SIZE];
    WCHAR NewFileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpExistingFileName, OldFileNameBuf);
    PrepareFileName(lpNewFileName, NewFileNameBuf);

    MaybeNotify(OldFileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);
    if (OptCaseInsensitive ?
        _wcsicmp(OldFileNameBuf, NewFileNameBuf) : wcscmp(OldFileNameBuf, NewFileNameBuf))
        MaybeNotify(NewFileNameBuf,
            FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeRequestOplock(OldFileNameBuf);
    if (OptCaseInsensitive ?
        _wcsicmp(OldFileNameBuf, NewFileNameBuf) : wcscmp(OldFileNameBuf, NewFileNameBuf))
        MaybeRequestOplock(NewFileNameBuf);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = MoveFileExW(OldFileNameBuf, NewFileNameBuf, dwFlags);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

HANDLE WINAPI HookFindFirstFileW(
    LPCWSTR lpFileName,
    LPWIN32_FIND_DATAW lpFindFileData)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    HANDLE Handle;

    PrepareFileName(lpFileName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeAdjustTraversePrivilege(FALSE);
    Handle = FindFirstFileW(FileNameBuf, lpFindFileData);
    MaybeAdjustTraversePrivilege(TRUE);
    return Handle;
}

HANDLE WINAPI HookFindFirstStreamW(
    LPCWSTR lpFileName,
    STREAM_INFO_LEVELS InfoLevel,
    LPVOID lpFindStreamData,
    DWORD dwFlags)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    HANDLE Handle;

    PrepareFileName(lpFileName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeAdjustTraversePrivilege(FALSE);
    Handle = FindFirstStreamW(FileNameBuf, InfoLevel, lpFindStreamData, dwFlags);
    MaybeAdjustTraversePrivilege(TRUE);
    return Handle;
}

BOOL WINAPI HookGetDiskFreeSpaceW(
    LPCWSTR lpRootPathName,
    LPDWORD lpSectorsPerCluster,
    LPDWORD lpBytesPerSector,
    LPDWORD lpNumberOfFreeClusters,
    LPDWORD lpTotalNumberOfClusters)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpRootPathName, FileNameBuf);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = GetDiskFreeSpaceW(
        FileNameBuf,
        lpSectorsPerCluster,
        lpBytesPerSector,
        lpNumberOfFreeClusters,
        lpTotalNumberOfClusters);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookGetVolumeInformationW(
    LPCWSTR lpRootPathName,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpRootPathName, FileNameBuf);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = GetVolumeInformationW(
        FileNameBuf,
        lpVolumeNameBuffer,
        nVolumeNameSize,
        lpVolumeSerialNumber,
        lpMaximumComponentLength,
        lpFileSystemFlags,
        lpFileSystemNameBuffer,
        nFileSystemNameSize);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookSetVolumeLabelW(
    LPCWSTR lpRootPathName,
    LPCWSTR lpVolumeName)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpRootPathName, FileNameBuf);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = SetVolumeLabelW(
        FileNameBuf,
        lpVolumeName);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookSetCurrentDirectoryW(
    LPCWSTR lpPathName)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpPathName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = SetCurrentDirectoryW(FileNameBuf);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}

BOOL WINAPI HookCreateProcessW(
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    BOOL Success;

    PrepareFileName(lpApplicationName, FileNameBuf);

    MaybeNotify(FileNameBuf,
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, FILE_ACTION_MODIFIED);

    MaybeAdjustTraversePrivilege(FALSE);
    Success = CreateProcessW(FileNameBuf,
        lpCommandLine,  /* we should probably change this as well */
        lpProcessAttributes,
        lpThreadAttributes,
        bInheritHandles,
        dwCreationFlags,
        lpEnvironment,
        lpCurrentDirectory,
        lpStartupInfo,
        lpProcessInformation);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}
