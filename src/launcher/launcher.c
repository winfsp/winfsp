/**
 * @file launcher/launcher.c
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

#include <launcher/launcher.h>
#include <sddl.h>

#define PROGNAME                        "WinFsp.Launcher"
#define REGKEY                          "SYSTEM\\CurrentControlSet\\Services\\" PROGNAME "\\Services"

typedef struct
{
    PWSTR ClassName;
    PWSTR InstanceName;
    PWSTR CommandLine;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    DWORD ProcessId;
    HANDLE Process;
    HANDLE ProcessWait;
    LIST_ENTRY ListEntry;
    WCHAR Buffer[];
} SVC_INSTANCE;

static CRITICAL_SECTION SvcInstanceLock;
static LIST_ENTRY SvcInstanceList = { &SvcInstanceList, &SvcInstanceList };

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Fired);

static SVC_INSTANCE *SvcInstanceLookup(PWSTR ClassName, PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        if (0 == lstrcmpW(ClassName, SvcInstance->ClassName) &&
            0 == lstrcmpW(InstanceName, SvcInstance->InstanceName))
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

static NTSTATUS SvcInstanceAccessCheck(HANDLE ClientToken, ULONG DesiredAccess,
    PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    static GENERIC_MAPPING GenericMapping =
    {
        .GenericRead = STANDARD_RIGHTS_READ | SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS |
            SERVICE_INTERROGATE | SERVICE_ENUMERATE_DEPENDENTS,
        .GenericWrite = STANDARD_RIGHTS_WRITE | SERVICE_CHANGE_CONFIG,
        .GenericExecute = STANDARD_RIGHTS_EXECUTE | SERVICE_START | SERVICE_STOP |
            SERVICE_PAUSE_CONTINUE | SERVICE_USER_DEFINED_CONTROL,
        .GenericAll = SERVICE_ALL_ACCESS,
    };
    UINT8 PrivilegeSetBuf[sizeof(PRIVILEGE_SET) + 15 * sizeof(LUID_AND_ATTRIBUTES)];
    PPRIVILEGE_SET PrivilegeSet = (PVOID)PrivilegeSetBuf;
    DWORD PrivilegeSetLength = sizeof PrivilegeSetBuf;
    ULONG GrantedAccess;
    BOOL AccessStatus;
    NTSTATUS Result;

    if (AccessCheck(SecurityDescriptor, ClientToken, DesiredAccess,
        &GenericMapping, PrivilegeSet, &PrivilegeSetLength, &GrantedAccess, &AccessStatus))
        Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
    else
        Result = FspNtStatusFromWin32(GetLastError());

    return Result;
}

NTSTATUS SvcInstanceCreate(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv,
    SVC_INSTANCE **PSvcInstance)
{
    SVC_INSTANCE *SvcInstance = 0;
    HKEY RegKey = 0;
    DWORD RegResult, RegSize;
    DWORD ClassNameSize, InstanceNameSize;
    WCHAR Executable[MAX_PATH], CommandLine[512], SecurityBuf[512] = L"O:SYG:SY";
    PWSTR Security;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    NTSTATUS Result;

    *PSvcInstance = 0;

    EnterCriticalSection(&SvcInstanceLock);

    if (0 != SvcInstanceLookup(ClassName, InstanceName))
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
    Executable[0] = L'\0';
    RegResult = RegGetValueW(RegKey, ClassName, L"Executable", RRF_RT_REG_SZ, 0,
        Executable, &RegSize);
    if (ERROR_SUCCESS != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    RegSize = sizeof CommandLine;
    CommandLine[0] = L'\0';
    RegResult = RegGetValueW(RegKey, ClassName, L"CommandLine", RRF_RT_REG_SZ, 0,
        CommandLine, &RegSize);
    if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    Security = SecurityBuf + lstrlenW(SecurityBuf);
    RegSize = (DWORD)(sizeof SecurityBuf - (Security - SecurityBuf) * sizeof(WCHAR));
    RegResult = RegGetValueW(RegKey, ClassName, L"Security", RRF_RT_REG_SZ, 0,
        Security, &RegSize);
    if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    RegCloseKey(RegKey);
    RegKey = 0;

    if (L'\0' == Security)
        lstrcpyW(Security, L"" SVC_INSTANCE_DEFAULT_SDDL);
    if (L'D' == Security[0] && L':' == Security[1])
        Security = SecurityBuf;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(Security, SDDL_REVISION_1,
        &SecurityDescriptor, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    FspDebugLogSD(__FUNCTION__ ": SDDL = %s", SecurityDescriptor);

    Result = SvcInstanceAccessCheck(ClientToken, SERVICE_START, SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

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
    SvcInstance->SecurityDescriptor = SecurityDescriptor;

    if (L'\0' != CommandLine)
    {
        Result = SvcInstanceReplaceArguments(CommandLine, Argc, Argv, &SvcInstance->CommandLine);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    memset(&StartupInfo, 0, sizeof StartupInfo);
    StartupInfo.cb = sizeof StartupInfo;
    if (!CreateProcessW(Executable, SvcInstance->CommandLine, 0, 0, FALSE, CREATE_NEW_PROCESS_GROUP, 0, 0,
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
        LocalFree(SecurityDescriptor);
        if (0 != SvcInstance)
        {
            MemFree(SvcInstance->CommandLine);
            MemFree(SvcInstance);
        }
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
    LocalFree(SvcInstance->SecurityDescriptor);
    MemFree(SvcInstance->CommandLine);
    MemFree(SvcInstance);
}

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Fired)
{
    SVC_INSTANCE *SvcInstance = Context;

    SvcInstanceDelete(SvcInstance);
}

NTSTATUS SvcInstanceStart(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv)
{
    SVC_INSTANCE *SvcInstance;

    return SvcInstanceCreate(ClientToken, ClassName, InstanceName, Argc, Argv, &SvcInstance);
}

NTSTATUS SvcInstanceStop(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;
    NTSTATUS Result;

    EnterCriticalSection(&SvcInstanceLock);

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName);
    if (0 == SvcInstance)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        goto exit;
    }

    Result = SvcInstanceAccessCheck(ClientToken, SERVICE_STOP, SvcInstance->SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, SvcInstance->ProcessId))
    {
        Result = FspNtStatusFromWin32(Result);
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    LeaveCriticalSection(&SvcInstanceLock);

    return Result;
}

NTSTATUS SvcInstanceGetInfo(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, PWSTR Buffer, PULONG PSize)
{
    SVC_INSTANCE *SvcInstance;
    PWSTR P = Buffer;
    ULONG ClassNameSize, InstanceNameSize, CommandLineSize;
    NTSTATUS Result;

    *PSize = 0;

    EnterCriticalSection(&SvcInstanceLock);

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName);
    if (0 == SvcInstance)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        goto exit;
    }

    Result = SvcInstanceAccessCheck(ClientToken, SERVICE_QUERY_STATUS, SvcInstance->SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    ClassNameSize = lstrlenW(SvcInstance->ClassName) + 1;
    InstanceNameSize = lstrlenW(SvcInstance->InstanceName) + 1;
    CommandLineSize = lstrlenW(SvcInstance->CommandLine) + 1;

    if (*PSize < (ClassNameSize + InstanceNameSize + CommandLineSize) * sizeof(WCHAR))
    {
        Result = STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }

    memcpy(P, SvcInstance->ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, SvcInstance->InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    memcpy(P, SvcInstance->CommandLine, CommandLineSize * sizeof(WCHAR)); P += CommandLineSize;

    *PSize = (ULONG)(P - Buffer);

    Result = STATUS_SUCCESS;

exit:
    LeaveCriticalSection(&SvcInstanceLock);

    return Result;
}

NTSTATUS SvcInstanceGetNameList(HANDLE ClientToken,
    PWSTR Buffer, PULONG PSize)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;
    PWSTR P = Buffer, BufferEnd = P + *PSize / sizeof(WCHAR);
    ULONG ClassNameSize, InstanceNameSize;

    EnterCriticalSection(&SvcInstanceLock);

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        ClassNameSize = lstrlenW(SvcInstance->ClassName) + 1;
        InstanceNameSize = lstrlenW(SvcInstance->InstanceName) + 1;

        if (BufferEnd < P + ClassNameSize + InstanceNameSize)
            break;

        memcpy(P, SvcInstance->ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
        memcpy(P, SvcInstance->InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    }

    LeaveCriticalSection(&SvcInstanceLock);

    *PSize = (ULONG)(P - Buffer);

    return STATUS_SUCCESS;
}

static HANDLE SvcThread, SvcEvent;
static DWORD SvcThreadId;
static HANDLE SvcPipe = INVALID_HANDLE_VALUE;
static OVERLAPPED SvcOverlapped;

static DWORD WINAPI SvcPipeServer(PVOID Context);
static VOID SvcPipeTransact(HANDLE ClientToken, PWSTR PipeBuf, PULONG PSize);

static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };

    InitializeCriticalSection(&SvcInstanceLock);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"" PIPE_SDDL, SDDL_REVISION_1,
        &SecurityAttributes.lpSecurityDescriptor, 0))
        goto fail;

    FspDebugLogSD(__FUNCTION__ ": SDDL = %s", SecurityAttributes.lpSecurityDescriptor);

    SvcEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == SvcEvent)
        goto fail;

    SvcOverlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == SvcOverlapped.hEvent)
        goto fail;

    SvcPipe = CreateNamedPipeW(L"" PIPE_NAME,
        PIPE_ACCESS_DUPLEX |
            FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, PIPE_DEFAULT_TIMEOUT, &SecurityAttributes);
    if (INVALID_HANDLE_VALUE == SvcPipe)
        goto fail;

    SvcThread = CreateThread(0, 0, SvcPipeServer, Service, 0, &SvcThreadId);
    if (0 == SvcThread)
        goto fail;

    LocalFree(SecurityAttributes.lpSecurityDescriptor);

    return STATUS_SUCCESS;

fail:
    DWORD LastError = GetLastError();

    if (0 != SvcThread)
        CloseHandle(SvcThread);

    if (INVALID_HANDLE_VALUE != SvcPipe)
        CloseHandle(SvcPipe);

    if (0 != SvcOverlapped.hEvent)
        CloseHandle(SvcOverlapped.hEvent);

    if (0 != SvcEvent)
        CloseHandle(SvcEvent);

    LocalFree(SecurityAttributes.lpSecurityDescriptor);

    DeleteCriticalSection(&SvcInstanceLock);

    return FspNtStatusFromWin32(LastError);
}

static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    if (GetCurrentThreadId() != SvcThreadId)
    {
        SetEvent(SvcEvent);
        FspServiceRequestTime(Service, 4500);   /* just under 5 sec */
        WaitForSingleObject(SvcThread, 4500);
    }

    if (0 != SvcThread)
        CloseHandle(SvcThread);

    if (INVALID_HANDLE_VALUE != SvcPipe)
        CloseHandle(SvcPipe);

    if (0 != SvcOverlapped.hEvent)
        CloseHandle(SvcOverlapped.hEvent);

    if (0 != SvcEvent)
        CloseHandle(SvcEvent);

    DeleteCriticalSection(&SvcInstanceLock);

    return STATUS_SUCCESS;
}

static inline DWORD SvcPipeWaitResult(BOOL Success, HANDLE StopEvent,
    HANDLE Handle, OVERLAPPED *Overlapped, PDWORD PBytesTransferred)
{
    HANDLE WaitObjects[2];
    DWORD WaitResult;

    if (!Success && ERROR_IO_PENDING != GetLastError())
        return GetLastError();

    WaitObjects[0] = StopEvent;
    WaitObjects[1] = Overlapped->hEvent;
    WaitResult = WaitForMultipleObjects(2, WaitObjects, FALSE, INFINITE);
    if (WAIT_OBJECT_0 == WaitResult)
        return -1; /* special: stop thread */
    else if (WAIT_OBJECT_0 + 1 == WaitResult)
    {
        if (!GetOverlappedResult(Handle, Overlapped, PBytesTransferred, TRUE))
            return GetLastError();
        return 0;
    }
    else
        return GetLastError();
}

static DWORD WINAPI SvcPipeServer(PVOID Context)
{
    static PWSTR LoopErrorMessage =
        L"Error in service main loop (%s = %ld). Exiting...";
    static PWSTR LoopWarningMessage =
        L"Error in service main loop (%s = %ld). Continuing...";
    FSP_SERVICE *Service = Context;
    PWSTR PipeBuf = 0;
    HANDLE ClientToken;
    DWORD LastError, BytesTransferred;

    PipeBuf = MemAlloc(PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
    {
        FspServiceSetExitCode(Service, ERROR_NO_SYSTEM_RESOURCES);
        goto exit;
    }

    for (;;)
    {
        LastError = SvcPipeWaitResult(
            ConnectNamedPipe(SvcPipe, &SvcOverlapped),
            SvcEvent, SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError &&
            ERROR_PIPE_CONNECTED != LastError && ERROR_NO_DATA != LastError)
        {
            FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                L"ConnectNamedPipe", LastError);
            continue;
        }

        LastError = SvcPipeWaitResult(
            ReadFile(SvcPipe, PipeBuf, PIPE_BUFFER_SIZE, &BytesTransferred, &SvcOverlapped),
            SvcEvent, SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError || sizeof(WCHAR) > BytesTransferred)
        {
            DisconnectNamedPipe(SvcPipe);
            if (0 != LastError)
                FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                    L"ReadFile", LastError);
            continue;
        }

        ClientToken = 0;
        if (!ImpersonateNamedPipeClient(SvcPipe) ||
            !OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &ClientToken) ||
            !RevertToSelf())
        {
            LastError = GetLastError();
            if (0 == ClientToken)
            {
                CloseHandle(ClientToken);
                DisconnectNamedPipe(SvcPipe);
                FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                    L"ImpersonateNamedPipeClient||OpenThreadToken", LastError);
                continue;
            }
            else
            {
                CloseHandle(ClientToken);
                DisconnectNamedPipe(SvcPipe);
                FspServiceLog(EVENTLOG_ERROR_TYPE, LoopErrorMessage,
                    L"RevertToSelf", LastError);
                break;
            }
        }

        SvcPipeTransact(ClientToken, PipeBuf, &BytesTransferred);

        CloseHandle(ClientToken);

        LastError = SvcPipeWaitResult(
            WriteFile(SvcPipe, PipeBuf, BytesTransferred, &BytesTransferred, &SvcOverlapped),
            SvcEvent, SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError)
        {
            DisconnectNamedPipe(SvcPipe);
            FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                L"WriteFile", LastError);
            continue;
        }

        DisconnectNamedPipe(SvcPipe);
    }

exit:
    MemFree(PipeBuf);

    FspServiceStop(Service);

    return 0;
}

static inline PWSTR SvcPipeTransactGetPart(PWSTR *PP, PWSTR PipeBufEnd)
{
    PWSTR PipeBufBeg = *PP, P;

    for (P = PipeBufBeg; PipeBufEnd > P && *P; P++)
        ;

    if (PipeBufEnd > P)
    {
        *PP = P + 1;
        return PipeBufBeg;
    }
    else
    {
        *PP = P;
        return 0;
    }
}

static inline VOID SvcPipeTransactResult(NTSTATUS Result, PWSTR PipeBuf, PULONG PSize)
{
    if (NT_SUCCESS(Result))
    {
        *PipeBuf = L'$';
        (*PSize)++;
    }
    else
        *PSize = wsprintfW(PipeBuf, L"!%08lx", FspNtStatusFromWin32(Result));
}

static VOID SvcPipeTransact(HANDLE ClientToken, PWSTR PipeBuf, PULONG PSize)
{
    if (sizeof(WCHAR) > *PSize)
        return;

    PWSTR P = PipeBuf, PipeBufEnd = PipeBuf + *PSize / sizeof(WCHAR);
    PWSTR ClassName, InstanceName;
    ULONG Argc; PWSTR Argv[10];
    NTSTATUS Result;

    *PSize = 0;

    switch (*P++)
    {
    case LauncherSvcInstanceStart:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        for (Argc = 0; 10 > Argc; Argc++)
            if (0 == (Argv[Argc] = SvcPipeTransactGetPart(&P, PipeBufEnd)))
                break;

        Result = STATUS_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Result = SvcInstanceStart(ClientToken, ClassName, InstanceName, Argc, Argv);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case LauncherSvcInstanceStop:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Result = STATUS_INVALID_PARAMETER;
        if (0 != InstanceName)
            Result = SvcInstanceStop(ClientToken, ClassName, InstanceName);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case LauncherSvcInstanceInfo:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Result = STATUS_INVALID_PARAMETER;
        if (0 != InstanceName)
        {
            *PSize = PIPE_BUFFER_SIZE - 1;
            Result = SvcInstanceGetInfo(ClientToken, ClassName, InstanceName, PipeBuf + 1, PSize);
        }

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case LauncherSvcInstanceList:
        *PSize = PIPE_BUFFER_SIZE - 1;
        Result = SvcInstanceGetNameList(ClientToken, PipeBuf + 1, PSize);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    default:
        SvcPipeTransactResult(STATUS_INVALID_PARAMETER, PipeBuf, PSize);
        break;
    }
}

int wmain(int argc, wchar_t **argv)
{
    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}

void wmainCRTStartup(void)
{
    extern HANDLE ProcessHeap;
    ProcessHeap = GetProcessHeap();
    if (0 == ProcessHeap)
        ExitProcess(GetLastError());

    ExitProcess(wmain(0, 0));
}

HANDLE ProcessHeap;
