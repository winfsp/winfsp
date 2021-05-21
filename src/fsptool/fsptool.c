/**
 * @file fsptool/fsptool.c
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
#include <shared/um/minimal.h>
#include <aclapi.h>
#include <sddl.h>

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
        "    lsvol                           list file system devices (volumes)\n"
        //"    list                            list running file system processes\n"
        //"    kill                            kill file system process\n"
        "    id [NAME|SID|UID]               print user id\n"
        "    perm [PATH|SDDL|UID:GID:MODE]   print permissions\n",
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

NTSTATUS FspToolGetTokenInfo(HANDLE Token,
    TOKEN_INFORMATION_CLASS TokenInformationClass, PVOID *PInfo)
{
    PVOID Info = 0;
    DWORD Size;
    NTSTATUS Result;

    if (GetTokenInformation(Token, TokenInformationClass, 0, 0, &Size))
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Info = MemAlloc(Size);
    if (0 == Info)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!GetTokenInformation(Token, TokenInformationClass, Info, Size, &Size))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    *PInfo = Info;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
        MemFree(Info);

    return Result;
}

NTSTATUS FspToolGetNameFromSid(PSID Sid, PWSTR *PName)
{
    WCHAR Name[256], Domn[256];
    DWORD NameSize, DomnSize;
    SID_NAME_USE Use;
    PWSTR P;

    NameSize = sizeof Name / sizeof Name[0];
    DomnSize = sizeof Domn / sizeof Domn[0];
    if (!LookupAccountSidW(0, Sid, Name, &NameSize, Domn, &DomnSize, &Use))
    {
        Name[0] = L'\0';
        Domn[0] = L'\0';
    }

    NameSize = lstrlenW(Name);
    DomnSize = lstrlenW(Domn);

    P = *PName = MemAlloc((DomnSize + 1 + NameSize + 1) * sizeof(WCHAR));
    if (0 == P)
        return STATUS_INSUFFICIENT_RESOURCES;

    if (0 < DomnSize)
    {
        memcpy(P, Domn, DomnSize * sizeof(WCHAR));
        P[DomnSize] = L'\\';
        P += DomnSize + 1;
    }
    memcpy(P, Name, NameSize * sizeof(WCHAR));
    P[NameSize] = L'\0';

    return STATUS_SUCCESS;
}

NTSTATUS FspToolGetSidFromName(PWSTR Name, PSID *PSid)
{
    PSID Sid;
    WCHAR Domn[256];
    DWORD SidSize, DomnSize;
    SID_NAME_USE Use;

    SidSize = 0;
    DomnSize = sizeof Domn / sizeof Domn[0];
    if (LookupAccountNameW(0, Name, 0, &SidSize, Domn, &DomnSize, &Use))
        return STATUS_INVALID_PARAMETER;

    if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
        return FspNtStatusFromWin32(GetLastError());

    Sid = MemAlloc(SidSize);
    if (0 == Sid)
        return STATUS_INSUFFICIENT_RESOURCES;

    DomnSize = sizeof Domn / sizeof Domn[0];
    if (!LookupAccountNameW(0, Name, Sid, &SidSize, Domn, &DomnSize, &Use))
    {
        MemFree(Sid);
        return FspNtStatusFromWin32(GetLastError());
    }

    *PSid = Sid;

    return STATUS_SUCCESS;
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

static NTSTATUS id_print_sid(const char *format, PSID Sid)
{
    PWSTR Str = 0;
    PWSTR Name = 0;
    UINT32 Uid;
    NTSTATUS Result;

    if (!ConvertSidToStringSidW(Sid, &Str))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = FspToolGetNameFromSid(Sid, &Name);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspPosixMapSidToUid(Sid, &Uid);
    if (!NT_SUCCESS(Result))
        goto exit;

    info(format, Str, Name, Uid);

    Result = STATUS_SUCCESS;

exit:
    MemFree(Name);
    LocalFree(Str);

    return Result;
}

static NTSTATUS id_name(PWSTR Name)
{
    PSID Sid = 0;
    NTSTATUS Result;

    Result = FspToolGetSidFromName(Name, &Sid);
    if (!NT_SUCCESS(Result))
        return Result;

    id_print_sid("%S(%S) (uid=%u)", Sid);

    MemFree(Sid);

    return STATUS_SUCCESS;
}

static NTSTATUS id_sid(PWSTR SidStr)
{
    PSID Sid = 0;

    if (!ConvertStringSidToSid(SidStr, &Sid))
        return FspNtStatusFromWin32(GetLastError());

    id_print_sid("%S(%S) (uid=%u)", Sid);

    LocalFree(Sid);

    return STATUS_SUCCESS;
}

static NTSTATUS id_uid(PWSTR UidStr)
{
    PSID Sid = 0;
    UINT32 Uid;
    NTSTATUS Result;

    Uid = wcstouint(UidStr, &UidStr, 10, 0);
    if (L'\0' != *UidStr)
        return STATUS_INVALID_PARAMETER;

    Result = FspPosixMapUidToSid(Uid, &Sid);
    if (!NT_SUCCESS(Result))
        return Result;

    id_print_sid("%S(%S) (uid=%u)", Sid);

    FspDeleteSid(Sid, FspPosixMapUidToSid);

    return STATUS_SUCCESS;
}

static NTSTATUS id_user(void)
{
    HANDLE Token = 0;
    TOKEN_USER *Uinfo = 0;
    TOKEN_OWNER *Oinfo = 0;
    TOKEN_PRIMARY_GROUP *Ginfo = 0;
    NTSTATUS Result;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = FspToolGetTokenInfo(Token, TokenUser, &Uinfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspToolGetTokenInfo(Token, TokenOwner, &Oinfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspToolGetTokenInfo(Token, TokenPrimaryGroup, &Ginfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    id_print_sid("User=%S(%S) (uid=%u)", Uinfo->User.Sid);
    id_print_sid("Owner=%S(%S) (uid=%u)", Oinfo->Owner);
    id_print_sid("Group=%S(%S) (gid=%u)", Ginfo->PrimaryGroup);

    Result = STATUS_SUCCESS;

exit:
    MemFree(Ginfo);
    MemFree(Oinfo);
    MemFree(Uinfo);

    if (0 != Token)
        CloseHandle(Token);

    return Result;
}

static int id(int argc, wchar_t **argv)
{
    if (2 < argc)
        usage();

    NTSTATUS Result;

    if (2 == argc)
    {
        if (L'S' == argv[1][0] && L'-' == argv[1][1] && L'1' == argv[1][2] && L'-' == argv[1][3])
            Result = id_sid(argv[1]);
        else
        {
            Result = id_uid(argv[1]);
            if (STATUS_INVALID_PARAMETER == Result)
                Result = id_name(argv[1]);
        }
    }
    else
        Result = id_user();

    return FspWin32FromNtStatus(Result);
}

static NTSTATUS perm_print_sd(PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    UINT32 Uid, Gid, Mode;
    PWSTR Sddl = 0;
    NTSTATUS Result;

    Result = FspPosixMapSecurityDescriptorToPermissions(SecurityDescriptor, &Uid, &Gid, &Mode);
    if (!NT_SUCCESS(Result))
        return Result;

    if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(
        SecurityDescriptor, SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &Sddl, 0))
        return FspNtStatusFromWin32(GetLastError());

    info("%S (perm=%u:%u:%d%d%d%d)",
        Sddl, Uid, Gid, (Mode >> 9) & 7, (Mode >> 6) & 7, (Mode >> 3) & 7, Mode & 7);

    LocalFree(Sddl);

    return STATUS_SUCCESS;
}

static NTSTATUS perm_path(PWSTR Path)
{
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    int ErrorCode;

    ErrorCode = GetNamedSecurityInfoW(Path, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        0, 0, 0, 0, &SecurityDescriptor);
    if (0 != ErrorCode)
        return FspNtStatusFromWin32(ErrorCode);

    perm_print_sd(SecurityDescriptor);

    LocalFree(SecurityDescriptor);

    return STATUS_SUCCESS;
}

static NTSTATUS perm_sddl(PWSTR Sddl)
{
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0))
        return FspNtStatusFromWin32(GetLastError());

    perm_print_sd(SecurityDescriptor);

    LocalFree(SecurityDescriptor);

    return STATUS_SUCCESS;
}

static NTSTATUS perm_mode(PWSTR PermStr)
{
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    UINT32 Uid, Gid, Mode;
    NTSTATUS Result;

    Uid = wcstouint(PermStr, &PermStr, 10, 0);
    if (L':' != *PermStr)
        return STATUS_INVALID_PARAMETER;
    Gid = wcstouint(PermStr + 1, &PermStr, 10, 0);
    if (L':' != *PermStr)
        return STATUS_INVALID_PARAMETER;
    Mode = wcstouint(PermStr + 1, &PermStr, 8, 0);
    if (L'\0' != *PermStr)
        return STATUS_INVALID_PARAMETER;

    Result = FspPosixMapPermissionsToSecurityDescriptor(Uid, Gid, Mode, &SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        return Result;

    perm_print_sd(SecurityDescriptor);

    FspDeleteSecurityDescriptor(SecurityDescriptor,
        FspPosixMapPermissionsToSecurityDescriptor);

    return STATUS_SUCCESS;
}

static int perm(int argc, wchar_t **argv)
{
    if (2 != argc)
        usage();

    NTSTATUS Result;
    PWSTR P;

    for (P = argv[1]; *P; P++)
        if (L'\\' == *P)
            break;

    if (L'\\' == *P)
        Result = perm_path(argv[1]);
    else
    {
        Result = perm_mode(argv[1]);
        if (STATUS_INVALID_PARAMETER == Result)
            Result = perm_sddl(argv[1]);
    }

    return FspWin32FromNtStatus(Result);
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
    if (0 == invariant_wcscmp(L"id", argv[0]))
        return id(argc, argv);
    else
    if (0 == invariant_wcscmp(L"perm", argv[0]))
        return perm(argc, argv);
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
