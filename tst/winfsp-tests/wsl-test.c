/**
 * @file wsl-test.c
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

typedef struct _FILE_STAT_INFORMATION
{
    LARGE_INTEGER FileId;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
    ULONG ReparseTag;
    ULONG NumberOfLinks;
    ACCESS_MASK EffectiveAccess;
} FILE_STAT_INFORMATION, *PFILE_STAT_INFORMATION;

typedef struct _FILE_STAT_LX_INFORMATION
{
    LARGE_INTEGER FileId;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
    ULONG ReparseTag;
    ULONG NumberOfLinks;
    ACCESS_MASK EffectiveAccess;
    ULONG LxFlags;
    ULONG LxUid;
    ULONG LxGid;
    ULONG LxMode;
    ULONG LxDeviceIdMajor;
    ULONG LxDeviceIdMinor;
} FILE_STAT_LX_INFORMATION, *PFILE_STAT_LX_INFORMATION;

NTSTATUS NTAPI NtQueryInformationFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass);

static void wsl_stat_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    WCHAR FilePath[MAX_PATH];
    FILE_STAT_INFORMATION StatInfo;
    FILE_STAT_LX_INFORMATION StatLxInfo;
    FILETIME FileTime;
    LONGLONG TimeLo, TimeHi;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Result;

    GetSystemTimeAsFileTime(&FileTime);
    TimeLo = ((PLARGE_INTEGER)&FileTime)->QuadPart;
    TimeHi = TimeLo + 10000 * 10000/* 10 seconds */;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    for (int repeat = 0; 2 > repeat; repeat++)
    {
        /* repeat this test to ensure that any caches are tested */

        Result = NtQueryInformationFile(Handle, &IoStatus, &StatInfo, sizeof StatInfo,
            68/*FileStatInformation*/);
        if (STATUS_SUCCESS == Result)
        {
            ASSERT(STATUS_SUCCESS == Result);
            if (-1 != Flags)
                ASSERT(
                    TimeLo <= StatInfo.CreationTime.QuadPart &&
                    TimeHi >  StatInfo.CreationTime.QuadPart);
            ASSERT(
                TimeLo <= StatInfo.LastAccessTime.QuadPart &&
                TimeHi >  StatInfo.LastAccessTime.QuadPart);
            ASSERT(
                TimeLo <= StatInfo.LastWriteTime.QuadPart &&
                TimeHi >  StatInfo.LastWriteTime.QuadPart);
            ASSERT(
                TimeLo <= StatInfo.ChangeTime.QuadPart &&
                TimeHi >  StatInfo.ChangeTime.QuadPart);
            ASSERT(0 == StatInfo.AllocationSize.QuadPart);
            ASSERT(0 == StatInfo.EndOfFile.QuadPart);
            //ASSERT(FILE_ATTRIBUTE_ARCHIVE == StatInfo.FileAttributes);
            ASSERT(0 == StatInfo.ReparseTag);
            ASSERT(1 == StatInfo.NumberOfLinks);
            //tlib_printf("%lx %lx", FILE_GENERIC_READ | FILE_GENERIC_WRITE, StatInfo.EffectiveAccess);
            //ASSERT((FILE_GENERIC_READ | FILE_GENERIC_WRITE) == StatInfo.EffectiveAccess);

            Result = NtQueryInformationFile(Handle, &IoStatus, &StatLxInfo, sizeof StatLxInfo,
                70/*FileStatLxInformation*/);
            ASSERT(STATUS_SUCCESS == Result);
            if (-1 != Flags)
                ASSERT(
                    TimeLo <= StatLxInfo.CreationTime.QuadPart &&
                    TimeHi >  StatLxInfo.CreationTime.QuadPart);
            ASSERT(
                TimeLo <= StatLxInfo.LastAccessTime.QuadPart &&
                TimeHi >  StatLxInfo.LastAccessTime.QuadPart);
            ASSERT(
                TimeLo <= StatLxInfo.LastWriteTime.QuadPart &&
                TimeHi >  StatLxInfo.LastWriteTime.QuadPart);
            ASSERT(
                TimeLo <= StatLxInfo.ChangeTime.QuadPart &&
                TimeHi >  StatLxInfo.ChangeTime.QuadPart);
            ASSERT(0 == StatLxInfo.AllocationSize.QuadPart);
            ASSERT(0 == StatLxInfo.EndOfFile.QuadPart);
            //ASSERT(FILE_ATTRIBUTE_ARCHIVE == StatLxInfo.FileAttributes);
            ASSERT(0 == StatLxInfo.ReparseTag);
            ASSERT(1 == StatLxInfo.NumberOfLinks);
            //tlib_printf("%lx %lx", FILE_GENERIC_READ | FILE_GENERIC_WRITE, StatLxInfo.EffectiveAccess);
            //ASSERT((FILE_GENERIC_READ | FILE_GENERIC_WRITE) == StatLxInfo.EffectiveAccess);
        }
        else
        {
            ASSERT(
                STATUS_INVALID_INFO_CLASS == Result ||
                STATUS_NOT_IMPLEMENTED == Result/* value returned under WOW64 */);
            FspDebugLog(__FUNCTION__ ": only works in Win10 with WSLinux\n");
        }
    }

    CloseHandle(Handle);

    memfs_stop(memfs);
}

static void wsl_stat_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        wsl_stat_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        wsl_stat_dotest(MemfsDisk, 0, 0);
        wsl_stat_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        wsl_stat_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        wsl_stat_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void wsl_tests(void)
{
    if (OptFuseExternal)
        return;

    TEST_OPT(wsl_stat_test);
}
