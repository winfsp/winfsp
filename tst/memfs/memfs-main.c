/**
 * @file memfs-main.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <winfsp/winfsp.h>
#include "memfs.h"

#define PROGNAME                        "memfs"

static void warn(const char *message)
{
    DWORD BytesTransferred;

    WriteFile(GetStdHandle(STD_ERROR_HANDLE),
        message, (DWORD)strlen(message),
        &BytesTransferred, 0);
    WriteFile(GetStdHandle(STD_ERROR_HANDLE),
        "\n", 1,
        &BytesTransferred, 0);
}

static void fail(const char *message)
{
    warn(message);
    exit(1);
}

static void usage(void)
{
    static char usage[] = ""
        "usage: " PROGNAME " OPTIONS MountPoint\n"
        "\n"
        "options:\n"
        "    -t FileInfoTimeout\n"
        "    -n MaxFileNodes\n"
        "    -s MaxFileSize\n";

    warn(usage);
    exit(2);
}

static HANDLE MainEvent;

static BOOL WINAPI ConsoleCtrlHandler(DWORD CtrlType)
{
    SetEvent(MainEvent);
    return TRUE;
}

int wmain(int argc, wchar_t **argv)
{
    NTSTATUS Result;
    MEMFS *Memfs;
    ULONG Flags = MemfsDisk;
    ULONG FileInfoTimeout = INFINITE;
    ULONG MaxFileNodes = 1024;
    ULONG MaxFileSize = 1024 * 1024;

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

    return 0;
}
