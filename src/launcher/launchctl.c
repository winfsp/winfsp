/**
 * @file launcher/launchctl.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <launcher/launcher.h>

#define PROGNAME                        "launchctl"

#define info(format, ...)               log(GetStdHandle(STD_OUTPUT_HANDLE), format, __VA_ARGS__)
#define warn(format, ...)               log(GetStdHandle(STD_ERROR_HANDLE), format, __VA_ARGS__)
#define fatal(ExitCode, format, ...)    (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

static void vlog(HANDLE h, const char *format, va_list ap)
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

static void log(HANDLE h, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vlog(h, format, ap);
    va_end(ap);
}

static void usage(void)
{
    fatal(ERROR_INVALID_PARAMETER,
        "usage: %s COMMAND ARGS\n"
        "\n"
        "commands:\n"
        "    start ClassName InstanceName Args...\n"
        "    stop ClassName InstanceName\n"
        "    list\n"
        "    info ClassName InstanceName\n",
        PROGNAME);
}

static int callpipe_and_report(PWSTR PipeBuf, ULONG SendSize, ULONG RecvSize)
{
    DWORD LastError, BytesTransferred;

    LastError = CallNamedPipeW(L"" PIPE_NAME, PipeBuf, SendSize, PipeBuf, RecvSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT) ? 0 : GetLastError();

    if (0 != LastError)
        warn("KO CallNamedPipeW = %ld", LastError);
    else if (0 == BytesTransferred)
        warn("KO launcher: empty buffer");
    else if (L'$' == PipeBuf[0])
    {
        if (1 == BytesTransferred)
            info("OK");
        else
        {
            for (PWSTR P = PipeBuf, PipeBufEnd = P + BytesTransferred / sizeof(WCHAR);
                PipeBufEnd > P; P++)
                if (L'\0' == *P)
                    *P = L'\n';

            if (BytesTransferred < RecvSize)
                PipeBuf[BytesTransferred / sizeof(WCHAR)] = L'\0';
            else
                PipeBuf[RecvSize / sizeof(WCHAR) - 1] = L'\0';

            info("OK\n%S", PipeBuf + 1);
        }
    }
    else if (L'!' == PipeBuf[0])
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
    PWSTR ClassName, PWSTR InstanceName, DWORD Argc, PWSTR *Argv)
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
    *P++ = LauncherSvcInstanceStart;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    for (DWORD Argi = 0; Argc > Argi; Argi++)
    {
        ArgvSize = lstrlenW(Argv[Argi]) + 1;
        memcpy(P, Argv[Argi], ArgvSize * sizeof(WCHAR)); P += ArgvSize;
    }

    return callpipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
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
    *P++ = LauncherSvcInstanceStop;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    return callpipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int list(PWSTR PipeBuf, ULONG PipeBufSize)
{
    PWSTR P;

    if (PipeBufSize < 1 * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = LauncherSvcInstanceList;

    return callpipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
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
    *P++ = LauncherSvcInstanceInfo;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    return callpipe_and_report(PipeBuf, (ULONG)((P - PipeBuf) * sizeof(WCHAR)), PipeBufSize);
}

int wmain(int argc, wchar_t **argv)
{
    PWSTR PipeBuf = 0;

    /* allocate our PipeBuf early on; freed on process exit by the system */
    PipeBuf = MemAlloc(PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
        return ERROR_NO_SYSTEM_RESOURCES;

    argc--;
    argv++;

    if (0 == argc)
        usage();

    if (0 == lstrcmpW(L"start", argv[0]))
    {
        if (3 > argc || argc > 12)
            usage();

        return start(PipeBuf, PIPE_BUFFER_SIZE, argv[1], argv[2], argc - 3, argv + 3);
    }
    else
    if (0 == lstrcmpW(L"stop", argv[0]))
    {
        if (3 != argc)
            usage();

        return stop(PipeBuf, PIPE_BUFFER_SIZE, argv[1], argv[2]);
    }
    else
    if (0 == lstrcmpW(L"list", argv[0]))
    {
        if (1 != argc)
            usage();

        return list(PipeBuf, PIPE_BUFFER_SIZE);
    }
    else
    if (0 == lstrcmpW(L"info", argv[0]))
    {
        if (3 != argc)
            usage();

        return getinfo(PipeBuf, PIPE_BUFFER_SIZE, argv[1], argv[2]);
    }
    else
        usage();

    return 0;
}

void wmainCRTStartup(void)
{
    DWORD Argc;
    PWSTR *Argv;

    extern HANDLE ProcessHeap;
    ProcessHeap = GetProcessHeap();
    if (0 == ProcessHeap)
        ExitProcess(GetLastError());

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
        ExitProcess(GetLastError());

    ExitProcess(wmain(Argc, Argv));
}

HANDLE ProcessHeap;
