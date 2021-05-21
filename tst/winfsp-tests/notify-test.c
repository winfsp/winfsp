/**
 * @file notify-test.c
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
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"bar";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"baz";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"\\foo";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"bar";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"baz";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"\\foo\\";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"bar";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    FspFileSystemAddNotifyInfo(&NotifyInfo.V, &Buffer, sizeof Buffer, &Length);

    FileName = L"baz";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
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

static
void notify_open_change_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);
    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);

    HANDLE DirHandle, FileHandle;
    WCHAR FilePath[MAX_PATH];
    union
    {
        FSP_FSCTL_NOTIFY_INFO V;
        UINT8 B[sizeof(FSP_FSCTL_NOTIFY_INFO) + MAX_PATH * sizeof(WCHAR)];
    } NotifyInfo;
    PWSTR FileName;
    BOOL Success;
    NTSTATUS Result;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    FileHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != FileHandle);

    //DWORD Bytes;
    //Success = WriteFile(FileHandle, "foobar", 6, &Bytes, 0);
    //ASSERT(Success);

    Result = FspFileSystemNotifyBegin(FileSystem, 1000);
    ASSERT(STATUS_SUCCESS == Result);

    FileName = L"\\dir1";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    Result = FspFileSystemNotify(FileSystem, &NotifyInfo.V, NotifyInfo.V.Size);
    ASSERT(STATUS_SUCCESS == Result);

    FileName = L"\\dir1\\file0";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_LAST_WRITE;
    NotifyInfo.V.Action = FILE_ACTION_MODIFIED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    Result = FspFileSystemNotify(FileSystem, &NotifyInfo.V, NotifyInfo.V.Size);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFileSystemNotifyEnd(FileSystem);
    ASSERT(STATUS_SUCCESS == Result);

    CloseHandle(FileHandle);

    CloseHandle(DirHandle);

    memfs_stop(memfs);
}

static
void notify_open_change_test(void)
{
    if (WinFspDiskTests)
    {
        notify_open_change_dotest(MemfsDisk, 0, 0);
        notify_open_change_dotest(MemfsDisk, 0, 1000);
        notify_open_change_dotest(MemfsDisk, 0, INFINITE);
    }
    if (WinFspNetTests)
    {
        notify_open_change_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        notify_open_change_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
        notify_open_change_dotest(MemfsNet, L"\\\\memfs\\share", INFINITE);
    }
}

static
unsigned __stdcall notify_dirnotify_dotest_thread(void *FileSystem0)
{
    FspDebugLog(__FUNCTION__ "\n");

    FSP_FILE_SYSTEM *FileSystem = FileSystem0;
    union
    {
        FSP_FSCTL_NOTIFY_INFO V;
        UINT8 B[sizeof(FSP_FSCTL_NOTIFY_INFO) + MAX_PATH * sizeof(WCHAR)];
    } NotifyInfo;
    PWSTR FileName;
    NTSTATUS Result;

    Sleep(1000); /* wait for ReadDirectoryChangesW */

    FileName = L"\\Directory\\Subdirectory\\file0";
    NotifyInfo.V.Size = (UINT16)(sizeof(FSP_FSCTL_NOTIFY_INFO) + wcslen(FileName) * sizeof(WCHAR));
    NotifyInfo.V.Filter = FILE_NOTIFY_CHANGE_FILE_NAME;
    NotifyInfo.V.Action = FILE_ACTION_ADDED;
    memcpy(NotifyInfo.V.FileNameBuf, FileName, NotifyInfo.V.Size - sizeof(FSP_FSCTL_NOTIFY_INFO));
    Result = FspFileSystemNotify(FileSystem, &NotifyInfo.V, NotifyInfo.V.Size);

    return Result;
}

static
void notify_dirnotify_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);
    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);

    WCHAR FilePath[MAX_PATH];
    HANDLE Handle;
    BOOL Success;
    HANDLE Thread;
    DWORD ExitCode;
    DWORD BytesTransferred;
    PFILE_NOTIFY_INFORMATION NotifyInfo;

    NotifyInfo = malloc(4096);
    ASSERT(0 != NotifyInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\Directory",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        FILE_LIST_DIRECTORY, FILE_SHARE_READ, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Thread = (HANDLE)_beginthreadex(0, 0, notify_dirnotify_dotest_thread, FileSystem, 0, 0);
    ASSERT(0 != Thread);

    Success = ReadDirectoryChangesW(Handle,
        NotifyInfo, 4096, TRUE, FILE_NOTIFY_CHANGE_FILE_NAME, &BytesTransferred, 0, 0);
    ASSERT(Success);
    ASSERT(0 < BytesTransferred);

    ASSERT(FILE_ACTION_ADDED == NotifyInfo->Action);
    ASSERT(wcslen(L"Subdirectory\\file0") * sizeof(WCHAR) == NotifyInfo->FileNameLength);
    ASSERT(0 == mywcscmp(L"Subdirectory\\file0", -1,
        NotifyInfo->FileName, NotifyInfo->FileNameLength / sizeof(WCHAR)));

    ASSERT(0 == NotifyInfo->NextEntryOffset);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);
    ASSERT(STATUS_SUCCESS == ExitCode);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    free(NotifyInfo);

    memfs_stop(memfs);
}

static
void notify_dirnotify_test(void)
{
    if (WinFspDiskTests &&
        !OptNoTraverseToken /* WinFsp does not support change notifications w/o traverse privilege */ &&
        !OptCaseRandomize)
    {
        notify_dirnotify_dotest(MemfsDisk, 0, 0);
        notify_dirnotify_dotest(MemfsDisk, 0, 1000);
        notify_dirnotify_dotest(MemfsDisk, 0, INFINITE);
    }
    if (WinFspNetTests &&
        !OptNoTraverseToken /* WinFsp does not support change notifications w/o traverse privilege */ &&
        !OptCaseRandomize)
    {
        notify_dirnotify_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        notify_dirnotify_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
        notify_dirnotify_dotest(MemfsNet, L"\\\\memfs\\share", INFINITE);
    }
}

void notify_tests(void)
{
    if (OptExternal || OptNotify)
        return;

    TEST(notify_abandon_test);
    TEST(notify_abandon_rename_test);
    TEST(notify_timeout_test);
    TEST(notify_change_test);
    TEST(notify_open_change_test);
    TEST(notify_dirnotify_test);
}
