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

#define fatal(ExitCode, format, ...)    (warn(format, __VA_ARGS__), ExitProcess(ExitCode))

static void vwarn(const char *format, va_list ap)
{
    char buf[1024];
        /* wvsprintf is only safe with a 1024 byte buffer */
    size_t len;
    DWORD BytesTransferred;

    wvsprintfA(buf, format, ap);
    buf[sizeof buf - 1] = '\0';

    len = lstrlenA(buf);
    buf[len++] = '\n';

    WriteFile(GetStdHandle(STD_ERROR_HANDLE),
        buf, (DWORD)len, &BytesTransferred, 0);
}

static void warn(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vwarn(format, ap);
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

static void report(PWSTR PipeBuf, ULONG PipeBufSize)
{
    if (0 == PipeBufSize)
        warn("KO received empty buffer from launcher");
    else if (L'$' == PipeBuf[0])
    {
        if (1 == PipeBufSize)
            warn("OK");
        else
        {
            for (PWSTR P = PipeBuf, PipeBufEnd = P + PipeBufSize / sizeof(WCHAR); PipeBufEnd > P; P++)
                if (L'\0' == *P)
                    *P = L'\n';

            if (PipeBufSize < PIPE_BUFFER_SIZE)
                PipeBuf[PipeBufSize / sizeof(WCHAR)] = L'\0';
            else
                PipeBuf[PIPE_BUFFER_SIZE / sizeof(WCHAR) - 1] = L'\0';

            warn("OK\n%S", PipeBuf + 1);
        }
    }
    else if (L'!' == PipeBuf[0])
    {
        if (PipeBufSize < PIPE_BUFFER_SIZE)
            PipeBuf[PipeBufSize / sizeof(WCHAR)] = L'\0';
        else
            PipeBuf[PIPE_BUFFER_SIZE / sizeof(WCHAR) - 1] = L'\0';

        warn("KO %S", PipeBuf + 1);
    }
    else 
        warn("KO received corrupted buffer from launcher", 0);
}

int start(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName, DWORD Argc, PWSTR *Argv)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize, ArgvSize;
    DWORD LastError, BytesTransferred;

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

    if (CallNamedPipeW(L"" PIPE_NAME,
        PipeBuf, (DWORD)((P - PipeBuf) * sizeof(WCHAR)), PipeBuf, PipeBufSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT))
    {
        LastError = 0;
        report(PipeBuf, BytesTransferred);
    }
    else
        LastError = GetLastError();

    return LastError;
}

int stop(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize;
    DWORD LastError, BytesTransferred;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = LauncherSvcInstanceStop;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    if (CallNamedPipeW(L"" PIPE_NAME,
        PipeBuf, (DWORD)((P - PipeBuf) * sizeof(WCHAR)), PipeBuf, PipeBufSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT))
    {
        LastError = 0;
        report(PipeBuf, BytesTransferred);
    }
    else
        LastError = GetLastError();

    return LastError;
}

int list(PWSTR PipeBuf, ULONG PipeBufSize)
{
    PWSTR P;
    DWORD LastError, BytesTransferred;

    if (PipeBufSize < 1 * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = LauncherSvcInstanceList;

    if (CallNamedPipeW(L"" PIPE_NAME,
        PipeBuf, (DWORD)((P - PipeBuf) * sizeof(WCHAR)), PipeBuf, PipeBufSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT))
    {
        LastError = 0;
        report(PipeBuf, BytesTransferred);
    }
    else
        LastError = GetLastError();

    return LastError;
}

int info(PWSTR PipeBuf, ULONG PipeBufSize,
    PWSTR ClassName, PWSTR InstanceName)
{
    PWSTR P;
    DWORD ClassNameSize, InstanceNameSize;
    DWORD LastError, BytesTransferred;

    ClassNameSize = lstrlenW(ClassName) + 1;
    InstanceNameSize = lstrlenW(InstanceName) + 1;

    if (PipeBufSize < (1 + ClassNameSize + InstanceNameSize) * sizeof(WCHAR))
        return ERROR_INVALID_PARAMETER;

    P = PipeBuf;
    *P++ = LauncherSvcInstanceInfo;
    memcpy(P, ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;

    if (CallNamedPipeW(L"" PIPE_NAME,
        PipeBuf, (DWORD)((P - PipeBuf) * sizeof(WCHAR)), PipeBuf, PipeBufSize,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT))
    {
        LastError = 0;
        report(PipeBuf, BytesTransferred);
    }
    else
        LastError = GetLastError();

    return LastError;
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

        return info(PipeBuf, PIPE_BUFFER_SIZE, argv[1], argv[2]);
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
