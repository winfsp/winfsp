/**
 * @file timeout-test.c
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

static unsigned __stdcall timeout_pending_dotest_thread(void *FilePath)
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

static unsigned __stdcall timeout_pending_dotest_thread2(void *FilePath)
{
    FspDebugLog(__FUNCTION__ ": \"%S\"\n", FilePath);

    Sleep(500);

    HANDLE Handle;
    Handle = CreateFileW(FilePath,
        FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return GetLastError();
    CloseHandle(Handle);
    return 0;
}

void timeout_pending_dotest(PWSTR DeviceName, PWSTR Prefix)
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
    VolumeParams.IrpTimeout = FspFsctlIrpTimeoutDebug;
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
    Thread = (HANDLE)_beginthreadex(0, 0, timeout_pending_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);

#if 0
    /* we no longer allow canceling IRP's after they started processing */
    Thread = (HANDLE)_beginthreadex(0, 0, timeout_pending_dotest_thread2, FilePath, 0, 0);
    ASSERT(0 != Thread);

    FSP_FSCTL_DECLSPEC_ALIGN UINT8 RequestBuf[FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMIN];
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 ResponseBuf[FSP_FSCTL_TRANSACT_RSP_SIZEMAX];
    UINT8 *RequestBufEnd;
    UINT8 *ResponseBufEnd = ResponseBuf + sizeof ResponseBuf;
    SIZE_T RequestBufSize;
    SIZE_T ResponseBufSize;
    FSP_FSCTL_TRANSACT_REQ *Request = (PVOID)RequestBuf, *NextRequest;
    FSP_FSCTL_TRANSACT_RSP *Response = (PVOID)ResponseBuf;

    ResponseBufSize = 0;
    RequestBufSize = sizeof RequestBuf;
    Result = FspFsctlTransact(VolumeHandle, 0, 0, RequestBuf, &RequestBufSize);
    ASSERT(STATUS_SUCCESS == Result);

    RequestBufEnd = RequestBuf + RequestBufSize;

    NextRequest = FspFsctlTransactConsumeRequest(Request, RequestBufEnd);
    ASSERT(0 != NextRequest);

    ASSERT(0 == Request->Version);
    ASSERT(FSP_FSCTL_TRANSACT_REQ_SIZEMAX >= Request->Size);
    ASSERT(0 != Request->Hint);
    ASSERT(FspFsctlTransactCreateKind == Request->Kind);
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
    ASSERT(
        0 == wcscmp((PVOID)Request->Buffer, L"\\file0") ||
        0 == wcscmp((PVOID)Request->Buffer, FilePath + 1));

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);
#endif

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);
}

void timeout_pending_test(void)
{
    if (WinFspDiskTests)
        timeout_pending_dotest(L"WinFsp.Disk", 0);
    if (WinFspNetTests)
        timeout_pending_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share");
}

static unsigned __stdcall timeout_transact_dotest_thread(void *FilePath)
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

void timeout_transact_dotest(PWSTR DeviceName, PWSTR Prefix)
{
    NTSTATUS Result;
    BOOL Success;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams = { 0 };
    WCHAR VolumeName[MAX_PATH];
    WCHAR FilePath[MAX_PATH];
    HANDLE VolumeHandle;
    HANDLE Thread;
    DWORD ExitCode;

    VolumeParams.TransactTimeout = 0 != Prefix ? 1000 : 5000;
    VolumeParams.SectorSize = 16384;
    VolumeParams.VolumeSerialNumber = 0x12345678;
    wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), L"\\winfsp-tests\\share");
    Result = FspFsctlCreateVolume(DeviceName, &VolumeParams,
        VolumeName, sizeof VolumeName, &VolumeHandle);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == wcsncmp(L"\\Device\\Volume{", VolumeName, 15));
    ASSERT(INVALID_HANDLE_VALUE != VolumeHandle);


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
    Result = FspFsctlTransact(VolumeHandle, ResponseBuf, ResponseBufSize, RequestBuf, &RequestBufSize, TRUE);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == RequestBufSize);

    StringCbPrintfW(FilePath, sizeof FilePath, L"%s%s\\file0",
        Prefix ? L"" : L"\\\\?\\GLOBALROOT", Prefix ? Prefix : VolumeName);
    Thread = (HANDLE)_beginthreadex(0, 0, timeout_transact_dotest_thread, FilePath, 0, 0);
    ASSERT(0 != Thread);

    ResponseBufSize = 0;
    RequestBufSize = sizeof RequestBuf;
    Result = FspFsctlTransact(VolumeHandle, ResponseBuf, ResponseBufSize, RequestBuf, &RequestBufSize, TRUE);
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
    }

    ResponseBufSize = 0;
    RequestBufSize = sizeof RequestBuf;
    Result = FspFsctlTransact(VolumeHandle, ResponseBuf, ResponseBufSize, RequestBuf, &RequestBufSize, TRUE);
    ASSERT(STATUS_SUCCESS == Result);
    ASSERT(0 == RequestBufSize);

    Success = CloseHandle(VolumeHandle);
    ASSERT(Success);

    WaitForSingleObject(Thread, INFINITE);
    GetExitCodeThread(Thread, &ExitCode);
    CloseHandle(Thread);

    ASSERT(ERROR_OPERATION_ABORTED == ExitCode);
}

void timeout_transact_test(void)
{
    if (WinFspDiskTests)
        timeout_transact_dotest(L"WinFsp.Disk", 0);
    if (WinFspNetTests)
        timeout_transact_dotest(L"WinFsp.Net", L"\\\\winfsp-tests\\share");
}

void timeout_tests(void)
{
    if (OptExternal || OptOplock)
        return;

    TEST_OPT(timeout_pending_test);
    TEST_OPT(timeout_transact_test);
}
