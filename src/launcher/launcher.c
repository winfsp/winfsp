/**
 * @file launcher.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <winfsp/winfsp.h>
#include <shared/minimal.h>
#include <sddl.h>

#define PROGNAME                        "WinFsp-Launcher"
#define REGKEY                          "SYSTEM\\CurrentControlSet\\Services\\" PROGNAME "\\Services"

HANDLE ProcessHeap;

typedef struct
{
    PWSTR ClassName;
    PWSTR InstanceName;
    PWSTR CommandLine;
    DWORD ProcessId;
    HANDLE Process;
    HANDLE ProcessWait;
    LIST_ENTRY ListEntry;
    WCHAR Buffer[];
} SVC_INSTANCE;

static CRITICAL_SECTION SvcInstanceLock;
static LIST_ENTRY SvcInstanceList = { &SvcInstanceList, &SvcInstanceList };

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Fired);

static SVC_INSTANCE *SvcInstanceFromName(PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        if (0 == lstrcmpW(InstanceName, SvcInstance->InstanceName))
            return SvcInstance;
    }

    return 0;
}

static ULONG SvcInstanceArgumentLength(PWSTR Arg)
{
    ULONG Length;

    Length = 2; /* for beginning and ending quotes */
    for (PWSTR P = Arg; *P; P++)
        if (L'"' != *P)
            Length++;

    return Length;
}

static PWSTR SvcInstanceArgumentCopy(PWSTR Dest, PWSTR Arg)
{
    *Dest++ = L'"';
    for (PWSTR P = Arg; *P; P++)
        if (L'"' != *P)
            *Dest++ = *P;
    *Dest++ = L'"';

    return Dest;
}

static NTSTATUS SvcInstanceReplaceArguments(PWSTR String, ULONG Argc, PWSTR *Argv,
    PWSTR *PNewString)
{
    PWSTR NewString = 0;
    ULONG Length;

    *PNewString = 0;

    Length = 0;
    for (PWSTR P = String; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            P++;
            if (L'0' <= *P && *P <= '9' && Argc > (ULONG)(*P - L'0'))
                Length += SvcInstanceArgumentLength(Argv[*P - L'0']);
            break;
        default:
            Length++;
            break;
        }
    }

    NewString = MemAlloc((Length + 1) * sizeof(WCHAR));
    if (0 == NewString)
        return STATUS_INSUFFICIENT_RESOURCES;

    *PNewString = NewString;
    for (PWSTR P = String, Q = NewString; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            P++;
            if (L'0' <= *P && *P <= '9' && Argc > (ULONG)(*P - L'0'))
                Q = SvcInstanceArgumentCopy(Q, Argv[*P - L'0']);
            break;
        default:
            Q++;
            break;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS SvcInstanceCreate(PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv,
    SVC_INSTANCE **PSvcInstance)
{
    SVC_INSTANCE *SvcInstance = 0;
    HKEY RegKey = 0;
    DWORD RegResult, RegSize;
    DWORD ClassNameSize, InstanceNameSize;
    WCHAR Executable[MAX_PATH], CommandLine[512];
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    NTSTATUS Result;

    *PSvcInstance = 0;

    EnterCriticalSection(&SvcInstanceLock);

    if (0 != SvcInstanceFromName(InstanceName))
    {
        Result = STATUS_OBJECT_NAME_COLLISION;
        goto exit;
    }

    RegResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"" REGKEY, 0, KEY_READ, &RegKey);
    if (ERROR_SUCCESS != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    RegSize = sizeof Executable;
    RegResult = RegGetValueW(RegKey, ClassName, L"Executable", RRF_RT_REG_SZ, 0,
        &Executable, &RegSize);
    if (ERROR_SUCCESS != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    RegSize = sizeof CommandLine;
    RegResult = RegGetValueW(RegKey, ClassName, L"CommandLine", RRF_RT_REG_SZ, 0,
        &CommandLine, &RegSize);
    if (ERROR_SUCCESS != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    RegCloseKey(RegKey);
    RegKey = 0;

    ClassNameSize = (lstrlenW(ClassName) + 1) * sizeof(WCHAR);
    InstanceNameSize = (lstrlenW(InstanceName) + 1) * sizeof(WCHAR);

    SvcInstance = MemAlloc(sizeof *SvcInstance + ClassNameSize + InstanceNameSize);
    if (0 == SvcInstance)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    memset(SvcInstance, 0, sizeof *SvcInstance);
    memcpy(SvcInstance->Buffer, ClassName, ClassNameSize);
    memcpy(SvcInstance->Buffer + ClassNameSize / sizeof(WCHAR), InstanceName, InstanceNameSize);
    SvcInstance->ClassName = SvcInstance->Buffer;
    SvcInstance->InstanceName = SvcInstance->Buffer + ClassNameSize / sizeof(WCHAR);

    Result = SvcInstanceReplaceArguments(CommandLine, Argc, Argv, &SvcInstance->CommandLine);
    if (!NT_SUCCESS(Result))
        goto exit;

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    if (!CreateProcessW(0, SvcInstance->CommandLine, 0, 0, FALSE, CREATE_NEW_PROCESS_GROUP, 0, 0,
        &StartupInfo, &ProcessInfo))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    CloseHandle(ProcessInfo.hThread);
    SvcInstance->ProcessId = ProcessInfo.dwProcessId;
    SvcInstance->Process = ProcessInfo.hProcess;

    if (!RegisterWaitForSingleObject(&SvcInstance->ProcessWait, SvcInstance->Process,
        SvcInstanceTerminated, SvcInstance, INFINITE, WT_EXECUTEONLYONCE))
    {
        /* we have no way when the new process will terminate so go ahead and close its handle */
        FspServiceLog(EVENTLOG_WARNING_TYPE,
            L"RegisterWaitForSingleObject = %ld", GetLastError());
        CloseHandle(SvcInstance->Process);
        SvcInstance->Process = 0;
    }

    InsertTailList(&SvcInstanceList, &SvcInstance->ListEntry);

    *PSvcInstance = SvcInstance;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        MemFree(SvcInstance->CommandLine);
        MemFree(SvcInstance);
    }

    if (0 != RegKey)
        RegCloseKey(RegKey);

    LeaveCriticalSection(&SvcInstanceLock);

    return Result;
}

VOID SvcInstanceDelete(SVC_INSTANCE *SvcInstance)
{
    EnterCriticalSection(&SvcInstanceLock);
    RemoveEntryList(&SvcInstance->ListEntry);
    LeaveCriticalSection(&SvcInstanceLock);

    if (0 != SvcInstance->ProcessWait)
        UnregisterWaitEx(SvcInstance->ProcessWait, 0);
    if (0 != SvcInstance->Process)
        CloseHandle(SvcInstance->Process);
    MemFree(SvcInstance->CommandLine);
    MemFree(SvcInstance);
}

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Fired)
{
    SVC_INSTANCE *SvcInstance = Context;

    SvcInstanceDelete(SvcInstance);
}

NTSTATUS SvcInstanceStart(PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv)
{
    SVC_INSTANCE *SvcInstance;

    return SvcInstanceCreate(ClassName, InstanceName, Argc, Argv, &SvcInstance);
}

VOID SvcInstanceStop(PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;

    EnterCriticalSection(&SvcInstanceLock);
    SvcInstance = SvcInstanceFromName(InstanceName);
    if (0 != SvcInstance)
        GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, SvcInstance->ProcessId);
    LeaveCriticalSection(&SvcInstanceLock);
}

static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    InitializeCriticalSection(&SvcInstanceLock);
    return STATUS_SUCCESS;
}

static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    DeleteCriticalSection(&SvcInstanceLock);
    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    ProcessHeap = GetProcessHeap();
    if (0 == ProcessHeap)
        return GetLastError();
    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}

int wmainCRTStartup(void)
{
    return wmain(0, 0);
}
