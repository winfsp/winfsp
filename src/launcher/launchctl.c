/**
 * @file launcher/launchctl.c
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

#include <winfsp/launch.h>
#include <shared/um/minimal.h>

#define PROGNAME                        "launchctl"

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
        "    start               ClassName InstanceName Args...\n"
        "    startWithSecret     ClassName InstanceName Args... Secret\n"
        "    stop                ClassName InstanceName\n"
        "    info                ClassName InstanceName\n"
        "    list\n",
        PROGNAME);
}

static int call_pipe_and_report(PWSTR PipeBuf, ULONG SendSize, ULONG RecvSize)
{
    NTSTATUS Result;
    DWORD LastError, BytesTransferred;

    Result = FspCallNamedPipeSecurelyEx(L"" FSP_LAUNCH_PIPE_NAME, PipeBuf, SendSize, PipeBuf, RecvSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT, TRUE, FSP_LAUNCH_PIPE_OWNER);
    LastError = FspWin32FromNtStatus(Result);

    if (0 != LastError)
        warn("KO CallNamedPipe = %ld", LastError);
    else if (sizeof(WCHAR) > BytesTransferred)
        warn("KO launcher: empty buffer");
    else if (FspLaunchCmdSuccess == PipeBuf[0])
    {
        if (sizeof(WCHAR) == BytesTransferred)
            info("OK");
        else
        {
            ULONG Count = 0;

            for (PWSTR P = PipeBuf, PipeBufEnd = P + BytesTransferred / sizeof(WCHAR);
                PipeBufEnd > P; P++)
                if (L'\0' == *P)
                {
                    /* print a newline every 2 nulls; this works for both list and info */
                    *P = 1 == Count % 2 ? L'\n' : L' ';
                    Count++;
                }

            if (BytesTransferred < RecvSize)
                PipeBuf[BytesTransferred / sizeof(WCHAR)] = L'\0';
            else
                PipeBuf[RecvSize / sizeof(WCHAR) - 1] = L'\0';

            info("OK\n%S", PipeBuf + 1);
        }
    }
    else if (FspLaunchCmdFailure == PipeBuf[0])
    {
        if (BytesTransferred < RecvSize)
            PipeBuf[BytesTransferred / sizeof(WCHAR)] = L'\0';
        else
            PipeBuf[RecvSize / sizeof(WCHAR) - 1] = L'\0';

        info("KO launcher: error %S", PipeBuf + 1);
    }
    else 
        warn("KO launcher: corrupted buffer", 0);

    return LastError;
}

int start(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName, DWORD Argc, PWSTR *Argv,
    BOOLEAN HasSecret)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize, ArgvSize;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;
    ArgvSize = 0;
    for (DWORD Argi = 0; Argc > Argi; Argi++)
        ArgvSize += lstrlenW(Argv[Argi]) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize + ArgvSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = HasSecret ? FspLaunchCmdStartWithSecret : FspLaunchCmdStart;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    for (DWORD Argi = 0; Argc > Argi; Argi++)
    {
        ArgvSize = lstrlenW(Argv[Argi]) + 1;
        memcpy(P, Argv[Argi], ArgvSize * sizeof(WCHAR)); P += ArgvSize;
    }

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int stop(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = FspLaunchCmdStop;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int getinfo(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = FspLaunchCmdGetInfo;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int list(PWSTR PipeBuf, ULONG PipeBufSize)
{
    PWSTR P;

    if (PipeBufSize < 1 * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = FspLaunchCmdGetNameList;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int quit(PWSTR PipeBuf, ULONG PipeBufSize)
{
    /* works only against DEBUG version of launcher */

    PWSTR P;

    if (PipeBufSize < 1 * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = FspLaunchCmdQuit;

    return call_pipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int wmain(int argc, wchar_t **argv)
{
    PWSTR PipeBuf = 0;

    /* allocate our PipeBuf early on; freed on process exit by the system */
    PipeBuf = MemAlloc(FSP_LAUNCH_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
        return ERROR_NO_SYSTEM_RESOURCES;

    argc--;
    argv++;

    if (0 == argc)
        usage();

    if (0 == invariant_wcscmp(L"start", argv[0]))
    {
        if (3 > argc || argc > 12)
            usage();

        return start(PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2], argc - 3, argv + 3,
            FALSE);
    }
    else
    if (0 == invariant_wcscmp(L"startWithSecret", argv[0]))
    {
        if (4 > argc || argc > 13)
            usage();

        return start(PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2], argc - 3, argv + 3,
            TRUE);
    }
    else
    if (0 == invariant_wcscmp(L"stop", argv[0]))
    {
        if (3 != argc)
            usage();

        return stop(PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2]);
    }
    else
    if (0 == invariant_wcscmp(L"info", argv[0]))
    {
        if (3 != argc)
            usage();

        return getinfo(PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE, argv[1], argv[2]);
    }
    else
    if (0 == invariant_wcscmp(L"list", argv[0]))
    {
        if (1 != argc)
            usage();

        return list(PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE);
    }
    else
    if (0 == invariant_wcscmp(L"quit", argv[0]))
    {
        if (1 != argc)
            usage();

        /* works only against DEBUG version of launcher */
        return quit(PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE);
    }
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
