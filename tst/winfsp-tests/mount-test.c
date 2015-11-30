#include <winfsp/winfsp.h>
#include <winfsp/fsctl.h>
#include <tlib/testsuite.h>

void mount_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOLEAN Success;
    FSP_FSCTL_VOLUME_PARAMS Params;
    WCHAR VolumePath[MAX_PATH];
    HANDLE VolumeHandle;

    Params.SectorSize = (UINT16)65536;
    Result = FspFsctlCreateVolume(DeviceName, &Params, 0, VolumePath, sizeof VolumePath);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFsctlOpenVolume(VolumePath, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFsctlDeleteVolume(VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);
}

void mount_test(void)
{
    mount_dotest(L"WinFsp.Disk");
    //mount_dotest(L"WinFsp.Net");
}

void mount_tests(void)
{
    TEST(mount_test);
}
