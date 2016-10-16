#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <process.h>
#include "memfs.h"

#include "winfsp-tests.h"

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout)
{
    if (-1 == Flags)
        return 0;

    MEMFS *Memfs;
    NTSTATUS Result;

    Result = MemfsCreate(
        (OptCaseInsensitive ? MemfsCaseInsensitive : 0) | Flags,
        FileInfoTimeout,
        1024,
        1024 * 1024,
        MemfsNet == Flags ? L"\\memfs\\share" : 0,
        0,
        &Memfs);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Memfs);

    Result = MemfsStart(Memfs);
    ASSERT(NT_SUCCESS(Result));

    return Memfs;
}

void *memfs_start(ULONG Flags)
{
    return memfs_start_ex(Flags, 1000);
}

void memfs_stop(void *data)
{
    if (0 == data)
        return;

    MEMFS *Memfs = data;

    MemfsStop(Memfs);

    MemfsDelete(Memfs);
}

PWSTR memfs_volumename(void *data)
{
    MEMFS *Memfs = data;
    return MemfsFileSystem(Memfs)->VolumeName;
}

void memfs_dotest(ULONG Flags)
{
    void *memfs = memfs_start(Flags);

    memfs_stop(memfs);
}

void memfs_test(void)
{
    if (WinFspDiskTests)
        memfs_dotest(MemfsDisk);
    if (WinFspNetTests)
        memfs_dotest(MemfsNet);
}

void memfs_tests(void)
{
    TEST(memfs_test);
}
