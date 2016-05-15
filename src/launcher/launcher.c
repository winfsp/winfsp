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
    HANDLE Process;
    HANDLE ProcessWait;
} KILL_PROCESS_DATA;

static VOID CALLBACK KillProcessWait(PVOID Context, BOOLEAN Timeout);

VOID KillProcess(ULONG ProcessId, HANDLE Process, ULONG Timeout)
{
    if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, ProcessId))
    {
        /*
         * If GenerateConsoleCtrlEvent succeeds, but the child process does not exit
         * timely we will terminate it with extreme prejudice. This is done by calling
         * RegisterWaitForSingleObject with timeout on a duplicated process handle.
         *
         * If GenerateConsoleCtrlEvent succeeds, but we are not able to successfully call
         * RegisterWaitForSingleObject, we do NOT terminate the child process forcibly.
         * This is by design as it is not the child process's fault and the child process
         * should (we hope in this case) respond to the console control event timely.
         */

        KILL_PROCESS_DATA *KillProcessData;

        KillProcessData = MemAlloc(sizeof *KillProcessData);
        if (0 != KillProcessData)
        {
            if (DuplicateHandle(GetCurrentProcess(), Process, GetCurrentProcess(), &KillProcessData->Process,
                0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                if (RegisterWaitForSingleObject(&KillProcessData->ProcessWait, KillProcessData->Process,
                    KillProcessWait, KillProcessData, Timeout, WT_EXECUTEONLYONCE))
                    KillProcessData = 0;
                else
                    CloseHandle(KillProcessData->Process);
            }

            MemFree(KillProcessData);
        }
    }
    else
        TerminateProcess(Process, 0);
}

static VOID CALLBACK KillProcessWait(PVOID Context, BOOLEAN Timeout)
{
    KILL_PROCESS_DATA *KillProcessData = Context;

    if (Timeout)
        TerminateProcess(KillProcessData->Process, 0);

    UnregisterWaitEx(KillProcessData->ProcessWait, 0);
    CloseHandle(KillProcessData->Process);
    MemFree(KillProcessData);
}

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
static HANDLE SvcInstanceEvent;
static LIST_ENTRY SvcInstanceList = { &SvcInstanceList, &SvcInstanceList };

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Timeout);

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
    PWSTR NewString = 0, P, Q;
    PWSTR EmptyArg = L"";
    ULONG Length;

    *PNewString = 0;

    Length = 0;
    for (P = String; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            P++;
            if (L'0' <= *P && *P <= '9')
            {
                if (Argc > (ULONG)(*P - L'0'))
                    Length += SvcInstanceArgumentLength(Argv[*P - L'0']);
                else
                    Length += SvcInstanceArgumentLength(EmptyArg);
            }
            else
                Length++;
            break;
        default:
            Length++;
            break;
        }
    }

    NewString = MemAlloc((Length + 1) * sizeof(WCHAR));
    if (0 == NewString)
        return STATUS_INSUFFICIENT_RESOURCES;

    for (P = String, Q = NewString; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            P++;
            if (L'0' <= *P && *P <= '9')
            {
                if (Argc > (ULONG)(*P - L'0'))
                    Q = SvcInstanceArgumentCopy(Q, Argv[*P - L'0']);
                else
                    Q = SvcInstanceArgumentCopy(Q, EmptyArg);
            }
            else
                *Q++ = *P;
            break;
        default:
            *Q++ = *P;
            break;
        }
    }
    *Q = L'\0';

    *PNewString = NewString;

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
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv0, HANDLE Job,
    SVC_INSTANCE **PSvcInstance)
{
    SVC_INSTANCE *SvcInstance = 0;
    HKEY RegKey = 0;
    DWORD RegResult, RegSize;
    DWORD ClassNameSize, InstanceNameSize;
    WCHAR Executable[MAX_PATH], CommandLineBuf[512], SecurityBuf[512];
    PWSTR CommandLine, Security;
    DWORD JobControl;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    PWSTR Argv[10];
    STARTUPINFOW StartupInfo;
    PROCESS_INFORMATION ProcessInfo;
    NTSTATUS Result;

    *PSvcInstance = 0;

    lstrcpyW(CommandLineBuf, L"%0 ");
    lstrcpyW(SecurityBuf, L"O:SYG:SY");

    if (Argc > sizeof Argv / sizeof Argv[0] - 1)
        Argc = sizeof Argv / sizeof Argv[0] - 1;
    memcpy(Argv + 1, Argv0, Argc * sizeof(PWSTR));
    Argv[0] = 0;
    Argc++;

    memset(&StartupInfo, 0, sizeof StartupInfo);
    memset(&ProcessInfo, 0, sizeof ProcessInfo);

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
    Argv[0] = Executable;

    CommandLine = CommandLineBuf + lstrlenW(CommandLineBuf);
    RegSize = (DWORD)(sizeof CommandLineBuf - (CommandLine - CommandLineBuf) * sizeof(WCHAR));
    RegResult = RegGetValueW(RegKey, ClassName, L"CommandLine", RRF_RT_REG_SZ, 0,
        CommandLine, &RegSize);
    if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }
    if (ERROR_FILE_NOT_FOUND == RegResult)
        CommandLine[-1] = L'\0';
    CommandLine = CommandLineBuf;

    Security = SecurityBuf + lstrlenW(SecurityBuf);
    RegSize = (DWORD)(sizeof SecurityBuf - (Security - SecurityBuf) * sizeof(WCHAR));
    RegResult = RegGetValueW(RegKey, ClassName, L"Security", RRF_RT_REG_SZ, 0,
        Security, &RegSize);
    if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    RegSize = sizeof JobControl;
    JobControl = 1; /* default is YES! */
    RegResult = RegGetValueW(RegKey, ClassName, L"JobControl", RRF_RT_REG_DWORD, 0,
        &JobControl, &RegSize);
    if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    RegCloseKey(RegKey);
    RegKey = 0;

    if (L'\0' == Security[0])
        lstrcpyW(Security, L"" SVC_INSTANCE_DEFAULT_SDDL);
    if (L'D' == Security[0] && L':' == Security[1])
        Security = SecurityBuf;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(Security, SDDL_REVISION_1,
        &SecurityDescriptor, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    FspDebugLogSD(__FUNCTION__ ": SDDL = %s\n", SecurityDescriptor);

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

    Result = SvcInstanceReplaceArguments(CommandLine, Argc, Argv, &SvcInstance->CommandLine);
    if (!NT_SUCCESS(Result))
        goto exit;

    StartupInfo.cb = sizeof StartupInfo;
    if (!CreateProcessW(Executable, SvcInstance->CommandLine, 0, 0, FALSE,
        CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP, 0, 0, &StartupInfo, &ProcessInfo))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    SvcInstance->ProcessId = ProcessInfo.dwProcessId;
    SvcInstance->Process = ProcessInfo.hProcess;

    if (!RegisterWaitForSingleObject(&SvcInstance->ProcessWait, SvcInstance->Process,
        SvcInstanceTerminated, SvcInstance, INFINITE, WT_EXECUTEONLYONCE))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (0 != Job && JobControl)
    {
        if (!AssignProcessToJobObject(Job, SvcInstance->Process))
            FspServiceLog(EVENTLOG_WARNING_TYPE,
                L"Ignorning error: AssignProcessToJobObject = %ld", GetLastError());
    }

    /*
     * ONCE THE PROCESS IS RESUMED NO MORE FAILURES ALLOWED!
     */

    ResumeThread(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hThread);
    ProcessInfo.hThread = 0;

    InsertTailList(&SvcInstanceList, &SvcInstance->ListEntry);
    ResetEvent(SvcInstanceEvent);

    *PSvcInstance = SvcInstance;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        LocalFree(SecurityDescriptor);

        if (0 != ProcessInfo.hThread)
            CloseHandle(ProcessInfo.hThread);

        if (0 != SvcInstance)
        {
            if (0 != SvcInstance->ProcessWait)
                UnregisterWaitEx(SvcInstance->ProcessWait, 0);

            if (0 != SvcInstance->Process)
            {
                TerminateProcess(SvcInstance->Process, 0);
                CloseHandle(SvcInstance->Process);
            }

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
    if (RemoveEntryList(&SvcInstance->ListEntry))
        SetEvent(SvcInstanceEvent);
    LeaveCriticalSection(&SvcInstanceLock);

    if (0 != SvcInstance->ProcessWait)
        UnregisterWaitEx(SvcInstance->ProcessWait, 0);
    if (0 != SvcInstance->Process)
        CloseHandle(SvcInstance->Process);

    LocalFree(SvcInstance->SecurityDescriptor);

    MemFree(SvcInstance->CommandLine);
    MemFree(SvcInstance);
}

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Timeout)
{
    SVC_INSTANCE *SvcInstance = Context;

    SvcInstanceDelete(SvcInstance);
}

NTSTATUS SvcInstanceStart(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv, HANDLE Job)
{
    SVC_INSTANCE *SvcInstance;

    return SvcInstanceCreate(ClientToken, ClassName, InstanceName, Argc, Argv, Job, &SvcInstance);
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

    KillProcess(SvcInstance->ProcessId, SvcInstance->Process, KILL_TIMEOUT);

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

    *PSize = (ULONG)(P - Buffer) * sizeof(WCHAR);

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

    *PSize = (ULONG)(P - Buffer) * sizeof(WCHAR);

    return STATUS_SUCCESS;
}

NTSTATUS SvcInstanceStopAndWaitAll(VOID)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    EnterCriticalSection(&SvcInstanceLock);

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        KillProcess(SvcInstance->ProcessId, SvcInstance->Process, KILL_TIMEOUT);
    }

    LeaveCriticalSection(&SvcInstanceLock);

    WaitForSingleObject(SvcInstanceEvent, STOP_TIMEOUT);

    return STATUS_SUCCESS;
}

static HANDLE SvcJob, SvcThread, SvcEvent;
static DWORD SvcThreadId;
static HANDLE SvcPipe = INVALID_HANDLE_VALUE;
static OVERLAPPED SvcOverlapped;

static DWORD WINAPI SvcPipeServer(PVOID Context);
static VOID SvcPipeTransact(HANDLE ClientToken, PWSTR PipeBuf, PULONG PSize);

static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };

    /*
     * Allocate a console in case we are running as a service without one.
     * This will ensure that we can send console control events to service instances.
     */
    if (AllocConsole())
        ShowWindow(GetConsoleWindow(), SW_HIDE);

    InitializeCriticalSection(&SvcInstanceLock);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"" PIPE_SDDL, SDDL_REVISION_1,
        &SecurityAttributes.lpSecurityDescriptor, 0))
        goto fail;

    FspDebugLogSD(__FUNCTION__ ": SDDL = %s\n", SecurityAttributes.lpSecurityDescriptor);

    SvcInstanceEvent = CreateEventW(0, TRUE, TRUE, 0);
    if (0 == SvcInstanceEvent)
        goto fail;

    SvcJob = CreateJobObjectW(0, 0);
    if (0 != SvcJob)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION LimitInfo;

        memset(&LimitInfo, 0, sizeof LimitInfo);
        LimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(SvcJob, JobObjectExtendedLimitInformation,
            &LimitInfo, sizeof LimitInfo))
        {
            CloseHandle(SvcJob);
            SvcJob = 0;
        }
    }

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

    /*
     * The OS will cleanup for us. So there is no need to explicitly release these resources.
     */
#if 0
    if (0 != SvcThread)
        CloseHandle(SvcThread);

    if (INVALID_HANDLE_VALUE != SvcPipe)
        CloseHandle(SvcPipe);

    if (0 != SvcOverlapped.hEvent)
        CloseHandle(SvcOverlapped.hEvent);

    if (0 != SvcEvent)
        CloseHandle(SvcEvent);

    if (0 != SvcJob)
        CloseHandle(SvcJob);

    if (0 != SvcInstanceEvent)
        CloseHandle(SvcInstanceEvent);

    LocalFree(SecurityAttributes.lpSecurityDescriptor);

    DeleteCriticalSection(&SvcInstanceLock);
#endif

    return FspNtStatusFromWin32(LastError);
}

static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    if (GetCurrentThreadId() != SvcThreadId)
    {
        SetEvent(SvcEvent);
        FspServiceRequestTime(Service, STOP_TIMEOUT);
        WaitForSingleObject(SvcThread, STOP_TIMEOUT);
    }

    /*
     * The OS will cleanup for us. So there is no need to explicitly release these resources.
     *
     * This also protects us from scenarios where not all child processes terminate timely
     * and KillProcess decides to terminate them forcibly, thus creating racing conditions
     * with SvcInstanceTerminated.
     */
#if 0
    if (0 != SvcThread)
        CloseHandle(SvcThread);

    if (INVALID_HANDLE_VALUE != SvcPipe)
        CloseHandle(SvcPipe);

    if (0 != SvcOverlapped.hEvent)
        CloseHandle(SvcOverlapped.hEvent);

    if (0 != SvcEvent)
        CloseHandle(SvcEvent);

    if (0 != SvcJob)
        CloseHandle(SvcJob);

    if (0 != SvcInstanceEvent)
        CloseHandle(SvcInstanceEvent);

    DeleteCriticalSection(&SvcInstanceLock);
#endif

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

    SvcInstanceStopAndWaitAll();

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
        *PSize += sizeof(WCHAR);
    }
    else
        *PSize = (wsprintfW(PipeBuf, L"!%ld", FspWin32FromNtStatus(Result)) + 1) * sizeof(WCHAR);
}

static VOID SvcPipeTransact(HANDLE ClientToken, PWSTR PipeBuf, PULONG PSize)
{
    if (sizeof(WCHAR) > *PSize)
        return;

    PWSTR P = PipeBuf, PipeBufEnd = PipeBuf + *PSize / sizeof(WCHAR);
    PWSTR ClassName, InstanceName;
    ULONG Argc; PWSTR Argv[9];
    NTSTATUS Result;

    *PSize = 0;

    switch (*P++)
    {
    case LauncherSvcInstanceStart:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        for (Argc = 0; sizeof Argv / sizeof Argv[0] > Argc; Argc++)
            if (0 == (Argv[Argc] = SvcPipeTransactGetPart(&P, PipeBufEnd)))
                break;

        Result = STATUS_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Result = SvcInstanceStart(ClientToken, ClassName, InstanceName, Argc, Argv, SvcJob);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case LauncherSvcInstanceStop:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Result = STATUS_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Result = SvcInstanceStop(ClientToken, ClassName, InstanceName);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case LauncherSvcInstanceInfo:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Result = STATUS_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
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

#if !defined(NDEBUG)
    case LauncherQuit:
        SetEvent(SvcEvent);

        SvcPipeTransactResult(STATUS_SUCCESS, PipeBuf, PSize);
        break;
#endif
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
