/**
 * @file fscrash-main.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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

#include <stdio.h>

#include "fscrash.h"
#include "memfs.h"

#define fail(format, ...)               fprintf(stderr, format, __VA_ARGS__)

ULONG OptCrashMask = -1, OptCrashFlags = FspCrashInterceptAccessViolation, OptCrashPercent = 10;
ULONG OptMemfsFlags = MemfsDisk;

int wmain(int argc, wchar_t **argv)
{
    for (int argi = 1; argc > argi; argi++)
    {
        const wchar_t *a = argv[argi];
        if ('-' == a[0])
        {
            if (0 == wcsncmp(L"--mask=", a, sizeof "--mask=" - 1))
                OptCrashMask = wcstoul(a + sizeof "--mask=" - 1, 0, 0);
            else if (0 == wcsncmp(L"--percent=", a, sizeof "--percent=" - 1))
                OptCrashPercent = wcstoul(a + sizeof "--percent=" - 1, 0, 10);
            else if (0 == wcscmp(L"--crash", a))
            {
                OptCrashFlags &= ~FspCrashInterceptTerminate;
                OptCrashFlags |= FspCrashInterceptAccessViolation;
            }
            else if (0 == wcscmp(L"--terminate", a))
            {
                OptCrashFlags &= ~FspCrashInterceptAccessViolation;
                OptCrashFlags |= FspCrashInterceptTerminate;
            }
            else if (0 == wcscmp(L"--enter", a))
                OptCrashFlags |= FspCrashInterceptEnter;
            else if (0 == wcscmp(L"--leave", a))
                OptCrashFlags |= FspCrashInterceptLeave;
            else if (0 == wcscmp(L"--disk", a))
                OptMemfsFlags = MemfsDisk;
            else if (0 == wcscmp(L"--n", a))
                OptMemfsFlags = MemfsDisk;
        }
    }

    MEMFS *Memfs;
    NTSTATUS Result;

    Result = MemfsCreate(
        OptMemfsFlags,
        -1,
        1024,
        1024 * 1024,
        (MemfsNet & OptMemfsFlags) ? L"\\memfs\\share" : 0,
        0,
        &Memfs);
    if (!NT_SUCCESS(Result))
    {
        fail("cannot create MEMFS file system: (Status=%lx)", Result);
        exit(1);
    }

    FspCrashIntercept(MemfsFileSystem(Memfs), OptCrashMask, OptCrashFlags, OptCrashPercent);

    Result = MemfsStart(Memfs);
    if (!NT_SUCCESS(Result))
    {
        fail("cannot start MEMFS file system: (Status=%lx)", Result);
        exit(1);
    }

    MemfsStop(Memfs);
    MemfsDelete(Memfs);

    return 0;
}
