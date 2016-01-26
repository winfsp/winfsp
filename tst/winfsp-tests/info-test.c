#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

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
    if (-1 == Flags)
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
    else if (0 == Prefix)
        ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0") * sizeof(WCHAR));
    else
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
    ASSERT(L'\\' == PNameInfo->FileName[0]);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof NameInfoBuf);
    ASSERT(Success);
    if (-1 == Flags)
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 6) * sizeof(WCHAR));
    else if (0 == Prefix)
        ASSERT(PNameInfo->FileNameLength == wcslen(L"\\file0") * sizeof(WCHAR));
    else
        ASSERT(PNameInfo->FileNameLength == wcslen(FilePath + 1) * sizeof(WCHAR));
    if (-1 == Flags)
        ASSERT(0 == memcmp(FilePath + 6, PNameInfo->FileName, PNameInfo->FileNameLength));
    else if (0 == Prefix)
        ASSERT(0 == memcmp(L"\\file0", PNameInfo->FileName, PNameInfo->FileNameLength));
    else
        ASSERT(0 == memcmp(FilePath + 1, PNameInfo->FileName, PNameInfo->FileNameLength));

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
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
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
        ASSERT(0 == wcscmp(FileSystemNameBuf, L"WinFsp"));
    }

    memfs_stop(memfs);
}

void getvolinfo_test(void)
{
    if (NtfsTests)
        getvolinfo_dotest(-1, L"C:", 0);
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

void info_tests(void)
{
    TEST(getfileinfo_test);
    TEST(getvolinfo_test);
}
