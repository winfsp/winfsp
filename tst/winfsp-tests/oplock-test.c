/**
 * @file oplock-test.c
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

typedef struct
{
    HANDLE File;
    HANDLE Wait;
    REQUEST_OPLOCK_INPUT_BUFFER OplockInputBuffer;
    REQUEST_OPLOCK_OUTPUT_BUFFER OplockOutputBuffer;
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
    default:
        Data->OplockInputBuffer.RequestedOplockLevel = Data->Information;
        Data->OplockInputBuffer.Flags = REQUEST_OPLOCK_INPUT_FLAG_ACK;
        Data->Information = 0;
        if ((Data->OplockOutputBuffer.Flags & REQUEST_OPLOCK_OUTPUT_FLAG_ACK_REQUIRED) &&
            !DeviceIoControl(Data->File, FSCTL_REQUEST_OPLOCK,
                &Data->OplockInputBuffer, sizeof Data->OplockInputBuffer,
                &Data->OplockOutputBuffer, sizeof Data->OplockOutputBuffer,
                &BytesTransferred, &Data->Overlapped) &&
            ERROR_IO_PENDING == GetLastError() &&
            RegisterWaitForSingleObject(&Data->Wait, Data->File,
                OplockBreakWait, Data, INFINITE, WT_EXECUTEONLYONCE))
        {
            break;
        }
        goto closefile;
    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
        Data->Information = 0;
        if (!DeviceIoControl(Data->File, FSCTL_OPLOCK_BREAK_ACKNOWLEDGE, 0, 0, 0, 0, &BytesTransferred,
                &Data->Overlapped) &&
            ERROR_IO_PENDING == GetLastError() &&
            RegisterWaitForSingleObject(&Data->Wait, Data->File,
                OplockBreakWait, Data, INFINITE, WT_EXECUTEONLYONCE))
        {
            break;
        }
        goto closefile;
    case 0:
    closefile:
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
    switch (RequestCode)
    {
    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:
    case FSCTL_REQUEST_BATCH_OPLOCK:
    case FSCTL_REQUEST_FILTER_OPLOCK:
        Success = DeviceIoControl(Data->File, RequestCode, 0, 0, 0, 0, &BytesTransferred,
            &Data->Overlapped);
        break;
    default:
        Data->OplockInputBuffer.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION;
        Data->OplockInputBuffer.StructureLength = sizeof Data->OplockInputBuffer;
        Data->OplockInputBuffer.RequestedOplockLevel = RequestCode;
        Data->OplockInputBuffer.Flags = REQUEST_OPLOCK_INPUT_FLAG_REQUEST;
        Success = DeviceIoControl(Data->File,
            FSCTL_REQUEST_OPLOCK,
            &Data->OplockInputBuffer, sizeof Data->OplockInputBuffer,
            &Data->OplockOutputBuffer, sizeof Data->OplockOutputBuffer,
            &BytesTransferred,
            &Data->Overlapped);
        break;
    }

    ASSERT(!Success);
    ASSERT(ERROR_IO_PENDING == GetLastError());

    Success = RegisterWaitForSingleObject(&Data->Wait, Data->File,
        OplockBreakWait, Data, INFINITE, WT_EXECUTEONLYONCE);
    ASSERT(Success);
}

static void oplock_dotest2(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout,
    ULONG RequestCode, ULONG BreakCode, ULONG WaitFlags,
    ULONG CreateFlags)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Semaphore;
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    PVOID Buffer;
    ULONG BufferSize;
    DWORD BytesTransferred;
    DWORD WaitResult;

    GetSystemInfo(&SystemInfo);
    BufferSize = 16 * SystemInfo.dwPageSize;
    Buffer = _aligned_malloc(BufferSize, BufferSize);
    ASSERT(0 != Buffer);

    Semaphore = CreateSemaphoreW(0, 0, 2, 0);
    ASSERT(0 != Semaphore);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    RequestOplock(FilePath, RequestCode, BreakCode, Semaphore);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, CreateFlags, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    if (WaitFlags & 1)
    {
        /* wait for oplock break after Create */
        WaitResult = WaitForSingleObject(Semaphore, INFINITE);
        ASSERT(WAIT_OBJECT_0 == WaitResult);
    }
    else
    {
        /* ensure no oplock break after Create */
        WaitResult = WaitForSingleObject(Semaphore, 300);
        ASSERT(WAIT_TIMEOUT == WaitResult);
    }

    if (FILE_FLAG_OVERLAPPED & CreateFlags)
    {
        OVERLAPPED Overlapped;
        memset(&Overlapped, 0, sizeof Overlapped);
        Success = WriteFile(Handle, Buffer, BufferSize, &BytesTransferred, &Overlapped);
        ASSERT(Success || ERROR_IO_PENDING == GetLastError());
        Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
        ASSERT(Success);
    }
    else
    {
        Success = WriteFile(Handle, Buffer, BufferSize, &BytesTransferred, 0);
        ASSERT(Success);
    }

    if (WaitFlags & 2)
    {
        /* wait for any oplock break after Write */
        WaitResult = WaitForSingleObject(Semaphore, INFINITE);
        ASSERT(WAIT_OBJECT_0 == WaitResult);
    }

    /* ensure no additional oplock breaks */
    WaitResult = WaitForSingleObject(Semaphore, 300);
    ASSERT(WAIT_TIMEOUT == WaitResult);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = CloseHandle(Semaphore);
    ASSERT(Success);

    _aligned_free(Buffer);

    memfs_stop(memfs);
}

static void oplock_dotest(ULONG Flags, PWSTR Prefix,
    ULONG RequestCode, ULONG BreakCode, ULONG WaitFlags)
{
    oplock_dotest2(Flags, Prefix, 0, RequestCode, BreakCode, WaitFlags, 0);
    oplock_dotest2(Flags, Prefix, 0, RequestCode, BreakCode, WaitFlags, FILE_FLAG_NO_BUFFERING);
    oplock_dotest2(Flags, Prefix, 0, RequestCode, BreakCode, WaitFlags, FILE_FLAG_OVERLAPPED);
    oplock_dotest2(Flags, Prefix, 0, RequestCode, BreakCode, WaitFlags, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING);
    if (-1 != Flags)
    {
        oplock_dotest2(Flags, Prefix, INFINITE, RequestCode, BreakCode, WaitFlags, 0);
        oplock_dotest2(Flags, Prefix, INFINITE, RequestCode, BreakCode, WaitFlags, FILE_FLAG_NO_BUFFERING);
        oplock_dotest2(Flags, Prefix, INFINITE, RequestCode, BreakCode, WaitFlags, FILE_FLAG_OVERLAPPED);
        oplock_dotest2(Flags, Prefix, INFINITE, RequestCode, BreakCode, WaitFlags, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING);
    }
}

static void oplock_level1_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            FSCTL_REQUEST_OPLOCK_LEVEL_1, FSCTL_OPLOCK_BREAK_ACKNOWLEDGE, 3);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            FSCTL_REQUEST_OPLOCK_LEVEL_1, FSCTL_OPLOCK_BREAK_ACKNOWLEDGE, 3);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            FSCTL_REQUEST_OPLOCK_LEVEL_1, FSCTL_OPLOCK_BREAK_ACKNOWLEDGE, 3);
}

static void oplock_level2_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            FSCTL_REQUEST_OPLOCK_LEVEL_2, 0, 2);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            FSCTL_REQUEST_OPLOCK_LEVEL_2, 0, 2);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            FSCTL_REQUEST_OPLOCK_LEVEL_2, 0, 2);
}

static void oplock_batch_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            FSCTL_REQUEST_BATCH_OPLOCK, 0, 1);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            FSCTL_REQUEST_BATCH_OPLOCK, 0, 1);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            FSCTL_REQUEST_BATCH_OPLOCK, 0, 1);
}

static void oplock_filter_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            FSCTL_REQUEST_FILTER_OPLOCK, 0, 1);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            FSCTL_REQUEST_FILTER_OPLOCK, 0, 1);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            FSCTL_REQUEST_FILTER_OPLOCK, 0, 1);
}

static void oplock_rwh_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE | OPLOCK_LEVEL_CACHE_HANDLE,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE, 3);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE | OPLOCK_LEVEL_CACHE_HANDLE,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE, 3);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE | OPLOCK_LEVEL_CACHE_HANDLE,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE, 3);
}

static void oplock_rw_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE,
            OPLOCK_LEVEL_CACHE_READ, 3);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE,
            OPLOCK_LEVEL_CACHE_READ, 3);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_WRITE,
            OPLOCK_LEVEL_CACHE_READ, 3);
}

static void oplock_rh_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE,
            0, 2);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE,
            0, 2);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE,
            0, 2);
}

static void oplock_r_test(void)
{
    if (OptShareName || OptOplock)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        oplock_dotest(-1, DirBuf,
            OPLOCK_LEVEL_CACHE_READ,
            0, 2);
    }
    if (WinFspDiskTests)
        oplock_dotest(MemfsDisk, 0,
            OPLOCK_LEVEL_CACHE_READ,
            0, 2);
    if (WinFspNetTests)
        oplock_dotest(MemfsNet, L"\\\\memfs\\share",
            OPLOCK_LEVEL_CACHE_READ,
            0, 2);
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

    TEST_OPT(oplock_level1_test);
    TEST_OPT(oplock_level2_test);
    TEST_OPT(oplock_batch_test);
    TEST_OPT(oplock_filter_test);
    TEST_OPT(oplock_rwh_test);
    TEST_OPT(oplock_rw_test);
    TEST_OPT(oplock_rh_test);
    TEST_OPT(oplock_r_test);
    TEST_OPT(oplock_not_granted_test);
}
