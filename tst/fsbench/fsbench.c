/**
 * @file fsbench.c
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

#include <windows.h>
#include <strsafe.h>
#include <tlib/testsuite.h>

static ULONG OptFileCount = 1000;
static ULONG OptListCount = 100;
static ULONG OptRdwrFileSize = 4096 * 1024;
static ULONG OptRdwrCcCount = 100;
static ULONG OptRdwrNcCount = 100;
static ULONG OptMmapFileSize = 4096 * 1024;
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
static void file_attr_test(void)
{
    WCHAR FileName[MAX_PATH];
    DWORD FileAttributes;

    for (ULONG ListIndex = 0; OptListCount > ListIndex; ListIndex++)
        for (ULONG Index = 0; OptFileCount > Index; Index++)
        {
            StringCbPrintfW(FileName, sizeof FileName, L"fsbench-file%lu", Index);
            FileAttributes = GetFileAttributesW(FileName);
            ASSERT(INVALID_FILE_ATTRIBUTES != FileAttributes);
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
static void file_list_single_test(void)
{
    HANDLE Handle;
    BOOL Success;
    WCHAR FileName[MAX_PATH];
    WIN32_FIND_DATAW FindData;

    for (ULONG ListIndex = 0; OptListCount > ListIndex; ListIndex++)
        for (ULONG Index = 0; OptFileCount > Index; Index++)
        {
            StringCbPrintfW(FileName, sizeof FileName, L"fsbench-file%lu", Index);
            Handle = FindFirstFileW(FileName, &FindData);
            ASSERT(INVALID_HANDLE_VALUE != Handle);
            do
            {
            } while (FindNextFileW(Handle, &FindData));
            Success = FindClose(Handle);
            ASSERT(Success);
        }
}
static void file_list_none_test(void)
{
    HANDLE Handle;
    WCHAR FileName[MAX_PATH];
    WIN32_FIND_DATAW FindData;

    for (ULONG ListIndex = 0; OptListCount > ListIndex; ListIndex++)
        for (ULONG Index = 0; OptFileCount > Index; Index++)
        {
            StringCbPrintfW(FileName, sizeof FileName, L"{5F849D7F-73AF-49AC-B7C3-657B36EAD5C4}");
            Handle = FindFirstFileW(FileName, &FindData);
            ASSERT(INVALID_HANDLE_VALUE == Handle);
            ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());
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
static void file_mkdir_test(void)
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
static void file_rmdir_test(void)
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
static void file_tests(void)
{
    TEST(file_create_test);
    TEST(file_open_test);
    TEST(file_overwrite_test);
    TEST(file_attr_test);
    TEST(file_list_test);
    TEST(file_list_single_test);
    TEST(file_list_none_test);
    TEST(file_delete_test);
    TEST(file_mkdir_test);
    TEST(file_rmdir_test);
}

static void rdwr_dotest(ULONG CreateDisposition, ULONG CreateFlags,
    ULONG FileSize, ULONG BufferSize, ULONG Count)
{
    WCHAR FileName[MAX_PATH];
    HANDLE Handle;
    BOOL Success;
    PVOID Buffer;
    DWORD BytesTransferred;

    Buffer = _aligned_malloc(BufferSize, BufferSize);
    ASSERT(0 != Buffer);
    memset(Buffer, 0, BufferSize);

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
        BytesTransferred = SetFilePointer(Handle, FileSize, 0, FILE_BEGIN);
        ASSERT(FileSize == BytesTransferred);
        SetEndOfFile(Handle);
    }

    for (ULONG Index = 0; Count > Index; Index++)
    {
        BytesTransferred = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
        ASSERT(0 == BytesTransferred);
        for (ULONG I = 0, N = FileSize / BufferSize; N > I; I++)
        {
            if (CREATE_NEW == CreateDisposition)
                Success = WriteFile(Handle, Buffer, BufferSize, &BytesTransferred, 0);
            else
                Success = ReadFile(Handle, Buffer, BufferSize, &BytesTransferred, 0);
            ASSERT(Success);
            ASSERT(BufferSize == BytesTransferred);
        }
    }

    Success = CloseHandle(Handle);
    ASSERT(Success);

    _aligned_free(Buffer);
}
static void rdwr_cc_write_sector_test(void)
{
    DWORD SC, BS, FC, TC;
    ASSERT(GetDiskFreeSpaceW(0, &SC, &BS, &FC, &TC));
    rdwr_dotest(CREATE_NEW, 0,
        OptRdwrFileSize, BS, OptRdwrCcCount);
}
static void rdwr_cc_read_sector_test(void)
{
    DWORD SC, BS, FC, TC;
    ASSERT(GetDiskFreeSpaceW(0, &SC, &BS, &FC, &TC));
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE,
        OptRdwrFileSize, BS, OptRdwrCcCount);
}
static void rdwr_nc_write_sector_test(void)
{
    DWORD SC, BS, FC, TC;
    ASSERT(GetDiskFreeSpaceW(0, &SC, &BS, &FC, &TC));
    rdwr_dotest(CREATE_NEW, 0 | FILE_FLAG_NO_BUFFERING,
        OptRdwrFileSize, BS, OptRdwrNcCount);
}
static void rdwr_nc_read_sector_test(void)
{
    DWORD SC, BS, FC, TC;
    ASSERT(GetDiskFreeSpaceW(0, &SC, &BS, &FC, &TC));
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_NO_BUFFERING,
        OptRdwrFileSize, BS, OptRdwrNcCount);
}
static void rdwr_cc_write_page_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(CREATE_NEW, 0,
        OptRdwrFileSize, SystemInfo.dwPageSize, OptRdwrCcCount);
}
static void rdwr_cc_read_page_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE,
        OptRdwrFileSize, SystemInfo.dwPageSize, OptRdwrCcCount);
}
static void rdwr_nc_write_page_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(CREATE_NEW, 0 | FILE_FLAG_NO_BUFFERING,
        OptRdwrFileSize, SystemInfo.dwPageSize, OptRdwrNcCount);
}
static void rdwr_nc_read_page_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_NO_BUFFERING,
        OptRdwrFileSize, SystemInfo.dwPageSize, OptRdwrNcCount);
}
static void rdwr_cc_write_large_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(CREATE_NEW, 0,
        OptRdwrFileSize, 16 * SystemInfo.dwPageSize, OptRdwrCcCount);
}
static void rdwr_cc_read_large_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE,
        OptRdwrFileSize, 16 * SystemInfo.dwPageSize, OptRdwrCcCount);
}
static void rdwr_nc_write_large_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(CREATE_NEW, 0 | FILE_FLAG_NO_BUFFERING,
        OptRdwrFileSize, 16 * SystemInfo.dwPageSize, OptRdwrNcCount);
}
static void rdwr_nc_read_large_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    rdwr_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_NO_BUFFERING,
        OptRdwrFileSize, 16 * SystemInfo.dwPageSize, OptRdwrNcCount);
}
static void rdwr_tests(void)
{
    //TEST(rdwr_cc_write_sector_test);
    //TEST(rdwr_cc_read_sector_test);
    TEST(rdwr_cc_write_page_test);
    TEST(rdwr_cc_read_page_test);
    TEST(rdwr_cc_write_large_test);
    TEST(rdwr_cc_read_large_test);
    //TEST(rdwr_nc_write_sector_test);
    //TEST(rdwr_nc_read_sector_test);
    TEST(rdwr_nc_write_page_test);
    TEST(rdwr_nc_read_page_test);
    TEST(rdwr_nc_write_large_test);
    TEST(rdwr_nc_read_large_test);
}

static void mmap_dotest(ULONG CreateDisposition, ULONG CreateFlags,
    ULONG FileSize, ULONG BufferSize, ULONG Count)
{
    WCHAR FileName[MAX_PATH];
    HANDLE Handle, Mapping;
    BOOL Success;
    PUINT8 MappedView;

    StringCbPrintfW(FileName, sizeof FileName, L"fsbench-file");
    Handle = CreateFileW(FileName,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        CreateDisposition,
        FILE_ATTRIBUTE_NORMAL | CreateFlags,
        0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Mapping = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, FileSize, 0);
    ASSERT(0 != Mapping);

    MappedView = MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView);

    for (ULONG Index = 0; Count > Index; Index++)
    {
        for (ULONG I = 0, N = FileSize / BufferSize; N > I; I++)
        {
            if (CREATE_NEW == CreateDisposition)
            {
                for (ULONG J = 0; BufferSize > J; J++)
                    MappedView[I * BufferSize + J] = 0;
            }
            else
            {
                ULONG Total = 0;
                for (ULONG J = 0; BufferSize > J; J++)
                    Total += MappedView[I * BufferSize + J];
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
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    mmap_dotest(CREATE_NEW, 0,
        OptMmapFileSize, SystemInfo.dwPageSize, OptMmapCount);
}
static void mmap_read_test(void)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    mmap_dotest(OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE,
        OptMmapFileSize, SystemInfo.dwPageSize, OptMmapCount);
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
            else if (0 == strncmp("--rdwr-nc=", a, sizeof "--rdwr-nc=" - 1))
            {
                OptRdwrNcCount = strtoul(a + sizeof "--rdwr-nc=" - 1, 0, 10);
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
