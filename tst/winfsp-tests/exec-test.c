/**
 * @file exec-test.c
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
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

static NTSTATUS WriteResource(
    HANDLE Handle, HANDLE Module, PWSTR ResourceName, PULONG PBytesTransferred)
{
    HRSRC Resource;
    HGLOBAL ResourceGlob;
    PVOID ResourceData;
    DWORD ResourceSize;

    if ((Resource = FindResourceW(Module, ResourceName, RT_RCDATA)) &&
        (ResourceGlob = LoadResource(Module, Resource)) &&
        (ResourceData = LockResource(ResourceGlob)) &&
        (ResourceSize = SizeofResource(Module, Resource)) &&
        (WriteFile(Handle, ResourceData, ResourceSize, PBytesTransferred, 0)))
        return STATUS_SUCCESS;
    else
        return FspNtStatusFromWin32(GetLastError());
}

static NTSTATUS ExtractHelperProgram(PWSTR FileName)
{
    HANDLE Handle;
    ULONG BytesTransferred;
    NTSTATUS Result;

    Handle = CreateFileW(FileName,
        FILE_WRITE_DATA, FILE_SHARE_WRITE, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return FspNtStatusFromWin32(GetLastError());

    Result = WriteResource(
        Handle,
        0,
#if defined(_WIN64)
        L"winfsp-tests-helper-x64.exe",
#elif defined(_WIN32)
        L"winfsp-tests-helper-x86.exe",
#else
#error
#endif
        &BytesTransferred);

    CloseHandle(Handle);

    return Result;
}

static NTSTATUS CreateHelperProcess(PWSTR FileName, ULONG Timeout, PHANDLE PProcess)
{
    HANDLE Event;
    SECURITY_ATTRIBUTES EventAttributes;
    WCHAR CommandLine[MAX_PATH + 64];
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    DWORD WaitResult;
    NTSTATUS Result;

    memset(&EventAttributes, 0, sizeof EventAttributes);
    EventAttributes.nLength = sizeof EventAttributes;
    EventAttributes.bInheritHandle = TRUE;

    Event = CreateEventW(&EventAttributes, TRUE, FALSE, 0);
    if (0 == Event)
        return FspNtStatusFromWin32(GetLastError());

    StringCbPrintfW(CommandLine, sizeof CommandLine, L"\"%s\" %lx %lx",
        FileName, (ULONG)(UINT_PTR)Event, Timeout);

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;

    // !!!: need hook
    if (!CreateProcessW(FileName, CommandLine, 0, 0, TRUE, 0, 0, 0, &StartupInfo, &ProcessInfo))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        CloseHandle(Event);
        return Result;
    }

    WaitResult = WaitForSingleObject(Event, 3000);
    if (WaitResult == WAIT_FAILED)
        Result = FspNtStatusFromWin32(GetLastError());
    else if (WaitResult == WAIT_TIMEOUT)
        Result = STATUS_UNSUCCESSFUL;
    else
        Result = STATUS_SUCCESS;

    CloseHandle(Event);
    CloseHandle(ProcessInfo.hThread);

    if (!NT_SUCCESS(Result))
        CloseHandle(ProcessInfo.hProcess);
    else
        *PProcess = ProcessInfo.hProcess;

    return Result;
}

static VOID ExecHelper(PWSTR FileName, ULONG Timeout, PHANDLE PProcess)
{
    NTSTATUS Result;

    Result = ExtractHelperProgram(FileName);
    ASSERT(NT_SUCCESS(Result));

    Result = CreateHelperProcess(FileName, Timeout, PProcess);
    ASSERT(NT_SUCCESS(Result));
}

static VOID WaitHelper(HANDLE Process, ULONG Timeout)
{
    DWORD ExitCode;

    ASSERT(WAIT_OBJECT_0 == WaitForSingleObject(Process, Timeout + 1000));

    ASSERT(GetExitCodeProcess(Process, &ExitCode));
    ASSERT(0 == ExitCode);

    ASSERT(CloseHandle(Process));
}

static void exec_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR FilePath[MAX_PATH];
    HANDLE Process;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\helper.exe",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    ExecHelper(FilePath, 0, &Process);
    WaitHelper(Process, 0);

    ASSERT(DeleteFileW(FilePath));

    memfs_stop(memfs);
}

static void exec_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        exec_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        exec_dotest(MemfsDisk, 0, 0);
        exec_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        exec_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        exec_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void exec_delete_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR FilePath[MAX_PATH];
    HANDLE Process;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\helper.exe",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    ExecHelper(FilePath, 1000, &Process);

    ASSERT(!DeleteFileW(FilePath));
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    {
        MY_FILE_DISPOSITION_INFO_EX DispositionInfo;
        HANDLE Handle;
        BOOLEAN Success;
        Handle = CreateFileW(FilePath,
            DELETE, FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, 0, 0);
        if (INVALID_HANDLE_VALUE != Handle)
        {
            DispositionInfo.Flags = 3/*FILE_DISPOSITION_DELETE | FILE_DISPOSITION_POSIX_SEMANTICS*/;
            Success = SetFileInformationByHandle(Handle,
                21/*FileDispositionInfoEx*/, &DispositionInfo, sizeof DispositionInfo);
            ASSERT(!Success);
            ASSERT(
                ERROR_INVALID_PARAMETER == GetLastError() ||
                ERROR_ACCESS_DENIED == GetLastError());
            Success = CloseHandle(Handle);
            ASSERT(Success);
        }
    }

    WaitHelper(Process, 1000);

    ASSERT(DeleteFileW(FilePath));

    memfs_stop(memfs);
}

static void exec_delete_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        exec_delete_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        exec_delete_dotest(MemfsDisk, 0, 0);
        exec_delete_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        exec_delete_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        exec_delete_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void exec_rename_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR FilePath[MAX_PATH], File2Path[MAX_PATH], File3Path[MAX_PATH];
    HANDLE Process;
    HANDLE Handle;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\helper.exe",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File2Path, sizeof File2Path, L"%s%s\\helper2.exe",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File3Path, sizeof File3Path, L"%s%s\\helper3.exe",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(File3Path,
        FILE_WRITE_DATA, FILE_SHARE_WRITE, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    ExecHelper(FilePath, 1000, &Process);

    ASSERT(MoveFileExW(FilePath, File2Path, MOVEFILE_REPLACE_EXISTING));
    ASSERT(MoveFileExW(File2Path, FilePath, MOVEFILE_REPLACE_EXISTING));

    ASSERT(!MoveFileExW(File3Path, FilePath, MOVEFILE_REPLACE_EXISTING));
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    WaitHelper(Process, 1000);

    ASSERT(MoveFileExW(File3Path, FilePath, MOVEFILE_REPLACE_EXISTING));

    ASSERT(DeleteFileW(FilePath));

    memfs_stop(memfs);
}

static void exec_rename_test(void)
{
    if (OptShareName)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        exec_rename_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        exec_rename_dotest(MemfsDisk, 0, 0);
        exec_rename_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        exec_rename_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        exec_rename_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void exec_rename_dir_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR Dir1Path[MAX_PATH], Dir2Path[MAX_PATH], FilePath[MAX_PATH];
    HANDLE Process;

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(Dir2Path, sizeof Dir2Path, L"%s%s\\dir2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\helper.exe",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    ASSERT(CreateDirectoryW(Dir1Path, 0));

    ExecHelper(FilePath, 2000, &Process);

    Sleep(1000); /* give time for file handles to be closed (FlushAndPurgeOnCleanup) */

    ASSERT(MoveFileExW(Dir1Path, Dir2Path, MOVEFILE_REPLACE_EXISTING));
    ASSERT(MoveFileExW(Dir2Path, Dir1Path, MOVEFILE_REPLACE_EXISTING));

    WaitHelper(Process, 2000);

    ASSERT(DeleteFileW(FilePath));

    ASSERT(RemoveDirectoryW(Dir1Path));

    memfs_stop(memfs);
}

static void exec_rename_dir_test(void)
{
    if (OptShareName)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        exec_rename_dir_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        exec_rename_dir_dotest(MemfsDisk, 0, 0);
        exec_rename_dir_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        exec_rename_dir_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        exec_rename_dir_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void exec_tests(void)
{
    TEST(exec_test);
    TEST(exec_delete_test);
    if (!OptShareName)
        TEST(exec_rename_test);
    if (!OptShareName)
        TEST(exec_rename_dir_test);
}
