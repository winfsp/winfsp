/**
 * @file flush-test.c
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

static void flush_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout, DWORD CreateFlags,
    BOOLEAN FlushVolume)
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

    if (FlushVolume)
    {
        WCHAR VolumePath[MAX_PATH];
        HANDLE VolumeHandle;

        StringCbPrintfW(VolumePath, sizeof VolumePath, L"%s%s",
            VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

        VolumeHandle = CreateFileW(VolumePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        if (INVALID_HANDLE_VALUE != VolumeHandle)
        {
            FspDebugLog(__FUNCTION__ ": VolumeHandle=%p\n", VolumeHandle);

            Success = FlushFileBuffers(VolumeHandle);
            ASSERT(Success);

            Success = CloseHandle(VolumeHandle);
            ASSERT(Success);
        }
    }
    else
    {
        Success = FlushFileBuffers(Handle);
        ASSERT(Success);
    }

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

void flush_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        flush_dotest(-1, DriveBuf, DirBuf, 0, 0, FALSE);
        flush_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_WRITE_THROUGH, FALSE);
        flush_dotest(-1, DriveBuf, DirBuf, 0, FILE_FLAG_NO_BUFFERING, FALSE);
    }
    if (WinFspDiskTests)
    {
         flush_dotest(MemfsDisk, 0, 0, INFINITE, 0, FALSE);
         flush_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_WRITE_THROUGH, FALSE);
         flush_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING, FALSE);
    }
    if (WinFspNetTests)
    {
        flush_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0, FALSE);
        flush_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_WRITE_THROUGH, FALSE);
        flush_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING, FALSE);
    }
}

void flush_volume_test(void)
{
    if (WinFspDiskTests)
    {
         flush_dotest(MemfsDisk, 0, 0, INFINITE, 0, TRUE);
         flush_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_WRITE_THROUGH, TRUE);
         flush_dotest(MemfsDisk, 0, 0, INFINITE, FILE_FLAG_NO_BUFFERING, TRUE);
    }
    if (WinFspNetTests)
    {
        flush_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, 0, TRUE);
        flush_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_WRITE_THROUGH, TRUE);
        flush_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", INFINITE, FILE_FLAG_NO_BUFFERING, TRUE);
    }
}

void flush_tests(void)
{
    TEST(flush_test);
    TEST(flush_volume_test);
}
