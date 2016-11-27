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

static ULONG OptFileCount = 1000;
static ULONG OptListCount = 100;
static ULONG OptRdwrCcCount = 100;
static ULONG OptRdwrNcCount = 100;
static ULONG OptMmapCount = 100;

static void file_create_dotest(ULONG CreateDisposition)
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
            CreateDisposition, FILE_ATTRIBUTE_NORMAL,
            0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }
}
static void file_create_test(void)
{
    file_create_dotest(CREATE_NEW);
}
static void file_open_test(void)
{
    file_create_dotest(OPEN_EXISTING);
}
static void file_overwrite_test(void)
{
    file_create_dotest(CREATE_ALWAYS);
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
    TEST(file_open_test);
    TEST(file_overwrite_test);
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

static void rdwr_dotest(ULONG CreateDisposition, ULONG CreateFlags, ULONG Count)
{
    SYSTEM_INFO SystemInfo;
    PVOID Buffer;
    HANDLE Handle;
    BOOL Success;
    DWORD BytesTransferred;
    WCHAR FileName[MAX_PATH];
    ULONG Iterations = 1000;


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

    if (CREATE_NEW == CreateDisposition)
    {
        BytesTransferred = SetFilePointer(Handle, Iterations * SystemInfo.dwPageSize, 0, FILE_BEGIN);
        ASSERT(Iterations * SystemInfo.dwPageSize == BytesTransferred);
        SetEndOfFile(Handle);
    }

    for (ULONG Index = 0; Count > Index; Index++)
    {
        BytesTransferred = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
        ASSERT(0 == BytesTransferred);
        for (ULONG I = 0; Iterations > I; I++)
        {
            if (CREATE_NEW == CreateDisposition)
                Success = WriteFile(Handle, Buffer, SystemInfo.dwPageSize, &BytesTransferred, 0);
            else
                Success = ReadFile(Handle, Buffer, SystemInfo.dwPageSize, &BytesTransferred, 0);
            ASSERT(Success);
            ASSERT(SystemInfo.dwPageSize == BytesTransferred);
        }
    }

    Success = CloseHandle(Handle);
    ASSERT(Success);

    _aligned_free(Buffer);
}
static void rdwr_cc_write_test(void)
{
    rdwr_dotest(CREATE_NEW, 0, OptRdwrCcCount);
}
static void rdwr_cc_read_test(void)
{
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, OptRdwrCcCount);
}
static void rdwr_nc_write_test(void)
{
    rdwr_dotest(CREATE_NEW, 0 | FILE_FLAG_NO_BUFFERING, OptRdwrNcCount);
}
static void rdwr_nc_read_test(void)
{
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_NO_BUFFERING,
        OptRdwrNcCount);
}
static void rdwr_tests(void)
{
    TEST(rdwr_cc_write_test);
    TEST(rdwr_cc_read_test);
    TEST(rdwr_nc_write_test);
    TEST(rdwr_nc_read_test);
}

static void mmap_dotest(ULONG CreateDisposition, ULONG CreateFlags, ULONG Count)
{
    SYSTEM_INFO SystemInfo;
    HANDLE Handle, Mapping;
    BOOL Success;
    WCHAR FileName[MAX_PATH];
    ULONG Iterations = 1000;
    PUINT8 MappedView;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FileName, sizeof FileName, L"fsbench-file");
    Handle = CreateFileW(FileName,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        CreateDisposition,
        FILE_ATTRIBUTE_NORMAL | CreateFlags,
        0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Mapping = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, Iterations * SystemInfo.dwPageSize, 0);
    ASSERT(0 != Mapping);

    MappedView = MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView);

    for (ULONG Index = 0; Count > Index; Index++)
    {
        for (ULONG I = 0; Iterations > I; I++)
        {
            if (CREATE_NEW == CreateDisposition)
            {
                for (ULONG J = 0; SystemInfo.dwPageSize > J; J++)
                    MappedView[J] = 0;
            }
            else
            {
                ULONG Total = 0;
                for (ULONG J = 0; SystemInfo.dwPageSize > J; J++)
                    Total += MappedView[J];
                ASSERT(0 == Total);
            }
        }
    }

    Success = UnmapViewOfFile(MappedView);
    ASSERT(Success);

    Success = CloseHandle(Mapping);
    ASSERT(Success);

    Success = CloseHandle(Handle);
    ASSERT(Success);
}
static void mmap_write_test(void)
{
    mmap_dotest(CREATE_NEW, 0, OptMmapCount);
}
static void mmap_read_test(void)
{
    mmap_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, OptMmapCount);
}
static void mmap_tests(void)
{
    TEST(mmap_write_test);
    TEST(mmap_read_test);
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
            else if (0 == strncmp("--rdwr-cc=", a, sizeof "--rdwr-cc=" - 1))
            {
                OptRdwrCcCount = strtoul(a + sizeof "--rdwr-cc=" - 1, 0, 10);
                rmarg(argv, argc, argi);
            }
            else if (0 == strncmp("--rdwr-nocc=", a, sizeof "--rdwr-nocc=" - 1))
            {
                OptRdwrNcCount = strtoul(a + sizeof "--rdwr-nocc=" - 1, 0, 10);
                rmarg(argv, argc, argi);
            }
            else if (0 == strncmp("--mmap=", a, sizeof "--mmap=" - 1))
            {
                OptMmapCount = strtoul(a + sizeof "--mmap=" - 1, 0, 10);
                rmarg(argv, argc, argi);
            }
        }
    }

    tlib_run_tests(argc, argv);
    return 0;
}
