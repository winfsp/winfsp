/**
 * @file rdwr-test.c
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
#include <time.h>
#include <VersionHelpers.h>
#include "memfs.h"

#include "winfsp-tests.h"

static void rdwr_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PVOID AllocBuffer[2], Buffer[2];
    ULONG AllocBufferSize;
    DWORD BytesTransferred;
    DWORD FilePointer;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);
    AllocBufferSize = 16 * SystemInfo.dwPageSize;

    AllocBuffer[0] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    AllocBuffer[1] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    ASSERT(0 != AllocBuffer[0] && 0 != AllocBuffer[1]);

    srand((unsigned)time(0));
    for (PUINT8 Bgn = AllocBuffer[0], End = Bgn + AllocBufferSize; End > Bgn; Bgn++)
        *Bgn = rand();

    Buffer[0] = (PVOID)((PUINT8)AllocBuffer[0] + BytesPerSector);
    Buffer[1] = (PVOID)((PUINT8)AllocBuffer[1] + BytesPerSector);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | CreateFlags, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 3 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(3 * BytesPerSector == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(0 == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Buffer[0] = AllocBuffer[0];
    Buffer[1] = AllocBuffer[0];

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | CreateFlags | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    _aligned_free(AllocBuffer[0]);
    _aligned_free(AllocBuffer[1]);

    memfs_stop(memfs);
}

static void rdwr_append_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PVOID AllocBuffer[2], Buffer[2];
    ULONG AllocBufferSize;
    DWORD BytesTransferred;
    DWORD FilePointer;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);
    AllocBufferSize = 16 * SystemInfo.dwPageSize;

    AllocBuffer[0] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    AllocBuffer[1] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    ASSERT(0 != AllocBuffer[0] && 0 != AllocBuffer[1]);

    srand((unsigned)time(0));
    for (PUINT8 Bgn = AllocBuffer[0], End = Bgn + AllocBufferSize; End > Bgn; Bgn++)
        *Bgn = rand();

    Buffer[0] = (PVOID)((PUINT8)AllocBuffer[0] + BytesPerSector);
    Buffer[1] = (PVOID)((PUINT8)AllocBuffer[1] + BytesPerSector);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        FILE_APPEND_DATA, FILE_SHARE_READ, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | CreateFlags, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);

    Success = WriteFile(Handle, (PUINT8)Buffer[0] + BytesPerSector, BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | CreateFlags | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FilePointer = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(2 * BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    _aligned_free(AllocBuffer[0]);
    _aligned_free(AllocBuffer[1]);

    memfs_stop(memfs);
}

static void rdwr_overlapped_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PVOID AllocBuffer[2], Buffer[2];
    ULONG AllocBufferSize;
    DWORD BytesTransferred;
    OVERLAPPED Overlapped;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);
    AllocBufferSize = 16 * SystemInfo.dwPageSize;

    AllocBuffer[0] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    AllocBuffer[1] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    ASSERT(0 != AllocBuffer[0] && 0 != AllocBuffer[1]);

    srand((unsigned)time(0));
    for (PUINT8 Bgn = AllocBuffer[0], End = Bgn + AllocBufferSize; End > Bgn; Bgn++)
        *Bgn = rand();

    Buffer[0] = (PVOID)((PUINT8)AllocBuffer[0] + BytesPerSector);
    Buffer[1] = (PVOID)((PUINT8)AllocBuffer[1] + BytesPerSector);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    memset(&Overlapped, 0, sizeof Overlapped);
    Overlapped.hEvent = CreateEvent(0, TRUE, FALSE, 0);
    ASSERT(0 != Overlapped.hEvent);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | CreateFlags | FILE_FLAG_OVERLAPPED,
        0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);

    Overlapped.Offset = 2 * BytesPerSector;
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 2 * BytesPerSector;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 2 * BytesPerSector;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 3 * BytesPerSector;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError() || ERROR_HANDLE_EOF == GetLastError());
    if (ERROR_HANDLE_EOF != GetLastError())
    {
        Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
        ASSERT(!Success && ERROR_HANDLE_EOF == GetLastError());
    }
    ASSERT(0 == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Buffer[0] = AllocBuffer[0];
    Buffer[1] = AllocBuffer[0];

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | CreateFlags | FILE_FLAG_OVERLAPPED | FILE_FLAG_DELETE_ON_CLOSE,
        0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Overlapped.Offset = 0;
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle, Buffer[1], 2 * SystemInfo.dwPageSize + BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);
    ASSERT(2 * SystemInfo.dwPageSize + BytesPerSector == BytesTransferred);
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    Success = CloseHandle(Overlapped.hEvent);
    ASSERT(Success);

    _aligned_free(AllocBuffer[0]);
    _aligned_free(AllocBuffer[1]);

    memfs_stop(memfs);
}

static void rdwr_mmap_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags,
    BOOLEAN EarlyClose)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, Mapping;
    PUINT8 MappedView;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PUINT8 Buffer[2];
    DWORD BytesTransferred;
    DWORD FilePointer;
    unsigned seed = (unsigned)time(0);

    GetSystemInfo(&SystemInfo);

    DWORD FileSize0 = 2 * SystemInfo.dwAllocationGranularity;
    DWORD FileSize1 = 100;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Buffer[0] = _aligned_malloc(BytesPerSector, BytesPerSector);
    Buffer[1] = _aligned_malloc(BytesPerSector, BytesPerSector);
    ASSERT(0 != Buffer[0] && 0 != Buffer[1]);

    for (PUINT8 Bgn = Buffer[0], End = Bgn + BytesPerSector; End > Bgn; Bgn++)
        *Bgn = (Bgn - Buffer[0]) & 0xff;

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | CreateFlags, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Mapping = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, FileSize0 + FileSize1, 0);
    ASSERT(0 != Mapping);

    if (EarlyClose)
    {
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    MappedView = MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView);

    srand(seed);
    for (PUINT8 Bgn = MappedView + FileSize1 / 2, End = Bgn + FileSize0; End > Bgn; Bgn++)
        *Bgn = rand() & 0xff;

    Success = UnmapViewOfFile(MappedView);
    ASSERT(Success);

    MappedView = MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView);

    srand(seed);
    for (PUINT8 Bgn = MappedView + FileSize1 / 2, End = Bgn + FileSize0; End > Bgn; Bgn++)
        ASSERT(*Bgn == (rand() & 0xff));

    Success = UnmapViewOfFile(MappedView);
    ASSERT(Success);

    Success = CloseHandle(Mapping);
    ASSERT(Success);

    if (!EarlyClose)
    {
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | CreateFlags, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Mapping = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, FileSize0 + FileSize1, 0);
    ASSERT(0 != Mapping);

    if (EarlyClose)
    {
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    MappedView = MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView);

    srand(seed);
    for (PUINT8 Bgn = MappedView + FileSize1 / 2, End = Bgn + FileSize0; End > Bgn; Bgn++)
        ASSERT(*Bgn == (rand() & 0xff));

    if (!EarlyClose)
    {
        FilePointer = SetFilePointer(Handle, FileSize0 / 2, 0, FILE_BEGIN);
        ASSERT(FileSize0 / 2 == FilePointer);
        Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, 0);
        ASSERT(Success);
        ASSERT(BytesPerSector == BytesTransferred);
        ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
    }

    Success = UnmapViewOfFile(MappedView);
    ASSERT(Success);

    Success = CloseHandle(Mapping);
    ASSERT(Success);

    if (!EarlyClose)
    {
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | CreateFlags, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Mapping = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, FileSize0 + FileSize1, 0);
    ASSERT(0 != Mapping);

    if (EarlyClose)
    {
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    MappedView = MapViewOfFile(Mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView);

    if (!EarlyClose)
    {
        FilePointer = SetFilePointer(Handle, FileSize0 / 2, 0, FILE_BEGIN);
        ASSERT(FileSize0 / 2 == FilePointer);
        memset(Buffer[1], 0, BytesPerSector);
        Success = ReadFile(Handle, Buffer[1], BytesPerSector, &BytesTransferred, 0);
        ASSERT(Success);
        ASSERT(BytesPerSector == BytesTransferred);
        ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle, 0, 0, FILE_CURRENT));
        ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));
    }

    srand(seed);
    for (PUINT8 Bgn = MappedView + FileSize1 / 2, End = MappedView + FileSize0 / 2; End > Bgn; Bgn++)
        ASSERT(*Bgn == (rand() & 0xff));
    if (!EarlyClose)
        ASSERT(0 == memcmp(Buffer[0], MappedView + FileSize0 / 2, BytesPerSector));
    for (size_t i = 0; BytesPerSector > i; i++)
        rand();
    for (PUINT8 Bgn = MappedView + FileSize0 / 2 + BytesPerSector, End = MappedView + FileSize1 / 2 + FileSize0;
        End > Bgn; Bgn++)
        ASSERT(*Bgn == (rand() & 0xff));

    Success = UnmapViewOfFile(MappedView);
    ASSERT(Success);

    Success = CloseHandle(Mapping);
    ASSERT(Success);

    if (!EarlyClose)
    {
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    _aligned_free(Buffer[0]);
    _aligned_free(Buffer[1]);

    memfs_stop(memfs);
}

static void rdwr_mixed_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle0, Handle1;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    PVOID AllocBuffer[2], Buffer[2];
    ULONG AllocBufferSize;
    DWORD BytesTransferred;
    DWORD FilePointer;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);
    AllocBufferSize = 16 * SystemInfo.dwPageSize;

    AllocBuffer[0] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    AllocBuffer[1] = _aligned_malloc(AllocBufferSize, SystemInfo.dwPageSize);
    ASSERT(0 != AllocBuffer[0] && 0 != AllocBuffer[1]);

    srand((unsigned)time(0));
    for (PUINT8 Bgn = AllocBuffer[0], End = Bgn + AllocBufferSize; End > Bgn; Bgn++)
        *Bgn = rand();

    Buffer[0] = (PVOID)((PUINT8)AllocBuffer[0] + BytesPerSector);
    Buffer[1] = (PVOID)((PUINT8)AllocBuffer[1] + BytesPerSector);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle0 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle0);

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle0);

    FilePointer = SetFilePointer(Handle0, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle0, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle0, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle1, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    Success = WriteFile(Handle1, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle1, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle0, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle0, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle0, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    FilePointer = SetFilePointer(Handle1, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle1, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle1, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle0);
    ASSERT(Success);

    Success = CloseHandle(Handle1);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Handle0 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle0);

    FilePointer = SetFilePointer(Handle0, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    Success = WriteFile(Handle0, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle0, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle0, 2 * BytesPerSector, 0, FILE_BEGIN);
    ASSERT(2 * BytesPerSector == FilePointer);
    Success = WriteFile(Handle0, Buffer[0], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle0, 0, 0, FILE_CURRENT));

    FilePointer = SetFilePointer(Handle0, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle0, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle0, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle0);
    ASSERT(Success);

    Handle1 = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle0);

    FilePointer = SetFilePointer(Handle1, 0, 0, FILE_BEGIN);
    ASSERT(0 == FilePointer);
    memset(AllocBuffer[1], 0, AllocBufferSize);
    Success = ReadFile(Handle1, Buffer[1], BytesPerSector, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(BytesPerSector == BytesTransferred);
    ASSERT(FilePointer + BytesTransferred == SetFilePointer(Handle1, 0, 0, FILE_CURRENT));
    ASSERT(0 == memcmp(Buffer[0], Buffer[1], BytesTransferred));

    Success = CloseHandle(Handle1);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    _aligned_free(AllocBuffer[0]);
    _aligned_free(AllocBuffer[1]);

    memfs_stop(memfs);
}

void rdwr_noncached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspDiskTests)
    {
        rdwr_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspNetTests)
    {
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING);
    }
}

/*
 * According to NtCreateFile documentation the FILE_NO_INTERMEDIATE_BUFFERING flag
 * "is incompatible with the DesiredAccess FILE_APPEND_DATA flag".
 */
#if 0
void rdwr_noncached_append_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_append_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspDiskTests)
    {
        rdwr_append_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_append_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspNetTests)
    {
        rdwr_append_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_append_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING);
    }
}
#endif

void rdwr_noncached_overlapped_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_overlapped_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspDiskTests)
    {
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspNetTests)
    {
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING);
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING);
    }
}

void rdwr_cached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_dotest(-1, DriveBuf, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_dotest(MemfsDisk, 0, 0, 1000, 0);
        rdwr_dotest(MemfsDisk, 0, 0, INFINITE, 0);
    }
    if (WinFspNetTests)
    {
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0);
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0);
    }
}

void rdwr_cached_append_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_append_dotest(-1, DriveBuf, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_append_dotest(MemfsDisk, 0, 0, 1000, 0);
        rdwr_append_dotest(MemfsDisk, 0, 0, INFINITE, 0);
    }
    if (WinFspNetTests)
    {
        rdwr_append_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0);
        rdwr_append_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0);
    }
}

void rdwr_cached_overlapped_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_overlapped_dotest(-1, DriveBuf, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, 1000, 0);
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, INFINITE, 0);
    }
    if (WinFspNetTests)
    {
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0);
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0);
    }
}

void rdwr_writethru_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_WRITE_THROUGH);
    }
    if (WinFspDiskTests)
    {
        rdwr_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_WRITE_THROUGH);
        rdwr_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_WRITE_THROUGH);
    }
    if (WinFspNetTests)
    {
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_WRITE_THROUGH);
        rdwr_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_WRITE_THROUGH);
    }
}

void rdwr_writethru_append_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_append_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_WRITE_THROUGH);
    }
    if (WinFspDiskTests)
    {
        rdwr_append_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_WRITE_THROUGH);
        rdwr_append_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_WRITE_THROUGH);
    }
    if (WinFspNetTests)
    {
        rdwr_append_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_WRITE_THROUGH);
        rdwr_append_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_WRITE_THROUGH);
    }
}

void rdwr_writethru_overlapped_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_overlapped_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_WRITE_THROUGH);
    }
    if (WinFspDiskTests)
    {
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_WRITE_THROUGH);
        rdwr_overlapped_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_WRITE_THROUGH);
    }
    if (WinFspNetTests)
    {
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_WRITE_THROUGH);
        rdwr_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_WRITE_THROUGH);
    }
}

void rdwr_mmap_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_mmap_dotest(-1, DriveBuf, DirBuf, 0, 0, FALSE);
        if (!OptShareTarget)
            /*
             * This test fails under Win8 (version 6.2 Build 9200)
             * See: http://www.osronline.com/showthread.cfm?link=279909
             */
            rdwr_mmap_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING, FALSE);
        rdwr_mmap_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_WRITE_THROUGH, FALSE);
        rdwr_mmap_dotest(-1, DriveBuf, DirBuf, 0, 0, TRUE);
        if (!OptShareTarget)
            /*
             * This test fails under Win8 (version 6.2 Build 9200)
             * See: http://www.osronline.com/showthread.cfm?link=279909
             */
            rdwr_mmap_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING, TRUE);
        rdwr_mmap_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_WRITE_THROUGH, TRUE);
    }
    if (WinFspDiskTests)
    {
        /*
         * WinFsp does not currently provide coherency between mmap'ed I/O and ReadFile/WriteFile
         * before Windows 7 in the following cases:
         *   - FileInfoTimeout != INFINITE
         *   - CreateFlags & FILE_FLAG_NO_BUFFERING
         *
         * In Windows 7 and above the new DDI CcCoherencyFlushAndPurgeCache allows us to provide
         * coherency in those cases.
         */

        if (IsWindows7OrGreater())
        {
            rdwr_mmap_dotest(MemfsDisk, 0, 0, 1000, 0, FALSE);
            rdwr_mmap_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING, FALSE);
            rdwr_mmap_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_WRITE_THROUGH, FALSE);
        }
        rdwr_mmap_dotest(MemfsDisk, 0, 0, 1000, 0, TRUE);
        rdwr_mmap_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING, TRUE);
        rdwr_mmap_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_WRITE_THROUGH, TRUE);

        rdwr_mmap_dotest(MemfsDisk, 0, 0, INFINITE, 0, FALSE);
        if (IsWindows7OrGreater())
            rdwr_mmap_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING, FALSE);
        rdwr_mmap_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_WRITE_THROUGH, FALSE);
        rdwr_mmap_dotest(MemfsDisk, 0, 0, INFINITE, 0, TRUE);
        rdwr_mmap_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING, TRUE);
        rdwr_mmap_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_WRITE_THROUGH, TRUE);
    }
    if (WinFspNetTests)
    {
        /*
         * WinFsp does not currently provide coherency between mmap'ed I/O and ReadFile/WriteFile
         * before Windows 7 in the following cases:
         *   - FileInfoTimeout != INFINITE
         *   - CreateFlags & FILE_FLAG_NO_BUFFERING
         *
         * In Windows 7 and above the new DDI CcCoherencyFlushAndPurgeCache allows us to provide
         * coherency in those cases.
         */

        if (IsWindows7OrGreater())
        {
            rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0, FALSE);
            rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING, FALSE);
            rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_WRITE_THROUGH, FALSE);
        }
        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0, TRUE);
        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING, TRUE);
        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_WRITE_THROUGH, TRUE);

        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0, FALSE);
        if (IsWindows7OrGreater())
            rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING, FALSE);
        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_WRITE_THROUGH, FALSE);
        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0, TRUE);
        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING, TRUE);
        rdwr_mmap_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_WRITE_THROUGH, TRUE);
    }
}

void rdwr_mixed_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rdwr_mixed_dotest(-1, DriveBuf, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_mixed_dotest(MemfsDisk, 0, 0, 1000);
        rdwr_mixed_dotest(MemfsDisk, 0, 0, INFINITE);
    }
    if (WinFspNetTests)
    {
        rdwr_mixed_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000);
        rdwr_mixed_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE);
    }
}

void rdwr_tests(void)
{
    TEST(rdwr_noncached_test);
#if 0
    /*
     * According to NtCreateFile documentation the FILE_NO_INTERMEDIATE_BUFFERING flag
     * "is incompatible with the DesiredAccess FILE_APPEND_DATA flag".
     */
    TEST(rdwr_noncached_append_test);
#endif
    TEST(rdwr_noncached_overlapped_test);
    TEST(rdwr_cached_test);
    TEST(rdwr_cached_append_test);
    TEST(rdwr_cached_overlapped_test);
    TEST(rdwr_writethru_test);
    TEST(rdwr_writethru_append_test);
    TEST(rdwr_writethru_overlapped_test);
    TEST(rdwr_mmap_test);
    TEST(rdwr_mixed_test);
}
