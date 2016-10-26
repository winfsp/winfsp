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

HANDLE HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];
    TOKEN_PRIVILEGES Privileges;

    PrepareFileName(lpFileName, FileNameBuf);

    if (OptNoTraverseToken)
    {
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = 0;
        Privileges.Privileges[0].Luid = OptNoTraverseLuid;
        if (!AdjustTokenPrivileges(OptNoTraverseToken, FALSE, &Privileges, 0, 0, 0))
            ABORT("cannot disable traverse privilege");
    }

    HANDLE h;
    if (!OptResilient)
        h = CreateFileW(
            FileNameBuf,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);
    else
        h = ResilientCreateFileW(
            FileNameBuf,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);
    DWORD LastError = GetLastError();

    if (OptNoTraverseToken)
    {
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        Privileges.Privileges[0].Luid = OptNoTraverseLuid;
        if (!AdjustTokenPrivileges(OptNoTraverseToken, FALSE, &Privileges, 0, 0, 0))
            ABORT("cannot enable traverse privilege");
    }

#if 0
    FspDebugLog("CreateFileW(\"%S\", %#lx, %#lx, %p, %#lx, %#lx, %p) = %p[%#lx]\n",
        FileNameBuf,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile,
        h, INVALID_HANDLE_VALUE != h ? 0 : LastError);
#endif

    SetLastError(LastError);
    return h;
}

BOOL HookCloseHandle(
    HANDLE hObject)
{
    if (!OptResilient)
        return CloseHandle(hObject);
    else
        return ResilientCloseHandle(hObject);
}

BOOL HookCreateDirectoryW(
    LPCWSTR lpPathName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];

    PrepareFileName(lpPathName, FileNameBuf);

    return CreateDirectoryW(FileNameBuf, lpSecurityAttributes);
}

BOOL HookDeleteFileW(
    LPCWSTR lpFileName)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];

    PrepareFileName(lpFileName, FileNameBuf);

    if (!OptResilient)
        return DeleteFileW(FileNameBuf);
    else
        return ResilientDeleteFileW(FileNameBuf);
}

BOOL HookRemoveDirectoryW(
    LPCWSTR lpPathName)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];

    PrepareFileName(lpPathName, FileNameBuf);

    return RemoveDirectoryW(FileNameBuf);
}

BOOL HookMoveFileExW(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    DWORD dwFlags)
{
    WCHAR OldFileNameBuf[FILENAMEBUF_SIZE];
    WCHAR NewFileNameBuf[FILENAMEBUF_SIZE];

    PrepareFileName(lpExistingFileName, OldFileNameBuf);
    PrepareFileName(lpNewFileName, NewFileNameBuf);

    return MoveFileExW(OldFileNameBuf, NewFileNameBuf, dwFlags);
}

HANDLE HookFindFirstFileW(
    LPCWSTR lpFileName,
    LPWIN32_FIND_DATAW lpFindFileData)
{
    WCHAR FileNameBuf[FILENAMEBUF_SIZE];

    PrepareFileName(lpFileName, FileNameBuf);

    return FindFirstFileW(FileNameBuf, lpFindFileData);
}
