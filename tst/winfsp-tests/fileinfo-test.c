#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <sddl.h>
#include <strsafe.h>
#include "memfs.h"

void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

void getinfo_dotest(ULONG Flags, PWSTR Prefix)
{
    void *memfs = memfs_start(Flags);

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

    GetSystemTimeAsFileTime(&FileTime);

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
    ASSERT(((PLARGE_INTEGER)&FileTime)->QuadPart <= BasicInfo.CreationTime.QuadPart &&
        ((PLARGE_INTEGER)&FileTime)->QuadPart + 10000000 > BasicInfo.CreationTime.QuadPart);
    ASSERT(((PLARGE_INTEGER)&FileTime)->QuadPart <= BasicInfo.LastAccessTime.QuadPart &&
        ((PLARGE_INTEGER)&FileTime)->QuadPart + 10000000 > BasicInfo.LastAccessTime.QuadPart);
    ASSERT(((PLARGE_INTEGER)&FileTime)->QuadPart <= BasicInfo.LastWriteTime.QuadPart &&
        ((PLARGE_INTEGER)&FileTime)->QuadPart + 10000000 > BasicInfo.LastWriteTime.QuadPart);
    ASSERT(((PLARGE_INTEGER)&FileTime)->QuadPart <= BasicInfo.ChangeTime.QuadPart &&
        ((PLARGE_INTEGER)&FileTime)->QuadPart + 10000000 > BasicInfo.ChangeTime.QuadPart);

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
    ASSERT(((PLARGE_INTEGER)&FileTime)->QuadPart <= ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart &&
        ((PLARGE_INTEGER)&FileTime)->QuadPart + 10000000 > ((PLARGE_INTEGER)&FileInfo.ftCreationTime)->QuadPart);
    ASSERT(((PLARGE_INTEGER)&FileTime)->QuadPart <= ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart &&
        ((PLARGE_INTEGER)&FileTime)->QuadPart + 10000000 > ((PLARGE_INTEGER)&FileInfo.ftLastAccessTime)->QuadPart);
    ASSERT(((PLARGE_INTEGER)&FileTime)->QuadPart <= ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart &&
        ((PLARGE_INTEGER)&FileTime)->QuadPart + 10000000 > ((PLARGE_INTEGER)&FileInfo.ftLastWriteTime)->QuadPart);
    ASSERT(0 == FileInfo.nFileSizeLow && 0 == FileInfo.nFileSizeHigh);
    ASSERT(1 == FileInfo.nNumberOfLinks);

    CloseHandle(Handle);

    memfs_stop(memfs);
}

void getinfo_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        getinfo_dotest(-1, DirBuf);
    }
    if (WinFspDiskTests)
        getinfo_dotest(MemfsDisk, 0);
    if (WinFspNetTests)
        getinfo_dotest(MemfsNet, L"\\\\memfs\\share");
}

void getinfo_tests(void)
{
    TEST(getinfo_test);
}
