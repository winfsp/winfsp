/**
 * @file fscrash-main.c
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
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>

#include "fscrash.h"
#include "memfs.h"

#define fail(format, ...)               fprintf(stderr, format "\n", __VA_ARGS__)
#define ASSERT(expr)                    \
    (!(expr) ?                          \
        (fail("ASSERT(%s) failed at %s:%d:%s\n", #expr, __FILE__, __LINE__, __func__), exit(1)) :\
        (void)0)
    /* do not use abort() in ASSERT to avoid Windows error dialogs */

#define FSCRASH_EVENT_NAME              "fscrash-626ECD8572604254A690C31D563B244C"

static VOID Test(PWSTR Prefix)
{
    static PWSTR Sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)";
    static const GUID ReparseGuid =
        { 0x2cf25cfa, 0x41af, 0x4796, { 0xb5, 0xef, 0xac, 0xa3, 0x85, 0x3, 0xe2, 0xd8 } };
    WCHAR FileName[1024], VolumeName[MAX_PATH];
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    HANDLE Handle;
    BOOL Success;
    UINT8 RdBuffer[4096], WrBuffer[4096];
    REPARSE_GUID_DATA_BUFFER ReparseDataBuf;
    DWORD BytesTransferred, Offset;
    WIN32_FIND_DATAW FindData;
    WIN32_FIND_STREAM_DATA FindStreamData;

    memset(WrBuffer, 'B', sizeof WrBuffer);

    Success = ConvertStringSecurityDescriptorToSecurityDescriptorW(
        Sddl, SDDL_REVISION_1, &SecurityDescriptor, 0);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\", Prefix);
    Success = GetVolumeInformationW(FileName, VolumeName, MAX_PATH, 0, 0, 0, 0, 0);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\", Prefix);
    Success = SetVolumeLabelW(FileName, VolumeName);
    //ASSERT(Success);

    wsprintfW(FileName, L"%s\\fscrash", Prefix);
    Success = CreateDirectoryW(FileName, 0);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\fscrash\\file0", Prefix);
    Handle = CreateFileW(FileName,
        GENERIC_ALL, 0, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    Success = CloseHandle(Handle);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\fscrash\\file0", Prefix);
    Handle = CreateFileW(FileName,
        GENERIC_ALL, 0, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);

    Success = WriteFile(Handle, WrBuffer, sizeof WrBuffer, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(sizeof WrBuffer == BytesTransferred);

    Success = FlushFileBuffers(Handle);
    ASSERT(Success);

    Offset = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == Offset);

    Success = ReadFile(Handle, RdBuffer, sizeof RdBuffer, &BytesTransferred, 0);
    ASSERT(Success);
    ASSERT(sizeof WrBuffer == BytesTransferred);

    Offset = SetFilePointer(Handle, 0, 0, FILE_BEGIN);
    ASSERT(0 == Offset);

    Success = SetEndOfFile(Handle);
    ASSERT(Success);

    Offset = GetFileSize(Handle, 0);
    ASSERT(0 == Offset);

    Success = LockFile(Handle, 0, 0, 1, 0);
    ASSERT(Success);

    Success = UnlockFile(Handle, 0, 0, 1, 0);
    ASSERT(Success);

    Success = SetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION, SecurityDescriptor);
    ASSERT(Success);

    Success = GetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION, 0, 0, &BytesTransferred);
    ASSERT(!Success);
    ASSERT(ERROR_INSUFFICIENT_BUFFER == GetLastError());

    ReparseDataBuf.ReparseTag = 0x1234;
    ReparseDataBuf.ReparseDataLength = 0;
    ReparseDataBuf.Reserved = 0;
    memcpy(&ReparseDataBuf.ReparseGuid, &ReparseGuid, sizeof ReparseGuid);

    Success = DeviceIoControl(Handle, FSCTL_SET_REPARSE_POINT,
        &ReparseDataBuf, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE + ReparseDataBuf.ReparseDataLength,
        0, 0,
        &BytesTransferred, 0);
    ASSERT(Success);

    Success = CloseHandle(Handle);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\fscrash\\*", Prefix);
    Handle = FindFirstFileW(FileName, &FindData);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    do
    {
    } while (FindNextFileW(Handle, &FindData));
    ASSERT(ERROR_NO_MORE_FILES == GetLastError());
    Success = FindClose(Handle);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\fscrash\\file0", Prefix);
    Handle = FindFirstStreamW(FileName, FindStreamInfoStandard, &FindStreamData, 0);
    ASSERT(INVALID_HANDLE_VALUE != Handle);
    do
    {
    } while (FindNextStreamW(Handle, &FindStreamData));
    ASSERT(ERROR_HANDLE_EOF == GetLastError());
    Success = FindClose(Handle);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\fscrash\\file0", Prefix);
    Success = DeleteFileW(FileName);
    ASSERT(Success);

    wsprintfW(FileName, L"%s\\fscrash", Prefix);
    Success = RemoveDirectoryW(FileName);
    ASSERT(Success);

    LocalFree(SecurityDescriptor);
}

static NTSTATUS CreateTestProcess(PWSTR GlobalRoot, PWSTR Prefix, PHANDLE PProcess)
{
    WCHAR Executable[MAX_PATH];
    WCHAR CommandLine[1024];
    STARTUPINFO StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    HANDLE Event;
    DWORD WaitResult, ExitCode = -1, MaxTries = 5;

    *PProcess = 0;

    GetModuleFileName(0, Executable, MAX_PATH);
    wsprintfW(CommandLine, L"%s --run-test=%s%s",
        GetCommandLineW(), GlobalRoot, Prefix);

    Event = CreateEventW(0, TRUE, FALSE, L"" FSCRASH_EVENT_NAME);
    if (0 == Event)
        return FspNtStatusFromWin32(GetLastError());

    memset(&StartupInfo, 0, sizeof StartupInfo);
    memset(&ProcessInfo, 0, sizeof ProcessInfo);
    StartupInfo.cb = sizeof StartupInfo;
    if (!CreateProcessW(Executable, CommandLine, 0, 0, FALSE, 0, 0, 0, &StartupInfo, &ProcessInfo))
        return FspNtStatusFromWin32(GetLastError());

    CloseHandle(ProcessInfo.hThread);

    do
    {
        WaitResult = WaitForSingleObject(Event, 1000);
        GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);
    } while (WAIT_TIMEOUT == WaitResult && 0 != --MaxTries && STILL_ACTIVE == ExitCode);

    if (WAIT_OBJECT_0 != WaitResult)
    {
        CloseHandle(ProcessInfo.hProcess);
        return STATUS_UNSUCCESSFUL;
    }

    *PProcess = ProcessInfo.hProcess;

    return STATUS_SUCCESS;
}

ULONG OptCrashMask = -1, OptCrashFlags = FspCrashInterceptAccessViolation, OptCrashPercent = 10;
ULONG OptMemfsFlags = MemfsDisk, OptFileInfoTimeout = 0;
ULONG OptIterations = -1;
PWSTR OptPrefix = 0;

int wmain(int argc, wchar_t **argv)
{
#if 0
    WCHAR CurrentDirectory[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, CurrentDirectory);
    Test(CurrentDirectory);
    exit(0);
#endif

    for (int argi = 1; argc > argi; argi++)
    {
        wchar_t *a = argv[argi];
        if ('-' == a[0])
        {
            if (0 == wcsncmp(L"--mask=", a, sizeof "--mask=" - 1))
                OptCrashMask = wcstoul(a + sizeof "--mask=" - 1, 0, 0);
            else if (0 == wcscmp(L"--crash", a))
            {
                OptCrashFlags &= ~FspCrashInterceptMask;
                OptCrashFlags |= FspCrashInterceptAccessViolation;
            }
            else if (0 == wcscmp(L"--terminate", a))
            {
                OptCrashFlags &= ~FspCrashInterceptMask;
                OptCrashFlags |= FspCrashInterceptTerminate;
            }
            else if (0 == wcscmp(L"--huge-alloc-size", a))
            {
                OptCrashFlags &= ~FspCrashInterceptMask;
                OptCrashFlags |= FspCrashInterceptHugeAllocationSize;
            }
            else if (0 == wcscmp(L"--enter", a))
                OptCrashFlags |= FspCrashInterceptEnter;
            else if (0 == wcscmp(L"--leave", a))
                OptCrashFlags |= FspCrashInterceptLeave;
            else if (0 == wcsncmp(L"--percent=", a, sizeof "--percent=" - 1))
                OptCrashPercent = wcstoul(a + sizeof "--percent=" - 1, 0, 10);
            else if (0 == wcscmp(L"--disk", a))
                OptMemfsFlags = MemfsDisk;
            else if (0 == wcscmp(L"--net", a))
                OptMemfsFlags = MemfsNet;
            else if (0 == wcscmp(L"--non-cached", a))
                OptFileInfoTimeout = 0;
            else if (0 == wcscmp(L"--cached", a))
                OptFileInfoTimeout = -1;
            else if (0 == wcsncmp(L"--iterations=", a, sizeof "--iterations=" - 1))
                OptIterations = wcstoul(a + sizeof "--iterations=" - 1, 0, 10);
            else if (0 == wcsncmp(L"--run-test=", a, sizeof "--run-test=" - 1))
                OptPrefix = a + sizeof "--run-test=" - 1;
            else
            {
                fail("unknown option %S", a);
                exit(2);
            }
        }
    }

    if (0 == OptPrefix)
    {
        MEMFS *Memfs;
        HANDLE Process;
        DWORD ExitCode = -1;
        NTSTATUS Result;

        Result = MemfsCreate(
            OptMemfsFlags,
            OptFileInfoTimeout,
            1024,
            1024 * 1024,
            (MemfsNet & OptMemfsFlags) ? L"\\memfs\\share" : 0,
            0,
            &Memfs);
        if (!NT_SUCCESS(Result))
        {
            fail("cannot create MEMFS file system: (Status=%lx)", Result);
            exit(1);
        }

        //FspFileSystemSetDebugLog(MemfsFileSystem(Memfs), -1);
        FspCrashIntercept(MemfsFileSystem(Memfs), OptCrashMask, OptCrashFlags, OptCrashPercent);

        Result = MemfsStart(Memfs);
        if (!NT_SUCCESS(Result))
        {
            fail("cannot start MEMFS file system: (Status=%lx)", Result);
            exit(1);
        }

        Result = CreateTestProcess(
            (MemfsNet & OptMemfsFlags) ? L"" : L"\\\\?\\GLOBALROOT",
            (MemfsNet & OptMemfsFlags) ? L"\\memfs\\share" : MemfsFileSystem(Memfs)->VolumeName,
            &Process);
        if (!NT_SUCCESS(Result))
        {
            fail("cannot create test process: (Status=%lx)", Result);
            exit(1);
        }

        FspCrash(MemfsFileSystem(Memfs));
        WaitForSingleObject(Process, INFINITE);
        GetExitCodeProcess(Process, &ExitCode);
        CloseHandle(Process);

        MemfsStop(Memfs);
        MemfsDelete(Memfs);

        if (0 != ExitCode)
        {
            fail("test process exitcode: %lx", ExitCode);
            exit(1);
        }
    }
    else
    {
        HANDLE Event;

        Event = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"" FSCRASH_EVENT_NAME);
        if (0 == Event)
        {
            fail("cannot create test event: (Status=%lx)", FspNtStatusFromWin32(GetLastError()));
            exit(1);
        }

        for (ULONG Iterations = 0; -1 == OptIterations || OptIterations != Iterations; Iterations++)
        {
            Test(OptPrefix);
            SetEvent(Event);
        }
    }

    return 0;
}
