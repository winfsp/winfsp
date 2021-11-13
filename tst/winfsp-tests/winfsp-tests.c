/**
 * @file winfsp-tests.c
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

#include <windows.h>
#include <dbghelp.h>
#include <lm.h>
#include <signal.h>
#include <tlib/testsuite.h>
#include <time.h>

#define WINFSP_TESTS_NO_HOOKS
#include "winfsp-tests.h"

int NtfsTests = 0;
int WinFspDiskTests = 1;
int WinFspNetTests = 1;

BOOLEAN OptExternal = FALSE;
BOOLEAN OptFuseExternal = FALSE;
BOOLEAN OptResilient = FALSE;
BOOLEAN OptCaseInsensitiveCmp = FALSE;
BOOLEAN OptCaseInsensitive = FALSE;
BOOLEAN OptCaseRandomize = FALSE;
BOOLEAN OptFlushAndPurgeOnCleanup = FALSE;
BOOLEAN OptLegacyUnlinkRename = FALSE;
BOOLEAN OptNotify = FALSE;
WCHAR OptOplock = 0;
WCHAR OptMountPointBuf[MAX_PATH], *OptMountPoint;
WCHAR OptShareNameBuf[MAX_PATH], *OptShareName, *OptShareTarget;
    WCHAR OptShareComputer[MAX_PATH] = L"\\\\localhost\\";
    ULONG OptSharePrefixLength; /* only counts single leading slash: \localhost\target\path */
HANDLE OptNoTraverseToken = 0;
    LUID OptNoTraverseLuid;

static void exiting(void);

int mywcscmp(PWSTR a, int alen, PWSTR b, int blen)
{
    int len, res;

    if (-1 == alen)
        alen = (int)wcslen(a);
    if (-1 == blen)
        blen = (int)wcslen(b);

    len = alen < blen ? alen : blen;

    /* we should still be in the C locale */
    if (OptCaseInsensitiveCmp)
        res = _wcsnicmp(a, b, len);
    else
        res = wcsncmp(a, b, len);

    if (0 == res)
        res = alen - blen;

    return res;
}

static unsigned myrandseed = 1;
int myrand(void)
{
    /*
     * This mimics MSVCRT rand(); we need our own version
     * as to not interfere with the program's rand().
     */

    myrandseed = myrandseed * 214013 + 2531011;
    return (myrandseed >> 16) & RAND_MAX;
}

VOID GetTestDirectoryEx(PWSTR DirBuf, ULONG DirBufSize, PWSTR DriveBuf)
{
    DirBufSize /= sizeof(WCHAR);
    if (MAX_PATH > DirBufSize)
        ABORT("test directory buffer must be at least MAX_PATH long");

    DWORD Result = GetCurrentDirectoryW(DirBufSize - 4, DirBuf + 4);
    if (0 == Result || Result >= DirBufSize - 4)
        ABORT("GetCurrentDirectoryW failed");

    if (!testalpha(DirBuf[4]) || L':' != DirBuf[5])
        ABORT("--ntfs/--external tests must be run from a drive");

    DirBuf[0] = L'\\';
    DirBuf[1] = L'\\';
    DirBuf[2] = L'?';
    DirBuf[3] = L'\\';
    if (L'\\' == DirBuf[6] && L'\0' == DirBuf[7])
        DirBuf[6] = L'\0';

    if (0 != DriveBuf)
    {
        DriveBuf[0] = DirBuf[4];
        DriveBuf[1] = L':';
        DriveBuf[2] = L'\0';
    }
}

static VOID DisableBackupRestorePrivileges(VOID)
{
    union
    {
        TOKEN_PRIVILEGES P;
        UINT B[sizeof(TOKEN_PRIVILEGES) + sizeof(LUID_AND_ATTRIBUTES)];
    } Privileges;
    HANDLE Token;

    Privileges.P.PrivilegeCount = 2;
    Privileges.P.Privileges[0].Attributes = 0;
    Privileges.P.Privileges[1].Attributes = 0;

    if (!LookupPrivilegeValueW(0, SE_BACKUP_NAME, &Privileges.P.Privileges[0].Luid) ||
        !LookupPrivilegeValueW(0, SE_RESTORE_NAME, &Privileges.P.Privileges[1].Luid))
        ABORT("cannot lookup backup/restore privileges");

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token))
        ABORT("cannot open process token");

    if (!AdjustTokenPrivileges(Token, FALSE, &Privileges.P, 0, 0, 0))
        ABORT("cannot disable backup/restore privileges");

    CloseHandle(Token);
}

static VOID AddNetShareIfNeeded(VOID)
{
    if (!OptShareTarget)
        return;

    SHARE_INFO_2 ShareInfo = { 0 };
    NET_API_STATUS NetStatus;

    ShareInfo.shi2_netname = OptShareName;
    ShareInfo.shi2_type = STYPE_DISKTREE;
    ShareInfo.shi2_permissions = ACCESS_ALL;
    ShareInfo.shi2_max_uses = -1;
    ShareInfo.shi2_path = OptShareTarget;

    NetShareDel(0, OptShareName, 0);
    NetStatus = NetShareAdd(0, 2, (PBYTE)&ShareInfo, 0);
    if (NERR_Success != NetStatus)
        ABORT("cannot add network share");
}

static VOID RemoveNetShareIfNeeded(VOID)
{
    if (!OptShareTarget)
        return;

    NetShareDel(0, OptShareName, 0);
}

static void abort_handler(int sig)
{
    DWORD Error = GetLastError();
    exiting();
    SetLastError(Error);
}

LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
    if (0 != ExceptionInfo && 0 != ExceptionInfo->ExceptionRecord)
    {
        static CHAR OutBuf[128];
        static union
        {
            SYMBOL_INFO V;
            UINT8 Buf[sizeof(SYMBOL_INFO) + 64];
        } Info;
        LARGE_INTEGER Large;
        Info.V.SizeOfStruct = sizeof(SYMBOL_INFO);
        Info.V.MaxNameLen = 64;
        if (SymFromAddr(GetCurrentProcess(),
            (DWORD64)ExceptionInfo->ExceptionRecord->ExceptionAddress,
            &Large.QuadPart,
            &Info.V))
        {
            wsprintfA(OutBuf, "\nEXCEPTION 0x%lX at %s+0x%lX(0x%p)\n",
                ExceptionInfo->ExceptionRecord->ExceptionCode,
                Info.V.Name,
                Large.LowPart,
                ExceptionInfo->ExceptionRecord->ExceptionAddress);
        }
        else
        {
            wsprintfA(OutBuf, "\nEXCEPTION 0x%lX at 0x%p\n",
                ExceptionInfo->ExceptionRecord->ExceptionCode,
                ExceptionInfo->ExceptionRecord->ExceptionAddress);
        }
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), OutBuf, lstrlenA(OutBuf), &Large.LowPart, 0);
    }

    exiting();
    return EXCEPTION_EXECUTE_HANDLER;
}

#define rmarg(argv, argc, argi)         \
    argc--,\
    memmove(argv + argi, argv + argi + 1, (argc - argi) * sizeof(char *)),\
    argi--,\
    argv[argc] = 0
int main(int argc, char *argv[])
{
    TESTSUITE(fuse_opt_tests);
    TESTSUITE(fuse_tests);
    TESTSUITE(posix_tests);
    TESTSUITE(uuid5_tests);
    TESTSUITE(eventlog_tests);
    TESTSUITE(path_tests);
    TESTSUITE(dirbuf_tests);
    TESTSUITE(version_tests);
    TESTSUITE(launch_tests);
    TESTSUITE(launcher_ptrans_tests);
    TESTSUITE(mount_tests);
    TESTSUITE(timeout_tests);
    TESTSUITE(memfs_tests);
    TESTSUITE(create_tests);
    TESTSUITE(info_tests);
    TESTSUITE(security_tests);
    TESTSUITE(rdwr_tests);
    TESTSUITE(flush_tests);
    TESTSUITE(lock_tests);
    TESTSUITE(dirctl_tests);
    TESTSUITE(exec_tests);
    TESTSUITE(devctl_tests);
    TESTSUITE(reparse_tests);
    TESTSUITE(ea_tests);
    TESTSUITE(stream_tests);
    TESTSUITE(oplock_tests);
    TESTSUITE(notify_tests);
    TESTSUITE(wsl_tests);
    TESTSUITE(volpath_tests);

    SymInitialize(GetCurrentProcess(), 0, TRUE);

    atexit(exiting);
    signal(SIGABRT, abort_handler);
#pragma warning(suppress: 4996)
    if (0 == getenv("WINFSP_TESTS_EXCEPTION_FILTER_DISABLE"))
        SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    for (int argi = 1; argc > argi; argi++)
    {
        const char *a = argv[argi];
        if ('-' == a[0])
        {
            if (0 == strcmp("--ntfs", a) || 0 == strcmp("--external", a))
            {
                OptExternal = TRUE;
                NtfsTests = 1;
                WinFspDiskTests = 0;
                WinFspNetTests = 0;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--fuse-external", a))
            {
                OptExternal = TRUE;
                OptFuseExternal = TRUE;
                NtfsTests = 1;
                WinFspDiskTests = 0;
                WinFspNetTests = 0;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--resilient", a))
            {
                OptResilient = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--case-insensitive-cmp", a))
            {
                OptCaseInsensitiveCmp = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--case-insensitive", a))
            {
                OptCaseInsensitive = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--case-randomize", a))
            {
                OptCaseRandomize = TRUE;
                OptCaseInsensitive = TRUE;
                OptCaseInsensitiveCmp = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--flush-and-purge-on-cleanup", a))
            {
                OptFlushAndPurgeOnCleanup = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--legacy-unlink-rename", a))
            {
                OptLegacyUnlinkRename = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--notify", a))
            {
                OptNotify = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--oplock=batch", a))
            {
                OptOplock = 'B';
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--oplock=filter", a))
            {
                OptOplock = 'F';
                rmarg(argv, argc, argi);
            }
            else if (0 == strncmp("--mountpoint=", a, sizeof "--mountpoint=" - 1))
            {
                if (0 != MultiByteToWideChar(CP_UTF8, 0,
                    a + sizeof "--mountpoint=" - 1, -1, OptMountPointBuf, MAX_PATH))
                {
                    OptMountPoint = OptMountPointBuf;
                    rmarg(argv, argc, argi);

                    if (!(testalpha(OptMountPoint[0]) &&
                        L':' == OptMountPoint[1] &&
                        L'\0' == OptMountPoint[2]))
                        WinFspNetTests = 0;
                }
            }
            else if (0 == strncmp("--share=", a, sizeof "--share=" - 1))
            {
                if (0 != MultiByteToWideChar(CP_UTF8, 0,
                    a + sizeof "--share=" - 1, -1, OptShareNameBuf, MAX_PATH))
                {
                    OptShareTarget = wcschr(OptShareNameBuf, L'=');
                    if (OptShareTarget)
                    {
                        *OptShareTarget++ = L'\0';
                        OptShareName = OptShareNameBuf;
                    }
                    else if (L'\\' == OptShareNameBuf[0] && L'\\' == OptShareNameBuf[1])
                    {
                        OptShareName = wcschr(OptShareNameBuf + 2, L'\\');
                        if (OptShareName)
                        {
                            OptShareName++;
                            memcpy(OptShareComputer, OptShareNameBuf,
                                (OptShareName - OptShareNameBuf) * sizeof(WCHAR));
                            OptShareComputer[OptShareName - OptShareNameBuf] = L'\0';
                        }
                    }
                }

                if (OptShareName)
                {
                    rmarg(argv, argc, argi);

                    OptSharePrefixLength = (ULONG)
                        ((wcslen(OptShareComputer) + wcslen(OptShareName) - 1) * sizeof(WCHAR));

                    WinFspDiskTests = 0;
                    WinFspNetTests = 0;
                }
            }
            else if (0 == strncmp("--share-prefix=", a, sizeof "--share-prefix=" - 1))
            {
                /* hack to allow name queries on network file systems with mapped drives */

                WCHAR SharePrefixBuf[MAX_PATH];

                if (0 != MultiByteToWideChar(CP_UTF8, 0,
                    a + sizeof "--share-prefix=" - 1, -1, SharePrefixBuf, MAX_PATH))
                {
                    rmarg(argv, argc, argi);

                    OptSharePrefixLength = (ULONG)(wcslen(SharePrefixBuf) * sizeof(WCHAR));
                }
            }
            else if (0 == strcmp("--no-traverse", a))
            {
                if (LookupPrivilegeValueW(0, SE_CHANGE_NOTIFY_NAME, &OptNoTraverseLuid) &&
                    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &OptNoTraverseToken))
                {
                    rmarg(argv, argc, argi);
                }
            }
        }
    }

    if (!NtfsTests && OptShareName)
        ABORT("option --share requires --ntfs/--external");

    DisableBackupRestorePrivileges();

    AddNetShareIfNeeded();

    myrandseed = (unsigned)time(0);

    tlib_run_tests(argc, argv);
    return 0;
}

static void exiting(void)
{
    OutputDebugStringA("winfsp-tests: exiting\n");

    RemoveNetShareIfNeeded();
}
