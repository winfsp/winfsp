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
    WCHAR FilePath[MAX_PATH];

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : memfs_volumename(memfs));

    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
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
