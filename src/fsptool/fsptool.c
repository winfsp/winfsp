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
        "    getuid      get current POSIX UID\n"
        "    getgid      get current POSIX GID\n"
        "    uidtosid    get SID from POSIX UID\n"
        "    sidtouid    get POSIX UID from SID\n"
        "    permtosd    get security descriptor from POSIX permissions\n"
        "    sdtoperm    get POSIX permissions from security descriptor\n",
        PROGNAME);
}

static NTSTATUS FspToolGetVolumeList(PWSTR DeviceName,
    PWCHAR *PVolumeListBuf, PSIZE_T PVolumeListSize)
{
    NTSTATUS Result;
    PWCHAR VolumeListBuf;
    SIZE_T VolumeListSize;

    *PVolumeListBuf = 0;
    *PVolumeListSize = 0;

    for (VolumeListSize = 1024;; VolumeListSize *= 2)
    {
        VolumeListBuf = MemAlloc(VolumeListSize);
        if (0 == VolumeListBuf)
            return STATUS_INSUFFICIENT_RESOURCES;

        Result = FspFsctlGetVolumeList(DeviceName,
            VolumeListBuf, &VolumeListSize);
        if (NT_SUCCESS(Result))
        {
            *PVolumeListBuf = VolumeListBuf;
            *PVolumeListSize = VolumeListSize;
            return Result;
        }

        MemFree(VolumeListBuf);

        if (STATUS_BUFFER_TOO_SMALL != Result)
            return Result;
    }
}

static WCHAR FspToolGetDriveLetter(PDWORD PLogicalDrives, PWSTR VolumeName)
{
    WCHAR VolumeNameBuf[MAX_PATH];
    WCHAR LocalNameBuf[3];
    WCHAR Drive;

    if (0 == *PLogicalDrives)
        return 0;

    LocalNameBuf[1] = L':';
    LocalNameBuf[2] = L'\0';

    for (Drive = 'Z'; 'A' <= Drive; Drive--)
        if (0 != (*PLogicalDrives & (1 << (Drive - 'A'))))
        {
            LocalNameBuf[0] = Drive;
            if (QueryDosDeviceW(LocalNameBuf, VolumeNameBuf, sizeof VolumeNameBuf / sizeof(WCHAR)))
            {
                if (0 == invariant_wcscmp(VolumeNameBuf, VolumeName))
                {
                    *PLogicalDrives &= ~(1 << (Drive - 'A'));
                    return Drive;
                }
            }
        }

    return 0;
}

NTSTATUS FspToolGetUid(HANDLE Token, PUINT32 PUid)
{
    UINT32 Uid;
    union
    {
        TOKEN_USER V;
        UINT8 B[128];
    } UserInfoBuf;
    PTOKEN_USER UserInfo = &UserInfoBuf.V;
    DWORD Size;
    NTSTATUS Result;

    if (!GetTokenInformation(Token, TokenUser, UserInfo, sizeof UserInfoBuf, &Size))
    {
        if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        UserInfo = MemAlloc(Size);
        if (0 == UserInfo)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        if (!GetTokenInformation(Token, TokenUser, UserInfo, Size, &Size))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    Result = FspPosixMapSidToUid(UserInfo->User.Sid, &Uid);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PUid = Uid;
    Result = STATUS_SUCCESS;

exit:
    if (UserInfo != &UserInfoBuf.V)
        MemFree(UserInfo);

    return Result;
}

NTSTATUS FspToolGetGid(HANDLE Token, PUINT32 PGid)
{
    UINT32 Gid;
    union
    {
        TOKEN_PRIMARY_GROUP V;
        UINT8 B[128];
    } GroupInfoBuf;
    PTOKEN_PRIMARY_GROUP GroupInfo = &GroupInfoBuf.V;
    DWORD Size;
    NTSTATUS Result;

    if (!GetTokenInformation(Token, TokenPrimaryGroup, GroupInfo, sizeof GroupInfoBuf, &Size))
    {
        if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        GroupInfo = MemAlloc(Size);
        if (0 == GroupInfo)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        if (!GetTokenInformation(Token, TokenPrimaryGroup, GroupInfo, Size, &Size))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    Result = FspPosixMapSidToUid(GroupInfo->PrimaryGroup, &Gid);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PGid = Gid;
    Result = STATUS_SUCCESS;

exit:
    if (GroupInfo != &GroupInfoBuf.V)
        MemFree(GroupInfo);

    return Result;
}

static NTSTATUS lsvol_dev(PWSTR DeviceName)
{
    NTSTATUS Result;
    PWCHAR VolumeListBuf, VolumeListBufEnd;
    SIZE_T VolumeListSize;
    DWORD LogicalDrives;
    WCHAR Drive[3] = L"\0:";

    Result = FspToolGetVolumeList(DeviceName, &VolumeListBuf, &VolumeListSize);
    if (!NT_SUCCESS(Result))
        return Result;
    VolumeListBufEnd = (PVOID)((PUINT8)VolumeListBuf + VolumeListSize);

    LogicalDrives = GetLogicalDrives();
    for (PWCHAR P = VolumeListBuf, VolumeName = P; VolumeListBufEnd > P; P++)
        if (L'\0' == *P)
        {
            Drive[0] = FspToolGetDriveLetter(&LogicalDrives, VolumeName);
            info("%-4S%S", Drive[0] ? Drive : L"", VolumeName);
            VolumeName = P + 1;
        }

    MemFree(VolumeListBuf);

    return STATUS_SUCCESS;
}

static int lsvol(int argc, wchar_t **argv)
{
    if (1 != argc)
        usage();

    NTSTATUS Result;

    Result = lsvol_dev(L"" FSP_FSCTL_DISK_DEVICE_NAME);
    if (!NT_SUCCESS(Result))
        return FspWin32FromNtStatus(Result);

    Result = lsvol_dev(L"" FSP_FSCTL_NET_DEVICE_NAME);
    if (!NT_SUCCESS(Result))
        return FspWin32FromNtStatus(Result);

    return 0;
}

static int getuid(int argc, wchar_t **argv)
{
    if (1 != argc)
        usage();

    HANDLE Token;
    UINT32 Uid;
    NTSTATUS Result;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
        return GetLastError();

    Result = FspToolGetUid(Token, &Uid);
    if (!NT_SUCCESS(Result))
        return FspWin32FromNtStatus(Result);

    CloseHandle(Token);

    info("%u", Uid);

    return 0;
}

static int getgid(int argc, wchar_t **argv)
{
    if (1 != argc)
        usage();

    HANDLE Token;
    UINT32 Gid;
    NTSTATUS Result;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
        return GetLastError();

    Result = FspToolGetGid(Token, &Gid);
    if (!NT_SUCCESS(Result))
        return FspWin32FromNtStatus(Result);

    CloseHandle(Token);

    info("%u", Gid);

    return 0;
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
