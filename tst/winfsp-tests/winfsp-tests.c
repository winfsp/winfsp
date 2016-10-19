#include <tlib/testsuite.h>
#include <time.h>

#include "winfsp-tests.h"

int NtfsTests = 0;
int WinFspDiskTests = 1;
int WinFspNetTests = 1;

BOOLEAN OptCaseInsensitive = FALSE;
BOOLEAN OptCaseRandomize = FALSE;
WCHAR OptMountPointBuf[MAX_PATH], *OptMountPoint;
HANDLE OptNoTraverseToken = 0;
    LUID OptNoTraverseLuid;

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
            abort();

        for (EndP = P + wcslen(P); EndP > P; P++)
            if (testalpha(*P) && myrand() <= (TogglePercent) * 0x7fff / 100)
                *P = togglealpha(*P);
    }

    if (OptMountPoint && memfs_running)
    {
        if (L'\\' == FileNameBuf[0] && L'\\' == FileNameBuf[1] &&
            L'?' == FileNameBuf[2] && L'\\' == FileNameBuf[3] &&
            testalpha(FileNameBuf[4]) && L':' == FileNameBuf[5] && L'\\' == FileNameBuf[6])
            ;
        else if (0 == wcsncmp(FileNameBuf, DevicePrefix, wcschr(DevicePrefix, L'{') - DevicePrefix))
        {
            P = FileNameBuf + wcslen(DevicePrefix);
            L1 = wcslen(P) + 1;
            L2 = wcslen(OptMountPoint);
            memmove(FileNameBuf + 1024 - L1, P, L1 * sizeof(WCHAR));
            memmove(FileNameBuf, OptMountPoint, L2 * sizeof(WCHAR));
            memmove(FileNameBuf + L2, P, L1 * sizeof(WCHAR));
        }
        else if (0 == mywcscmp(
            FileNameBuf, (int)wcslen(MemfsSharePrefix), MemfsSharePrefix, (int)wcslen(MemfsSharePrefix)))
        {
            P = FileNameBuf + wcslen(MemfsSharePrefix);
            L1 = wcslen(P) + 1;
            L2 = wcslen(OptMountPoint);
            memmove(FileNameBuf + 1024 - L1, P, L1 * sizeof(WCHAR));
            memmove(FileNameBuf, OptMountPoint, L2 * sizeof(WCHAR));
            memmove(FileNameBuf + L2, P, L1 * sizeof(WCHAR));
        }
        else if (testalpha(FileNameBuf[0]) && L':' == FileNameBuf[1] && L'\\' == FileNameBuf[2])
            ;
        else
            abort();
    }

    if (OptNoTraverseToken)
    {
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = 0;
        Privileges.Privileges[0].Luid = OptNoTraverseLuid;
        if (!AdjustTokenPrivileges(OptNoTraverseToken, FALSE, &Privileges, 0, 0, 0))
            abort();
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
            abort();
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
            else if (0 == strcmp("--no-traverse", a))
            {
                if (LookupPrivilegeValue(0, SE_CHANGE_NOTIFY_NAME, &OptNoTraverseLuid) &&
                    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &OptNoTraverseToken))
                {
                    rmarg(argv, argc, argi);
                }
            }
        }
    }

    myrandseed = (unsigned)time(0);

    tlib_run_tests(argc, argv);
    return 0;
}
