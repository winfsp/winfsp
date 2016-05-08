/**
 * @file memfs-main.c
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

#include <winfsp/winfsp.h>
#include "memfs.h"

#define PROGNAME                        "memfs"

#define info(format, ...)               FspServiceLog(EVENTLOG_INFORMATION_TYPE, format, __VA_ARGS__)
#define warn(format, ...)               FspServiceLog(EVENTLOG_WARNING_TYPE, format, __VA_ARGS__)
#define fail(format, ...)               FspServiceLog(EVENTLOG_ERROR_TYPE, format, __VA_ARGS__)

#define fatal(format, ...)              (fail(format, __VA_ARGS__), exit(ERROR_GEN_FAILURE))

#define argtos(v)                       if (arge > ++argp) v = *argp; else goto usage
#define argtol(v)                       if (arge > ++argp) v = wcstol_deflt(*argp, v); else goto usage

static ULONG wcstol_deflt(wchar_t *w, ULONG deflt)
{
    wchar_t *endp;
    ULONG ul = wcstol(w, &endp, 10);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    wchar_t **argp, **arge;
    ULONG Flags = MemfsDisk;
    ULONG FileInfoTimeout = INFINITE;
    ULONG MaxFileNodes = 1024;
    ULONG MaxFileSize = 16 * 1024 * 1024;
    PWSTR MountPoint = 0;
    PWSTR VolumePrefix = 0;
    PWSTR RootSddl = 0;
    MEMFS *Memfs;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'm':
            argtos(MountPoint);
            break;
        case L'n':
            argtol(MaxFileNodes);
            break;
        case L'S':
            argtos(RootSddl);
            break;
        case L's':
            argtol(MaxFileSize);
            break;
        case L't':
            argtol(FileInfoTimeout);
            break;
        case L'u':
            argtos(VolumePrefix);
            Flags = MemfsNet;
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    Result = MemfsCreate(Flags, FileInfoTimeout, MaxFileNodes, MaxFileSize, VolumePrefix, RootSddl,
        &Memfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create MEMFS");
        goto exit;
    }

    Result = FspFileSystemSetMountPoint(MemfsFileSystem(Memfs), MountPoint);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot mount MEMFS");
        goto exit;
    }

    Result = MemfsStart(Memfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot start MEMFS");
        goto exit;
    }

    MountPoint = FspFileSystemMountPoint(MemfsFileSystem(Memfs));

    Service->UserContext = Memfs;
    Result = STATUS_SUCCESS;

exit:
    info(L"%s -t %ld -n %ld -s %ld%s%s%s%s -m %s",
        L"" PROGNAME, FileInfoTimeout, MaxFileNodes, MaxFileSize,
        RootSddl ? L" -S " : L"", RootSddl ? RootSddl : L"",
        VolumePrefix ? L" -u " : L"", VolumePrefix ? VolumePrefix : L"",
        MountPoint);

    if (!NT_SUCCESS(Result) && 0 != Memfs)
        MemfsDelete(Memfs);

    return Result;

usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -t FileInfoTimeout  [millis]\n"
        "    -n MaxFileNodes\n"
        "    -s MaxFileSize      [bytes]\n"
        "    -S RootSddl         [file rights: FA, etc; NO generic rights: GA, etc.]\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -m MountPoint       [X:]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;
}

NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    MEMFS *Memfs = Service->UserContext;

    MemfsStop(Memfs);
    MemfsDelete(Memfs);

    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    FSP_SERVICE *Service;
    NTSTATUS Result;
    ULONG ExitCode;

    Result = FspServiceCreate(L"" PROGNAME, SvcStart, SvcStop, 0, &Service);
    if (!NT_SUCCESS(Result))
        fatal(L"cannot create service (Status=%lx)", Result);

    FspServiceAllowConsoleMode(Service);
    Result = FspServiceLoop(Service);
    ExitCode = FspServiceGetExitCode(Service);
    FspServiceDelete(Service);

    if (!NT_SUCCESS(Result))
        fatal(L"cannot run service (Status=%lx)", Result);

    return ExitCode;
}
