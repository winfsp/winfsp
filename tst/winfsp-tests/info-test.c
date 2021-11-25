/**
 * @file info-test.c
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
#include <time.h>
#include "memfs.h"

#include "winfsp-tests.h"

void getfileattr_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    DWORD FileAttributes;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT(INVALID_FILE_ATTRIBUTES != FileAttributes);

    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    /* create directory with SYNCHRONIZE|DELETE|FILE_READ_ATTRIBUTES|FILE_TRAVERSE|FILE_ADD_FILE access */
    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;0x001100a2;;;SY)(A;;0x001100a2;;;BA)(A;;0x001100a2;;;WD)", SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);
    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
    Success = CreateDirectoryW(Dir1Path, &SecurityAttributes);
    ASSERT(Success);
    LocalFree(SecurityDescriptor);

    /* create file with DELETE access only */
    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;SD;;;SY)(A;;SD;;;BA)(A;;SD;;;WD)", SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);
    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    LocalFree(SecurityDescriptor);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT(INVALID_FILE_ATTRIBUTES == FileAttributes);
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir2\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    /* create directory with SYNCHRONIZE|DELETE|FILE_READ_ATTRIBUTES|FILE_TRAVERSE|FILE_ADD_FILE|FILE_LIST_DIRECTORY access */
    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;0x001100a3;;;SY)(A;;0x001100a3;;;BA)(A;;0x001100a3;;;WD)", SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);
    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
    Success = CreateDirectoryW(Dir1Path, &SecurityAttributes);
    ASSERT(Success);
    LocalFree(SecurityDescriptor);

    /* create file with DELETE access only */
    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;SD;;;SY)(A;;SD;;;BA)(A;;SD;;;WD)", SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);
    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    LocalFree(SecurityDescriptor);

    FileAttributes = GetFileAttributesW(FilePath);
    ASSERT(INVALID_FILE_ATTRIBUTES != FileAttributes);

    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    memfs_stop(memfs);
}

void getfileattr_test(void)
{
    if (OptShareName)
        /* why does this fail with shares? */
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        getfileattr_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        getfileattr_dotest(MemfsDisk, 0, 0);
        getfileattr_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        getfileattr_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        getfileattr_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void getfileinfo_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;
    FILE_BASIC_INFO BasicInfo;
    FILE_STANDARD_INFO StandardInfo;
    PUINT8 NameInfoBuf[sizeof(FILE_NAME_INFO) + MAX_PATH];
    PFILE_NAME_INFO PNameInfo = (PVOID)NameInfoBuf;
    BY_HANDLE_FILE_INFORMATION FileInfo;
    FILETIME FileTime;
    LONGLONG TimeLo, TimeHi;

    GetSystemTimeAsFileTime(&FileTime);
    TimeLo = ((PLARGE_INTEGER)&FileTime)->QuadPart;
    TimeHi = TimeLo + 10000 * 10000/* 10 seconds */;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetFileInformationByHandleEx(Handle, FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == AttributeTagInfo.FileAttributes);
    ASSERT(0 == AttributeTagInfo.ReparseTag);

    Success = GetFileInformationByHandleEx(Handle, FileBasicInfo, &BasicInfo, sizeof BasicInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == BasicInfo.FileAttributes);
    if (-1 != Flags)
        ASSERT(
            TimeLo <= BasicInfo.CreationTime.QuadPart &&
            TimeHi >  BasicInfo.CreationTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.LastAccessTime.QuadPart &&
        TimeHi >  BasicInfo.LastAccessTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.LastWriteTime.QuadPart &&
        TimeHi >  BasicInfo.LastWriteTime.QuadPart);
    ASSERT(
        TimeLo <= BasicInfo.ChangeTime.QuadPart &&
        TimeHi >  BasicInfo.ChangeTime.QuadPart);

    Success = GetFileInformationByHandleEx(Handle, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);
    ASSERT(0 == StandardInfo.AllocationSize.QuadPart);
    ASSERT(0 == StandardInfo.EndOfFile.QuadPart);
    ASSERT(1 == StandardInfo.NumberOfLinks);
    ASSERT(!StandardInfo.DeletePending);
    ASSERT(!StandardInfo.Directory);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof *PNameInfo);
    ASSERT(!Success);
    ASSERT(ERROR_MORE_DATA == GetLastError());
    if (OptSharePrefixLength)
    {
        PNameInfo->FileNameLength -= OptSharePrefixLength;
    }
    if (-1 == Flags)
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
    else if (0 == Prefix)
        ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0") * sizeof(WCHAR));
    else
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
    ASSERT(L'\\' == PNameInfo->FileName[0]);

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

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == FileInfo.dwFileAttributes);
    if (-1 != Flags)
        ASSERT(
            TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart &&
            TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart);
    ASSERT(
        TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart &&
        TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart);
    ASSERT(
        TimeLo <= ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart &&
        TimeHi >  ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart);
    ASSERT(0 == FileInfo.nFileSizeLow && 0 == FileInfo.nFileSizeHigh);
    ASSERT(1 == FileInfo.nNumberOfLinks);

    CloseHandle(Handle);

    memfs_stop(memfs);
}

void getfileinfo_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        getfileinfo_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        getfileinfo_dotest(MemfsDisk, 0, 0);
        getfileinfo_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        getfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        getfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void getfileinfo_name_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    BOOLEAN CaseRandomizeSave = OptCaseRandomize;
    OptCaseRandomize = FALSE;

    HANDLE Handle;
    WCHAR OrigPath[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    WCHAR FinalPath[MAX_PATH];
    DWORD Result;

    if (-1 == Flags)
        StringCbPrintfW(OrigPath, sizeof OrigPath, L"%s\\fileFILE",
            Prefix + 6);
    else if (0 == Prefix)
        StringCbPrintfW(OrigPath, sizeof OrigPath, L"\\fileFILE");
    else
        StringCbPrintfW(OrigPath, sizeof OrigPath, L"%s\\fileFILE",
            Prefix + 1);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\fileFILE",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    if (-1 == Flags || OptCaseInsensitive)
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\FILEfile",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    else
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\fileFILE",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Result = GetFinalPathNameByHandleW(
        Handle, FinalPath, MAX_PATH - 1, VOLUME_NAME_NONE | FILE_NAME_OPENED);
    ASSERT(0 != Result && Result < MAX_PATH);
    if (OptSharePrefixLength)
    {
        memmove(FinalPath,
            FinalPath + OptSharePrefixLength / sizeof(WCHAR),
            (wcslen(FinalPath) + 1) * sizeof(WCHAR) - OptSharePrefixLength);
        ASSERT(0 == _wcsicmp(OrigPath, FinalPath)); /* use wcsicmp when going through share (?) */
    }
    else
        ASSERT(0 == wcscmp(OrigPath, FinalPath)); /* don't use mywcscmp */

    if (!OptNoTraverseToken || -1 != Flags)
    {
        /*
         * FILE_NAME_NORMALIZED fails without Traverse privilege on NTFS.
         * FILE_NAME_OPENED succeeds. Go figure!
         */
        Result = GetFinalPathNameByHandleW(
            Handle, FinalPath, MAX_PATH - 1, VOLUME_NAME_NONE | FILE_NAME_NORMALIZED);
        ASSERT(0 != Result && Result < MAX_PATH);
        if (OptSharePrefixLength)
        {
            memmove(FinalPath,
                FinalPath + OptSharePrefixLength / sizeof(WCHAR),
                (wcslen(FinalPath) + 1) * sizeof(WCHAR) - OptSharePrefixLength);
        }
        ASSERT(0 == wcscmp(OrigPath, FinalPath)); /* don't use mywcscmp */
    }

    CloseHandle(Handle);

    OptCaseRandomize = CaseRandomizeSave;

    memfs_stop(memfs);
}

void getfileinfo_name_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        getfileinfo_name_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        getfileinfo_name_dotest(MemfsDisk, 0, 0);
        getfileinfo_name_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        getfileinfo_name_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        getfileinfo_name_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void setfileinfo_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    BY_HANDLE_FILE_INFORMATION FileInfo0, FileInfo;
    FILETIME FileTime;
    DWORD Offset;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetFileInformationByHandle(Handle, &FileInfo0);
    ASSERT(Success);
    //ASSERT(FILE_ATTRIBUTE_ARCHIVE == FileInfo0.dwFileAttributes);

    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN);
    ASSERT(Success);

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    ASSERT(FILE_ATTRIBUTE_HIDDEN == FileInfo.dwFileAttributes);

    *(PUINT64)&FileTime = 116444736000000000ULL + 0x4200000042ULL;
    Success = SetFileTime(Handle, 0, &FileTime, &FileTime);
    ASSERT(Success);

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    ASSERT(*(PUINT64)&FileInfo0.ftCreationTime == *(PUINT64)&FileInfo.ftCreationTime);
    ASSERT(116444736000000000ULL + 0x4200000042ULL == *(PUINT64)&FileInfo.ftLastAccessTime);
    ASSERT(116444736000000000ULL + 0x4200000042ULL == *(PUINT64)&FileInfo.ftLastWriteTime);

    Success = SetFileTime(Handle, &FileTime, 0, 0);
    ASSERT(Success);

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    ASSERT(116444736000000000ULL + 0x4200000042ULL == *(PUINT64)&FileInfo.ftCreationTime);

    Offset = SetFilePointer(Handle, 42, 0, 0);
    ASSERT(42 == Offset);

    Success = SetEndOfFile(Handle);
    ASSERT(Success);

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);
    ASSERT(42 == FileInfo.nFileSizeLow);
    ASSERT(0 == FileInfo.nFileSizeHigh);

    CloseHandle(Handle);

    memfs_stop(memfs);
}

void setfileinfo_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        setfileinfo_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        setfileinfo_dotest(MemfsDisk, 0, 0);
        setfileinfo_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        setfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        setfileinfo_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void delete_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH];
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_DIR_NOT_EMPTY == GetLastError());

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void delete_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        delete_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        delete_dotest(MemfsDisk, 0, 0);
        delete_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        delete_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        delete_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void delete_access_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_READONLY, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(!Success);
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    Success = SetFileAttributesW(FilePath, FILE_ATTRIBUTE_NORMAL);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    static PWSTR Sddl = L"D:P(D;;GA;;;SY)(D;;GA;;;BA)(D;;GA;;;WD)";
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &SecurityAttributes,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = DeleteFileW(FilePath);
    ASSERT(Success);

    LocalFree(SecurityDescriptor);

    memfs_stop(memfs);
}

void delete_access_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        delete_access_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        delete_access_dotest(MemfsDisk, 0, 0);
        delete_access_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        delete_access_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        delete_access_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void delete_pending_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, Handle2;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    MY_FILE_DISPOSITION_INFO DispositionInfo;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Handle = CreateFileW(FilePath,
        DELETE, FILE_SHARE_DELETE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    DispositionInfo.Disposition = TRUE;
    Success = SetFileInformationByHandle(Handle,
        FileDispositionInfo, &DispositionInfo, sizeof DispositionInfo);
    ASSERT(Success);

    Handle2 = CreateFileW(FilePath,
        FILE_READ_ATTRIBUTES, 0, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle2);
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = DeleteFileW(FilePath);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void delete_pending_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        delete_pending_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        delete_pending_dotest(MemfsDisk, 0, 0);
        delete_pending_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        delete_pending_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        delete_pending_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void delete_mmap_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, Mapping;
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    SYSTEM_INFO SystemInfo;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Mapping = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE == Handle);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    Success = CloseHandle(Mapping);
    ASSERT(Success);

    memfs_stop(memfs);
}

void delete_mmap_test(void)
{
    if (OptShareName)
        /* this test fails with shares */
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        delete_mmap_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        delete_mmap_dotest(MemfsDisk, 0, 0);
        delete_mmap_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        delete_mmap_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        delete_mmap_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void delete_standby_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, Mapping0, Mapping1;
    PUINT8 MappedView0, MappedView1;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH];
    WCHAR File0Path[MAX_PATH];
    WCHAR File1Path[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    unsigned seed = (unsigned)time(0);

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\dir1\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    srand(seed);

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping0 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping0);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView0 = MapViewOfFile(Mapping0, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView0);
    for (PUINT8 P = MappedView0, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        *P = rand() & 0xff;
    Success = UnmapViewOfFile(MappedView0);
    ASSERT(Success);
    Success = CloseHandle(Mapping0);
    ASSERT(Success);

    Handle = CreateFileW(File1Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping1 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping1);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView1 = MapViewOfFile(Mapping1, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView1);
    for (PUINT8 P = MappedView1, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        *P = rand() & 0xff;
    Success = UnmapViewOfFile(MappedView1);
    ASSERT(Success);
    Success = CloseHandle(Mapping1);
    ASSERT(Success);

    Success = DeleteFileW(File0Path);
    ASSERT(Success);

    Success = DeleteFileW(File1Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    srand(seed);

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping0 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping0);
    MappedView0 = MapViewOfFile(Mapping0, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView0);
    for (PUINT8 P = MappedView0, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        *P = rand() & 0xff;
    Success = UnmapViewOfFile(MappedView0);
    ASSERT(Success);
    Success = CloseHandle(Mapping0);
    ASSERT(Success);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(File1Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping1 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping1);
    MappedView1 = MapViewOfFile(Mapping1, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView1);
    for (PUINT8 P = MappedView1, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        *P = rand() & 0xff;
    Success = UnmapViewOfFile(MappedView1);
    ASSERT(Success);
    Success = CloseHandle(Mapping1);
    ASSERT(Success);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void delete_standby_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        delete_standby_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        delete_standby_dotest(MemfsDisk, 0, 0);
        delete_standby_dotest(MemfsDisk, 0, 1000);
        delete_standby_dotest(MemfsDisk, 0, INFINITE);
    }
    if (WinFspNetTests)
    {
        delete_standby_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        delete_standby_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
        delete_standby_dotest(MemfsDisk, 0, INFINITE);
    }
}

static void delete_ex_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout)
{
    BOOLEAN Success;
    DWORD FileSystemFlags;

    Success = GetVolumeInformationW(L"C:\\",
        0, 0,
        0, 0, &FileSystemFlags,
        0, 0);
    if (!Success || 0 == (FileSystemFlags & 0x400/*FILE_SUPPORTS_POSIX_UNLINK_RENAME*/))
        /* skip this test if the system lacks FILE_SUPPORTS_POSIX_UNLINK_RENAME capability */
        return;

    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    NTSYSCALLAPI NTSTATUS NTAPI
    NtSetInformationFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FileInformation,
        ULONG Length,
        FILE_INFORMATION_CLASS FileInformationClass);
    typedef struct
    {
        ULONG Flags;
    } FILE_DISPOSITION_INFORMATION_EX, *PFILE_DISPOSITION_INFORMATION_EX;

    HANDLE Handle0, Handle1, Handle2, FindHandle;
    WCHAR FilePath[MAX_PATH];
    WIN32_FIND_DATAW FindData;
    FILE_DISPOSITION_INFORMATION_EX DispositionInfo;
    IO_STATUS_BLOCK IoStatus;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetVolumeInformationW(FilePath,
        0, 0,
        0, 0, &FileSystemFlags,
        0, 0);
    ASSERT(Success);
    if (0 != (FileSystemFlags & 0x400/*FILE_SUPPORTS_POSIX_UNLINK_RENAME*/))
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        /* POSIX Semantics / Ignore Readonly */

        Handle0 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            CREATE_NEW, FILE_ATTRIBUTE_READONLY, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        Handle1 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 3; /* DELETE | POSIX_SEMANTICS */
        IoStatus.Status = NtSetInformationFile(
            Handle1, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(STATUS_CANNOT_DELETE == IoStatus.Status);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 0x13; /* DELETE | POSIX_SEMANTICS | IGNORE_READONLY_ATTRIBUTE */
        IoStatus.Status = NtSetInformationFile(
            Handle1, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(0 == IoStatus.Status);

        FindHandle = FindFirstFileW(FilePath, &FindData);
        ASSERT(INVALID_HANDLE_VALUE != FindHandle);
        ASSERT(0 == mywcscmp(FindData.cFileName, 4, L"file", 4));
        FindClose(FindHandle);

        Handle2 = CreateFileW(FilePath,
            0, 0, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());

        CloseHandle(Handle1);

        FindHandle = FindFirstFileW(FilePath, &FindData);
        ASSERT(INVALID_HANDLE_VALUE == FindHandle);
        ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

        Handle2 = CreateFileW(FilePath,
            0, 0, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE == Handle2);
        ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 0; /* DO_NOT_DELETE */
        IoStatus.Status = NtSetInformationFile(
            Handle0, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(STATUS_FILE_DELETED == IoStatus.Status);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 1; /* DELETE */
        IoStatus.Status = NtSetInformationFile(
            Handle0, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(STATUS_FILE_DELETED == IoStatus.Status);

        CloseHandle(Handle0);

        /* POSIX Semantics / Set/Reset */

        Handle0 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        Handle1 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 3; /* DELETE | POSIX_SEMANTICS */
        IoStatus.Status = NtSetInformationFile(
            Handle1, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(STATUS_SUCCESS == IoStatus.Status);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 0; /* DO_NOT_DELETE */
        IoStatus.Status = NtSetInformationFile(
            Handle1, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(STATUS_SUCCESS == IoStatus.Status);

        CloseHandle(Handle1);

        FindHandle = FindFirstFileW(FilePath, &FindData);
        ASSERT(INVALID_HANDLE_VALUE != FindHandle);
        ASSERT(0 == mywcscmp(FindData.cFileName, 4, L"file", 4));
        FindClose(FindHandle);

        Handle1 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 3; /* DELETE | POSIX_SEMANTICS */
        IoStatus.Status = NtSetInformationFile(
            Handle1, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(STATUS_SUCCESS == IoStatus.Status);

        CloseHandle(Handle1);

        CloseHandle(Handle0);

#if 0
        /* On Close */

        Handle0 = CreateFileW(FilePath,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 8; /* DO_NOT_DELETE | ON_CLOSE */
        IoStatus.Status = NtSetInformationFile(
            Handle0, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(0 == IoStatus.Status);

        CloseHandle(Handle0);

        Handle0 = CreateFileW(FilePath,
            DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 9; /* DELETE | ON_CLOSE */;
        IoStatus.Status = NtSetInformationFile(
            Handle0, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(STATUS_NOT_SUPPORTED == IoStatus.Status);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 3; /* DELETE | POSIX_SEMANTICS */;
        IoStatus.Status = NtSetInformationFile(
            Handle0, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(0 == IoStatus.Status);

        CloseHandle(Handle0);
#endif
    }

    memfs_stop(memfs);
}

void delete_ex_test(void)
{
    if (OptLegacyUnlinkRename)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        delete_ex_dotest(-1, DriveBuf, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        delete_ex_dotest(MemfsDisk, 0, 0, 0);
        delete_ex_dotest(MemfsDisk, 0, 0, 1000);
    }
    if (WinFspNetTests)
    {
        delete_ex_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 0);
        delete_ex_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000);
    }
}

static void rename_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH];
    WCHAR Dir2Path[MAX_PATH];
    WCHAR File0Path[MAX_PATH];
    WCHAR File1Path[MAX_PATH];
    WCHAR File2Path[MAX_PATH];

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(Dir2Path, sizeof Dir2Path, L"%s%s\\dir2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\dir1\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File2Path, sizeof File2Path, L"%s%s\\dir2\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    Success = CreateDirectoryW(Dir2Path, 0);
    ASSERT(Success);

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = MoveFileExW(File0Path, File1Path, 0);
    ASSERT(Success);

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = MoveFileExW(File0Path, File1Path, 0);
    ASSERT(!Success);
    ASSERT(ERROR_ALREADY_EXISTS == GetLastError());

    Success = MoveFileExW(File0Path, File1Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);

    Success = MoveFileExW(File1Path, File2Path, 0);
    ASSERT(Success);

    /* cannot replace existing directory regardless of MOVEFILE_REPLACE_EXISTING -- test MEMFS */
    Success = MoveFileExW(Dir2Path, Dir1Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(!Success);
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    Success = MoveFileExW(Dir2Path, Dir1Path, 0);
    ASSERT(Success);

    Success = MoveFileExW(Dir1Path, Dir2Path, 0);
    ASSERT(Success);

    Handle = CreateFileW(File2Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = MoveFileExW(Dir2Path, Dir1Path, 0);
    ASSERT(!Success);
    ASSERT(ERROR_ACCESS_DENIED == GetLastError());

    CloseHandle(Handle);

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    /* cannot replace existing directory regardless of MOVEFILE_REPLACE_EXISTING -- test FSD */
    if (!OptShareName)
    {
        Handle = CreateFileW(File2Path,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Success = MoveFileExW(Dir1Path, Dir2Path, MOVEFILE_REPLACE_EXISTING);
        ASSERT(!Success);
        ASSERT(ERROR_ACCESS_DENIED == GetLastError());
        CloseHandle(Handle);
    }

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    Success = DeleteFileW(File2Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir2Path);
    ASSERT(Success);

    memfs_stop(memfs);
}

void rename_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        rename_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rename_dotest(MemfsDisk, 0, 0);
        rename_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        rename_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        rename_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void rename_backslash_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH];
    WCHAR Dir2Path[MAX_PATH];
    WCHAR File0Path[MAX_PATH];
    WCHAR File1Path[MAX_PATH];

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(Dir2Path, sizeof Dir2Path, L"%s%s\\dir2\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\dir1\\file1\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = MoveFileExW(File0Path, File1Path, 0);
    ASSERT(Success);

    Success = MoveFileExW(Dir1Path, Dir2Path, 0);
    ASSERT(Success);

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\dir2\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = DeleteFileW(File1Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir2Path);
    ASSERT(Success);

    memfs_stop(memfs);
}

void rename_backslash_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        rename_backslash_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rename_backslash_dotest(MemfsDisk, 0, 0);
        rename_backslash_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        rename_backslash_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        rename_backslash_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void rename_open_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    NTSYSCALLAPI NTSTATUS NTAPI
    NtSetInformationFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FileInformation,
        ULONG Length,
        FILE_INFORMATION_CLASS FileInformationClass);
    typedef struct
    {
        BOOLEAN ReplaceIfExists;
        HANDLE RootDirectory;
        ULONG FileNameLength;
        WCHAR FileName[1];
    } FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;

    HANDLE Handle0, Handle1, Handle2;
    WCHAR File0Path[MAX_PATH];
    WCHAR File2Path[MAX_PATH];
    union
    {
        FILE_RENAME_INFORMATION I;
        UINT8 B[sizeof(FILE_RENAME_INFORMATION) + MAX_PATH * sizeof(WCHAR)];
    } RenameInfo;
    IO_STATUS_BLOCK IoStatus;
    BOOLEAN Success;

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File2Path, sizeof File2Path, L"%s%s\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle0 = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle0);
    CloseHandle(Handle0);

    Handle0 = CreateFileW(File0Path,
        DELETE, 0, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle0);

    Handle1 = CreateFileW(File0Path,
        FILE_READ_ATTRIBUTES, 0, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);

    memset(&RenameInfo.I, 0, sizeof RenameInfo.I);
    RenameInfo.I.FileNameLength = (ULONG)(wcslen(L"file2") * sizeof(WCHAR));
    memcpy(RenameInfo.I.FileName, L"file2", RenameInfo.I.FileNameLength);
    IoStatus.Status = NtSetInformationFile(
        Handle0, &IoStatus,
        &RenameInfo.I, FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + RenameInfo.I.FileNameLength,
        10/*FileRenameInformation*/);
    ASSERT(0 == IoStatus.Status);

    Handle2 = CreateFileW(File2Path,
        FILE_READ_ATTRIBUTES, 0, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle1);

    CloseHandle(Handle2);
    CloseHandle(Handle1);
    CloseHandle(Handle0);

    Success = DeleteFileW(File2Path);
    ASSERT(Success);

    memfs_stop(memfs);
}

void rename_open_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        rename_open_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rename_open_dotest(MemfsDisk, 0, 0);
        rename_open_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        rename_open_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        rename_open_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void rename_caseins_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH];
    WCHAR Dir2Path[MAX_PATH];
    WCHAR File1Path[MAX_PATH];
    WCHAR File2Path[MAX_PATH];

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(Dir2Path, sizeof Dir2Path, L"%s%s\\DIR1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File2Path, sizeof File2Path, L"%s%s\\FILE1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    Success = MoveFileExW(Dir1Path, Dir1Path, 0);
    ASSERT(Success);

    Success = MoveFileExW(Dir1Path, Dir2Path, 0);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir2Path);
    ASSERT(Success);

    Handle = CreateFileW(File1Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = MoveFileExW(File1Path, File1Path, 0);
    ASSERT(Success);

    Success = MoveFileExW(File1Path, File2Path, 0);
    ASSERT(Success);

    Success = DeleteFileW(File2Path);
    ASSERT(Success);

    memfs_stop(memfs);
}

void rename_caseins_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        rename_caseins_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rename_caseins_dotest(MemfsDisk, 0, 0);
        rename_caseins_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        rename_caseins_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        rename_caseins_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void rename_flipflop_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, ULONG NumMappings)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, Mappings[80];
    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR FilePath2[MAX_PATH];
    SYSTEM_INFO SystemInfo;

    ASSERT(ARRAYSIZE(Mappings) >= NumMappings);

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short\\subdir",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Success = CreateDirectoryW(FilePath, 0);
    ASSERT(Success);

    for (ULONG j = 1; NumMappings >= j; j++)
    {
        if (NumMappings / 2 >= j)
            StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short\\%.*s",
                Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs),
                j, L"01234567890123456789012345678901234567890123456789012345678901234567890123456789");
        else
            StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short\\subdir\\%.*s",
                Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs),
                j, L"01234567890123456789012345678901234567890123456789012345678901234567890123456789");
        Handle = CreateFileW(FilePath, GENERIC_ALL, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        Mappings[j - 1] = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
            0, SystemInfo.dwAllocationGranularity, 0);
        ASSERT(0 != Mappings[j - 1]);
        Success = CloseHandle(Handle);
        ASSERT(Success);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(FilePath2, sizeof FilePath2, L"%s%s\\longlonglonglonglonglonglonglong",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    for (ULONG i = 0; 10 > i; i++)
    {
        Success = MoveFileExW(FilePath, FilePath2, 0);
        ASSERT(Success);
        Success = MoveFileExW(FilePath2, FilePath, 0);
        ASSERT(Success);
    }

    for (ULONG j = 1; NumMappings >= j; j++)
    {
        Success = CloseHandle(Mappings[j - 1]);
        ASSERT(Success);
    }

    for (ULONG j = 1; NumMappings >= j; j++)
    {
        if (NumMappings / 2 >= j)
            StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short\\%.*s",
                Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs),
                j, L"01234567890123456789012345678901234567890123456789012345678901234567890123456789");
        else
            StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short\\subdir\\%.*s",
                Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs),
                j, L"01234567890123456789012345678901234567890123456789012345678901234567890123456789");
        Success = DeleteFileW(FilePath);
        ASSERT(Success);
    }

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short\\subdir",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\short",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));
    Success = RemoveDirectoryW(FilePath);
    ASSERT(Success);

    memfs_stop(memfs);
}

void rename_flipflop_test(void)
{
    if (OptShareName)
        /* this test fails with shares */
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        rename_flipflop_dotest(-1, DirBuf, 0, 10);
    }
    if (WinFspDiskTests)
    {
        rename_flipflop_dotest(MemfsDisk, 0, 0, 10);
        rename_flipflop_dotest(MemfsDisk, 0, 1000, 10);
        rename_flipflop_dotest(MemfsDisk, 0, 0, 40);
        rename_flipflop_dotest(MemfsDisk, 0, 1000, 40);
    }
    if (WinFspNetTests)
    {
        rename_flipflop_dotest(MemfsNet, L"\\\\memfs\\share", 0, 10);
        rename_flipflop_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 10);
        rename_flipflop_dotest(MemfsNet, L"\\\\memfs\\share", 0, 40);
        rename_flipflop_dotest(MemfsNet, L"\\\\memfs\\share", 1000, 40);
    }
}

static void rename_mmap_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, Mapping0, Mapping1;
    BOOL Success;
    WCHAR File0Path[MAX_PATH];
    WCHAR File1Path[MAX_PATH];
    WCHAR File2Path[MAX_PATH];
    SYSTEM_INFO SystemInfo;

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File2Path, sizeof File2Path, L"%s%s\\file2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping0 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping0);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    Handle = CreateFileW(File1Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping1 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping1);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    Success = MoveFileExW(File0Path, File2Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);
    Success = MoveFileExW(File2Path, File1Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);

    Success = CloseHandle(Mapping0);
    ASSERT(Success);
    Success = CloseHandle(Mapping1);
    ASSERT(Success);

    Success = DeleteFileW(File1Path);
    ASSERT(Success);

    Success = DeleteFileW(File0Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    Success = DeleteFileW(File1Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    Success = DeleteFileW(File2Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void rename_mmap_test(void)
{
    if (OptShareName)
        /* this test fails with shares */
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        rename_mmap_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rename_mmap_dotest(MemfsDisk, 0, 0);
        rename_mmap_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        rename_mmap_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        rename_mmap_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

static void rename_standby_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    HANDLE Handle, Mapping0, Mapping1;
    PUINT8 MappedView0, MappedView1;
    BOOL Success;
    WCHAR Dir1Path[MAX_PATH];
    WCHAR Dir2Path[MAX_PATH];
    WCHAR File0Path[MAX_PATH];
    WCHAR File1Path[MAX_PATH];
    SYSTEM_INFO SystemInfo;
    unsigned seed = (unsigned)time(0);

    GetSystemInfo(&SystemInfo);

    StringCbPrintfW(Dir1Path, sizeof Dir1Path, L"%s%s\\dir1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(Dir2Path, sizeof Dir2Path, L"%s%s\\dir2",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\dir1\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = CreateDirectoryW(Dir1Path, 0);
    ASSERT(Success);

    srand(seed);

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping0 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping0);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView0 = MapViewOfFile(Mapping0, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView0);
    for (PUINT8 P = MappedView0, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        *P = rand() & 0xff;
    Success = UnmapViewOfFile(MappedView0);
    ASSERT(Success);
    Success = CloseHandle(Mapping0);
    ASSERT(Success);

    Handle = CreateFileW(File1Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping1 = CreateFileMappingW(Handle, 0, PAGE_READWRITE,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping1);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView1 = MapViewOfFile(Mapping1, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    ASSERT(0 != MappedView1);
    for (PUINT8 P = MappedView1, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        *P = rand() & 0xff;
    Success = UnmapViewOfFile(MappedView1);
    ASSERT(Success);
    Success = CloseHandle(Mapping1);
    ASSERT(Success);

    Success = MoveFileExW(Dir1Path, Dir2Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\dir2\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\dir2\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    srand(seed);

    Handle = CreateFileW(File0Path,
        GENERIC_READ, FILE_SHARE_READ, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping0 = CreateFileMappingW(Handle, 0, PAGE_READONLY,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping0);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView0 = MapViewOfFile(Mapping0, FILE_MAP_READ, 0, 0, 0);
    ASSERT(0 != MappedView0);
    for (PUINT8 P = MappedView0, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        ASSERT(*P == (rand() & 0xff));
    Success = UnmapViewOfFile(MappedView0);
    ASSERT(Success);
    Success = CloseHandle(Mapping0);
    ASSERT(Success);

    Handle = CreateFileW(File1Path,
        GENERIC_READ, FILE_SHARE_READ, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping1 = CreateFileMappingW(Handle, 0, PAGE_READONLY,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping1);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView1 = MapViewOfFile(Mapping1, FILE_MAP_READ, 0, 0, 0);
    ASSERT(0 != MappedView1);
    for (PUINT8 P = MappedView1, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        ASSERT(*P == (rand() & 0xff));
    Success = UnmapViewOfFile(MappedView1);
    ASSERT(Success);
    Success = CloseHandle(Mapping1);
    ASSERT(Success);

    Success = MoveFileExW(Dir2Path, Dir1Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\dir1\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\dir1\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    srand(seed);

    Handle = CreateFileW(File0Path,
        GENERIC_READ, FILE_SHARE_READ, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping0 = CreateFileMappingW(Handle, 0, PAGE_READONLY,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping0);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView0 = MapViewOfFile(Mapping0, FILE_MAP_READ, 0, 0, 0);
    ASSERT(0 != MappedView0);
    for (PUINT8 P = MappedView0, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        ASSERT(*P == (rand() & 0xff));
    Success = UnmapViewOfFile(MappedView0);
    ASSERT(Success);
    Success = CloseHandle(Mapping0);
    ASSERT(Success);

    Handle = CreateFileW(File1Path,
        GENERIC_READ, FILE_SHARE_READ, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping1 = CreateFileMappingW(Handle, 0, PAGE_READONLY,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping1);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView1 = MapViewOfFile(Mapping1, FILE_MAP_READ, 0, 0, 0);
    ASSERT(0 != MappedView1);
    for (PUINT8 P = MappedView1, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        ASSERT(*P == (rand() & 0xff));
    Success = UnmapViewOfFile(MappedView1);
    ASSERT(Success);
    Success = CloseHandle(Mapping1);
    ASSERT(Success);

    Success = MoveFileExW(File0Path, File1Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);

    srand(seed);

    Handle = CreateFileW(File1Path,
        GENERIC_READ, FILE_SHARE_READ, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping1 = CreateFileMappingW(Handle, 0, PAGE_READONLY,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping1);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView1 = MapViewOfFile(Mapping1, FILE_MAP_READ, 0, 0, 0);
    ASSERT(0 != MappedView1);
    for (PUINT8 P = MappedView1, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        ASSERT(*P == (rand() & 0xff));
    Success = UnmapViewOfFile(MappedView1);
    ASSERT(Success);
    Success = CloseHandle(Mapping1);
    ASSERT(Success);

    Success = MoveFileExW(File1Path, File0Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);

    srand(seed);

    Handle = CreateFileW(File0Path,
        GENERIC_READ, FILE_SHARE_READ, 0,
        OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Mapping0 = CreateFileMappingW(Handle, 0, PAGE_READONLY,
        0, 16 * SystemInfo.dwAllocationGranularity, 0);
    ASSERT(0 != Mapping0);
    Success = CloseHandle(Handle);
    ASSERT(Success);
    MappedView0 = MapViewOfFile(Mapping0, FILE_MAP_READ, 0, 0, 0);
    ASSERT(0 != MappedView0);
    for (PUINT8 P = MappedView0, EndP = P + 16 * SystemInfo.dwAllocationGranularity; EndP > P; P++)
        ASSERT(*P == (rand() & 0xff));
    Success = UnmapViewOfFile(MappedView0);
    ASSERT(Success);
    Success = CloseHandle(Mapping0);
    ASSERT(Success);

    Success = DeleteFileW(File0Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(Success);

    Success = RemoveDirectoryW(Dir1Path);
    ASSERT(!Success);
    ASSERT(ERROR_FILE_NOT_FOUND == GetLastError());

    memfs_stop(memfs);
}

void rename_standby_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH];
        GetTestDirectory(DirBuf);
        rename_standby_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rename_standby_dotest(MemfsDisk, 0, 0);
        rename_standby_dotest(MemfsDisk, 0, 1000);
        rename_standby_dotest(MemfsDisk, 0, INFINITE);
    }
    if (WinFspNetTests)
    {
        rename_standby_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        rename_standby_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
        rename_standby_dotest(MemfsDisk, 0, INFINITE);
    }
}

FSP_FILE_SYSTEM_OPERATION *rename_pid_SetInformationOp;
volatile UINT32 rename_pid_Pass, rename_pid_Fail;

NTSTATUS rename_pid_SetInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (10/*FileRenameInformation*/ == Request->Req.SetInformation.FileInformationClass)
    {
        if (FspFileSystemOperationProcessId() == GetCurrentProcessId())
            InterlockedIncrement(&rename_pid_Pass);
        else
            InterlockedIncrement(&rename_pid_Fail);
    }
    else
    {
        if (FspFileSystemOperationProcessId() == 0)
            InterlockedIncrement(&rename_pid_Pass);
        else
            InterlockedIncrement(&rename_pid_Fail);
    }
    return rename_pid_SetInformationOp(FileSystem, Request, Response);
}

void rename_pid_dotest(ULONG Flags, PWSTR Prefix)
{
    rename_pid_Pass = rename_pid_Fail = 0;

    void *memfs = memfs_start(Flags);

    FSP_FILE_SYSTEM *FileSystem = MemfsFileSystem(memfs);
    rename_pid_SetInformationOp = FileSystem->Operations[FspFsctlTransactSetInformationKind];
    FileSystem->Operations[FspFsctlTransactSetInformationKind] = rename_pid_SetInformation;

    HANDLE Handle;
    BOOL Success;
    WCHAR File0Path[MAX_PATH];
    WCHAR File1Path[MAX_PATH];

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    StringCbPrintfW(File1Path, sizeof File1Path, L"%s%s\\file1",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(File0Path,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    CloseHandle(Handle);

    Success = MoveFileExW(File0Path, File1Path, MOVEFILE_REPLACE_EXISTING);
    ASSERT(Success);

    Success = DeleteFileW(File1Path);
    ASSERT(Success);

    memfs_stop(memfs);

    if (!(0 < rename_pid_Pass && 0 == rename_pid_Fail))
        tlib_printf("rename_pid_Pass=%u, rename_pid_Fail=%u", rename_pid_Pass, rename_pid_Fail);

    ASSERT(0 < rename_pid_Pass);// && 0 == rename_pid_Fail);
}

static void rename_ex_dotest(ULONG Flags, PWSTR VolPrefix, PWSTR Prefix, ULONG FileInfoTimeout)
{
    BOOLEAN Success;
    DWORD FileSystemFlags;

    Success = GetVolumeInformationW(L"C:\\",
        0, 0,
        0, 0, &FileSystemFlags,
        0, 0);
    if (!Success || 0 == (FileSystemFlags & 0x400/*FILE_SUPPORTS_POSIX_UNLINK_RENAME*/))
        /* skip this test if the system lacks FILE_SUPPORTS_POSIX_UNLINK_RENAME capability */
        return;

    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    NTSYSCALLAPI NTSTATUS NTAPI
    NtSetInformationFile(
        HANDLE FileHandle,
        PIO_STATUS_BLOCK IoStatusBlock,
        PVOID FileInformation,
        ULONG Length,
        FILE_INFORMATION_CLASS FileInformationClass);
    typedef struct
    {
        ULONG Flags;
        HANDLE RootDirectory;
        ULONG FileNameLength;
        WCHAR FileName[1];
    } FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;
    typedef struct
    {
        ULONG Flags;
    } FILE_DISPOSITION_INFORMATION_EX, *PFILE_DISPOSITION_INFORMATION_EX;

    HANDLE Handle0, Handle1, Handle2;
    WCHAR File0Path[MAX_PATH];
    WCHAR File2Path[MAX_PATH];
    union
    {
        FILE_RENAME_INFORMATION I;
        UINT8 B[sizeof(FILE_RENAME_INFORMATION) + MAX_PATH * sizeof(WCHAR)];
    } RenameInfo;
    FILE_DISPOSITION_INFORMATION_EX DispositionInfo;
    IO_STATUS_BLOCK IoStatus;

    StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\",
        VolPrefix ? L"" : L"\\\\?\\GLOBALROOT", VolPrefix ? VolPrefix : memfs_volumename(memfs));

    Success = GetVolumeInformationW(File0Path,
        0, 0,
        0, 0, &FileSystemFlags,
        0, 0);
    ASSERT(Success);
    if (0 != (FileSystemFlags & 0x400/*FILE_SUPPORTS_POSIX_UNLINK_RENAME*/))
    {
        StringCbPrintfW(File0Path, sizeof File0Path, L"%s%s\\file0",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        StringCbPrintfW(File2Path, sizeof File2Path, L"%s%s\\file2",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        /* File Test */

        Handle0 = CreateFileW(File0Path,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);
        CloseHandle(Handle0);

        Handle0 = CreateFileW(File0Path,
            DELETE, 0, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        Handle1 = CreateFileW(File0Path,
            FILE_READ_ATTRIBUTES, 0, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        memset(&RenameInfo.I, 0, sizeof RenameInfo.I);
        RenameInfo.I.Flags = 2/*FILE_RENAME_POSIX_SEMANTICS*/;
        RenameInfo.I.FileNameLength = (ULONG)(wcslen(L"file2") * sizeof(WCHAR));
        memcpy(RenameInfo.I.FileName, L"file2", RenameInfo.I.FileNameLength);
        IoStatus.Status = NtSetInformationFile(
            Handle0, &IoStatus,
            &RenameInfo.I, FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + RenameInfo.I.FileNameLength,
            65/*FileRenameInformationEx*/);
        ASSERT(0 == IoStatus.Status);

        Handle2 = CreateFileW(File2Path,
            FILE_READ_ATTRIBUTES, 0, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle2);

        CloseHandle(Handle2);
        CloseHandle(Handle1);
        CloseHandle(Handle0);

        Handle0 = CreateFileW(File0Path,
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);
        CloseHandle(Handle0);

        Handle1 = CreateFileW(File0Path,
            GENERIC_READ | GENERIC_WRITE, 0, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(File2Path,
            DELETE, 0, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        memset(&RenameInfo.I, 0, sizeof RenameInfo.I);
        RenameInfo.I.Flags = 1/*FILE_RENAME_REPLACE_IF_EXISTS*/;
        RenameInfo.I.FileNameLength = (ULONG)(wcslen(L"file0") * sizeof(WCHAR));
        memcpy(RenameInfo.I.FileName, L"file0", RenameInfo.I.FileNameLength);
        IoStatus.Status = NtSetInformationFile(
            Handle2, &IoStatus,
            &RenameInfo.I, FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + RenameInfo.I.FileNameLength,
            10/*FileRenameInformation*/);
        ASSERT(STATUS_ACCESS_DENIED == IoStatus.Status);

        memset(&RenameInfo.I, 0, sizeof RenameInfo.I);
        RenameInfo.I.Flags = 3/*FILE_RENAME_REPLACE_IF_EXISTS|FILE_RENAME_POSIX_SEMANTICS*/;
        RenameInfo.I.FileNameLength = (ULONG)(wcslen(L"file0") * sizeof(WCHAR));
        memcpy(RenameInfo.I.FileName, L"file0", RenameInfo.I.FileNameLength);
        IoStatus.Status = NtSetInformationFile(
            Handle2, &IoStatus,
            &RenameInfo.I, FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + RenameInfo.I.FileNameLength,
            65/*FileRenameInformationEx*/);
        ASSERT(STATUS_SHARING_VIOLATION == IoStatus.Status);

        CloseHandle(Handle2);
        CloseHandle(Handle1);

        Handle1 = CreateFileW(File0Path,
            FILE_READ_ATTRIBUTES, 0, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        Handle2 = CreateFileW(File2Path,
            DELETE, 0, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        memset(&RenameInfo.I, 0, sizeof RenameInfo.I);
        RenameInfo.I.Flags = 3/*FILE_RENAME_REPLACE_IF_EXISTS|FILE_RENAME_POSIX_SEMANTICS*/;
        RenameInfo.I.FileNameLength = (ULONG)(wcslen(L"file0") * sizeof(WCHAR));
        memcpy(RenameInfo.I.FileName, L"file0", RenameInfo.I.FileNameLength);
        IoStatus.Status = NtSetInformationFile(
            Handle2, &IoStatus,
            &RenameInfo.I, FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + RenameInfo.I.FileNameLength,
            65/*FileRenameInformationEx*/);
        ASSERT(0 == IoStatus.Status);

        CloseHandle(Handle2);
        CloseHandle(Handle1);

        Success = DeleteFileW(File0Path);
        ASSERT(Success);

        /* Deleted File Test */

        Handle0 = CreateFileW(File0Path,
            GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle0);

        Handle1 = CreateFileW(File0Path,
            DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
            OPEN_EXISTING, 0, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle1);

        memset(&DispositionInfo, 0, sizeof DispositionInfo);
        DispositionInfo.Flags = 3; /* DELETE | POSIX_SEMANTICS */
        IoStatus.Status = NtSetInformationFile(
            Handle1, &IoStatus,
            &DispositionInfo, sizeof DispositionInfo,
            64/*FileDispositionInformationEx*/);
        ASSERT(0 == IoStatus.Status);

        CloseHandle(Handle1);

        memset(&RenameInfo.I, 0, sizeof RenameInfo.I);
        RenameInfo.I.Flags = 2/*FILE_RENAME_POSIX_SEMANTICS*/;
        RenameInfo.I.FileNameLength = (ULONG)(wcslen(L"file2") * sizeof(WCHAR));
        memcpy(RenameInfo.I.FileName, L"file2", RenameInfo.I.FileNameLength);
        IoStatus.Status = NtSetInformationFile(
            Handle0, &IoStatus,
            &RenameInfo.I, FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + RenameInfo.I.FileNameLength,
            65/*FileRenameInformationEx*/);
        ASSERT(STATUS_ACCESS_DENIED == IoStatus.Status);

        CloseHandle(Handle0);
    }

    memfs_stop(memfs);
}

void rename_ex_test(void)
{
    if (OptLegacyUnlinkRename)
        return;

    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        rename_ex_dotest(-1, DriveBuf, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rename_ex_dotest(MemfsDisk, 0, 0, 0);
        rename_ex_dotest(MemfsDisk, 0, 0, 1000);
    }
    if (WinFspNetTests)
    {
        rename_ex_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 0);
        rename_ex_dotest(MemfsNet, L"\\\\memfs\\share", L"\\\\memfs\\share", 1000);
    }
}

void rename_pid_test(void)
{
    if (NtfsTests)
        return;

    if (WinFspDiskTests)
        rename_pid_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        rename_pid_dotest(MemfsNet, L"\\\\memfs\\share");
}

void getvolinfo_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR VolumeLabelBuf[MAX_PATH];
    DWORD VolumeSerialNumber;
    DWORD MaxComponentLength;
    DWORD FileSystemFlags;
    WCHAR FileSystemNameBuf[MAX_PATH];
    DWORD SectorsPerCluster;
    DWORD BytesPerSector;
    DWORD FreeClusters;
    DWORD TotalClusters;
    ULARGE_INTEGER CallerFreeBytes;
    ULARGE_INTEGER TotalBytes;
    ULARGE_INTEGER FreeBytes;
    HANDLE Handle;
    FILE_FS_PERSISTENT_VOLUME_INFORMATION PersistentVolumeInfo, PersistentVolumeInfoOut;
    DWORD BytesTransferred;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = GetVolumeInformationW(FilePath,
        VolumeLabelBuf, sizeof VolumeLabelBuf,
        &VolumeSerialNumber, &MaxComponentLength, &FileSystemFlags,
        FileSystemNameBuf, sizeof FileSystemNameBuf);
    ASSERT(Success);
    if (-1 != Flags)
    {
        ASSERT(0 == wcscmp(VolumeLabelBuf, L"MEMFS"));
        ASSERT(255 == MaxComponentLength);
        ASSERT(0 != (FileSystemFlags &
            (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_PERSISTENT_ACLS)));
        ASSERT(0 == wcscmp(FileSystemNameBuf, L"WinFsp-MEMFS"));
    }

    Success = GetDiskFreeSpaceW(FilePath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &TotalClusters);
    ASSERT(Success);

    Success = GetDiskFreeSpaceExW(FilePath, &CallerFreeBytes, &TotalBytes, &FreeBytes);
    ASSERT(Success);

#if 0
    UINT DriveType = GetDriveTypeW(FilePath);
    ASSERT(
        ((0 == Prefix || L'\\' != Prefix[0]) && DRIVE_FIXED == DriveType) ||
        ((0 != Prefix && L'\\' == Prefix[0]) && DRIVE_REMOTE == DriveType));
#endif

    Handle = CreateFileW(FilePath,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    DWORD FileType = GetFileType(Handle);
    ASSERT(FILE_TYPE_DISK == FileType);
    CloseHandle(Handle);

    if (!OptShareName)
    {
        StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s%s",
            -1 == Flags ? L"\\\\?\\" : L"",
            Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

        Handle = CreateFileW(FilePath,
            FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, 0);
        ASSERT(INVALID_HANDLE_VALUE != Handle);
        memset(&PersistentVolumeInfo, 0, sizeof PersistentVolumeInfo);
        PersistentVolumeInfo.FlagMask = PERSISTENT_VOLUME_STATE_SHORT_NAME_CREATION_DISABLED;
        PersistentVolumeInfo.Version = 1;
        Success = DeviceIoControl(Handle, FSCTL_QUERY_PERSISTENT_VOLUME_STATE,
            &PersistentVolumeInfo, sizeof PersistentVolumeInfo,
            &PersistentVolumeInfoOut, sizeof PersistentVolumeInfoOut,
            &BytesTransferred,
            0);
        ASSERT(Success);
        ASSERT(sizeof PersistentVolumeInfoOut == BytesTransferred);
        if (-1 != Flags)
            ASSERT(PERSISTENT_VOLUME_STATE_SHORT_NAME_CREATION_DISABLED == PersistentVolumeInfoOut.VolumeFlags);
        CloseHandle(Handle);
    }

    memfs_stop(memfs);
}

void getvolinfo_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        getvolinfo_dotest(-1, DriveBuf, 0);
    }
    if (WinFspDiskTests)
    {
        getvolinfo_dotest(MemfsDisk, 0, 0);
        getvolinfo_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        getvolinfo_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        getvolinfo_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void setvolinfo_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    if (-1 == Flags)
        return;/* avoid accidentally changing the volume label on our NTFS disk */
    if (0 != Prefix)
        return;/* cannot do SetVolumeLabel on a network share! */

    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    BOOL Success;
    WCHAR FilePath[MAX_PATH];
    WCHAR VolumeLabelBuf[MAX_PATH];
    DWORD VolumeSerialNumber;
    DWORD MaxComponentLength;
    DWORD FileSystemFlags;
    WCHAR FileSystemNameBuf[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Success = SetVolumeLabelW(FilePath, L"12345678901234567890123456789012");
    if (Success)
    {
        Success = GetVolumeInformationW(FilePath,
            VolumeLabelBuf, sizeof VolumeLabelBuf,
            &VolumeSerialNumber, &MaxComponentLength, &FileSystemFlags,
            FileSystemNameBuf, sizeof FileSystemNameBuf);
        ASSERT(Success);
        if (-1 != Flags)
        {
            ASSERT(0 == wcscmp(VolumeLabelBuf, L"12345678901234567890123456789012"));
            ASSERT(255 == MaxComponentLength);
            ASSERT(0 != (FileSystemFlags &
                (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_PERSISTENT_ACLS)));
            ASSERT(0 == wcscmp(FileSystemNameBuf, L"WinFsp-MEMFS"));
        }

        Success = SetVolumeLabelW(FilePath, L"TestLabel");
        ASSERT(Success);

        Success = GetVolumeInformationW(FilePath,
            VolumeLabelBuf, sizeof VolumeLabelBuf,
            &VolumeSerialNumber, &MaxComponentLength, &FileSystemFlags,
            FileSystemNameBuf, sizeof FileSystemNameBuf);
        ASSERT(Success);
        if (-1 != Flags)
        {
            ASSERT(0 == wcscmp(VolumeLabelBuf, L"TestLabel"));
            ASSERT(255 == MaxComponentLength);
            ASSERT(0 != (FileSystemFlags &
                (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_PERSISTENT_ACLS)));
            ASSERT(0 == wcscmp(FileSystemNameBuf, L"WinFsp-MEMFS"));
        }

        Success = SetVolumeLabelW(FilePath, L"123456789012345678901234567890123");
        ASSERT(Success);

        Success = GetVolumeInformationW(FilePath,
            VolumeLabelBuf, sizeof VolumeLabelBuf,
            &VolumeSerialNumber, &MaxComponentLength, &FileSystemFlags,
            FileSystemNameBuf, sizeof FileSystemNameBuf);
        ASSERT(Success);
        if (-1 != Flags)
        {
            ASSERT(0 == wcscmp(VolumeLabelBuf, L"12345678901234567890123456789012"));
            ASSERT(255 == MaxComponentLength);
            ASSERT(0 != (FileSystemFlags &
                (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_PERSISTENT_ACLS)));
            ASSERT(0 == wcscmp(FileSystemNameBuf, L"WinFsp-MEMFS"));
        }
    }

    memfs_stop(memfs);
}

void setvolinfo_test(void)
{
#if 0
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH], DriveBuf[3];
        GetTestDirectoryAndDrive(DirBuf, DriveBuf);
        setvolinfo_dotest(-1, DriveBuf, 0);
    }
#endif
    if (WinFspDiskTests)
    {
        setvolinfo_dotest(MemfsDisk, 0, 0);
        setvolinfo_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        setvolinfo_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        setvolinfo_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void query_winfsp_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout, BOOLEAN ExpectWinFsp)
{
    void* memfs = memfs_start_ex(Flags, FileInfoTimeout);

    WCHAR FilePath[MAX_PATH];
    HANDLE Handle;
    DWORD BytesTransferred;

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    if (ExpectWinFsp)
        ASSERT(DeviceIoControl(Handle, FSP_FSCTL_QUERY_WINFSP, 0, 0, 0, 0, &BytesTransferred, 0));
    else
        ASSERT(!DeviceIoControl(Handle, FSP_FSCTL_QUERY_WINFSP, 0, 0, 0, 0, &BytesTransferred, 0) &&
            ERROR_INVALID_FUNCTION == GetLastError());

    CloseHandle(Handle);

    memfs_stop(memfs);
}

void query_winfsp_test(void)
{
    if (NtfsTests)
        return;

    if (WinFspDiskTests)
        query_winfsp_dotest(MemfsDisk, 0, 0, TRUE);
    if (WinFspNetTests)
        query_winfsp_dotest(MemfsNet, L"\\\\memfs\\share", 0, TRUE);
}

void info_tests(void)
{
    if (!OptFuseExternal && !OptShareName)
        TEST(getfileattr_test);
    TEST(getfileinfo_test);
    if (!OptFuseExternal)
        TEST(getfileinfo_name_test);
    TEST(setfileinfo_test);
    TEST(delete_test);
    TEST(delete_access_test);
    TEST(delete_pending_test);
    if (!OptShareName)
        TEST(delete_mmap_test);
    TEST(delete_standby_test);
    if (!OptLegacyUnlinkRename)
        TEST(delete_ex_test);
    TEST(rename_test);
    TEST(rename_backslash_test);
    TEST(rename_open_test);
    TEST(rename_caseins_test);
    if (!OptShareName)
        TEST(rename_flipflop_test);
    if (!OptShareName)
        TEST(rename_mmap_test);
    TEST(rename_standby_test);
    if (!OptLegacyUnlinkRename)
        TEST(rename_ex_test);
    if (!NtfsTests)
        TEST(rename_pid_test);
    TEST(getvolinfo_test);
    TEST(setvolinfo_test);
    if (!NtfsTests)
        TEST(query_winfsp_test);
}
