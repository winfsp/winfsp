#include <winfsp/winfsp.h>
#include <winfsp/fsctl.h>
#include <tlib/testsuite.h>
#include <process.h>
#include <strsafe.h>

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

void mount_create_delete_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOL Success;
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

void mount_create_delete_test(void)
{
    mount_create_delete_dotest(L"WinFsp.Disk");
    //mount_create_delete_dotest(L"WinFsp.Net");
}

static unsigned __stdcall mount_volume_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

void mount_volume_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS Params = { 0 };
    WCHAR VolumePath[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE VolumeHandle;
    HANDLE Thread;
    DWORD ExitCode;

    Params.SectorSize = 16384;
    Params.SerialNumber = 0x12345678;
    Result = FspFsctlCreateVolume(DeviceName, &Params, 0, VolumePath, sizeof VolumePath);
    ASSERT(STATUS_SUCCESS == Result);

    StringCbPrintfW(FilePath, sizeof FilePath, L"\\\\?\\GLOBALROOT%s\\file0", VolumePath);
    Thread = (HANDLE)_beginthreadex(0, 0, mount_volume_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Sleep(1000); /* give some time to the thread to execute */

    Result = FspFsctlOpenVolume(VolumePath, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    Result = FspFsctlDeleteVolume(VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);
}

void mount_volume_test(void)
{
    mount_volume_dotest(L"WinFsp.Disk");
    //mount_volume_dotest(L"WinFsp.Net");
}

void mount_tests(void)
{
    TEST(mount_invalid_test);
    TEST(mount_create_delete_test);
    TEST(mount_volume_test);
}
