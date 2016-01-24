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

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = GetFileInformationByHandleEx(Handle, FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo);
    ASSERT(Success);

    Success = GetFileInformationByHandleEx(Handle, FileBasicInfo, &BasicInfo, sizeof BasicInfo);
    ASSERT(Success);

    Success = GetFileInformationByHandleEx(Handle, FileStandardInfo, &StandardInfo, sizeof StandardInfo);
    ASSERT(Success);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof *PNameInfo);
    ASSERT(!Success);
    ASSERT(ERROR_MORE_DATA == GetLastError());
    ASSERT(L'\\' == PNameInfo->FileName[0]);

    Success = GetFileInformationByHandleEx(Handle, FileNameInfo, PNameInfo, sizeof NameInfoBuf);
    ASSERT(Success);

    Success = GetFileInformationByHandle(Handle, &FileInfo);
    ASSERT(Success);

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
