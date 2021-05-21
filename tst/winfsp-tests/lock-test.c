/**
 * @file lock-test.c
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

static void lock_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags)
{
    /* this is not a true locking test since you need 2 processes to really test locks */

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

    Success = LockFile(Handle, BytesPerSector / 2, 0, BytesPerSector, 0);
    ASSERT(Success);

    Success = LockFile(Handle, BytesPerSector + BytesPerSector / 2, 0, BytesPerSector, 0);
    ASSERT(Success);

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

    Success = UnlockFile(Handle, 0, 0, BytesPerSector, 0);
    ASSERT(!Success && ERROR_NOT_LOCKED == GetLastError());

    Success = UnlockFile(Handle, BytesPerSector / 2, 0, BytesPerSector, 0);
    ASSERT(Success);

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

static void lock_overlapped_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags)
{
    /* this is not a true locking test since you need 2 processes to really test locks */

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

    Overlapped.Offset = BytesPerSector / 2;
    Success = LockFileEx(Handle, 0, 0, BytesPerSector, 0, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);

    Overlapped.Offset = BytesPerSector + BytesPerSector / 2;
    Success = LockFileEx(Handle, 0 | LOCKFILE_FAIL_IMMEDIATELY, 0,
        BytesPerSector, 0, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);

    Overlapped.Offset = BytesPerSector + BytesPerSector / 2;
    Success = LockFileEx(Handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0,
        BytesPerSector, 0, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError() || ERROR_LOCK_VIOLATION == GetLastError());
    if (ERROR_LOCK_VIOLATION != GetLastError())
    {
        Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
        ASSERT(!Success && ERROR_LOCK_VIOLATION == GetLastError());
    }

    Overlapped.Offset = 0;
    Success = WriteFile(Handle, Buffer[0], BytesPerSector, &BytesTransferred, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError() || ERROR_LOCK_VIOLATION == GetLastError());
    if (ERROR_LOCK_VIOLATION != GetLastError())
    {
        Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
        ASSERT(!Success && ERROR_LOCK_VIOLATION == GetLastError());
    }

    Overlapped.Offset = BytesPerSector / 2;
    Success = UnlockFileEx(Handle, 0, BytesPerSector, 0, &Overlapped);
    ASSERT(Success);

    Overlapped.Offset = BytesPerSector + BytesPerSector / 2;
    Success = UnlockFileEx(Handle, 0, BytesPerSector, 0, &Overlapped);
    ASSERT(Success);

    Overlapped.Offset = BytesPerSector / 2;
    Success = LockFileEx(Handle, LOCKFILE_EXCLUSIVE_LOCK, 0, BytesPerSector, 0, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);

    Overlapped.Offset = BytesPerSector + BytesPerSector / 2;
    Success = LockFileEx(Handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0,
        BytesPerSector, 0, &Overlapped);
    ASSERT(Success || ERROR_IO_PENDING == GetLastError());
    Success = GetOverlappedResult(Handle, &Overlapped, &BytesTransferred, TRUE);
    ASSERT(Success);

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

    Overlapped.Offset = 0;
    Success = UnlockFileEx(Handle, 0, BytesPerSector, 0, &Overlapped);
    ASSERT(!Success && ERROR_NOT_LOCKED == GetLastError());

    Overlapped.Offset = BytesPerSector / 2;
    Success = UnlockFileEx(Handle, 0, BytesPerSector, 0, &Overlapped);
    ASSERT(Success);

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

void lock_noncached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        lock_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspDiskTests)
    {
        lock_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING);
        lock_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspNetTests)
    {
        lock_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING);
        lock_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING);
    }
}

void lock_noncached_overlapped_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        lock_overlapped_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspDiskTests)
    {
        lock_overlapped_dotest(MemfsDisk, 0, 0, 1000, FILE_FLAG_NO_BUFFERING);
        lock_overlapped_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING);
    }
    if (WinFspNetTests)
    {
        lock_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, FILE_FLAG_NO_BUFFERING);
        lock_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING);
    }
}

void lock_cached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        lock_dotest(-1, DriveBuf, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        lock_dotest(MemfsDisk, 0, 0, 1000, 0);
        lock_dotest(MemfsDisk, 0, 0, INFINITE, 0);
    }
    if (WinFspNetTests)
    {
        lock_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0);
        lock_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0);
    }
}

void lock_cached_overlapped_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        lock_overlapped_dotest(-1, DriveBuf, DirBuf, 0, 0);
    }
    if (WinFspDiskTests)
    {
        lock_overlapped_dotest(MemfsDisk, 0, 0, 1000, 0);
        lock_overlapped_dotest(MemfsDisk, 0, 0, INFINITE, 0);
    }
    if (WinFspNetTests)
    {
        lock_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000, 0);
        lock_overlapped_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0);
    }
}

void lock_tests(void)
{
    TEST(lock_noncached_test);
    TEST(lock_noncached_overlapped_test);
    TEST(lock_cached_test);
    TEST(lock_cached_overlapped_test);
}
