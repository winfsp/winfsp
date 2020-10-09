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

static
void notify_timeout_dotest(ULONG Flags)
{
    void *memfs = memfs_start(Flags);
    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);
    NTSTATUS Result;

    Result = FspFsctlNotify(FileSystem->VolumeHandle, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFileSystemNotifyBegin(FileSystem, 0);
    ASSERT(STATUS_CANT_WAIT == Result);

    Result = FspFileSystemNotifyBegin(FileSystem, 9);
    ASSERT(STATUS_CANT_WAIT == Result);

    Result = FspFileSystemNotifyBegin(FileSystem, 10);
    ASSERT(STATUS_CANT_WAIT == Result);

    Result = FspFileSystemNotifyBegin(FileSystem, 11);
    ASSERT(STATUS_CANT_WAIT == Result);

    Result = FspFileSystemNotifyBegin(FileSystem, 20);
    ASSERT(STATUS_CANT_WAIT == Result);

    Result = FspFileSystemNotifyBegin(FileSystem, 1000);
    ASSERT(STATUS_CANT_WAIT == Result);

    memfs_stop(memfs);
}

static
void notify_timeout_test(void)
{
    if (WinFspDiskTests)
        notify_timeout_dotest(MemfsDisk);
    if (WinFspNetTests)
        notify_timeout_dotest(MemfsNet);
}

static
void notify_change_dotest(ULONG Flags)
{
    void *memfs = memfs_start(Flags);
    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);
    union
    {
        FSP_FSCTL_NOTIFY_INFO V;
        UINT8 B[1024];
    } Buffer;
    ULONG Length = 0;
    union
    {
        FSP_FSCTL_NOTIFY_INFO V;
        UINT8 B[sizeof(FSP_FSCTL_NOTIFY_INFO) + MAX_PATH * sizeof(WCHAR)];
    } NotifyInfo;
    PWSTR FileName;
    NTSTATUS Result;

    Result = FspFileSystemNotifyBegin(FileSystem, 0);
    ASSERT(STATUS_SUCCESS == Result);

    FileName = L"\\";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"bar";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"baz";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"\\foo";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"bar";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"baz";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"\\foo\\";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"bar";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"baz";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = 0;
    NotifyInfo.V.Action = 0;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    Result = FspFileSystemNotify(FileSystem, &Buffer.V, Length);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFileSystemNotifyEnd(FileSystem);
    ASSERT(STATUS_SUCCESS == Result);

    memfs_stop(memfs);
}

static
void notify_change_test(void)
{
    if (WinFspDiskTests)
        notify_change_dotest(MemfsDisk);
    if (WinFspNetTests)
        notify_change_dotest(MemfsNet);
}

void notify_tests(void)
{
    if (!OptExternal)
    {
        TEST(notify_abandon_test);
        TEST(notify_abandon_rename_test);
        TEST(notify_timeout_test);
        TEST(notify_change_test);
    }
}
