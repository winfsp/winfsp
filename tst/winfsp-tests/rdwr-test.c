#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include "memfs.h"

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout);
void *memfs_start(ULONG Flags);
void memfs_stop(void *data);
PWSTR memfs_volumename(void *data);

extern int NtfsTests;
extern int WinFspDiskTests;
extern int WinFspNetTests;

void rdwr_noncached_dotest(ULONG Flags, PWSTR Prefix, ULONG FileInfoTimeout)
{
    void *memfs = memfs_start_ex(Flags, FileInfoTimeout);

    memfs_stop(memfs);
}

void rdwr_noncached_test(void)
{
    if (NtfsTests)
    {
        WCHAR DirBuf[MAX_PATH] = L"\\\\?\\";
        GetCurrentDirectoryW(MAX_PATH - 4, DirBuf + 4);
        rdwr_noncached_dotest(-1, DirBuf, 0);
    }
    if (WinFspDiskTests)
    {
        rdwr_noncached_dotest(MemfsDisk, 0, 0);
        rdwr_noncached_dotest(MemfsDisk, 0, 1000);
    }
    if (WinFspNetTests)
    {
        rdwr_noncached_dotest(MemfsNet, L"\\\\memfs\\share", 0);
        rdwr_noncached_dotest(MemfsNet, L"\\\\memfs\\share", 1000);
    }
}

void rdwr_tests(void)
{
    TEST(rdwr_noncached_test);
}
