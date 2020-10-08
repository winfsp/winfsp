/**
 * @file notify-test.c
 *
 * @copyright 2015-2020 Bill Zissimopoulos
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
#include <tlib/testsuite.h>
#include <process.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

static
void notify_abandon_dotest(ULONG Flags)
{
    void *memfs = memfs_start(Flags);
    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);
    NTSTATUS Result;

    Result = FspFsctlNotify(FileSystem->VolumeHandle, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFsctlNotify(FileSystem->VolumeHandle, 0, 0);
    ASSERT(STATUS_CANT_WAIT == Result);

    memfs_stop(memfs);
}

static
void notify_abandon_test(void)
{
    if (WinFspDiskTests)
        notify_abandon_dotest(MemfsDisk);
    if (WinFspNetTests)
        notify_abandon_dotest(MemfsNet);
}

static
unsigned __stdcall notify_abandon_rename_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    WCHAR NewFilePath[MAX_PATH];
    HANDLE Handle;
    BOOL Success;

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);

    StringCbPrintfW(NewFilePath, sizeof NewFilePath, L"%s.new", FilePath);
    Success = MoveFileExW(FilePath, NewFilePath, 0);

    return Success ? 0 : GetLastError();
}

static
void notify_abandon_rename_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);
    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);
    WCHAR FilePath[MAX_PATH];
    HANDLE Thread;
    DWORD ExitCode;
    NTSTATUS Result;

    Result = FspFsctlNotify(FileSystem->VolumeHandle, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Thread = (HANDLE)_beginthreadex(0, 0, notify_abandon_rename_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Sleep(1000);

    memfs_stop(memfs);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);
    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);
}

static
void notify_abandon_rename_test(void)
{
    if (WinFspDiskTests)
        notify_abandon_rename_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        notify_abandon_rename_dotest(MemfsNet, L"\\\\memfs\\share");
}

void notify_tests(void)
{
    if (!OptExternal)
    {
        TEST(notify_abandon_test);
        TEST(notify_abandon_rename_test);
    }
}
