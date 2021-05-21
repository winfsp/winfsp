/**
 * @file mount-test.c
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
#include <process.h>
#include <strsafe.h>

#include "winfsp-tests.h"

void mount_invalid_test(void)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { 0 };
    WCHAR VolumeName[MAX_PATH] = L"foo";
    HANDLE VolumeHandle;

    VolumeParams.SectorSize = 16384;
    VolumeParams.VolumeSerialNumber = 0x12345678;
    Result = FspFsctlCreateVolume(L"WinFsp.DoesNotExist", &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_NO_SUCH_DEVICE == Result);
    ASSERT(L'\0' == VolumeName[0]);
    ASSERT(INVALID_HANDLE_VALUE == VolumeHandle);
}

void mount_open_device_dotest(PWSTR DeviceName)
{
    BOOL Success;
    HANDLE DeviceHandle;
    WCHAR DevicePath[MAX_PATH];

    StringCbPrintfW(DevicePath, sizeof DevicePath, L"\\\\?\\GLOBALROOT\\Device\\%s", DeviceName);
    DeviceHandle = CreateFileW(DevicePath,
        0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    ASSERT(INVALID_HANDLE_VALUE != DeviceHandle);

    Success = CloseHandle(DeviceHandle);
    ASSERT(Success);
}

void mount_open_device_test(void)
{
    if (WinFspDiskTests)
        mount_open_device_dotest(L"WinFsp.Disk");
    if (WinFspNetTests)
        mount_open_device_dotest(L"WinFsp.Net");
}

void mount_create_volume_v0_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS_V0 VolumeParams = { 0 };
    WCHAR VolumeName[MAX_PATH];
    HANDLE VolumeHandle;

    VolumeParams.SectorSize = 16384;
    VolumeParams.VolumeSerialNumber = 0x12345678;
    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), L"\\winfsp-tests\\share");
    Result = FspFsctlCreateVolume(DeviceName, (FSP_FSCTL_VOLUME_PARAMS *)&VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);
}

void mount_create_volume_v0_test(void)
{
    if (WinFspDiskTests)
        mount_create_volume_v0_dotest(L"WinFsp.Disk");
    if (WinFspNetTests)
        mount_create_volume_v0_dotest(L"WinFsp.Net");
}

void mount_create_volume_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { .Version = sizeof VolumeParams };
    WCHAR VolumeName[MAX_PATH];
    HANDLE VolumeHandle;

    VolumeParams.SectorSize = 16384;
    VolumeParams.VolumeSerialNumber = 0x12345678;
    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), L"\\winfsp-tests\\share");
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);
}

void mount_create_volume_test(void)
{
    if (WinFspDiskTests)
        mount_create_volume_dotest(L"WinFsp.Disk");
    if (WinFspNetTests)
        mount_create_volume_dotest(L"WinFsp.Net");
}

static unsigned __stdcall mount_volume_cancel_dotest_thread(void *FilePath)
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

void mount_volume_cancel_dotest(PWSTR DeviceName, PWSTR Prefix)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { 0 };
    WCHAR VolumeName[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE VolumeHandle;
    HANDLE Thread;
    DWORD ExitCode;

    VolumeParams.SectorSize = 16384;
    VolumeParams.VolumeSerialNumber = 0x12345678;
    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), L"\\winfsp-tests\\share");
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : VolumeName);
    Thread = (HANDLE)_beginthreadex(0, 0, mount_volume_cancel_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Sleep(0 == Prefix ? 1000 : 5000); /* give some time to the thread to execute */

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);
}

void mount_volume_cancel_test(void)
{
    if (WinFspDiskTests)
        mount_volume_cancel_dotest(L"WinFsp.Disk", 0);
    if (WinFspNetTests)
        mount_volume_cancel_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share");
}

static unsigned __stdcall mount_volume_transact_dotest_thread(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

void mount_volume_transact_dotest(PWSTR DeviceName, PWSTR Prefix)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { 0 };
    WCHAR VolumeName[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE VolumeHandle;
    HANDLE Thread;
    DWORD ExitCode;

    VolumeParams.TransactTimeout = 10000; /* allow for longer transact timeout to handle MUP redir */
    VolumeParams.SectorSize = 16384;
    VolumeParams.VolumeSerialNumber = 0x12345678;
    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), L"\\winfsp-tests\\share");
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : VolumeName);
    Thread = (HANDLE)_beginthreadex(0, 0, mount_volume_transact_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    Sleep(1000); /* give some time to the thread to execute */

    FSP_FSCTL_DECLSPEC_ALIGN UINT8 RequestBuf[FSP_FSCTL_TRANSACT_BATCH_BUFFER_SIZEMIN];
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 ResponseBuf[FSP_FSCTL_TRANSACT_RSP_SIZEMAX];
    UINT8 *RequestBufEnd;
    UINT8 *ResponseBufEnd = ResponseBuf + sizeof ResponseBuf;
    SIZE_T RequestBufSize;
    SIZE_T ResponseBufSize;
    FSP_FSCTL_TRANSACT_REQ *Request = (PVOID)RequestBuf, *NextRequest;
    FSP_FSCTL_TRANSACT_RSP *Response = (PVOID)ResponseBuf;

    ResponseBufSize = 0;
    RequestBufSize = sizeof RequestBuf;
    Result = FspFsctlTransact(VolumeHandle, 0, 0, RequestBuf, &RequestBufSize, TRUE);
    ASSERT(STATUS_SUCCESS == Result);

    RequestBufEnd = RequestBuf + RequestBufSize;

    NextRequest = FspFsctlTransactConsumeRequest(Request, RequestBufEnd);
    ASSERT(0 != NextRequest);

    ASSERT(0 == Request->Version);
    ASSERT(FSP_FSCTL_TRANSACT_REQ_SIZEMAX >= Request->Size);
    ASSERT(0 != Request->Hint);
    ASSERT(FspFsctlTransactCreateKind == Request->Kind ||
        FspFsctlTransactQueryVolumeInformationKind == Request->Kind);
    if (FspFsctlTransactCreateKind == Request->Kind)
    {
        ASSERT(FILE_CREATE == ((Request->Req.Create.CreateOptions >> 24) & 0xff));
        ASSERT(0 == Request->Req.Create.FileAttributes);
        ASSERT(0 == Request->Req.Create.SecurityDescriptor.Offset);
        ASSERT(0 == Request->Req.Create.SecurityDescriptor.Size);
        ASSERT(0 == Request->Req.Create.AllocationSize);
        ASSERT(FILE_GENERIC_READ == Request->Req.Create.DesiredAccess);
        ASSERT((FILE_SHARE_READ | FILE_SHARE_WRITE) == Request->Req.Create.ShareAccess);
        ASSERT(0 == Request->Req.Create.Ea.Offset);
        ASSERT(0 == Request->Req.Create.Ea.Size);
        ASSERT(Request->Req.Create.UserMode);
        ASSERT(!OptNoTraverseToken == Request->Req.Create.HasTraversePrivilege);
        ASSERT(!Request->Req.Create.OpenTargetDirectory);
        ASSERT(!Request->Req.Create.CaseSensitive);
        ASSERT(0 == Request->FileName.Offset);
        ASSERT((wcslen((PVOID)Request->Buffer) + 1) * sizeof(WCHAR) == Request->FileName.Size);
        ASSERT(0 == mywcscmp((PVOID)Request->Buffer, -1, L"\\file0", -1));
    }

    ASSERT(FspFsctlTransactCanProduceResponse(Response, ResponseBufEnd));

    RtlZeroMemory(Response, sizeof *Response);
    Response->Size = sizeof *Response;
    Response->Hint = Request->Hint;
    Response->Kind = Request->Kind;
    Response->IoStatus.Status = STATUS_ACCESS_DENIED;
    Response->IoStatus.Information = 0;

    Response = FspFsctlTransactProduceResponse(Response, Response->Size);
    ASSERT(0 != Response);

    Request = NextRequest;
    NextRequest = FspFsctlTransactConsumeRequest(Request, RequestBufEnd);
    ASSERT(0 == NextRequest);

    ResponseBufSize = (PUINT8)Response - ResponseBuf;
    RequestBufSize = 0;
    Result = FspFsctlTransact(VolumeHandle, ResponseBuf, ResponseBufSize, 0, &RequestBufSize, TRUE);
    ASSERT(STATUS_SUCCESS == Result);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_ACCESS_DENIED == ExitCode || ERROR_OPERATION_ABORTED == ExitCode);
}

void mount_volume_transact_test(void)
{
    if (WinFspDiskTests)
        mount_volume_transact_dotest(L"WinFsp.Disk", 0);
    if (WinFspNetTests)
        mount_volume_transact_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share");
}

void mount_preflight_dotest(PWSTR DeviceName)
{
    NTSTATUS Result;
    WCHAR MountPoint[MAX_PATH];
    WCHAR DirBuf[MAX_PATH];
    DWORD Drives;
    WCHAR Drive;
    BOOL Success;

    MountPoint[0] = L'C';
    MountPoint[1] = L':';
    MountPoint[2] = L'\0';

    GetTestDirectory(DirBuf);
    /*
     * Mount points starting with \\?\X: or \\.\X: are now considered MountManager mountpoints.
     * So skip the \\?\ prefix to avoid this problem.
     */
    if (L'\\' == DirBuf[0] &&
        L'\\' == DirBuf[1] &&
        (L'?' == DirBuf[2] || L'.' == DirBuf[2]) &&
        L'\\' == DirBuf[3])
        memmove(DirBuf, DirBuf + 4, (wcslen(DirBuf + 4) + 1) * sizeof(WCHAR));

    Drives = GetLogicalDrives();
    ASSERT(0 != Drives);

    Result = FspFileSystemPreflight(DeviceName, 0);
    ASSERT(STATUS_SUCCESS == Result);

    for (Drive = 'Z'; 'A' <= Drive; Drive--)
        if (0 == (Drives & (1 << (Drive - 'A'))))
            break;
    ASSERT('A' <= Drive);

    MountPoint[0] = Drive;
    Result = FspFileSystemPreflight(DeviceName, MountPoint);
    ASSERT(STATUS_SUCCESS == Result);

    for (Drive = 'Z'; 'A' <= Drive; Drive--)
        if (0 != (Drives & (1 << (Drive - 'A'))))
            break;
    ASSERT('A' <= Drive);

    MountPoint[0] = Drive;
    Result = FspFileSystemPreflight(DeviceName, MountPoint);
    ASSERT(STATUS_OBJECT_NAME_COLLISION == Result);

    StringCbPrintfW(MountPoint, sizeof MountPoint, L"%s\\dir1", DirBuf);

    Result = FspFileSystemPreflight(DeviceName, MountPoint);
    ASSERT(STATUS_SUCCESS == Result);

    Success = CreateDirectoryW(MountPoint, 0);
    ASSERT(Success);

    Result = FspFileSystemPreflight(DeviceName, MountPoint);
    ASSERT(STATUS_OBJECT_NAME_COLLISION == Result);

    Success = RemoveDirectoryW(MountPoint);
    ASSERT(Success);
}

void mount_preflight_test(void)
{
    if (WinFspDiskTests)
        mount_preflight_dotest(L"WinFsp.Disk");
    if (WinFspNetTests)
        mount_preflight_dotest(L"WinFsp.Net");
}

void mount_tests(void)
{
    if (OptExternal || OptOplock)
        return;

    TEST_OPT(mount_invalid_test);
    TEST_OPT(mount_open_device_test);
    TEST_OPT(mount_create_volume_v0_test);
    TEST_OPT(mount_create_volume_test);
    TEST_OPT(mount_volume_cancel_test);
    TEST_OPT(mount_volume_transact_test);
    TEST_OPT(mount_preflight_test);
}
