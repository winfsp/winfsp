/**
 * @file fsptool/fsptool.c
 *
 * @copyright 2015-2017 Bill Zissimopoulos
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
#include <shared/minimal.h>

#define PROGNAME                        "fsptool"

#define info(format, ...)               printlog(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               printlog(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fatal(ExitCode, format, ...)    (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

static void vprintlog(HANDLE h, const char *format, va_list ap)
{
    char buf[1024];
        /* wvsprintf is only safe with a 1024 byte buffer */
    size_t len;
    DWORD BytesTransferred;

    wvsprintfA(buf, format, ap);
    buf[sizeof buf - 1] = '\0';

    len = lstrlenA(buf);
    buf[len++] = '\n';

    WriteFile(h, buf, (DWORD)len, &BytesTransferred, 0);
}

static void printlog(HANDLE h, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vprintlog(h, format, ap);
    va_end(ap);
}

static void usage(void)
{
    fatal(ERROR_INVALID_PARAMETER,
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    lsvol       list file system devices (volumes)\n"
        //"    list        list running file system processes\n"
        //"    kill        kill file system process\n"
        "    getsid      get current SID\n"
        "    getuid      get current POSIX UID\n"
        "    getgid      get current POSIX GID\n"
        "    uidtosid    get SID from POSIX UID\n"
        "    sidtouid    get POSIX UID from SID\n"
        "    permtosd    get security descriptor from POSIX permissions\n"
        "    sdtoperm    get POSIX permissions from security descriptor\n"
        PROGNAME);
}

static int lsvol(int argc, wchar_t **argv)
{
    return 1;
}

static int getsid(int argc, wchar_t **argv)
{
    return 1;
}

static int getuid(int argc, wchar_t **argv)
{
    return 1;
}

static int getgid(int argc, wchar_t **argv)
{
    return 1;
}

static int uidtosid(int argc, wchar_t **argv)
{
    return 1;
}

static int sidtouid(int argc, wchar_t **argv)
{
    return 1;
}

static int permtosd(int argc, wchar_t **argv)
{
    return 1;
}

static int sdtoperm(int argc, wchar_t **argv)
{
    return 1;
}

int wmain(int argc, wchar_t **argv)
{
    argc--;
    argv++;

    if (0 == argc)
        usage();

    if (0 == invariant_wcscmp(L"lsvol", argv[0]))
        return lsvol(argc, argv);
    else
    if (0 == invariant_wcscmp(L"getsid", argv[0]))
        return getsid(argc, argv);
    else
    if (0 == invariant_wcscmp(L"getuid", argv[0]))
        return getuid(argc, argv);
    else
    if (0 == invariant_wcscmp(L"getgid", argv[0]))
        return getgid(argc, argv);
    else
    if (0 == invariant_wcscmp(L"uidtosid", argv[0]))
        return uidtosid(argc, argv);
    else
    if (0 == invariant_wcscmp(L"sidtouid", argv[0]))
        return sidtouid(argc, argv);
    else
    if (0 == invariant_wcscmp(L"permtosd", argv[0]))
        return permtosd(argc, argv);
    else
    if (0 == invariant_wcscmp(L"sdtoperm", argv[0]))
        return sdtoperm(argc, argv);

    else
        usage();

    return 0;
}

void wmainCRTStartup(void)
{
    DWORD Argc;
    PWSTR *Argv;

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
        ExitProcess(GetLastError());

    ExitProcess(wmain(Argc, Argv));
}
