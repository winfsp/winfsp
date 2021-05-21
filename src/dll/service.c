/**
 * @file dll/service.c
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

#include <dll/library.h>

enum
{
    SetStatus_ServiceType               = 0x0001,
    SetStatus_CurrentState              = 0x0002,
    SetStatus_ControlsAccepted          = 0x0004,
    SetStatus_Win32ExitCode             = 0x0008,
    SetStatus_ServiceSpecificExitCode   = 0x0010,
    SetStatus_CheckPoint                = 0x0020,
    SetStatus_WaitHint                  = 0x0040,
    AddStatus_CheckPoint                = 0x0080,
    GetStatus_ServiceType               = 0x0100,
    GetStatus_CurrentState              = 0x0200,
    GetStatus_ControlsAccepted          = 0x0400,
    GetStatus_Win32ExitCode             = 0x0800,
    GetStatus_ServiceSpecificExitCode   = 0x1000,
    GetStatus_CheckPoint                = 0x2000,
    GetStatus_WaitHint                  = 0x4000,
};

static SERVICE_TABLE_ENTRYW *FspServiceTable;
static HANDLE FspServiceConsoleModeEvent;
static UINT32 FspServiceConsoleCtrlHandlerDisabled;

static VOID FspServiceSetStatus(FSP_SERVICE *Service, ULONG Flags, SERVICE_STATUS *ServiceStatus);
static VOID WINAPI FspServiceEntry(DWORD Argc, PWSTR *Argv);
static VOID FspServiceMain(FSP_SERVICE *Service, DWORD Argc, PWSTR *Argv);
static DWORD WINAPI FspServiceCtrlHandler(
    DWORD Control, DWORD EventType, PVOID EventData, PVOID Context);
static DWORD WINAPI FspServiceConsoleModeThread(PVOID Context);
BOOL WINAPI FspServiceConsoleCtrlHandler(DWORD CtrlType);

#define FspServiceFromTable()           (0 != FspServiceTable ?\
    (FSP_SERVICE *)((PUINT8)FspServiceTable[0].lpServiceName - FIELD_OFFSET(FSP_SERVICE, ServiceName)) :\
    0)

VOID FspServiceFinalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     *
     * We must close our console mode event handle. We only do so when we are
     * explicitly unloaded. If the process is exiting the OS will clean up for us.
     */
    if (Dynamic && 0 != FspServiceConsoleModeEvent)
        CloseHandle(FspServiceConsoleModeEvent);
}

FSP_API ULONG FspServiceRunEx(PWSTR ServiceName,
    FSP_SERVICE_START *OnStart,
    FSP_SERVICE_STOP *OnStop,
    FSP_SERVICE_CONTROL *OnControl,
    PVOID UserContext)
{
    FSP_SERVICE *Service;
    NTSTATUS Result;
    ULONG ExitCode;

    Result = FspServiceCreate(ServiceName, OnStart, OnStop, OnControl, &Service);
    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s cannot be created (Status=%lx).", ServiceName, Result);
        return FspWin32FromNtStatus(Result);
    }
    Service->UserContext = UserContext;

    FspServiceAllowConsoleMode(Service);
    Result = FspServiceLoop(Service);
    ExitCode = FspServiceGetExitCode(Service);
    FspServiceDelete(Service);

    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s has failed to run (Status=%lx).", ServiceName, Result);
        return FspWin32FromNtStatus(Result);
    }

    return ExitCode;
}

FSP_API NTSTATUS FspServiceCreate(PWSTR ServiceName,
    FSP_SERVICE_START *OnStart,
    FSP_SERVICE_STOP *OnStop,
    FSP_SERVICE_CONTROL *OnControl,
    FSP_SERVICE **PService)
{
    FSP_SERVICE *Service;
    DWORD Size;

    *PService = 0;

    Size = (lstrlenW(ServiceName) + 1) * sizeof(WCHAR);

    Service = MemAlloc(sizeof *Service + Size);
    if (0 == Service)
        return STATUS_INSUFFICIENT_RESOURCES;
    memset(Service, 0, sizeof *Service);
    memcpy(Service->ServiceName, ServiceName, Size);

    Service->OnStart = OnStart;
    Service->OnStop = OnStop;
    Service->OnControl = OnControl;
    Service->AcceptControl = OnStop ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;

    InitializeCriticalSection(&Service->ServiceStatusGuard);
    InitializeCriticalSection(&Service->ServiceStopGuard);

    *PService = Service;

    return STATUS_SUCCESS;
}

FSP_API VOID FspServiceDelete(FSP_SERVICE *Service)
{
    DeleteCriticalSection(&Service->ServiceStopGuard);
    DeleteCriticalSection(&Service->ServiceStatusGuard);
    MemFree(Service);
}

static VOID FspServiceSetStatus(FSP_SERVICE *Service, ULONG Flags, SERVICE_STATUS *ServiceStatus)
{
#define XCHG(FIELD)\
    if (Flags & SetStatus_##FIELD)\
    {\
        DWORD Temp = ServiceStatus->dw##FIELD;\
        if (Flags & GetStatus_##FIELD)\
            ServiceStatus->dw##FIELD = Service->ServiceStatus.dw##FIELD;\
        Service->ServiceStatus.dw##FIELD = Temp;\
    }

    EnterCriticalSection(&Service->ServiceStatusGuard);

    //XCHG(ServiceType);
    XCHG(CurrentState);
    XCHG(ControlsAccepted);
    XCHG(Win32ExitCode);
    //XCHG(ServiceSpecificExitCode);
    if (Flags & AddStatus_CheckPoint)
    {
        DWORD Temp = ServiceStatus->dwCheckPoint;
        if (Flags & GetStatus_CheckPoint)
            ServiceStatus->dwCheckPoint = Service->ServiceStatus.dwCheckPoint;
        Service->ServiceStatus.dwCheckPoint += Temp;
    }
    else
        XCHG(CheckPoint);
    XCHG(WaitHint);

    if (0 != Service->StatusHandle)
    {
        if (!SetServiceStatus(Service->StatusHandle, &Service->ServiceStatus))
            FspServiceLog(EVENTLOG_ERROR_TYPE,
                L"" __FUNCTION__ ": SetServiceStatus = %ld", GetLastError());
    }
    else if (0 != FspServiceConsoleModeEvent &&
        SERVICE_STOPPED == Service->ServiceStatus.dwCurrentState)
    {
        SetEvent(FspServiceConsoleModeEvent);
    }

    LeaveCriticalSection(&Service->ServiceStatusGuard);

#undef XCHG
}

FSP_API VOID FspServiceAllowConsoleMode(FSP_SERVICE *Service)
{
    Service->AllowConsoleMode = TRUE;
}

FSP_API VOID FspServiceAcceptControl(FSP_SERVICE *Service, ULONG Control)
{
    Service->AcceptControl = Control & ~SERVICE_ACCEPT_PAUSE_CONTINUE;
}

FSP_API VOID FspServiceRequestTime(FSP_SERVICE *Service, ULONG Time)
{
    SERVICE_STATUS ServiceStatus;

    ServiceStatus.dwCheckPoint = +1;
    ServiceStatus.dwWaitHint = Time;
    FspServiceSetStatus(Service,
        AddStatus_CheckPoint | SetStatus_WaitHint, &ServiceStatus);
}

FSP_API VOID FspServiceSetExitCode(FSP_SERVICE *Service, ULONG ExitCode)
{
    Service->ExitCode = ExitCode;
}

FSP_API ULONG FspServiceGetExitCode(FSP_SERVICE *Service)
{
    return Service->ServiceStatus.dwWin32ExitCode;
}

FSP_API NTSTATUS FspServiceLoop(FSP_SERVICE *Service)
{
    NTSTATUS Result;
    SERVICE_TABLE_ENTRYW ServiceTable[2];

    Service->ExitCode = NO_ERROR;
    Service->ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    Service->ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    Service->ServiceStatus.dwControlsAccepted = 0;
    Service->ServiceStatus.dwWin32ExitCode = NO_ERROR;
    Service->ServiceStatus.dwServiceSpecificExitCode = 0;
    Service->ServiceStatus.dwCheckPoint = 0;
    Service->ServiceStatus.dwWaitHint = 0;

    ServiceTable[0].lpServiceName = Service->ServiceName;
    ServiceTable[0].lpServiceProc = FspServiceEntry;
    ServiceTable[1].lpServiceName = 0;
    ServiceTable[1].lpServiceProc = 0;
    FspServiceTable = ServiceTable;

    if (!StartServiceCtrlDispatcherW(ServiceTable))
    {
        HANDLE Thread;
        PWSTR *Argv;
        DWORD Argc;
        DWORD WaitResult;
        DWORD LastError;

        LastError = GetLastError();
        if (!Service->AllowConsoleMode || ERROR_FAILED_SERVICE_CONTROLLER_CONNECT != LastError)
        {
            Result = FspNtStatusFromWin32(LastError);
            goto exit;
        }

        /* ENTER CONSOLE MODE! */

        /* create the console mode event and console control handler */
        if (0 == FspServiceConsoleModeEvent)
        {
            FspServiceConsoleModeEvent = CreateEventW(0, TRUE, FALSE, 0);
            if (0 == FspServiceConsoleModeEvent)
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto console_mode_exit;
            }

            if (!SetConsoleCtrlHandler(FspServiceConsoleCtrlHandler, TRUE))
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto console_mode_exit;
            }
        }
#if 0
        else
        {
            ResetEvent(FspServiceConsoleModeEvent);
            FspServiceConsoleCtrlHandlerDisabled = 0;
            MemoryBarrier();
        }
#endif

        /* prepare the command line arguments */
        Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
        if (0 == Argv)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto console_mode_exit;
        }
        Argv[0] = Service->ServiceName;

        /* create the console mode startup thread (mimic StartServiceCtrlDispatcherW) */
        Thread = CreateThread(0, 0, FspServiceConsoleModeThread, Argv/* give ownership */, 0, 0);
        if (0 == Thread)
        {
            LocalFree(Argv);
            Result = FspNtStatusFromWin32(GetLastError());
            goto console_mode_exit;
        }

        /* wait for the console mode startup thread to terminate */
        WaitResult = WaitForSingleObject(Thread, INFINITE);
        CloseHandle(Thread);
        if (WAIT_OBJECT_0 != WaitResult)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto console_mode_exit;
        }

        /* wait until signaled by the console control handler */
        WaitResult = WaitForSingleObject(FspServiceConsoleModeEvent, INFINITE);
        if (WAIT_OBJECT_0 != WaitResult)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto console_mode_exit;
        }

        if (Service->AcceptControl & SERVICE_ACCEPT_STOP)
            FspServiceCtrlHandler(SERVICE_CONTROL_STOP, 0, 0, Service);

    console_mode_exit:
        /*
         * Turns out that if we are sleeping/waiting in the FspServiceConsoleCtrlHandler
         * we cannot call SetConsoleCtrlHandler, because we will deadlock. So we cannot
         * really cleanup our console control handler upon return from this function.
         *
         * What we do instead is disable our handler by setting a variable.
         */
        FspServiceConsoleCtrlHandlerDisabled = 1;
        MemoryBarrier();
    }

    Result = STATUS_SUCCESS;

exit:
    FspServiceTable = 0;

    return Result;
}

FSP_API VOID FspServiceStop(FSP_SERVICE *Service)
{
    SERVICE_STATUS ServiceStatus;
    BOOLEAN Stopped;
    NTSTATUS Result;

    if (!TryEnterCriticalSection(&Service->ServiceStopGuard))
        return; /* the service is already being stopped! */

    EnterCriticalSection(&Service->ServiceStatusGuard);
    Stopped = SERVICE_STOPPED == Service->ServiceStatus.dwCurrentState;
    LeaveCriticalSection(&Service->ServiceStatusGuard);
    if (Stopped)
        goto exit;

    ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;
    FspServiceSetStatus(Service,
        SetStatus_CurrentState | SetStatus_CheckPoint | SetStatus_WaitHint |
        GetStatus_CurrentState | GetStatus_CheckPoint | GetStatus_WaitHint,
        &ServiceStatus);

    Result = STATUS_SUCCESS;
    if (0 != Service->OnStop)
        Result = Service->OnStop(Service);

    if (NT_SUCCESS(Result))
    {
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode = Service->ExitCode;
        FspServiceSetStatus(Service,
            SetStatus_CurrentState | SetStatus_Win32ExitCode, &ServiceStatus);

        FspServiceLog(EVENTLOG_INFORMATION_TYPE,
            L"The service %s has been stopped.", Service->ServiceName);
    }
    else
    {
        /* revert the service status */
        FspServiceSetStatus(Service,
            SetStatus_CurrentState | SetStatus_CheckPoint | SetStatus_WaitHint,
            &ServiceStatus);

        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s has failed to stop (Status=%lx).", Service->ServiceName, Result);
    }

exit:
    LeaveCriticalSection(&Service->ServiceStopGuard);
}

static VOID WINAPI FspServiceEntry(DWORD Argc, PWSTR *Argv)
{
    FSP_SERVICE *Service;

    Service = FspServiceFromTable();
    if (0 == Service)
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": internal error: FspServiceFromTable = 0");
        return;
    }

    Service->StatusHandle = RegisterServiceCtrlHandlerExW(Service->ServiceName,
        FspServiceCtrlHandler, Service);
    if (0 == Service->StatusHandle)
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": RegisterServiceCtrlHandlerW = %ld", GetLastError());
        return;
    }

    FspServiceMain(Service, Argc, Argv);
}

static VOID FspServiceMain(FSP_SERVICE *Service, DWORD Argc, PWSTR *Argv)
{
    SERVICE_STATUS ServiceStatus;
    NTSTATUS Result;

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    ServiceStatus.dwControlsAccepted = 0;
    FspServiceSetStatus(Service,
        SetStatus_CurrentState | SetStatus_ControlsAccepted, &ServiceStatus);

    Result = STATUS_SUCCESS;
    if (0 != Service->OnStart)
        Result = Service->OnStart(Service, Argc, Argv);

    if (NT_SUCCESS(Result))
    {
        ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        ServiceStatus.dwControlsAccepted = Service->AcceptControl;
        FspServiceSetStatus(Service,
            SetStatus_CurrentState | SetStatus_ControlsAccepted, &ServiceStatus);

        FspServiceLog(EVENTLOG_INFORMATION_TYPE,
            L"The service %s has been started.", Service->ServiceName);
    }
    else
    {
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode = FspWin32FromNtStatus(Result);
        FspServiceSetStatus(Service,
            SetStatus_CurrentState | SetStatus_Win32ExitCode, &ServiceStatus);

        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s has failed to start (Status=%lx).", Service->ServiceName, Result);
    }
}

static DWORD WINAPI FspServiceCtrlHandler(
    DWORD Control, DWORD EventType, PVOID EventData, PVOID Context)
{
    FSP_SERVICE *Service = Context;
    NTSTATUS Result;

    switch (Control)
    {
    case SERVICE_CONTROL_SHUTDOWN:
        /*
         * Shutdown simply falls through to Stop. If specific Shutdown handling is needed
         * we need to enable this. We also need to arrange for console mode to have two
         * events: one to signal Stop and another to signal Shutdown. We currently use a
         * single event for both console mode controls.
         */
#if 0
        Result = STATUS_NOT_IMPLEMENTED;
        if (0 != Service->OnControl)
            Result = Service->OnControl(Service, Control, EventType, EventData);
        if (STATUS_NOT_IMPLEMENTED != Result)
            return FspWin32FromNtStatus(Result);
        /* fall through */
#endif

    case SERVICE_CONTROL_STOP:
        FspServiceStop(Service);
        return NO_ERROR;

    case SERVICE_CONTROL_PAUSE:
    case SERVICE_CONTROL_CONTINUE:
        return ERROR_CALL_NOT_IMPLEMENTED;

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    default:
        Result = STATUS_SUCCESS;
        if (0 != Service->OnControl)
            Result = Service->OnControl(Service, Control, EventType, EventData);
        return FspWin32FromNtStatus(Result);
    }
}

static DWORD WINAPI FspServiceConsoleModeThread(PVOID Context)
{
    FSP_SERVICE *Service;
    PWSTR *Argv = Context;
    DWORD Argc;

    for (Argc = 0; 0 != Argv[Argc]; Argc++)
        ;

    Service = FspServiceFromTable();
    if (0 == Service)
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": internal error: FspServiceFromTable = 0");
    else
        FspServiceMain(Service, Argc, Argv);

    LocalFree(Argv);

    return 0;
}

/* expose FspServiceConsoleCtrlHandler so it can be used from fsp_fuse_signal_handler */
BOOL WINAPI FspServiceConsoleCtrlHandler(DWORD CtrlType)
{
    UINT32 Disabled = FspServiceConsoleCtrlHandlerDisabled;
    MemoryBarrier();
    if (Disabled)
        return FALSE;

    switch (CtrlType)
    {
    default:
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        if (0 != FspServiceConsoleModeEvent)
            SetEvent(FspServiceConsoleModeEvent);
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        /*
         * Returning from these events will kill the process. OTOH if we do not return timely
         * the OS will kill us within 5-20 seconds. Our strategy is to wait some time (30 sec)
         * to give the process some time to cleanup itself.
         *
         * We only do so if we have a Close event or we are interactive. If we are running as
         * a service the OS will not kill us after delivering a Shutdown (or Logoff) event.
         */
        if (0 != FspServiceConsoleModeEvent)
            SetEvent(FspServiceConsoleModeEvent);
        if (CTRL_CLOSE_EVENT == CtrlType || FspServiceIsInteractive())
            Sleep(30000);
        return TRUE;
    case CTRL_LOGOFF_EVENT:
        /* services should ignore this! */
        return TRUE;
    }
}

FSP_API BOOLEAN FspServiceIsInteractive(VOID)
{
    /*
     * Modeled after System.Environment.UserInteractive.
     * See http://referencesource.microsoft.com/#mscorlib/system/environment.cs,947ad026e7cb830c
     */
    static HWINSTA ProcessWindowStation;
    static BOOLEAN IsInteractive;
    HWINSTA CurrentWindowStation;
    USEROBJECTFLAGS Flags;

    CurrentWindowStation = GetProcessWindowStation();
    if (0 != CurrentWindowStation && ProcessWindowStation != CurrentWindowStation)
    {
        if (GetUserObjectInformationW(CurrentWindowStation, UOI_FLAGS, &Flags, sizeof Flags, 0))
            IsInteractive = 0 != (Flags.dwFlags & WSF_VISIBLE);
        ProcessWindowStation = CurrentWindowStation;
    }
    return IsInteractive;
}

FSP_API NTSTATUS FspServiceContextCheck(HANDLE Token, PBOOLEAN PIsLocalSystem)
{
    NTSTATUS Result;
    PSID LocalSystemSid, ServiceSid;
    BOOLEAN IsLocalSystem = FALSE;
    BOOL HasServiceSid = FALSE;
    HANDLE ProcessToken = 0, ImpersonationToken = 0;
    DWORD SessionId, Size;
    union
    {
        TOKEN_USER V;
        UINT8 B[128];
    } UserInfoBuf;
    PTOKEN_USER UserInfo = &UserInfoBuf.V;

    LocalSystemSid = FspWksidGet(WinLocalSystemSid);
    ServiceSid = FspWksidGet(WinServiceSid);
    if (0 == LocalSystemSid || 0 == ServiceSid)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (0 == Token)
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &ProcessToken) ||
            !DuplicateToken(ProcessToken, SecurityImpersonation, &ImpersonationToken))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        Token = ImpersonationToken;
    }

    if (!GetTokenInformation(Token, TokenSessionId, &SessionId, sizeof SessionId, &Size))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (0 != SessionId)
    {
        Result = STATUS_ACCESS_DENIED;
        goto exit;
    }

    if (!GetTokenInformation(Token, TokenUser, UserInfo, sizeof UserInfoBuf, &Size))
    {
        if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        UserInfo = MemAlloc(Size);
        if (0 == UserInfo)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        if (!GetTokenInformation(Token, TokenUser, UserInfo, Size, &Size))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    IsLocalSystem = EqualSid(LocalSystemSid, UserInfo->User.Sid);
    if (IsLocalSystem)
    {
        Result = STATUS_SUCCESS;
        goto exit;
    }

    if (!CheckTokenMembership(Token, ServiceSid, &HasServiceSid))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = HasServiceSid ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;

exit:
    if (0 != PIsLocalSystem)
        *PIsLocalSystem = NT_SUCCESS(Result) ? IsLocalSystem : FALSE;

    if (UserInfo != &UserInfoBuf.V)
        MemFree(UserInfo);

    if (0 != ImpersonationToken)
        CloseHandle(ImpersonationToken);

    if (0 != ProcessToken)
        CloseHandle(ProcessToken);

    return Result;
}

FSP_API VOID FspServiceLog(ULONG Type, PWSTR Format, ...)
{
    va_list ap;

    va_start(ap, Format);
    FspServiceLogV(Type, Format, ap);
    va_end(ap);
}

FSP_API VOID FspServiceLogV(ULONG Type, PWSTR Format, va_list ap)
{
    if (FspServiceIsInteractive())
    {
        WCHAR BufW[1024];
            /* wvsprintfW is only safe with a 1024 WCHAR buffer */
        PSTR BufA;
        DWORD Length;

        wvsprintfW(BufW, Format, ap);
        BufW[(sizeof BufW / sizeof BufW[0]) - 1] = L'\0';

        Length = lstrlenW(BufW);
        BufA = MemAlloc(Length * 3 + 1/* '\n' */);
        if (0 != BufA)
        {
            Length = WideCharToMultiByte(CP_UTF8, 0, BufW, Length, BufA, Length * 3 + 1/* '\n' */,
                0, 0);
            if (0 < Length)
            {
                BufA[Length++] = '\n';
                WriteFile(GetStdHandle(STD_ERROR_HANDLE), BufA, Length, &Length, 0);
            }
            MemFree(BufA);
        }
    }
    else
        FspEventLogV(Type, Format, ap);
}
