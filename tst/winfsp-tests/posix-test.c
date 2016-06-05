#include <winfsp/winfsp.h>
#include <tlib/testsuite.h>
#include <sddl.h>

void posix_map_sid_test(void)
{
    struct
    {
        PWSTR SidStr;
        UINT32 Uid;
    } map[] =
    {
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

void posix_tests(void)
{
    TEST(posix_map_sid_test);
}
