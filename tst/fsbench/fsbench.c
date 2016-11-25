/**
 * @file fsbench.c
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

#include <windows.h>
#include <strsafe.h>
#include <tlib/testsuite.h>

ULONG OptFileCount = 1000;
ULONG OptListCount = 100;
ULONG OptRdwrCount = 10000;

static void file_create_test(void)
{
    HANDLE Handle;
    BOOL Success;
    WCHAR FileName[MAX_PATH];

    for (ULONG Index = 0; OptFileCount > Index; Index++)
    {
        StringCbPrintfW(FileName, sizeof FileName, L"fsbench-file%lu", Index);
        Handle = CreateFileW(FileName,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
            0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }
}
static void file_list_test(void)
{
    HANDLE Handle;
    BOOL Success;
    WIN32_FIND_DATAW FindData;

    for (ULONG Index = 0; OptListCount > Index; Index++)
    {
        Handle = FindFirstFileW(L"*", &FindData);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        do
        {
        } while (FindNextFileW(Handle, &FindData));
        Success = FindClose(Handle);
        ASSERT(Success);
    }
}
static void file_delete_test(void)
{
    BOOL Success;
    WCHAR FileName[MAX_PATH];

    for (ULONG Index = 0; OptFileCount > Index; Index++)
    {
        StringCbPrintfW(FileName, sizeof FileName, L"fsbench-file%lu", Index);
        Success = DeleteFileW(FileName);
        ASSERT(Success);
    }
}
static void file_tests(void)
{
    TEST(file_create_test);
    TEST(file_list_test);
    TEST(file_delete_test);
}

static void dir_mkdir_test(void)
{
    BOOL Success;
    WCHAR FileName[MAX_PATH];

    for (ULONG Index = 0; OptFileCount > Index; Index++)
    {
        StringCbPrintfW(FileName, sizeof FileName, L"fsbench-dir%lu", Index);
        Success = CreateDirectoryW(FileName, 0);
        ASSERT(Success);
    }
}
static void dir_rmdir_test(void)
{
    BOOL Success;
    WCHAR FileName[MAX_PATH];

    for (ULONG Index = 0; OptFileCount > Index; Index++)
    {
        StringCbPrintfW(FileName, sizeof FileName, L"fsbench-dir%lu", Index);
        Success = RemoveDirectoryW(FileName);
        ASSERT(Success);
    }
}
static void dir_tests(void)
{
    TEST(dir_mkdir_test);
    TEST(dir_rmdir_test);
}

static void rdwr_dotest(ULONG CreateDisposition, ULONG CreateFlags)
{
    SYSTEM_INFO SystemInfo;
    PVOID Buffer;
    HANDLE Handle;
    BOOL Success;
    DWORD BytesTransferred;
    WCHAR FileName[MAX_PATH];

    GetSystemInfo(&SystemInfo);
    Buffer = _aligned_malloc(SystemInfo.dwPageSize, SystemInfo.dwPageSize);
    ASSERT(0 != Buffer);
    memset(Buffer, 0, SystemInfo.dwPageSize);

    StringCbPrintfW(FileName, sizeof FileName, L"fsbench-file");
    Handle = CreateFileW(FileName,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        CreateDisposition,
        FILE_ATTRIBUTE_NORMAL | CreateFlags,
        0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    for (ULONG Index = 0; OptRdwrCount > Index; Index++)
    {
        if (CREATE_NEW == CreateDisposition)
            Success = WriteFile(Handle, Buffer, SystemInfo.dwPageSize, &BytesTransferred, 0);
        else
            Success = ReadFile(Handle, Buffer, SystemInfo.dwPageSize, &BytesTransferred, 0);
        ASSERT(Success);
        ASSERT(SystemInfo.dwPageSize == BytesTransferred);
    }

    Success = CloseHandle(Handle);
    ASSERT(Success);

    _aligned_free(Buffer);
}
static void rdwr_cached_write_test(void)
{
    rdwr_dotest(CREATE_NEW, 0);
}
static void rdwr_cached_read_test(void)
{
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE);
}
static void rdwr_noncached_write_test(void)
{
    rdwr_dotest(CREATE_NEW, 0 | FILE_FLAG_NO_BUFFERING);
}
static void rdwr_noncached_read_test(void)
{
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_NO_BUFFERING);
}
static void rdwr_tests(void)
{
    TEST(rdwr_cached_write_test);
    TEST(rdwr_cached_read_test);
    TEST(rdwr_noncached_write_test);
    TEST(rdwr_noncached_read_test);
}

static void mmap_cached_test(void)
{
}
static void mmap_noncached_test(void)
{
}
static void mmap_tests(void)
{
    TEST(mmap_cached_test);
    TEST(mmap_noncached_test);
}

#define rmarg(argv, argc, argi)         \
    argc--,                             \
    memmove(argv + argi, argv + argi + 1, (argc - argi) * sizeof(char *)),\
    argi--,                             \
    argv[argc] = 0
int main(int argc, char *argv[])
{
    TESTSUITE(file_tests);
    TESTSUITE(dir_tests);
    TESTSUITE(rdwr_tests);
    TESTSUITE(mmap_tests);

    for (int argi = 1; argc > argi; argi++)
    {
        const char *a = argv[argi];
        if ('-' == a[0])
        {
            if (0 == strncmp("--files=", a, sizeof "--files=" - 1))
            {
                OptFileCount = strtoul(a + sizeof "--files=" - 1, 0, 10);
                rmarg(argv, argc, argi);
            }
            else if (0 == strncmp("--list=", a, sizeof "--list=" - 1))
            {
                OptListCount = strtoul(a + sizeof "--list=" - 1, 0, 10);
                rmarg(argv, argc, argi);
            }
            else if (0 == strncmp("--rdwr=", a, sizeof "--rdwr=" - 1))
            {
                OptRdwrCount = strtoul(a + sizeof "--rdwr=" - 1, 0, 10);
                rmarg(argv, argc, argi);
            }
        }
    }

    tlib_run_tests(argc, argv);
    return 0;
}
