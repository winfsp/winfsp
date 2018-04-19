/**
 * @file memfs-test.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <process.h>
#include "memfs.h"

#include "winfsp-tests.h"

int memfs_running;

void *memfs_start_ex(ULONG Flags, ULONG FileInfoTimeout)
{
    if (-1 == Flags)
    {
        memfs_running = 1;
        return 0;
    }

    MEMFS *Memfs;
    NTSTATUS Result;

    Result = MemfsCreateFunnel(
        Flags |
            (OptCaseInsensitive ? MemfsCaseInsensitive : 0) |
            (OptFlushAndPurgeOnCleanup ? MemfsFlushAndPurgeOnCleanup : 0),
        FileInfoTimeout,
        1024,
        1024 * 1024,
        50, /*SlowioMaxDelay*/
        10, /*SlowioPercentDelay*/
        5,  /*SlowioRarefyDelay*/
        0,
        MemfsNet == Flags ? L"\\memfs\\share" : 0,
        0,
        &Memfs);
    ASSERT(NT_SUCCESS(Result));
    ASSERT(0 != Memfs);

    if (OptMountPoint)
    {
        Result = FspFileSystemSetMountPoint(MemfsFileSystem(Memfs), OptMountPoint);
        ASSERT(NT_SUCCESS(Result));
    }

    Result = MemfsStart(Memfs);
    ASSERT(NT_SUCCESS(Result));

    memfs_running = 1;

    return Memfs;
}

void *memfs_start(ULONG Flags)
{
    return memfs_start_ex(Flags, 1000);
}

void memfs_stop(void *data)
{
    memfs_running = 0;

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
    if (OptExternal)
        return;

    TEST(memfs_test);
}
