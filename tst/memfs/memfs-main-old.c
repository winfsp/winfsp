/**
 * @file memfs-main-old.c
 *
 * This file serves as an example that it is possible to create
 * a WinFsp file system without using the FspService framework.
 *
 * Please note that this file is no longer maintained.
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
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -t FileInfoTimeout  [millis]\n"
        "    -n MaxFileNodes\n"
        "    -s MaxFileSize      [bytes]\n"
        "    -S RootSddl         [file rights: FA, etc; NO generic rights: GA, etc.]\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -m MountPoint       [X:]\n";

    warn(usage, PROGNAME);
    exit(2);
}

static ULONG argtol(wchar_t **argp, ULONG deflt)
{
    if (0 == argp[0])
        usage();

    wchar_t *endp;
    ULONG ul = wcstol(argp[0], &endp, 10);
    return L'\0' != argp[0][0] && L'\0' == *endp ? ul : deflt;
}

static wchar_t *argtos(wchar_t **argp)
{
    if (0 == argp[0])
        usage();

    return argp[0];
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
    ULONG MaxFileSize = 16 * 1024 * 1024;
    PWSTR MountPoint = 0;
    PWSTR VolumePrefix = 0;
    PWSTR RootSddl = 0;

    for (argp = argv + 1; 0 != argp[0]; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            usage();
            break;
        case L'm':
            MountPoint = argtos(++argp);
            break;
        case L'n':
            MaxFileNodes = argtol(++argp, MaxFileNodes);
            break;
        case L'S':
            RootSddl = argtos(++argp);
            break;
        case L's':
            MaxFileSize = argtol(++argp, MaxFileSize);
            break;
        case L't':
            FileInfoTimeout = argtol(++argp, FileInfoTimeout);
            break;
        case L'u':
            VolumePrefix = argtos(++argp);
            Flags = MemfsNet;
            break;
        default:
            usage();
            break;
        }
    }

    if (0 != argp[0])
        usage();

    MainEvent = CreateEvent(0, TRUE, FALSE, 0);
    if (0 == MainEvent)
        fail("error: cannot create MainEvent");

    Result = MemfsCreate(Flags, FileInfoTimeout, MaxFileNodes, MaxFileSize, VolumePrefix, RootSddl,
        &Memfs);
    if (!NT_SUCCESS(Result))
        fail("error: cannot create MEMFS");
    Result = MemfsStart(Memfs);
    if (!NT_SUCCESS(Result))
        fail("error: cannot start MEMFS");
    Result = FspFileSystemSetMountPoint(MemfsFileSystem(Memfs), MountPoint);
    if (!NT_SUCCESS(Result))
        fail("error: cannot mount MEMFS");
    MountPoint = FspFileSystemMountPoint(MemfsFileSystem(Memfs));

    warn("%s -t %ld -n %ld -s %ld%s%S%s%S -m %S",
        PROGNAME, FileInfoTimeout, MaxFileNodes, MaxFileSize,
        RootSddl ? " -S " : "", RootSddl ? RootSddl : L"",
        VolumePrefix ? " -u " : "", VolumePrefix ? VolumePrefix : L"",
        MountPoint);
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
