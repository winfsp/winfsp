#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include "memfs.h"

void *memfs_start(ULONG Flags);
void memfs_stop(void *data);

extern int WinFspDiskTests;
extern int WinFspNetTests;

void create_dotest(ULONG Flags)
{
    void *memfs = memfs_start(Flags);

    memfs_stop(memfs);
}

void create_test(void)
{
    if (WinFspDiskTests)
        create_dotest(MemfsDisk);
    if (WinFspNetTests)
        create_dotest(MemfsNet);
}

void create_tests(void)
{
    TEST(create_test);
}
