/**
 * @file fuse-test.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include <fuse/fuse.h>
#include <tlib/testsuite.h>
#include <process.h>

#include "winfsp-tests.h"

static unsigned __stdcall fuse_tests_thread(void *f)
{
    return fuse_loop_mt(f);
}

static void fuse_sequential_test(void)
{
    static struct fuse_operations ops;
    char *argv[] = { "UNKNOWN" };
    struct fuse_args args = FUSE_ARGS_INIT(1, argv);
    struct fuse_chan *ch[10] = { 0 };
    struct fuse *f[10];
    HANDLE Thread[10];
    DWORD ExitCode;

    for (int i = 0; sizeof(f) / sizeof(f[0]) > i; i++)
    {
        ch[i] = fuse_mount("*", &args);
        ASSERT(0 != ch[i]);

        f[i] = fuse_new(ch[i], &args, &ops, sizeof ops, 0);
        ASSERT(0 != f[i]);

        Thread[i] = (HANDLE)_beginthreadex(0, 0, fuse_tests_thread, f[i], 0, 0);
        ASSERT(0 != Thread[i]);

        if (0 != i)
            Sleep(i * 20);

        fuse_exit(f[i]);

        WaitForSingleObject(Thread[i], INFINITE);
        GetExitCodeThread(Thread[i], &ExitCode);
        CloseHandle(Thread[i]);

        fuse_destroy(f[i]);

        fuse_unmount("*", ch[i]);

        ASSERT(0 == ExitCode);
    }
}

static void fuse_parallel_test(void)
{
    static struct fuse_operations ops;
    char *argv[] = { "UNKNOWN" };
    struct fuse_args args = FUSE_ARGS_INIT(1, argv);
    struct fuse_chan *ch[10] = { 0 };
    struct fuse *f[10];
    HANDLE Thread[10];
    DWORD ExitCode;

    for (int i = 0; sizeof(f) / sizeof(f[0]) > i; i++)
    {
        ch[i] = fuse_mount("*", &args);
        ASSERT(0 != ch[i]);

        f[i] = fuse_new(ch[i], &args, &ops, sizeof ops, 0);
        ASSERT(0 != f[i]);

        Thread[i] = (HANDLE)_beginthreadex(0, 0, fuse_tests_thread, f[i], 0, 0);
        ASSERT(0 != Thread[i]);
    }

    Sleep(1000);

    for (int i = 0; sizeof(f) / sizeof(f[0]) > i; i++)
    {
        fuse_exit(f[i]);

        WaitForSingleObject(Thread[i], INFINITE);
        GetExitCodeThread(Thread[i], &ExitCode);
        CloseHandle(Thread[i]);

        fuse_destroy(f[i]);

        fuse_unmount("*", ch[i]);

        ASSERT(0 == ExitCode);
    }
}

void fuse_tests(void)
{
    if (OptExternal)
        return;

    TEST_OPT(fuse_sequential_test);
    TEST_OPT(fuse_parallel_test);
}
