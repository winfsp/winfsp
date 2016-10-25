#include <windows.h>
#include <lm.h>
#include <signal.h>
#include <tlib/testsuite.h>
#include <time.h>

#include "winfsp-tests.h"

#define ABORT(s)\
    do\
    {\
        tlib_printf("ABORT: %s: %s\n", __func__, s);\
        abort();\
    } while (0,0)

int NtfsTests = 0;
int WinFspDiskTests = 1;
int WinFspNetTests = 1;

BOOLEAN OptCaseInsensitive = FALSE;
BOOLEAN OptCaseRandomize = FALSE;
WCHAR OptMountPointBuf[MAX_PATH], *OptMountPoint;
WCHAR OptShareNameBuf[MAX_PATH], *OptShareName, *OptShareTarget;
    WCHAR OptShareComputer[] = L"\\\\localhost\\";
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
    if (OptCaseRandomize)
        res = _wcsnicmp(a, b, len);
    else
        res = wcsncmp(a, b, len);

    if (0 == res)
        res = alen - blen;

    return res;
}

#define testalpha(c)                    ('a' <= ((c) | 0x20) && ((c) | 0x20) <= 'z')
#define togglealpha(c)                  ((c) ^ 0x20)
#undef CreateFileW
static unsigned myrandseed = 1;
static int myrand(void)
{
    /*
     * This mimics MSVCRT rand(); we need our own version
     * as to not interfere with the program's rand().
     */

    myrandseed = myrandseed * 214013 + 2531011;
    return (myrandseed >> 16) & RAND_MAX;
}
HANDLE HookCreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    static WCHAR DevicePrefix[] =
        L"\\\\?\\GLOBALROOT\\Device\\Volume{01234567-0123-0123-0101-010101010101}";
    static WCHAR MemfsSharePrefix[] =
        L"\\\\memfs\\share";
    static const TogglePercent = 25;
    WCHAR FileNameBuf[1024];
    TOKEN_PRIVILEGES Privileges;
    PWSTR P, EndP;
    size_t L1, L2;

    wcscpy_s(FileNameBuf, sizeof FileNameBuf / sizeof(WCHAR), lpFileName);

    if (OptCaseRandomize)
    {
        if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1] &&
            L'?' == FileNameBuf[2] && L'\\' == FileNameBuf[3] &&
            testalpha(FileNameBuf[4]) && L':' == FileNameBuf[5] && L'\\' == FileNameBuf[6])
            P = FileNameBuf + 7;
        else if (0 == wcsncmp(FileNameBuf, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
            P = FileNameBuf + wcslen(DevicePrefix);
        else if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1])
            P = FileNameBuf + 2;
        else if (testalpha(FileNameBuf[0]) && L':' == FileNameBuf[1] && L'\\' == FileNameBuf[2])
            P = FileNameBuf + 3;
        else
            ABORT("unknown filename format");

        for (EndP = P + wcslen(P); EndP > P; P++)
            if (testalpha(*P) && myrand() <= (TogglePercent) * 0x7fff / 100)
                *P = togglealpha(*P);
    }

    if (OptMountPoint && memfs_running)
    {
        if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1] &&
            L'?' == FileNameBuf[2] && L'\\' == FileNameBuf[3] &&
            testalpha(FileNameBuf[4]) && L':' == FileNameBuf[5] && L'\\' == FileNameBuf[6])
            ABORT("--mountpoint not supported with NTFS");
        else if (0 == wcsncmp(FileNameBuf, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
            P = FileNameBuf + wcslen(DevicePrefix);
        else if (0 == mywcscmp(
            FileNameBuf, (int)wcslen(MemfsSharePrefix), MemfsSharePrefix, (int)wcslen(MemfsSharePrefix)))
            P = FileNameBuf + wcslen(MemfsSharePrefix);
        else if (testalpha(FileNameBuf[0]) && L':' == FileNameBuf[1] && L'\\' == FileNameBuf[2])
            ABORT("--mountpoint not supported with NTFS");
        else
            ABORT("unknown filename format");

        L1 = wcslen(P) + 1;
        L2 = wcslen(OptMountPoint);
        memmove(FileNameBuf + 1024 - L1, P, L1 * sizeof(WCHAR));
        memmove(FileNameBuf, OptMountPoint, L2 * sizeof(WCHAR));
        memmove(FileNameBuf + L2, FileNameBuf + 1024 - L1, L1 * sizeof(WCHAR));
    }

    if (OptShareName && memfs_running)
    {
        if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1] &&
            L'?' == FileNameBuf[2] && L'\\' == FileNameBuf[3] &&
            testalpha(FileNameBuf[4]) && L':' == FileNameBuf[5] && L'\\' == FileNameBuf[6])
            /* NTFS testing can only been done when the whole drive is being shared */
            P = FileNameBuf + 6;
        else if (0 == wcsncmp(FileNameBuf, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
            P = FileNameBuf + wcslen(DevicePrefix);
        else if (0 == mywcscmp(
            FileNameBuf, (int)wcslen(MemfsSharePrefix), MemfsSharePrefix, (int)wcslen(MemfsSharePrefix)))
            P = FileNameBuf + wcslen(MemfsSharePrefix);
        else if (testalpha(FileNameBuf[0]) && L':' == FileNameBuf[1] && L'\\' == FileNameBuf[2])
            /* NTFS testing can only been done when the whole drive is being shared */
            P = FileNameBuf + 2;
        else
            ABORT("unknown filename format");

        L1 = wcslen(P) + 1;
        L2 = wcslen(OptShareName);
        memmove(FileNameBuf + 1024 - L1, P, L1 * sizeof(WCHAR));
        memmove(FileNameBuf, OptShareComputer, sizeof OptShareComputer - sizeof(WCHAR));
        memmove(FileNameBuf + (sizeof OptShareComputer - sizeof(WCHAR)) / sizeof(WCHAR),
            OptShareName, L2 * sizeof(WCHAR));
        memmove(FileNameBuf + (sizeof OptShareComputer - sizeof(WCHAR)) / sizeof(WCHAR) + L2,
            FileNameBuf + 1024 - L1, L1 * sizeof(WCHAR));
    }

    if (OptNoTraverseToken)
    {
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = 0;
        Privileges.Privileges[0].Luid = OptNoTraverseLuid;
        if (!AdjustTokenPrivileges(OptNoTraverseToken, FALSE, &Privileges, 0, 0, 0))
            ABORT("cannot disable traverse privilege");
    }

    HANDLE h = CreateFileW(
        FileNameBuf,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);
    DWORD LastError = GetLastError();

    if (OptNoTraverseToken)
    {
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        Privileges.Privileges[0].Luid = OptNoTraverseLuid;
        if (!AdjustTokenPrivileges(OptNoTraverseToken, FALSE, &Privileges, 0, 0, 0))
            ABORT("cannot enable traverse privilege");
    }

#if 0
    FspDebugLog("CreateFileW(\"%S\", %#lx, %#lx, %p, %#lx, %#lx, %p) = %p[%#lx]\n",
        FileNameBuf,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile,
        h, INVALID_HANDLE_VALUE != h ? 0 : LastError);
#endif

    SetLastError(LastError);
    return h;
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

VOID AddNetShareIfNeeded(VOID)
{
    if (!OptShareName)
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

VOID RemoveNetShareIfNeeded(VOID)
{
    if (!OptShareName)
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
    TESTSUITE(posix_tests);
    TESTSUITE(eventlog_tests);
    TESTSUITE(path_tests);
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
    TESTSUITE(reparse_tests);
    TESTSUITE(stream_tests);

    atexit(exiting);
    signal(SIGABRT, abort_handler);
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    for (int argi = 1; argc > argi; argi++)
    {
        const char *a = argv[argi];
        if ('-' == a[0])
        {
            if (0 == strcmp("--case-insensitive", a))
            {
                OptCaseInsensitive = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strcmp("--case-randomize", a))
            {
                OptCaseRandomize = TRUE;
                OptCaseInsensitive = TRUE;
                rmarg(argv, argc, argi);
            }
            else if (0 == strncmp("--mountpoint=", a, sizeof "--mountpoint=" - 1))
            {
                if (0 != MultiByteToWideChar(CP_UTF8, 0,
                    a + sizeof "--mountpoint=" - 1, -1, OptMountPointBuf, MAX_PATH))
                {
                    OptMountPoint = OptMountPointBuf;
                    rmarg(argv, argc, argi);

                    NtfsTests = 0;
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
                        rmarg(argv, argc, argi);

                        OptSharePrefixLength = (ULONG)
                            (sizeof OptShareComputer - 2 * sizeof(WCHAR) + (wcslen(OptShareName) * sizeof(WCHAR)));

                        WinFspNetTests = 0;
                    }
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

    DisableBackupRestorePrivileges();

    myrandseed = (unsigned)time(0);

    tlib_run_tests(argc, argv);
    return 0;
}

static void exiting(void)
{
    OutputDebugStringA("winfsp-tests: exiting\n");
    RemoveNetShareIfNeeded();
}
