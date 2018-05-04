/**
 * @file devctl-test.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
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

static void devctl_dotest(ULONG Flags, PWSTR Prefix, PWSTR Drive)
{
    void *memfs = memfs_start(Flags);

    WCHAR FilePath[1024];
    HANDLE Handle;
    BOOL Success;
    CHAR Buffer[26];
    DWORD BytesTransferred;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = DeviceIoControl(Handle,
        CTL_CODE(0x8000 + 'M', 'R', METHOD_BUFFERED, FILE_ANY_ACCESS),
        "ABCDEFghijklmNOPQRStuvwxyz", 26,
        Buffer, sizeof Buffer,
        &BytesTransferred,
        0);
    ASSERT(Success);

    ASSERT(26 == BytesTransferred);
    ASSERT(0 == memcmp("NOPQRStuvwxyzABCDEFghijklm", Buffer, BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void devctl_test(void)
{
    if (WinFspDiskTests)
        devctl_dotest(MemfsDisk, 0, 0);
    if (WinFspNetTests)
        devctl_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share");
}

void devctl_tests(void)
{
    if (OptExternal)
        return;

    TEST(devctl_test);
}
