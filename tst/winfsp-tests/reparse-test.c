/**
 * @file reparse-test.c
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

static void reparse_guid_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    static const GUID reparse_guid =
        { 0x895fc61e, 0x7b91, 0x4677, { 0xba, 0x3e, 0x79, 0x34, 0xed, 0xf2, 0xb7, 0x43 } };
    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    union
    {
        //REPARSE_DATA_BUFFER D;
        REPARSE_GUID_DATA_BUFFER G;
        UINT8 B[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    } ReparseDataBuf;
    DWORD Bytes;
    static const char *datstr = "foobar";

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    ReparseDataBuf.G.ReparseTag = 0x1234;
    ReparseDataBuf.G.ReparseDataLength = (USHORT)strlen(datstr);
    ReparseDataBuf.G.Reserved = 0;
    memcpy(&ReparseDataBuf.G.ReparseGuid, &reparse_guid, sizeof reparse_guid);
    memcpy(ReparseDataBuf.G.GenericReparseBuffer.DataBuffer, datstr, strlen(datstr));

    Success = DeviceIoControl(Handle, FSCTL_SET_REPARSE_POINT,
        &ReparseDataBuf, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE + ReparseDataBuf.G.ReparseDataLength,
        0, 0,
        &Bytes, 0);
    ASSERT(Success);

    Success = DeviceIoControl(Handle, FSCTL_GET_REPARSE_POINT,
        0, 0,
        &ReparseDataBuf, sizeof ReparseDataBuf,
        &Bytes, 0);
    ASSERT(Success);

    ASSERT(ReparseDataBuf.G.ReparseTag == 0x1234);
    ASSERT(ReparseDataBuf.G.ReparseDataLength == strlen(datstr));
    ASSERT(ReparseDataBuf.G.Reserved == 0);
    ASSERT(0 == memcmp(&ReparseDataBuf.G.ReparseGuid, &reparse_guid, sizeof reparse_guid));
    ASSERT(0 == memcmp(ReparseDataBuf.G.GenericReparseBuffer.DataBuffer, datstr, strlen(datstr)));

    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_CANT_ACCESS_FILE == GetLastError());

    Handle = CreateFileW(FilePath,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    ReparseDataBuf.G.ReparseDataLength = 0;

    Success = DeviceIoControl(Handle, FSCTL_DELETE_REPARSE_POINT,
        &ReparseDataBuf, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE + ReparseDataBuf.G.ReparseDataLength,
        0, 0,
        &Bytes, 0);
    ASSERT(Success);

    Success = DeviceIoControl(Handle, FSCTL_GET_REPARSE_POINT,
        0, 0,
        &ReparseDataBuf, sizeof ReparseDataBuf,
        &Bytes, 0);
    ASSERT(!Success);
    ASSERT(ERROR_NOT_A_REPARSE_POINT == GetLastError());

    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);
}

void reparse_guid_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        reparse_guid_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        reparse_guid_dotest(MemfsDisk, 0, 0);
    }
    if (WinFspNetTests)
    {
        reparse_guid_dotest(MemfsNet, L"\\\\memfs\\share", 0);
    }
}

static void reparse_nfs_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    union
    {
        REPARSE_DATA_BUFFER D;
        //REPARSE_GUID_DATA_BUFFER G;
        UINT8 B[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
    } ReparseDataBuf;
    DWORD Bytes;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    ReparseDataBuf.D.ReparseTag = IO_REPARSE_TAG_NFS;
    ReparseDataBuf.D.ReparseDataLength = 16;
    ReparseDataBuf.D.Reserved = 0;
    *(PUINT64)(ReparseDataBuf.D.GenericReparseBuffer.DataBuffer +  0) = 0x524843;/* NFS_SPECFILE_CHR */
    *(PUINT32)(ReparseDataBuf.D.GenericReparseBuffer.DataBuffer +  8) = 0x42; /* major */
    *(PUINT32)(ReparseDataBuf.D.GenericReparseBuffer.DataBuffer + 12) = 0x62; /* minor */

    Success = DeviceIoControl(Handle, FSCTL_SET_REPARSE_POINT,
        &ReparseDataBuf, REPARSE_DATA_BUFFER_HEADER_SIZE + ReparseDataBuf.D.ReparseDataLength,
        0, 0,
        &Bytes, 0);
    ASSERT(Success);

    Success = DeviceIoControl(Handle, FSCTL_GET_REPARSE_POINT,
        0, 0,
        &ReparseDataBuf, sizeof ReparseDataBuf,
        &Bytes, 0);
    ASSERT(Success);

    ASSERT(ReparseDataBuf.D.ReparseTag == IO_REPARSE_TAG_NFS);
    ASSERT(ReparseDataBuf.D.ReparseDataLength == 16);
    ASSERT(ReparseDataBuf.D.Reserved == 0);
    ASSERT(*(PUINT64)(ReparseDataBuf.D.GenericReparseBuffer.DataBuffer +  0) == 0x524843);
    ASSERT(*(PUINT32)(ReparseDataBuf.D.GenericReparseBuffer.DataBuffer +  8) == 0x42);
    ASSERT(*(PUINT32)(ReparseDataBuf.D.GenericReparseBuffer.DataBuffer + 12) == 0x62);

    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_CANT_ACCESS_FILE == GetLastError());

    Handle = CreateFileW(FilePath,
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    if (!OptFuseExternal)
    {
        /* FUSE cannot delete reparse points */

        ReparseDataBuf.D.ReparseDataLength = 0;

        Success = DeviceIoControl(Handle, FSCTL_DELETE_REPARSE_POINT,
            &ReparseDataBuf, REPARSE_DATA_BUFFER_HEADER_SIZE + ReparseDataBuf.D.ReparseDataLength,
            0, 0,
            &Bytes, 0);
        ASSERT(Success);

        Success = DeviceIoControl(Handle, FSCTL_GET_REPARSE_POINT,
            0, 0,
            &ReparseDataBuf, sizeof ReparseDataBuf,
            &Bytes, 0);
        ASSERT(!Success);
        ASSERT(ERROR_NOT_A_REPARSE_POINT == GetLastError());
    }

    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);
}

void reparse_nfs_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        reparse_nfs_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        reparse_nfs_dotest(MemfsDisk, 0, 0);
    }
    if (WinFspNetTests)
    {
        reparse_nfs_dotest(MemfsNet, L"\\\\memfs\\share", 0);
    }
}

static void reparse_symlink_dotest0(ULONG Flags, PWSTR Prefix,
    PWSTR FilePath, PWSTR LinkPath, PWSTR TargetPath)
{
    HANDLE Handle;
    BOOL Success;
    PUINT8 NameInfoBuf[sizeof(FILE_NAME_INFO) + MAX_PATH];
    PFILE_NAME_INFO PNameInfo = (PVOID)NameInfoBuf;

    Success = BestEffortCreateSymbolicLinkW(LinkPath, TargetPath, 0);
    if (Success)
    {
        Handle = CreateFileW(FilePath,
            FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        CloseHandle(Handle);

        Handle = CreateFileW(LinkPath,
            FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            0, OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);

        Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof NameInfoBuf);
        ASSERT(Success);
        if (OptSharePrefixLength)
        {
            memmove(PNameInfo->FileName,
                PNameInfo->FileName + OptSharePrefixLength / sizeof(WCHAR),
                PNameInfo->FileNameLength - OptSharePrefixLength);
            PNameInfo->FileNameLength -= OptSharePrefixLength;
        }
        if (-1 == Flags)
            ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
        else if (0 == Prefix)
            ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0") * sizeof(WCHAR));
        else
            ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
        if (-1 == Flags)
            ASSERT(0 == mywcscmp(FilePath + 6, -1, PNameInfo->FileName, PNameInfo->FileNameLength / sizeof(WCHAR)));
        else if (0 == Prefix)
            ASSERT(0 == mywcscmp(L"\\file0", -1, PNameInfo->FileName, PNameInfo->FileNameLength / sizeof(WCHAR)));
        else
            ASSERT(0 == mywcscmp(FilePath + 1, -1, PNameInfo->FileName, PNameInfo->FileNameLength / sizeof(WCHAR)));

        CloseHandle(Handle);

        Success = DeleteFileW(LinkPath);
        ASSERT(Success);

        Success = DeleteFileW(FilePath);
        ASSERT(Success);
    }
    else
    {
        ASSERT(ERROR_PRIVILEGE_NOT_HELD == GetLastError());
        FspDebugLog(__FUNCTION__ ": need SE_CREATE_SYMBOLIC_LINK_PRIVILEGE\n");
    }
}

static void reparse_symlink_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR FilePath[MAX_PATH], LinkPath[MAX_PATH], TargetPath[MAX_PATH];
    PUINT8 NameInfoBuf[sizeof(FILE_NAME_INFO) + MAX_PATH];
    PFILE_NAME_INFO PNameInfo = (PVOID)NameInfoBuf;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(LinkPath, sizeof LinkPath, L"%s%s\\link0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    if (OptShareName)
        StringCbPrintfW(TargetPath, sizeof TargetPath, L"%s%s%s\\file0",
            OptShareComputer, OptShareName, -1 == Flags ? Prefix + 6 : L"");

    reparse_symlink_dotest0(Flags, Prefix, FilePath, LinkPath,
        OptShareName ? TargetPath : FilePath);

    StringCbPrintfW(TargetPath, sizeof TargetPath, L"%s\\file0",
        -1 == Flags ? Prefix + 6 : L"");
    reparse_symlink_dotest0(Flags, Prefix, FilePath, LinkPath, TargetPath);

    reparse_symlink_dotest0(Flags, Prefix, FilePath, LinkPath, L"file0");

    memfs_stop(memfs);
}

void reparse_symlink_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        reparse_symlink_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        reparse_symlink_dotest(MemfsDisk, 0, 0);
    }
    if (WinFspNetTests)
    {
        reparse_symlink_dotest(MemfsNet, L"\\\\memfs\\share", 0);
    }
}

static BOOL my_mkdir_fn(PWSTR Prefix, void *memfs, PWSTR FileName)
{
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s%s",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), FileName);

    return CreateDirectoryW(FilePath, 0);
}

static BOOL my_rmdir_fn(PWSTR Prefix, void *memfs, PWSTR FileName)
{
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s%s",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), FileName);

    return RemoveDirectoryW(FilePath);
}

static BOOL my_make_fn(PWSTR Prefix, void *memfs, PWSTR FileName)
{
    WCHAR FilePath[MAX_PATH];
    HANDLE Handle;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s%s",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), FileName);

    Handle = CreateFileW(FilePath, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return FALSE;
    CloseHandle(Handle);
    return TRUE;
}

static BOOL my_unlink_fn(PWSTR Prefix, void *memfs, PWSTR FileName)
{
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s%s",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), FileName);

    return DeleteFileW(FilePath);
}

static BOOL my_symlink_fn(ULONG Flags, PWSTR Prefix, void *memfs, PWSTR LinkName, PWSTR FileName,
    DWORD SymlinkFlags)
{
    WCHAR LinkPath[MAX_PATH], FilePath[MAX_PATH];

    StringCbPrintfW(LinkPath, sizeof LinkPath, L"%s%s%s",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), LinkName);
    if (-1 == Flags && L'\\' == FileName[0])
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s",
            Prefix ? Prefix : L"", FileName);
    else
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s",
            FileName);

    return BestEffortCreateSymbolicLinkW(LinkPath,
        -1 == Flags && L'\\' == FileName[0] ? FilePath + 6 : FilePath,
        SymlinkFlags);
}

static BOOL my_namecheck_fn(ULONG Flags, PWSTR Prefix, void *memfs, PWSTR FileName, PWSTR Expected)
{
    WCHAR FilePath[MAX_PATH], ExpectedPath[MAX_PATH];
    HANDLE Handle;
    PUINT8 NameInfoBuf[sizeof(FILE_NAME_INFO) + MAX_PATH];
    PFILE_NAME_INFO PNameInfo = (PVOID)NameInfoBuf;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s%s",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs), FileName);
    StringCbPrintfW(ExpectedPath, sizeof ExpectedPath, L"%s%s",
        Prefix ? Prefix : L"", Expected);

    Handle = CreateFileW(FilePath, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        /*
         * Prior to Windows 11 it used to be the case that NTFS open with FILE_FLAG_BACKUP_SEMANTICS
         * did not care about SYMLINK/SYMLINKD difference!
         *
         * On Windows 11 this no longer appears to be true. In order to keep this test around, we perform
         * an alternative name check in this case.
         */

        if (-1 == Flags && (ERROR_ACCESS_DENIED == GetLastError() || ERROR_DIRECTORY == GetLastError()))
            ; /* Windows 11: if NTFS and appropriate error then ignore */
        else
            return FALSE;

        Handle = CreateFileW(FilePath, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
        if (INVALID_HANDLE_VALUE == Handle)
            return FALSE;

        union
        {
            REPARSE_DATA_BUFFER D;
            UINT8 B[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
        } ReparseDataBuf;
        DWORD Bytes;
        BOOL Success;

        Success = DeviceIoControl(Handle, FSCTL_GET_REPARSE_POINT,
            0, 0,
            &ReparseDataBuf, sizeof ReparseDataBuf,
            &Bytes, 0);
        if (Success)
        {
            Success = Success &&
                ReparseDataBuf.D.ReparseTag == IO_REPARSE_TAG_SYMLINK;
            Success = Success &&
                ReparseDataBuf.D.SymbolicLinkReparseBuffer.SubstituteNameLength ==
                    wcslen(ExpectedPath + 6) * sizeof(WCHAR);
            Success = Success &&
                0 == mywcscmp(
                    ExpectedPath + 6,
                    -1,
                    ReparseDataBuf.D.SymbolicLinkReparseBuffer.PathBuffer +
                        ReparseDataBuf.D.SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR),
                    ReparseDataBuf.D.SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR));
        }

        CloseHandle(Handle);
        return Success;
    }

    if (GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof NameInfoBuf))
    {
        if (OptSharePrefixLength)
        {
            memmove(PNameInfo->FileName,
                PNameInfo->FileName + OptSharePrefixLength / sizeof(WCHAR),
                PNameInfo->FileNameLength - OptSharePrefixLength);
            PNameInfo->FileNameLength -= OptSharePrefixLength;
        }
        if (-1 == Flags)
            ASSERT(PNameInfo->FileNameLength == wcslen(ExpectedPath + 6) * sizeof(WCHAR));
        else if (0 == Prefix)
            ASSERT(PNameInfo->FileNameLength == wcslen(ExpectedPath) * sizeof(WCHAR));
        else
            ASSERT(PNameInfo->FileNameLength == wcslen(ExpectedPath + 1) * sizeof(WCHAR));
        if (-1 == Flags)
            ASSERT(0 == mywcscmp(ExpectedPath + 6, -1, PNameInfo->FileName, PNameInfo->FileNameLength / sizeof(WCHAR)));
        else if (0 == Prefix)
            ASSERT(0 == mywcscmp(ExpectedPath, -1, PNameInfo->FileName, PNameInfo->FileNameLength / sizeof(WCHAR)));
        else
            ASSERT(0 == mywcscmp(ExpectedPath + 1, -1, PNameInfo->FileName, PNameInfo->FileNameLength / sizeof(WCHAR)));
    }

    CloseHandle(Handle);
    return TRUE;
}

#define my_mkdir(FileName)              ASSERT(my_mkdir_fn(Prefix, memfs, FileName))
#define my_rmdir(FileName)              ASSERT(my_rmdir_fn(Prefix, memfs, FileName))
#define my_make(FileName)               ASSERT(my_make_fn(Prefix, memfs, FileName))
#define my_unlink(FileName)             ASSERT(my_unlink_fn(Prefix, memfs, FileName))
#define my_symlinkd(LinkName, FileName) ASSERT(my_symlink_fn(Flags, Prefix, memfs, LinkName, FileName,\
    SYMBOLIC_LINK_FLAG_DIRECTORY))
#define my_symlink(LinkName, FileName)  ASSERT(my_symlink_fn(Flags, Prefix, memfs, LinkName, FileName, 0))
#define my_namecheck(FileName, Expected)ASSERT(my_namecheck_fn(Flags, Prefix, memfs, FileName, Expected))
#define my_failcheck(FileName)          ASSERT(!my_namecheck_fn(Flags, Prefix, memfs, FileName, L""))

#define my_symlink_noassert(LinkName, FileName)\
    my_symlink_fn(Flags, Prefix, memfs, LinkName, FileName, 0)

static void reparse_symlink_relative_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    if (my_symlink_noassert(L"\\l0", L"NON-EXISTANT"))
        my_unlink(L"\\l0");
    else
    {
        ASSERT(ERROR_PRIVILEGE_NOT_HELD == GetLastError());
        FspDebugLog(__FUNCTION__ ": need SE_CREATE_SYMBOLIC_LINK_PRIVILEGE\n");
        goto exit;
    }

    my_mkdir(L"\\1");
    my_mkdir(L"\\1\\1.1");
    my_make(L"\\1\\1.2");
    my_make(L"\\1\\1.3");
    my_make(L"\\1\\1.1\\1.1.1");
    my_mkdir(L"\\2");
    my_make(L"\\2\\2.1");

    my_symlink(L"\\l0", L"NON-EXISTANT");
    my_symlink(L"\\loop", L"loop");
    my_symlink(L"\\lf", L"\\1");
    my_symlinkd(L"\\ld", L"\\1\\1.1\\1.1.1");
    my_symlink(L"\\1\\1.1\\l1.1.1", L"1.1.1");
    my_symlinkd(L"\\2\\l1", L"..\\1\\.");
    my_symlinkd(L"\\1\\l2", L"..\\.\\2");
    my_symlinkd(L"\\2\\a1", L"\\1");
    my_symlinkd(L"\\1\\a2", L"\\2");

    my_failcheck(L"\\l0");
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());
    my_failcheck(L"\\loop");
    ASSERT(ERROR_CANT_RESOLVE_FILENAME == GetLastError());

    /*
     * NTFS open with FILE_FLAG_BACKUP_SEMANTICS does not care about SYMLINK/SYMLINKD difference!
     *
     * UPDATE: Appears to no longer be true on Windows 11!
     */
    my_namecheck(L"\\lf", L"\\1");
    my_namecheck(L"\\ld", L"\\1\\1.1\\1.1.1");

    my_namecheck(L"\\1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");
    my_namecheck(L"\\2\\l1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");
    my_namecheck(L"\\1\\l2\\l1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");
    my_namecheck(L"\\2\\a1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");
    my_namecheck(L"\\2\\l1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");
    my_namecheck(L"\\1\\a2\\l1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");
    my_namecheck(L"\\1\\a2\\a1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");
    my_namecheck(L"\\1\\a2\\a1\\l2\\l1\\1.1\\l1.1.1", L"\\1\\1.1\\1.1.1");

    my_rmdir(L"\\ld");
    my_unlink(L"\\lf");
    my_rmdir(L"\\1\\a2");
    my_rmdir(L"\\2\\a1");
    my_rmdir(L"\\1\\l2");
    my_rmdir(L"\\2\\l1");
    my_unlink(L"\\1\\1.1\\l1.1.1");
    my_unlink(L"\\loop");
    my_unlink(L"\\l0");

    my_unlink(L"\\2\\2.1");
    my_rmdir(L"\\2");
    my_unlink(L"\\1\\1.1\\1.1.1");
    my_unlink(L"\\1\\1.3");
    my_unlink(L"\\1\\1.2");
    my_rmdir(L"\\1\\1.1");
    my_rmdir(L"\\1");

exit:
    memfs_stop(memfs);
}

void reparse_symlink_relative_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        reparse_symlink_relative_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        reparse_symlink_relative_dotest(MemfsDisk, 0, 0);
    }
    if (WinFspNetTests)
    {
        reparse_symlink_relative_dotest(MemfsNet, L"\\\\memfs\\share", 0);
    }
}

void reparse_tests(void)
{
    if (!OptFuseExternal)
        TEST(reparse_guid_test);
    TEST(reparse_nfs_test);
    TEST(reparse_symlink_test);
    TEST(reparse_symlink_relative_test);
}
