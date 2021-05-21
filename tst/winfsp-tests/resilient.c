/**
 * @file resilient.c
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

#include <windows.h>

#define DeleteMaxTries                  30
#define DeleteSleepTimeout              300

static VOID WaitDeletePending(PCWSTR FileName);

HANDLE WINAPI ResilientCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    HANDLE Handle;
    DWORD LastError;

    Handle = CreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);
    LastError = GetLastError();

    if (INVALID_HANDLE_VALUE != Handle &&
        (FILE_FLAG_DELETE_ON_CLOSE & dwFlagsAndAttributes))
    {
        /* HACK: remember FILE_FLAG_DELETE_ON_CLOSE through HANDLE_FLAG_PROTECT_FROM_CLOSE */
        SetHandleInformation(Handle,
            HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
    }

    SetLastError(LastError);
    return Handle;
}

BOOL WINAPI ResilientCloseHandle(
    HANDLE hObject)
{
    BOOL Success;
    DWORD LastError;
    DWORD HandleFlags = 0, FileNameLen;
    WCHAR FileNameBuf[sizeof "\\\\?\\GLOBALROOT" - 1 + 1024] = L"\\\\?\\GLOBALROOT";

    if (GetHandleInformation(hObject, &HandleFlags) &&
        (HANDLE_FLAG_PROTECT_FROM_CLOSE & HandleFlags))
    {
        SetHandleInformation(hObject,
            HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
        FileNameLen = GetFinalPathNameByHandle(hObject,
            FileNameBuf + sizeof "\\\\?\\GLOBALROOT" - 1, 1023,
            FILE_NAME_OPENED | VOLUME_NAME_NT);
        if (0 == FileNameLen || FileNameLen >= 1024)
            HandleFlags = 0;
    }

    Success = CloseHandle(
        hObject);
    LastError = GetLastError();

    if (Success)
    {
        if (HANDLE_FLAG_PROTECT_FROM_CLOSE & HandleFlags)
            WaitDeletePending(FileNameBuf);
    }

    SetLastError(LastError);
    return Success;
}

BOOL WINAPI ResilientDeleteFileW(
    LPCWSTR lpFileName)
{
    BOOL Success;
    DWORD LastError;

    Success = DeleteFileW(lpFileName);
    LastError = GetLastError();

    if (Success)
        WaitDeletePending(lpFileName);
    else
    {
        for (ULONG MaxTries = DeleteMaxTries;
            !Success && ERROR_SHARING_VIOLATION == GetLastError() && 0 != MaxTries;
            MaxTries--)
        {
            Sleep(DeleteSleepTimeout);
            Success = DeleteFileW(lpFileName);
        }
    }

    SetLastError(LastError);
    return Success;
}

BOOL WINAPI ResilientRemoveDirectoryW(
    LPCWSTR lpPathName)
{
    BOOL Success;
    DWORD LastError;

    Success = RemoveDirectoryW(lpPathName);
    LastError = GetLastError();

    if (Success)
        WaitDeletePending(lpPathName);
    else
    {
        for (ULONG MaxTries = DeleteMaxTries;
            !Success &&
            (ERROR_SHARING_VIOLATION == GetLastError() || ERROR_DIR_NOT_EMPTY == GetLastError()) &&
            0 != MaxTries;
            MaxTries--)
        {
            Sleep(DeleteSleepTimeout);
            Success = RemoveDirectoryW(lpPathName);
        }
    }

    SetLastError(LastError);
    return Success;
}

static VOID WaitDeletePending(PCWSTR FileName)
{
    for (ULONG MaxTries = DeleteMaxTries; 0 != MaxTries; MaxTries--)
    {
        HANDLE Handle = CreateFileW(FileName,
            FILE_READ_ATTRIBUTES, 0, 0, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
        if (INVALID_HANDLE_VALUE != Handle)
            /* should never happen! */
            CloseHandle(Handle);
        else if (ERROR_ACCESS_DENIED == GetLastError())
            /* STATUS_DELETE_PENDING */
            Sleep(DeleteSleepTimeout);
        else
            break;
    }
}
