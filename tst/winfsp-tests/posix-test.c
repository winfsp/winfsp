/**
 * @file posix-test.c
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
#include <tlib/testsuite.h>
#include <sddl.h>

#include "winfsp-tests.h"

static void posix_map_sid_test(void)
{
    struct
    {
        PWSTR SidStr;
        UINT32 Uid;
    } map[] =
    {
        { L"S-1-0-65534", 65534 },
        { L"S-1-0-0", 0x10000 },
        { L"S-1-1-0", 0x10100 },
        { L"S-1-2-0", 0x10200 },
        { L"S-1-2-1", 0x10201 },
        { L"S-1-3-0", 0x10300 },
        { L"S-1-3-1", 0x10301 },
        { L"S-1-3-2", 0x10302 },
        { L"S-1-3-3", 0x10303 },
        { L"S-1-3-4", 0x10304 },
        { L"S-1-5-1", 1 },
        { L"S-1-5-2", 2 },
        { L"S-1-5-3", 3 },
        { L"S-1-5-4", 4 },
        { L"S-1-5-6", 6 },
        { L"S-1-5-7", 7 },
        { L"S-1-5-8", 8 },
        { L"S-1-5-9", 9 },
        { L"S-1-5-10", 10 },
        { L"S-1-5-11", 11 },
        { L"S-1-5-12", 12 },
        { L"S-1-5-13", 13 },
        { L"S-1-5-14", 14 },
        { L"S-1-5-15", 15 },
        { L"S-1-5-17", 17 },
        { L"S-1-5-18", 18 },
        { L"S-1-5-19", 19 },
        { L"S-1-5-20", 20 },
        { L"S-1-5-32-544", 544 },
        { L"S-1-5-32-545", 545 },
        { L"S-1-5-32-546", 546 },
        { L"S-1-5-32-547", 547 },
        { L"S-1-5-32-548", 548 },
        { L"S-1-5-32-549", 549 },
        { L"S-1-5-32-550", 550 },
        { L"S-1-5-32-551", 551 },
        { L"S-1-5-32-552", 552 },
        { L"S-1-5-32-554", 554 },
        { L"S-1-5-32-555", 555 },
        { L"S-1-5-32-556", 556 },
        { L"S-1-5-32-557", 557 },
        { L"S-1-5-32-558", 558 },
        { L"S-1-5-32-559", 559 },
        { L"S-1-5-32-560", 560 },
        { L"S-1-5-32-561", 561 },
        { L"S-1-5-32-562", 562 },
        { L"S-1-5-32-573", 573 },
        { L"S-1-5-32-574", 574 },
        { L"S-1-5-32-575", 575 },
        { L"S-1-5-32-576", 576 },
        { L"S-1-5-32-577", 577 },
        { L"S-1-5-32-578", 578 },
        { L"S-1-5-32-579", 579 },
        { L"S-1-5-32-580", 580 },
        { L"S-1-5-64-10", 0x4000A },
        { L"S-1-5-64-14", 0x4000E },
        { L"S-1-5-64-21", 0x40015 },
        { L"S-1-5-80-0", 0x50000 },
        { L"S-1-5-83-0", 0x53000 },
        { L"S-1-16-0", 0x60000 },
        { L"S-1-16-4096", 0x61000 },
        { L"S-1-16-8192", 0x62000 },
        { L"S-1-16-8448", 0x62100 },
        { L"S-1-16-12288", 0x63000 },
        { L"S-1-16-16384", 0x64000 },
        { L"S-1-16-20480", 0x65000 },
        { L"S-1-16-28672", 0x67000 },
        { 0, 0 },
        { 0, 0 },
    };
    NTSTATUS Result;
    BOOL Success;
    HANDLE Token;
    PTOKEN_USER UserInfo;
    PTOKEN_PRIMARY_GROUP GroupInfo;
    DWORD InfoSize;
    PSID Sid0, Sid1;
    UINT32 Uid;

    Success = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token);
    ASSERT(Success);

    Success = GetTokenInformation(Token, TokenUser, 0, 0, &InfoSize);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());

    UserInfo = malloc(InfoSize);
    ASSERT(0 != UserInfo);

    Success = GetTokenInformation(Token, TokenUser, UserInfo, InfoSize, &InfoSize);
    ASSERT(Success);

    Success = ConvertSidToStringSidW(UserInfo->User.Sid, &map[sizeof map / sizeof map[0] - 1].SidStr);
    ASSERT(Success);

    free(UserInfo);

    Success = GetTokenInformation(Token, TokenPrimaryGroup, 0, 0, &InfoSize);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());

    GroupInfo = malloc(InfoSize);
    ASSERT(0 != UserInfo);

    Success = GetTokenInformation(Token, TokenPrimaryGroup, GroupInfo, InfoSize, &InfoSize);
    ASSERT(Success);

    Success = ConvertSidToStringSidW(GroupInfo->PrimaryGroup, &map[sizeof map / sizeof map[0] - 2].SidStr);
    ASSERT(Success);

    free(GroupInfo);

    CloseHandle(Token);

    for (size_t i = 0; sizeof map / sizeof map[0] > i; i++)
    {
        Success = ConvertStringSidToSidW(map[i].SidStr, &Sid0);
        ASSERT(Success);

        Result = FspPosixMapSidToUid(Sid0, &Uid);
        ASSERT(NT_SUCCESS(Result));

        if (0 != map[i].Uid)
            ASSERT(Uid == map[i].Uid);

        Result = FspPosixMapUidToSid(Uid, &Sid1);
        ASSERT(NT_SUCCESS(Result));

        ASSERT(EqualSid(Sid0, Sid1));

        FspDeleteSid(Sid1, FspPosixMapUidToSid);
        LocalFree(Sid0);
    }

    LocalFree(map[sizeof map / sizeof map[0] - 2].SidStr);
    LocalFree(map[sizeof map / sizeof map[0] - 1].SidStr);
}

static void posix_map_sd_test(void)
{
    struct
    {
        PWSTR Sddl;
        UINT32 Uid, Gid, Mode;
    } map[] =
    {
        { L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(A;;0x120088;;;BA)(A;;0x120088;;;WD)", 18, 544, 00000 },

        { L"O:SYG:BAD:P(A;;0x1f0199;;;SY)(A;;0x120088;;;BA)(A;;0x120088;;;WD)", 18, 544, 00400 },
        { L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(D;;CC;;;SY)(A;;FR;;;BA)(A;;0x120088;;;WD)", 18, 544, 00040 },
        { L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(D;;CC;;;SY)(A;;0x120088;;;BA)(D;;CC;;;BA)(A;;FR;;;WD)", 18, 544, 00004 },

        { L"O:SYG:BAD:P(A;;0x1f019e;;;SY)(A;;0x120088;;;BA)(A;;0x120088;;;WD)", 18, 544, 00200 },
        { L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(D;;DCLC;;;SY)(A;;0x12018e;;;BA)(A;;0x120088;;;WD)", 18, 544, 00020 },
        { L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(D;;DCLC;;;SY)(A;;0x120088;;;BA)(D;;DCLCCR;;;BA)(A;;0x12018e;;;WD)", 18, 544, 00002 },

        { L"O:SYG:BAD:P(A;;0x1f01b8;;;SY)(A;;0x120088;;;BA)(A;;0x120088;;;WD)", 18, 544, 00100 },
        { L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(D;;WP;;;SY)(A;;0x1200a8;;;BA)(A;;0x120088;;;WD)", 18, 544, 00010 },
        { L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(D;;WP;;;SY)(A;;0x120088;;;BA)(D;;WP;;;BA)(A;;0x1200a8;;;WD)", 18, 544, 00001 },

        { L"O:SYG:BAD:P(A;;0x1f01b9;;;SY)(D;;DCLC;;;SY)(A;;0x1201af;;;BA)(A;;0x1200a9;;;WD)", 18, 544, 00575 },
        { L"O:SYG:BAD:P(A;;0x1f01bf;;;SY)(A;;0x1200a9;;;BA)(D;;DCLCCR;;;BA)(A;;0x1201af;;;WD)", 18, 544, 00757 },

        { L"O:SYG:BAD:P(A;;0x1f01bf;;;SY)(A;;0x1200a9;;;BA)(A;;0x1200a9;;;WD)", 18, 544, 00755 },
        { L"O:SYG:BAD:P(A;;FA;;;SY)(A;;0x1200a9;;;BA)(A;;0x1200a9;;;WD)", 18, 544, 0040755 },

        { L"O:SYG:BAD:P(A;;0x1f01bf;;;SY)(A;;0x1201af;;;BA)(A;;0x1201af;;;WD)", 18, 544, 00777 },
        { L"O:SYG:BAD:P(A;;FA;;;SY)(A;;0x1201ef;;;BA)(A;;0x1201ef;;;WD)", 18, 544, 0040777 },
        { L"O:SYG:BAD:P(A;;FA;;;SY)(A;;0x1201af;;;BA)(A;;0x1201af;;;WD)", 18, 544, 0041777 },

        { L"O:BAG:BAD:P(A;;0x1f0199;;;BA)(A;;FR;;;BA)(A;;FR;;;WD)", 544, 544, 0444 },
    };
    NTSTATUS Result;
    BOOL Success;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    PWSTR Sddl;
    UINT32 Uid, Gid, Mode;

    for (size_t i = 0; sizeof map / sizeof map[0] > i; i++)
    {
        Result = FspPosixMapPermissionsToSecurityDescriptor(
            map[i].Uid, map[i].Gid, map[i].Mode, &SecurityDescriptor);
        ASSERT(NT_SUCCESS(Result));

        Success = ConvertSecurityDescriptorToStringSecurityDescriptorW(
            SecurityDescriptor, SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            &Sddl, 0);
        ASSERT(Success);
        ASSERT(0 == wcscmp(map[i].Sddl, Sddl));
        LocalFree(Sddl);

        Result = FspPosixMapSecurityDescriptorToPermissions(
            SecurityDescriptor, &Uid, &Gid, &Mode);
        ASSERT(NT_SUCCESS(Result));
        ASSERT(map[i].Uid == Uid);
        ASSERT(map[i].Gid == Gid);
        ASSERT((map[i].Mode & 01777) == Mode);

        FspDeleteSecurityDescriptor(SecurityDescriptor,
            FspPosixMapPermissionsToSecurityDescriptor);
    }
}

static void posix_merge_sd_test(void)
{
    struct
    {
        UINT32 Uid, Gid, Mode;
        PWSTR ExistingSddl;
        PWSTR Sddl;
    } map[] =
    {
        {
            18, 544, 00000,
            0,
            L"O:SYG:BAD:P(A;;0x1f0198;;;SY)(A;;0x120088;;;BA)(A;;0x120088;;;WD)"
        },
        {
            18, 544, 00000,
            L"",
            L"O:SYG:BA"
        },
        {
            18, 544, 00000,
            L"O:WD",
            L"O:WDG:BA"
        },
        {
            18, 544, 00000,
            L"G:WD",
            L"O:SYG:WD"
        },
        {
            18, 544, 00000,
            L"O:WDG:WD",
            L"O:WDG:WD"
        },
        {
            18, 544, 00000,
            L"D:P",
            L"O:SYG:BAD:P"
        },
        {
            18, 544, 00000,
            L"D:P(A;;FA;;;SY)",
            L"O:SYG:BAD:P(A;;FA;;;SY)"
        },
        {
            18, 544, 00000,
            L"O:WDG:WDD:P(A;;FA;;;SY)",
            L"O:WDG:WDD:P(A;;FA;;;SY)"
        },
    };
    NTSTATUS Result;
    BOOL Success;
    PSECURITY_DESCRIPTOR ExistingSecurityDescriptor, SecurityDescriptor;
    PWSTR Sddl;

    for (size_t i = 0; sizeof map / sizeof map[0] > i; i++)
    {
        if (0 != map[i].ExistingSddl)
        {
            Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(
                map[i].ExistingSddl, SDDL_REVISION_1,
                &ExistingSecurityDescriptor, 0);
            ASSERT(Success);
        }
        else
            ExistingSecurityDescriptor = 0;

        Result = FspPosixMergePermissionsToSecurityDescriptor(
            map[i].Uid, map[i].Gid, map[i].Mode, ExistingSecurityDescriptor, &SecurityDescriptor);
        ASSERT(NT_SUCCESS(Result));

        Success = ConvertSecurityDescriptorToStringSecurityDescriptorW(
            SecurityDescriptor, SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            &Sddl, 0);
        ASSERT(Success);
        ASSERT(0 == wcscmp(map[i].Sddl, Sddl));
        LocalFree(Sddl);

        FspDeleteSecurityDescriptor(SecurityDescriptor,
            FspPosixMergePermissionsToSecurityDescriptor);

        LocalFree(ExistingSecurityDescriptor);
    }
}

static void posix_map_path_test(void)
{
    struct
    {
        PWSTR WindowsPath;
        const char *PosixPath;
    } map[] =
    {
        { L"\\foo\\bar", "/foo/bar" },
        { L"\\foo\xf03c\xf03e\xf03a\xf02f\xf05c\xf022\xf07c\xf03f\xf02a\\bar", "/foo<>:\xef\x80\xaf\\\"|?*/bar" },
    };
    NTSTATUS Result;
    PWSTR WindowsPath;
    char *PosixPath;

    for (size_t i = 0; sizeof map / sizeof map[0] > i; i++)
    {
        Result = FspPosixMapWindowsToPosixPath(map[i].WindowsPath, &PosixPath);
        ASSERT(NT_SUCCESS(Result));
        ASSERT(0 == strcmp(map[i].PosixPath, PosixPath));

        Result = FspPosixMapPosixToWindowsPath(map[i].PosixPath, &WindowsPath);
        ASSERT(NT_SUCCESS(Result));
        ASSERT(0 == wcscmp(map[i].WindowsPath, WindowsPath));

        FspPosixDeletePath(WindowsPath);
        FspPosixDeletePath(PosixPath);
    }
}

void posix_tests(void)
{
    if (OptExternal)
        return;

    TEST(posix_map_sid_test);
    TEST(posix_map_sd_test);
    TEST(posix_merge_sd_test);
    TEST(posix_map_path_test);
}
