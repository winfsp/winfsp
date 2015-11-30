#include <winfsp/winfsp.h>
#include <winfsp/fsctl.h>
#include <tlib/testsuite.h>

void mount_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS Params;

    Params.SectorSize = 65536;
    //Result = FspFsctlCreateVolume(DeviceName, &Params, );
}

void mount_test(void)
{
    mount_dotest("WinFsp.Disk");
    mount_dotest("WinFsp.Net");
}

void mount_tests(void)
{
    TEST(mount_test);
}
