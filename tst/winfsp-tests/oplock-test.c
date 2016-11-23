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

void oplock_not_granted_dotest(ULONG Flags, PWSTR Prefix)
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

void oplock_not_granted_test(void)
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

    TEST(oplock_not_granted_test);
}
