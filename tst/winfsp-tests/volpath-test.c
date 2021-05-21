/**
 * @file volpath-test.c
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

static void volpath_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

    HANDLE Handle;
    BOOLEAN Success, VolumePathNameSuccess[8];
    WCHAR FilePath[MAX_PATH];
    WCHAR VolumePathName[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[0] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[1] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[2] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);

    ASSERT(VolumePathNameSuccess[0] == VolumePathNameSuccess[1]);
    ASSERT(VolumePathNameSuccess[1] == VolumePathNameSuccess[2]);
}

static void volpath_test(void)
{
    /*
     * GetVolumePathName is not reliable on WinFsp file systems
     * when *not* using the MountManager and therefore disable
     * this test when using a non-MountManager mount point.
     */
    if (!NtfsTests && !OptMountPoint)
        return;

    if (WinFspDiskTests)
        volpath_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        volpath_dotest(MemfsNet, L"\\\\memfs\\share");
}

static void volpath_mount_dotest(ULONG Flags, PWSTR Prefix, PWSTR MountPoint)
{
    void *memfs = memfs_start(Flags);

    NTSTATUS Result;
    HANDLE Handle;
    BOOLEAN Success, VolumePathNameSuccess[8];
    WCHAR FilePath[MAX_PATH];
    WCHAR VolumePathName[MAX_PATH], VolumeName[MAX_PATH];

    Result = FspFileSystemSetMountPoint(MemfsFileSystem(memfs), MountPoint);
    ASSERT(NT_SUCCESS(Result));

    Prefix = FspFileSystemMountPoint(MemfsFileSystem(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[0] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);
    VolumePathNameSuccess[4] = GetVolumeNameForVolumeMountPointW(VolumePathName, VolumeName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[1] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);
    VolumePathNameSuccess[5] = GetVolumeNameForVolumeMountPointW(VolumePathName, VolumeName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[2] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);
    VolumePathNameSuccess[6] = GetVolumeNameForVolumeMountPointW(VolumePathName, VolumeName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    FspFileSystemRemoveMountPoint(MemfsFileSystem(memfs));

    ASSERT(VolumePathNameSuccess[0]);
    ASSERT(VolumePathNameSuccess[1]);
    ASSERT(VolumePathNameSuccess[2]);

    if (MemfsNet != Flags)
    {
        ASSERT(VolumePathNameSuccess[4]);
        ASSERT(VolumePathNameSuccess[5]);
        ASSERT(VolumePathNameSuccess[6]);
    }

    Result = FspFileSystemSetMountPoint(MemfsFileSystem(memfs), MountPoint);
    ASSERT(NT_SUCCESS(Result));

    Prefix = FspFileSystemMountPoint(MemfsFileSystem(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[0] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);
    VolumePathNameSuccess[4] = GetVolumeNameForVolumeMountPointW(VolumePathName, VolumeName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[1] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);
    VolumePathNameSuccess[5] = GetVolumeNameForVolumeMountPointW(VolumePathName, VolumeName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    VolumePathNameSuccess[2] = GetVolumePathNameW(FilePath, VolumePathName, MAX_PATH);
    VolumePathNameSuccess[6] = GetVolumeNameForVolumeMountPointW(VolumePathName, VolumeName, MAX_PATH);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    FspFileSystemRemoveMountPoint(MemfsFileSystem(memfs));

    ASSERT(VolumePathNameSuccess[0]);
    ASSERT(VolumePathNameSuccess[1]);
    ASSERT(VolumePathNameSuccess[2]);

    if (MemfsNet != Flags)
    {
        ASSERT(VolumePathNameSuccess[4]);
        ASSERT(VolumePathNameSuccess[5]);
        ASSERT(VolumePathNameSuccess[6]);
    }

    memfs_stop(memfs);
}

static void volpath_mount_test(void)
{
    /*
     * This test does FspFileSystemSetMountPoint and therefore
     * cannot be used with --external or --mountpoint.
     */
    if (NtfsTests || OptMountPoint)
        return;

    if (WinFspDiskTests)
    {
        WCHAR MountPoint[7];
        DWORD Drives;
        WCHAR Drive;

        MountPoint[0] = L'\\';
        MountPoint[1] = L'\\';
        MountPoint[2] = L'.';
        MountPoint[3] = L'\\';
        MountPoint[4] = L'C';
        MountPoint[5] = L':';
        MountPoint[6] = L'\0';

        Drives = GetLogicalDrives();
        ASSERT(0 != Drives);

        for (Drive = 'Z'; 'A' <= Drive; Drive--)
            if (0 == (Drives & (1 << (Drive - 'A'))))
                break;
        ASSERT('A' <= Drive);

        MountPoint[4] = Drive;

        //volpath_mount_dotest(MemfsDisk, 0, 0);
        volpath_mount_dotest(MemfsDisk, 0, MountPoint);
    }
    if (WinFspNetTests)
    {
        volpath_mount_dotest(MemfsNet, L"\\\\memfs\\share", 0);
    }
}

void volpath_tests(void)
{
    /*
     * GetVolumePathName is not reliable on WinFsp file systems
     * when *not* using the MountManager and therefore disable
     * this test when using a non-MountManager mount point.
     */
    if (!NtfsTests && !OptMountPoint)
        TEST(volpath_test);

    /*
     * This test does FspFileSystemSetMountPoint and therefore
     * cannot be used with --external or --mountpoint.
     */
    if (!NtfsTests && !OptMountPoint)
        TEST(volpath_mount_test);
}
