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
    };
    NTSTATUS Result;
    BOOL Success;
    PSID Sid;
    PWSTR SidStr;

    for (size_t i = 0; sizeof map / sizeof map[0] > i; i++)
    {
        Result = FspPosixMapUidToSid(map[i].Uid, &Sid);
        ASSERT(NT_SUCCESS(Result));

        Success = ConvertSidToStringSidW(Sid, &SidStr);
        ASSERT(Success);
        ASSERT(0 == wcscmp(map[i].SidStr, SidStr));
        LocalFree(SidStr);

        Result = FspPosixMapSidToUid(Sid, &map[i].Uid);
        ASSERT(NT_SUCCESS(Result));

        FspDeleteSid(Sid, FspPosixMapUidToSid);
    }
}

void posix_tests(void)
{
    TEST(posix_map_sid_test);
}
