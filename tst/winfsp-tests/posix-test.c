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
        { L"S-1-5-80-0", 0x50000 },
        { 0, 0 },
    };
    NTSTATUS Result;
    BOOL Success;
    HANDLE Token;
    PTOKEN_USER UserInfo;
    DWORD UserInfoSize;
    PSID Sid0, Sid1;
    UINT32 Uid;

    Success = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &Token);
    ASSERT(Success);

    Success = GetTokenInformation(Token, TokenUser, 0, 0, &UserInfoSize);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());

    UserInfo = malloc(UserInfoSize);
    ASSERT(0 != UserInfo);

    Success = GetTokenInformation(Token, TokenUser, UserInfo, UserInfoSize, &UserInfoSize);
    ASSERT(Success);

    Success = ConvertSidToStringSidW(UserInfo->User.Sid, &map[sizeof map / sizeof map[0] - 1].SidStr);
    ASSERT(Success);

    free(UserInfo);

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

    LocalFree(map[sizeof map / sizeof map[0] - 1].SidStr);
}

void posix_tests(void)
{
    TEST(posix_map_sid_test);
}
