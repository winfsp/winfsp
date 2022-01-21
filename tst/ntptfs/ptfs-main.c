/**
 * @file ptfs-main.c
 *
 * @copyright 2015-2022 Bill Zissimopoulos
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

#include "ptfs.h"

static NTSTATUS EnablePrivileges(PWSTR PrivilegeName, ...)
{
    va_list ap;
    HANDLE Token = 0;
    TOKEN_PRIVILEGES Privileges;
    WCHAR WarnNames[1024], *WarnNameP = WarnNames;
    NTSTATUS Result;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    va_start(ap, PrivilegeName);
    for (PWSTR Name = PrivilegeName; 0 != Name; Name = va_arg(ap, PWSTR))
    {
        Privileges.PrivilegeCount = 1;
        Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!LookupPrivilegeValueW(0, Name, &Privileges.Privileges[0].Luid) ||
            !AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, 0, 0))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        if (ERROR_NOT_ALL_ASSIGNED == GetLastError())
        {
            *WarnNameP++ = ' ';
            size_t len = wcslen(Name);
            memcpy(WarnNameP, Name, len * sizeof(WCHAR));
            WarnNameP += len;
            *WarnNameP = '\0';
        }
    }
    va_end(ap);

    if (WarnNames != WarnNameP)
        warn(L"cannot enable privileges:%s", WarnNames);

    Result = STATUS_SUCCESS;

exit:
    if (0 != Token)
        CloseHandle(Token);

    return Result;
}

static ULONG wcstol_deflt(wchar_t *w, ULONG deflt)
{
    wchar_t *endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
#define argtos(v)                       if (arge > ++argp) v = *argp; else goto usage
#define argtol(v)                       if (arge > ++argp) v = wcstol_deflt(*argp, v); else goto usage

    wchar_t **argp, **arge;
    PWSTR RootPath = 0;
    ULONG FileInfoTimeout = INFINITE;
    ULONG FsAttributeMask = 0;
    PWSTR VolumePrefix = 0;
    PWSTR MountPoint = 0;
    PWSTR OptionString = 0;
    PWSTR DebugLogFile = 0;
    ULONG DebugFlags = 0;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
    WCHAR RootPathBuf[MAX_PATH];
    PTFS *Ptfs = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'd':
            argtol(DebugFlags);
            break;
        case L'D':
            argtos(DebugLogFile);
            break;
        case L'm':
            argtos(MountPoint);
            break;
        case L'o':
            argtos(OptionString);
            if (0 == _wcsicmp(L"ExtraFeatures", OptionString))
                FsAttributeMask |=
                    PtfsReparsePoints |
                    PtfsNamedStreams |
                    PtfsExtendedAttributes |
                    PtfsWslFeatures;
            else if (0 == _wcsicmp(L"ReparsePoints", OptionString))
                FsAttributeMask |= PtfsReparsePoints;
            else if (0 == _wcsicmp(L"NamedStreams", OptionString))
                FsAttributeMask |= PtfsNamedStreams;
            else if (0 == _wcsicmp(L"ExtendedAttributes", OptionString))
                FsAttributeMask |= PtfsExtendedAttributes;
            else if (0 == _wcsicmp(L"WslFeatures", OptionString))
                FsAttributeMask |= PtfsWslFeatures;
            else if (0 == _wcsicmp(L"FlushAndPurgeOnCleanup", OptionString))
                FsAttributeMask |= PtfsFlushAndPurgeOnCleanup;
            else if (0 == _wcsicmp(L"SetAllocationSizeOnCleanup", OptionString))
                FsAttributeMask |= PtfsSetAllocationSizeOnCleanup;
            else
                goto usage;
            break;
        case L'p':
            argtos(RootPath);
            break;
        case L't':
            argtol(FileInfoTimeout);
            break;
        case L'u':
            argtos(VolumePrefix);
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (0 == RootPath && 0 != VolumePrefix)
    {
        PWSTR P;

        P = wcschr(VolumePrefix, L'\\');
        if (0 != P && L'\\' != P[1])
        {
            P = wcschr(P + 1, L'\\');
            if (0 != P &&
                (
                (L'A' <= P[1] && P[1] <= L'Z') ||
                (L'a' <= P[1] && P[1] <= L'z')
                ) &&
                L'$' == P[2])
            {
                StringCbPrintf(RootPathBuf, sizeof RootPathBuf, L"%c:%s", P[1], P + 3);
                RootPath = RootPathBuf;
            }
        }
    }

    if (0 == RootPath || 0 == MountPoint)
        goto usage;

    if (0 != DebugLogFile)
    {
        if (0 == wcscmp(L"-", DebugLogFile))
            DebugLogHandle = GetStdHandle(STD_ERROR_HANDLE);
        else
            DebugLogHandle = CreateFileW(
                DebugLogFile,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == DebugLogHandle)
        {
            fail(L"cannot open debug log file");
            goto usage;
        }

        FspDebugLogSetHandle(DebugLogHandle);
    }

    EnablePrivileges(SE_SECURITY_NAME, SE_BACKUP_NAME, SE_RESTORE_NAME, SE_CREATE_SYMBOLIC_LINK_NAME, 0);

    Result = PtfsCreate(
        RootPath,
        FileInfoTimeout,
        FsAttributeMask,
        VolumePrefix,
        MountPoint,
        DebugFlags,
        &Ptfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create file system");
        goto exit;
    }

    Result = FspFileSystemStartDispatcher(Ptfs->FileSystem, 0);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot start file system");
        goto exit;
    }

    MountPoint = FspFileSystemMountPoint(Ptfs->FileSystem);

    info(L"%s -t %ld%s%s -p %s -m %s",
        L"" PROGNAME,
        FileInfoTimeout,
        0 != VolumePrefix && L'\0' != VolumePrefix[0] ? L" -u " : L"",
            0 != VolumePrefix && L'\0' != VolumePrefix[0] ? VolumePrefix : L"",
        RootPath,
        MountPoint);

    Service->UserContext = Ptfs;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != Ptfs)
        PtfsDelete(Ptfs);

    return Result;

usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -d DebugFlags       [-1: enable all debug logs]\n"
        "    -D DebugLogFile     [file path; use - for stderr]\n"
        "    -t FileInfoTimeout  [millis]\n"
        "    -o ExtraFeatures    [extra Windows file system features]\n"
        "        -o ReparsePoints\n"
        "        -o NamedStreams\n"
        "        -o ExtendedAttributes\n"
        "        -o WslFeatures\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -p Directory        [directory to expose as pass through file system]\n"
        "    -m MountPoint       [X:|*|directory]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;

#undef argtos
#undef argtol
}

static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    PTFS *Ptfs = Service->UserContext;

    FspFileSystemStopDispatcher(Ptfs->FileSystem);
    PtfsDelete(Ptfs);

    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    if (!NT_SUCCESS(FspLoad(0)))
        return ERROR_DELAY_LOAD_FAILED;

    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}
