/**
 * @file oplock-test.c
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

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

typedef struct
{
    HANDLE File;
    HANDLE Wait;
    OVERLAPPED Overlapped;
    ULONG Information;
    HANDLE Semaphore;
} OPLOCK_BREAK_WAIT_DATA;

static VOID CALLBACK OplockBreakWait(PVOID Context, BOOLEAN Timeout)
{
    OPLOCK_BREAK_WAIT_DATA *Data = Context;
    DWORD BytesTransferred;

    UnregisterWaitEx(Data->Wait, 0);

    switch (Data->Information)
    {
    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
        Data->Information = FSCTL_OPLOCK_BREAK_ACK_NO_2;
        if (!DeviceIoControl(Data->File, FSCTL_OPLOCK_BREAK_ACKNOWLEDGE, 0, 0, 0, 0, &BytesTransferred,
            &Data->Overlapped) && ERROR_IO_PENDING == GetLastError() &&
            RegisterWaitForSingleObject(&Data->Wait, Data->File,
                OplockBreakWait, Data, INFINITE, WT_EXECUTEONLYONCE))
        {
            break;
        }
        /* fall through! */
    default:
        CloseHandle(Data->File);
        HeapFree(GetProcessHeap(), 0, Data);
        break;
    }

    ReleaseSemaphore(Data->Semaphore, 1, 0);
}

static VOID RequestOplock(PWSTR FileName, ULONG RequestCode, ULONG BreakCode, HANDLE Semaphore)
{
    OPLOCK_BREAK_WAIT_DATA *Data;
    DWORD BytesTransferred;
    BOOLEAN Success;

    Data = HeapAlloc(GetProcessHeap(), 0, sizeof *Data);
    ASSERT(0 != Data);
    memset(Data, 0, sizeof *Data);

    Data->Semaphore = Semaphore;

    Data->File = CreateFileW(FileName,
        FILE_READ_ATTRIBUTES, 0,
        0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    ASSERT(INVALID_HANDLE_VALUE != Data->File);

    Data->Information = BreakCode;
    Success = DeviceIoControl(Data->File, RequestCode, 0, 0, 0, 0, &BytesTransferred,
        &Data->Overlapped);
    ASSERT(!Success);
    ASSERT(ERROR_IO_PENDING == GetLastError());

    Success = RegisterWaitForSingleObject(&Data->Wait, Data->File,
        OplockBreakWait, Data, INFINITE, WT_EXECUTEONLYONCE);
    ASSERT(Success);
}

static void oplock_level1_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Semaphore;
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    DWORD BytesTransferred;
    DWORD WaitResult;

    Semaphore = CreateSemaphoreW(0, 0, 2, 0);
    ASSERT(0 != Semaphore);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    RequestOplock(FilePath, FSCTL_REQUEST_OPLOCK_LEVEL_1, FSCTL_OPLOCK_BREAK_ACKNOWLEDGE,
        Semaphore);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    /* wait for level1 to level2 break */
    WaitResult = WaitForSingleObject(Semaphore, INFINITE);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    Success = WriteFile(Handle, L"foobar", 6, &BytesTransferred, 0);
    ASSERT(Success);

    /* wait for break to none */
    WaitResult = WaitForSingleObject(Semaphore, INFINITE);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = CloseHandle(Semaphore);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void oplock_level1_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_level1_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        oplock_level1_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        oplock_level1_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void oplock_level2_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Semaphore;
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    DWORD BytesTransferred;
    DWORD WaitResult;

    Semaphore = CreateSemaphoreW(0, 0, 2, 0);
    ASSERT(0 != Semaphore);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    RequestOplock(FilePath, FSCTL_REQUEST_OPLOCK_LEVEL_2, 0,
        Semaphore);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = WriteFile(Handle, L"foobar", 6, &BytesTransferred, 0);
    ASSERT(Success);

    /* wait for break to none */
    WaitResult = WaitForSingleObject(Semaphore, INFINITE);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    /* double check there isn't any remaining count on the semaphore */
    WaitResult = WaitForSingleObject(Semaphore, 100);
    ASSERT(WAIT_TIMEOUT == WaitResult);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = CloseHandle(Semaphore);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void oplock_level2_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_level2_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        oplock_level2_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        oplock_level2_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void oplock_batch_dotest(ULONG Flags, PWSTR Prefix, ULONG RequestCode)
{
    void *memfs = memfs_start(Flags);

    HANDLE Semaphore;
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    DWORD BytesTransferred;
    DWORD WaitResult;

    Semaphore = CreateSemaphoreW(0, 0, 2, 0);
    ASSERT(0 != Semaphore);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    RequestOplock(FilePath, RequestCode, 0,
        Semaphore);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    /* wait for break to none */
    WaitResult = WaitForSingleObject(Semaphore, INFINITE);
    ASSERT(WAIT_OBJECT_0 == WaitResult);

    Success = WriteFile(Handle, L"foobar", 6, &BytesTransferred, 0);
    ASSERT(Success);

    /* double check there isn't any remaining count on the semaphore */
    WaitResult = WaitForSingleObject(Semaphore, 100);
    ASSERT(WAIT_TIMEOUT == WaitResult);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = CloseHandle(Semaphore);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void oplock_batch_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_batch_dotest(-1, DirBuf, FSCTL_REQUEST_BATCH_OPLOCK);
    }
    if (WinFspDiskTests)
        oplock_batch_dotest(MemfsDisk, 0, FSCTL_REQUEST_BATCH_OPLOCK);
    if (WinFspNetTests)
        oplock_batch_dotest(MemfsNet, L"\\\\memfs\\share", FSCTL_REQUEST_BATCH_OPLOCK);
}

static void oplock_filter_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_batch_dotest(-1, DirBuf, FSCTL_REQUEST_FILTER_OPLOCK);
    }
    if (WinFspDiskTests)
        oplock_batch_dotest(MemfsDisk, 0, FSCTL_REQUEST_FILTER_OPLOCK);
    if (WinFspNetTests)
        oplock_batch_dotest(MemfsNet, L"\\\\memfs\\share", FSCTL_REQUEST_FILTER_OPLOCK);
}

static void oplock_not_granted_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle1, Handle2;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    OVERLAPPED Overlapped;
    DWORD BytesTransferred;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);

    Handle2 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle2);

    memset(&Overlapped, 0, sizeof Overlapped);
    Success = DeviceIoControl(Handle2, FSCTL_REQUEST_FILTER_OPLOCK, 0, 0, 0, 0, &BytesTransferred,
        &Overlapped);
    ASSERT(!Success);
    ASSERT(ERROR_OPLOCK_NOT_GRANTED == GetLastError() || ERROR_IO_PENDING == GetLastError());
    if (ERROR_IO_PENDING == GetLastError())
    {
        Success = GetOverlappedResult(Handle2, &Overlapped, &BytesTransferred, TRUE);
        ASSERT(!Success && ERROR_OPLOCK_NOT_GRANTED == GetLastError());
    }

    Success = CloseHandle(Handle2);
    ASSERT(Success);

    Success = CloseHandle(Handle1);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void oplock_not_granted_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_not_granted_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        oplock_not_granted_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        oplock_not_granted_dotest(MemfsNet, L"\\\\memfs\\share");
}

void oplock_tests(void)
{
    if (OptShareName || OptOplock)
        return;

    TEST(oplock_level1_test);
    TEST(oplock_level2_test);
    TEST(oplock_batch_test);
    TEST(oplock_filter_test);
    TEST(oplock_not_granted_test);
}
