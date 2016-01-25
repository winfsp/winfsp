#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <process.h>
#include "memfs.h"

extern int WinFspDiskTests;
extern int WinFspNetTests;

struct memfs_data
{
    MEMFS *Memfs;
    HANDLE Thread;
};

static unsigned __stdcall memfs_thread(void *Memfs0)
{
    MEMFS *Memfs = Memfs0;
    return FspFileSystemLoop(MemfsFileSystem(Memfs));
}

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout)
{
    if (-1 == Flags)
        return 0;

    struct memfs_data *data;
    MEMFS *Memfs;
    HANDLE Thread;
    NTSTATUS Result;

    data = malloc(sizeof *data);
    ASSERT(0 != data);

    Result = MemfsCreate(Flags, FileInfoTimeout, 1000, 65500, &Memfs);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Memfs);

    Thread = (HANDLE)_beginthreadex(0, 0, memfs_thread, Memfs, 0, 0);
    ASSERT(0 != Thread);

    data->Memfs = Memfs;
    data->Thread = Thread;

    return data;
}

void *memfs_start(ULONG Flags)
{
    return memfs_start_ex(Flags, 0);
}

void memfs_stop(void *data)
{
    if (0 == data)
        return;

    MEMFS *Memfs = ((struct memfs_data *)data)->Memfs;
    HANDLE Thread = ((struct memfs_data *)data)->Thread;
    DWORD ExitCode;

    FspFileSystemSetDispatcherResult(MemfsFileSystem(Memfs), STATUS_CANCELLED);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(STATUS_CANCELLED == ExitCode);

    MemfsDelete(Memfs);
}

PWSTR memfs_volumename(void *data)
{
    MEMFS *Memfs = ((struct memfs_data *)data)->Memfs;
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
