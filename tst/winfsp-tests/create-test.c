/**
 * @file create-test.c
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
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

#include "winfsp-tests.h"

void create_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_EXISTS == GetLastError());

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    if (0 == Prefix)
    {
        /* double backslash at path root */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\\\\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        if (0 == OptMountPoint)
        {
            Handle = CreateFileW(FilePath,
                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
            ASSERT(INVALID_HANDLE_VALUE == Handle);
            ASSERT(ERROR_INVALID_NAME == GetLastError());
        }
    }

    /* invalid chars (wildcards) not allowed */
    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    /* stream names can only appear as the last path component */
    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\DOESNOTEXIST:foo\\file0*",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(!Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    {
        /* attempt to DELETE_ON_CLOSE a non-empty directory! */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    }

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    if (-1 == Flags || 0 == Prefix)
    {
        /* backslash at path end */

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0\\",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        if (0 == OptMountPoint)
        {
            Handle = CreateFileW(FilePath,
                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
            ASSERT(INVALID_HANDLE_VALUE == Handle);
            ASSERT(ERROR_INVALID_NAME == GetLastError());
        }

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\\\",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        if (0 == OptMountPoint && 0 == OptShareName)
        {
            Success = CreateDirectoryW(FilePath, 0);
            ASSERT(!Success);
            ASSERT(ERROR_INVALID_NAME == GetLastError());
        }

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Success = CreateDirectoryW(FilePath, 0);
        ASSERT(Success);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    memfs_stop(memfs);
}

void create_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void create_fileattr_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle;
    BOOLEAN Success;
    DWORD FileAttributes;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT(FILE_ATTRIBUTE_ARCHIVE == FileAttributes);
    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_READONLY, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT((FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY) == FileAttributes);
    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_NORMAL);
    ASSERT(Success);
    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_SYSTEM, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT((FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_SYSTEM) == FileAttributes);
    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_NORMAL);
    ASSERT(Success);
    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT((FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN) == FileAttributes);
    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_NORMAL);
    ASSERT(Success);
    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT(FILE_ATTRIBUTE_ARCHIVE == FileAttributes);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_READONLY, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT((FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY) == FileAttributes);
    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_NORMAL);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_SYSTEM, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT((FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_SYSTEM) == FileAttributes);
    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_NORMAL);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT((FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN) == FileAttributes);
    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_NORMAL);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void create_fileattr_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_fileattr_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_fileattr_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_fileattr_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void create_readonlydir_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle;
    BOOLEAN Success;
    DWORD FileAttributes;
    WCHAR DirPath[MAX_PATH], FilePath[MAX_PATH];

    StringCbPrintfW(DirPath, sizeof DirPath, L"%s%s\\dir0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir0\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(DirPath, 0);
    ASSERT(Success);

    Success = SetFileAttributesW(DirPath, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY);
    ASSERT(Success);

    FileAttributes = GetFileAttributesW(DirPath);
    ASSERT((FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY) == FileAttributes);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = RemoveDirectoryW(DirPath);
    ASSERT(!Success);
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    Success = SetFileAttributesW(DirPath, FILE_ATTRIBUTE_DIRECTORY);
    ASSERT(Success);

    FileAttributes = GetFileAttributesW(DirPath);
    ASSERT(FILE_ATTRIBUTE_DIRECTORY == FileAttributes);

    Success = RemoveDirectoryW(DirPath);
    ASSERT(Success);

    memfs_stop(memfs);
}

static void create_readonlydir_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_readonlydir_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_readonlydir_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_readonlydir_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_related_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE DirHandle, FileHandle;
    NTSTATUS Result;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR UnicodePathBuf[MAX_PATH] = L"file2";
    UNICODE_STRING UnicodePath;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK Iosb;
    LARGE_INTEGER LargeZero = { 0 };

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);
    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &LargeZero, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_CREATE, FILE_DELETE_ON_CLOSE, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);
    CloseHandle(FileHandle);

    CloseHandle(DirHandle);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == DirHandle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void create_related_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_related_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_related_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_related_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_allocation_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE DirHandle, FileHandle, FileHandle2;
    NTSTATUS Result;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR UnicodePathBuf[MAX_PATH] = L"file2";
    UNICODE_STRING UnicodePath;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK Iosb;
    LARGE_INTEGER AllocationSize;
    FILE_STANDARD_INFO StandardInfo;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    DirHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != DirHandle);

    AllocationSize.QuadPart = 65536;
    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);
    Result = NtCreateFile(&FileHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, &Obja, &Iosb,
        &AllocationSize, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_CREATE, 0, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);

    Success = GetFileInformationByHandleEx(FileHandle, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);
    ASSERT(65536 == StandardInfo.AllocationSize.QuadPart);

    AllocationSize.QuadPart = 0;
    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);
    Result = NtCreateFile(&FileHandle2,
        FILE_READ_ATTRIBUTES, &Obja, &Iosb,
        &AllocationSize, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, 0, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);

    Success = GetFileInformationByHandleEx(FileHandle2, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);
    ASSERT(65536 == StandardInfo.AllocationSize.QuadPart);

    CloseHandle(FileHandle2);

    AllocationSize.QuadPart = 0;
    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);
    Result = NtCreateFile(&FileHandle2,
        FILE_READ_ATTRIBUTES, &Obja, &Iosb,
        &AllocationSize, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN, 0, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);

    Success = GetFileInformationByHandleEx(FileHandle2, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);
    ASSERT(65536 == StandardInfo.AllocationSize.QuadPart);

    CloseHandle(FileHandle2);
    CloseHandle(FileHandle);

    AllocationSize.QuadPart = 0;
    UnicodePath.Length = (USHORT)wcslen(UnicodePathBuf) * sizeof(WCHAR);
    UnicodePath.MaximumLength = sizeof UnicodePathBuf;
    UnicodePath.Buffer = UnicodePathBuf;
    InitializeObjectAttributes(&Obja, &UnicodePath, 0, DirHandle, 0);
    Result = NtCreateFile(&FileHandle,
        FILE_READ_ATTRIBUTES, &Obja, &Iosb,
        &AllocationSize, FILE_ATTRIBUTE_NORMAL, 0,
        FILE_OPEN, 0, 0, 0);
    ASSERT(STATUS_SUCCESS == Result);

    Success = GetFileInformationByHandleEx(FileHandle, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);
    ASSERT(0 == StandardInfo.AllocationSize.QuadPart);

    CloseHandle(FileHandle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    FileHandle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != FileHandle);
    CloseHandle(FileHandle);

    CloseHandle(DirHandle);

    memfs_stop(memfs);
}

void create_allocation_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_allocation_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        create_allocation_dotest(MemfsDisk, 0, 0);
        create_allocation_dotest(MemfsDisk, 0, 1000);
        create_allocation_dotest(MemfsDisk, 0, INFINITE);
    }
    if (WinFspNetTests)
    {
        create_allocation_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        create_allocation_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
        create_allocation_dotest(MemfsNet, L"\\\\memfs\\share", INFINITE);
    }
}

void create_sd_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    static PWSTR Sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)";
    UINT8 AbsoluteSecurityDescriptorBuf[SECURITY_DESCRIPTOR_MIN_LENGTH];
    PSECURITY_DESCRIPTOR SecurityDescriptor, AbsoluteSecurityDescriptor = AbsoluteSecurityDescriptorBuf;
    PSID Owner;
    PACL Dacl;
    BOOL OwnerDefaulted, DaclDefaulted, DaclPresent;
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
    HANDLE Handle;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, &SecurityAttributes);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = InitializeSecurityDescriptor(AbsoluteSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    ASSERT(Success);
    Success = GetSecurityDescriptorOwner(SecurityDescriptor, &Owner, &OwnerDefaulted);
    ASSERT(Success);
    Success = SetSecurityDescriptorOwner(AbsoluteSecurityDescriptor, Owner, OwnerDefaulted);
    ASSERT(Success);
    Success = GetSecurityDescriptorGroup(SecurityDescriptor, &Owner, &OwnerDefaulted);
    ASSERT(Success);
    Success = SetSecurityDescriptorGroup(AbsoluteSecurityDescriptor, Owner, OwnerDefaulted);
    ASSERT(Success);
    Success = GetSecurityDescriptorDacl(SecurityDescriptor, &DaclPresent, &Dacl, &DaclDefaulted);
    ASSERT(Success);
    Success = SetSecurityDescriptorDacl(AbsoluteSecurityDescriptor, DaclPresent, Dacl, DaclDefaulted);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = AbsoluteSecurityDescriptor;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, &SecurityAttributes);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

void create_sd_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_sd_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_sd_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_sd_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_notraverse_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    static PWSTR Sddl = L"D:P(A;;GRGWSD;;;WD)";
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
    LUID Luid;
    TOKEN_PRIVILEGES Privileges;
    HANDLE Handle, Token;
    BOOLEAN Success;
    WCHAR FilePath[MAX_PATH];

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, &SecurityAttributes);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\dir2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, &SecurityAttributes);
    ASSERT(Success);

    Success = LookupPrivilegeValueW(0, SE_CHANGE_NOTIFY_NAME, &Luid);
    ASSERT(Success);
    Success = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token);
    ASSERT(Success);
    Privileges.PrivilegeCount = 1;
    Privileges.Privileges[0].Attributes = 0;
    Privileges.Privileges[0].Luid = Luid;
    Success = AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, 0, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\dir2\\dir3",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, &SecurityAttributes);
    ASSERT(!Success);
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    Privileges.PrivilegeCount = 1;
    Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    Privileges.Privileges[0].Luid = Luid;
    Success = AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, 0, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\dir2\\dir3",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, &SecurityAttributes);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\dir2\\dir3",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        DELETE, 0, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\dir2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        DELETE, 0, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        DELETE, 0, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

void create_notraverse_test(void)
{
    if (OptNoTraverseToken)
        /* this test needs traverse access privilege in order to work */
        return;

    if (OptShareName)
        /*
         * This test does not work when going through a share!
         * Does this suggest that traverse privilege is always on when going through shares?
         */
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_notraverse_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_notraverse_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_notraverse_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_backup_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    LUID Luid;
    PRIVILEGE_SET RequiredPrivileges;
    TOKEN_PRIVILEGES Privileges;
    HANDLE Token;
    BOOL Success;

    Success = LookupPrivilegeValueW(0, SE_BACKUP_NAME, &Luid);
    ASSERT(Success);
    Success = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token);
    ASSERT(Success);
    Privileges.PrivilegeCount = 1;
    Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    Privileges.Privileges[0].Luid = Luid;
    Success = AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, 0, 0);
    ASSERT(Success);

    RequiredPrivileges.PrivilegeCount = 1;
    RequiredPrivileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
    RequiredPrivileges.Privilege[0].Attributes = 0;
    RequiredPrivileges.Privilege[0].Luid = Luid;
    ASSERT(PrivilegeCheck(Token, &RequiredPrivileges, &Success));

    if (Success)
    {
        FspDebugLog(__FUNCTION__ ": HasBackupPrivilege\n");

        static PWSTR Sddl = L"D:P(A;;SD;;;WD)";
        PSECURITY_DESCRIPTOR SecurityDescriptor;
        SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
        HANDLE Handle;
        WCHAR FilePath[MAX_PATH];

        Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
        ASSERT(Success);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Success = CreateDirectoryW(FilePath, 0);
        ASSERT(Success);

        SecurityAttributes.nLength = sizeof SecurityAttributes;
        SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &SecurityAttributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        Handle = CreateFileW(FilePath,
            GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        Handle = CreateFileW(FilePath,
            GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = 0;
        Privileges.Privileges[0].Luid = Luid;
        Success = AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, 0, 0);
        ASSERT(Success);

        Handle = CreateFileW(FilePath,
            GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        Handle = CreateFileW(FilePath,
            GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        Success = DeleteFileW(FilePath);
        ASSERT(Success);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Success = RemoveDirectoryW(FilePath);
        ASSERT(Success);

        LocalFree(SecurityDescriptor);
    }

    CloseHandle(Token);

    memfs_stop(memfs);
}

void create_backup_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_backup_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_backup_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_backup_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_restore_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    LUID Luid;
    PRIVILEGE_SET RequiredPrivileges;
    TOKEN_PRIVILEGES Privileges;
    HANDLE Token;
    BOOL Success;

    Success = LookupPrivilegeValueW(0, SE_RESTORE_NAME, &Luid);
    ASSERT(Success);
    Success = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token);
    ASSERT(Success);
    Privileges.PrivilegeCount = 1;
    Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    Privileges.Privileges[0].Luid = Luid;
    Success = AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, 0, 0);
    ASSERT(Success);

    RequiredPrivileges.PrivilegeCount = 1;
    RequiredPrivileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
    RequiredPrivileges.Privilege[0].Attributes = 0;
    RequiredPrivileges.Privilege[0].Luid = Luid;
    ASSERT(PrivilegeCheck(Token, &RequiredPrivileges, &Success));

    if (Success)
    {
        FspDebugLog(__FUNCTION__ ": HasRestorePrivilege\n");

        static PWSTR Sddl = L"D:P(A;;SD;;;WD)";
        PSECURITY_DESCRIPTOR SecurityDescriptor;
        SECURITY_ATTRIBUTES SecurityAttributes = { 0 };
        HANDLE DirHandle, Handle;
        WCHAR FilePath[MAX_PATH];

        Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
        ASSERT(Success);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        SecurityAttributes.nLength = sizeof SecurityAttributes;
        SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

        Success = CreateDirectoryW(FilePath, &SecurityAttributes);
        ASSERT(Success);

        DirHandle = CreateFileW(FilePath,
            DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != DirHandle);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        if (!OptNoTraverseToken)
        {
            Handle = CreateFileW(FilePath,
                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
            ASSERT(INVALID_HANDLE_VALUE != Handle);
            CloseHandle(Handle);

            Success = DeleteFileW(FilePath);
            ASSERT(Success);
        }
        else
        {
            Handle = CreateFileW(FilePath,
                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
            ASSERT(INVALID_HANDLE_VALUE == Handle);
            ASSERT(ERROR_ACCESS_DENIED == GetLastError());
        }

        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = 0;
        Privileges.Privileges[0].Luid = Luid;
        Success = AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, 0, 0);
        ASSERT(Success);

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        Handle = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        CloseHandle(DirHandle);

        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        DirHandle = CreateFileW(FilePath,
            DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        ASSERT(INVALID_HANDLE_VALUE == DirHandle);
        ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

        LocalFree(SecurityDescriptor);
    }

    CloseHandle(Token);

    memfs_stop(memfs);
}

void create_restore_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_restore_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_restore_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_restore_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_share_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle1, Handle2;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);
    CloseHandle(Handle1);

    {
        /* share test */

        Handle1 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);

        CloseHandle(Handle1);
        CloseHandle(Handle2);

        Handle1 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);

        Handle1 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());

        CloseHandle(Handle1);

        /* GitHub issue #364 */
        Handle1 = CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);
        Handle2 = CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());
        CloseHandle(Handle1);

#if 0
        /* from winfstest */
        Handle1 = CreateFileW(FilePath, GENERIC_READ, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);
        Handle2 = CreateFileW(FilePath, GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());
        Handle2 = CreateFileW(FilePath, GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);
        Handle2 = CreateFileW(FilePath, DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());
        Handle2 = CreateFileW(FilePath, DELETE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());
        CloseHandle(Handle1);

        /* from winfstest */
        Handle1 = CreateFileW(FilePath, DELETE, FILE_SHARE_DELETE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);
        Handle2 = CreateFileW(FilePath, GENERIC_WRITE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());
        Handle2 = CreateFileW(FilePath, DELETE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_SHARING_VIOLATION == GetLastError());
        Handle2 = CreateFileW(FilePath, DELETE, FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);
        CloseHandle(Handle2);
        CloseHandle(Handle1);
#endif
    }

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);
    CloseHandle(Handle1);

    memfs_stop(memfs);
}

void create_share_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_share_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_share_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_share_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_curdir_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    WCHAR CurrentDirectory[MAX_PATH], FilePath[MAX_PATH];
    BOOL Success;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = GetCurrentDirectoryW(MAX_PATH, CurrentDirectory);
    ASSERT(Success);

    Success = SetCurrentDirectoryW(FilePath);
    ASSERT(Success);

    Success = RealSetCurrentDirectoryW(CurrentDirectory);
    ASSERT(Success);

    memfs_stop(memfs);
}

void create_curdir_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        create_curdir_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        create_curdir_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_curdir_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_namelen_dotest(ULONG Flags, PWSTR Prefix, PWSTR Drive)
{
    void *memfs = memfs_start(Flags);

    WCHAR FilePath[1024];
    PWSTR FilePathBgn, P, EndP;
    DWORD MaxComponentLength;
    HANDLE Handle;
    BOOL Success;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Drive ? Drive : memfs_volumename(memfs));

    Success = GetVolumeInformationW(FilePath,
        0, 0,
        0, &MaxComponentLength, 0,
        0, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    FilePathBgn = FilePath + wcslen(FilePath);

    for (P = FilePathBgn, EndP = P + MaxComponentLength - 1; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + '0';
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    for (P = FilePathBgn, EndP = P + MaxComponentLength; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + '0';
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    for (P = FilePathBgn, EndP = P + MaxComponentLength + 1; EndP > P; P++)
        *P = (P - FilePathBgn) % 10 + '0';
    *P = L'\0';

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_INVALID_NAME == GetLastError());

    memfs_stop(memfs);
}

void create_namelen_test(void)
{
    if (OptShareName || OptMountPoint)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        create_namelen_dotest(-1, DirBuf, DriveBuf);
    }
    if (WinFspDiskTests)
        create_namelen_dotest(MemfsDisk, 0, 0);
#if 0
    /* This test does not work when going through the MUP! */
    if (WinFspNetTests)
        create_namelen_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share");
#endif
}

FSP_FILE_SYSTEM_OPERATION *create_pid_CreateOp;
volatile UINT32 create_pid_Pass, create_pid_Fail;

NTSTATUS create_pid_Create(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (FspFileSystemOperationProcessId() == GetCurrentProcessId())
        InterlockedIncrement(&create_pid_Pass);
    else
        InterlockedIncrement(&create_pid_Fail);
    return create_pid_CreateOp(FileSystem, Request, Response);
}

void create_pid_dotest(ULONG Flags, PWSTR Prefix)
{
    create_pid_Pass = create_pid_Fail = 0;

    void *memfs = memfs_start(Flags);

    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);
    create_pid_CreateOp = FileSystem->Operations[FspFsctlTransactCreateKind];
    FileSystem->Operations[FspFsctlTransactCreateKind] = create_pid_Create;

    HANDLE Handle;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    memfs_stop(memfs);

    if (!(0 < create_pid_Pass && 0 == create_pid_Fail))
        tlib_printf("create_pid_Pass=%u, create_pid_Fail=%u", create_pid_Pass, create_pid_Fail);

    ASSERT(0 < create_pid_Pass);// && 0 == create_pid_Fail);
}

void create_pid_test(void)
{
    if (NtfsTests)
        return;

    if (WinFspDiskTests)
        create_pid_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        create_pid_dotest(MemfsNet, L"\\\\memfs\\share");
}

void create_tests(void)
{
    TEST(create_test);
    TEST(create_fileattr_test);
    TEST(create_readonlydir_test);
    TEST(create_related_test);
    if (!OptFuseExternal)
        TEST(create_allocation_test);
    TEST(create_sd_test);
    if (!OptFuseExternal && !OptNoTraverseToken && !OptShareName)
        TEST(create_notraverse_test);
    TEST(create_backup_test);
    TEST(create_restore_test);
    TEST(create_share_test);
    TEST(create_curdir_test);
    if (!OptShareName && !OptMountPoint)
        TEST(create_namelen_test);
    if (!NtfsTests)
        TEST(create_pid_test);
}
