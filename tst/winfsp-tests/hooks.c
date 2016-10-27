/**
 * @file hooks.c
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

#define WINFSP_TESTS_NO_HOOKS
#include "winfsp-tests.h"

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
            ABORT("unknown filename format");

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
            ABORT("unknown filename format");

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
            ABORT("unknown filename format");

        L1 = wcslen(P) + 1;
        L2 = wcslen(OptShareComputer);
        L3 = wcslen(OptShareName);
        memmove(FileNameBuf + FILENAMEBUF_SIZE - L1, P, L1 * sizeof(WCHAR));
        memmove(FileNameBuf, OptShareComputer, L2 * sizeof(WCHAR));
        memmove(FileNameBuf + L2, OptShareName, L3 * sizeof(WCHAR));
        memmove(FileNameBuf + L2 + L3, FileNameBuf + FILENAMEBUF_SIZE - L1, L1 * sizeof(WCHAR));
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

    MaybeAdjustTraversePrivilege(FALSE);
    Success = SetCurrentDirectoryW(FileNameBuf);
    MaybeAdjustTraversePrivilege(TRUE);
    return Success;
}
