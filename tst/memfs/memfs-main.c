/**
 * @file memfs-main.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <winfsp/winfsp.h>
#include "memfs.h"

#define PROGNAME                        "memfs"

static void vwarn(const char *format, va_list ap)
{
    char buf[1024];
        /* wvsprintf is only safe with a 1024 byte buffer */
    DWORD BytesTransferred;

    wvsprintfA(buf, format, ap);
    buf[sizeof buf - 1] = '\0';

    WriteFile(GetStdHandle(STD_ERROR_HANDLE),
        "memfs: ", 7,
        &BytesTransferred, 0);
    WriteFile(GetStdHandle(STD_ERROR_HANDLE),
        buf, (DWORD)strlen(buf),
        &BytesTransferred, 0);
    WriteFile(GetStdHandle(STD_ERROR_HANDLE),
        "\n", 1,
        &BytesTransferred, 0);
}

static void warn(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);
}

static void fail(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
    va_end(ap);

    exit(1);
}

static void usage(void)
{
    static char usage[] = ""
        "usage: %s OPTIONS MountPoint\n"
        "\n"
        "options:\n"
        "    -t FileInfoTimeout\n"
        "    -n MaxFileNodes\n"
        "    -s MaxFileSize\n";

    warn(usage, PROGNAME);
    exit(2);
}

static inline
ULONG argul(wchar_t **argp, ULONG deflt)
{
    if (0 == argp[0])
        usage();

    wchar_t *endp;
    ULONG ul = wcstoul(argp[0], &endp, 10);
    return 0 != ul ? ul : deflt;
}

static HANDLE MainEvent;

static BOOL WINAPI ConsoleCtrlHandler(DWORD CtrlType)
{
    SetEvent(MainEvent);
    return TRUE;
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t **argp;
    NTSTATUS Result;
    MEMFS *Memfs;
    ULONG Flags = MemfsDisk;
    ULONG FileInfoTimeout = INFINITE;
    ULONG MaxFileNodes = 1024;
    ULONG MaxFileSize = 1024 * 1024;
    PWSTR MountPoint = 0;

    for (argp = argv + 1; 0 != argp[0]; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'n':
            MaxFileNodes = argul(++argp, MaxFileNodes);
            break;
        case L's':
            MaxFileSize = argul(++argp, MaxFileSize);
            break;
        case L't':
            FileInfoTimeout = argul(++argp, FileInfoTimeout);
            break;
        default:
            usage();
            break;
        }
    }

    MountPoint = *argp++;
    if (0 == MountPoint || 0 != argp[0])
        usage();

    MainEvent = CreateEvent(0, TRUE, FALSE, 0);
    if (0 == MainEvent)
        fail("error: cannot create MainEvent");

    Result = MemfsCreate(Flags, FileInfoTimeout, MaxFileNodes, MaxFileSize, &Memfs);
    if (!NT_SUCCESS(Result))
        fail("error: cannot create MEMFS");
    Result = MemfsStart(Memfs);
    if (!NT_SUCCESS(Result))
        fail("error: cannot start MEMFS");
    Result = FspFileSystemSetMountPoint(MemfsFileSystem(Memfs), MountPoint);
    if (!NT_SUCCESS(Result))
        fail("error: cannot mount MEMFS");

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    if (WAIT_OBJECT_0 != WaitForSingleObject(MainEvent, INFINITE))
        fail("error: cannot wait on MainEvent");

    FspFileSystemRemoveMountPoint(MemfsFileSystem(Memfs));
    MemfsStop(Memfs);
    MemfsDelete(Memfs);

    /* the OS will handle this! */
    // CloseHandle(MainEvent);
    // MainEvent = 0;

    return 0;
}
