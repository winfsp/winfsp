#include <winfsp/winfsp.h>
#include <winfsp/fsctl.h>
#include <tlib/testsuite.h>

void mount_invalid_test(void)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS Params = { 0 };
    WCHAR VolumePath[MAX_PATH];
    HANDLE VolumeHandle;

    Params.SectorSize = 16384;
    Params.SerialNumber = 0x12345678;
    Result = FspFsctlCreateVolume(L"WinFsp.DoesNotExist", &Params, 0, VolumePath, sizeof VolumePath);
    ASSERT(STATUS_NO_SUCH_DEVICE == Result);

    Result = FspFsctlOpenVolume(L"\\Device\\Volume{31EF947D-B0F3-4A19-A4E7-BE0C1BE94886}.DoesNotExist", &VolumeHandle);
    ASSERT(STATUS_NO_SUCH_DEVICE == Result);
}

void mount_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOLEAN Success;
    FSP_FSCTL_VOLUME_PARAMS Params = { 0 };
    WCHAR VolumePath[MAX_PATH];
    HANDLE VolumeHandle;

    Params.SectorSize = 16384;
    Params.SerialNumber = 0x12345678;
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
    TEST(mount_invalid_test);
    TEST(mount_test);
}
