/**
 * @file dll/service.c
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
static VOID FspServiceSetStatus(FSP_SERVICE *Service, ULONG Flags, SERVICE_STATUS *ServiceStatus);
static VOID WINAPI FspServiceEntry(DWORD Argc, PWSTR *Argv);
static VOID FspServiceMain(FSP_SERVICE *Service, DWORD Argc, PWSTR *Argv);
static DWORD WINAPI FspServiceCtrlHandler(
    DWORD Control, DWORD EventType, PVOID EventData, PVOID Context);
static DWORD WINAPI FspServiceStopRoutine(PVOID Context);
static DWORD WINAPI FspServiceInteractiveThread(PVOID Context);
static BOOL WINAPI FspServiceConsoleCtrlHandler(DWORD CtrlType);

static inline FSP_SERVICE *FspServiceFromTable(VOID)
{
    FSP_SERVICE *Service = 0;

    if (0 != FspServiceTable)
        Service = (PVOID)((PUINT8)FspServiceTable[0].lpServiceName -
            FIELD_OFFSET(FSP_SERVICE, ServiceName));

    return Service;
}

FSP_API ULONG FspServiceRun(PWSTR ServiceName,
    FSP_SERVICE_START *OnStart,
    FSP_SERVICE_STOP *OnStop,
    FSP_SERVICE_CONTROL *OnControl)
{
    FSP_SERVICE *Service;
    NTSTATUS Result;
    ULONG ExitCode;

    Result = FspServiceCreate(ServiceName, OnStart, OnStop, OnControl, &Service);
    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s cannot be created (Status=%lx).", Service->ServiceName, Result);
        return FspWin32FromNtStatus(Result);
    }

    FspServiceAllowConsoleMode(Service);
    Result = FspServiceLoop(Service);
    ExitCode = FspServiceGetExitCode(Service);
    FspServiceDelete(Service);

    if (!NT_SUCCESS(Result))
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"The service %s has failed to run (Status=%lx).", Service->ServiceName, Result);
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

    *PService = Service;

    return STATUS_SUCCESS;
}

FSP_API VOID FspServiceDelete(FSP_SERVICE *Service)
{
    if (0 != Service->ConsoleModeEvent)
        CloseHandle(Service->ConsoleModeEvent);

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
    else if (0 != Service->ConsoleModeEvent &&
        SERVICE_STOPPED == Service->ServiceStatus.dwCurrentState)
    {
        SetEvent(Service->ConsoleModeEvent);
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
        DWORD WaitResult;
        DWORD LastError;

        LastError = GetLastError();
        if (!Service->AllowConsoleMode || ERROR_FAILED_SERVICE_CONTROLLER_CONNECT != LastError)
        {
            Result = FspNtStatusFromWin32(LastError);
            goto exit;
        }

        /* enter console mode! */

        if (0 == Service->ConsoleModeEvent)
        {
            Service->ConsoleModeEvent = CreateEventW(0, TRUE, FALSE, 0);
            if (0 == Service->ConsoleModeEvent)
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }
        }
        else
            ResetEvent(Service->ConsoleModeEvent);

        /* create a thread to mimic what StartServiceCtrlDispatcherW does */
        Thread = CreateThread(0, 0, FspServiceInteractiveThread, Service, 0, 0);
        if (0 == Thread)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        WaitResult = WaitForSingleObject(Thread, INFINITE);
        CloseHandle(Thread);
        if (WAIT_OBJECT_0 != WaitResult)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        if (!SetConsoleCtrlHandler(FspServiceConsoleCtrlHandler, TRUE))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        WaitResult = WaitForSingleObject(Service->ConsoleModeEvent, INFINITE);
        if (WAIT_OBJECT_0 != WaitResult)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    Result = STATUS_SUCCESS;

exit:
    FspServiceTable = 0;

    return Result;
}

FSP_API VOID FspServiceStop(FSP_SERVICE *Service)
{
    SERVICE_STATUS ServiceStatus;
    NTSTATUS Result;

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
        Result = STATUS_NOT_IMPLEMENTED;
        if (0 != Service->OnControl)
            Result = Service->OnControl(Service, Control, EventType, EventData);
        if (STATUS_NOT_IMPLEMENTED != Result)
            return FspWin32FromNtStatus(Result);
        /* fall through */

    case SERVICE_CONTROL_STOP:
        if (!QueueUserWorkItem(FspServiceStopRoutine, Service, WT_EXECUTEDEFAULT))
            FspServiceStopRoutine(Service);
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

static DWORD WINAPI FspServiceStopRoutine(PVOID Context)
{
    FSP_SERVICE *Service = Context;

    FspServiceStop(Service);
    return 0;
}

static DWORD WINAPI FspServiceInteractiveThread(PVOID Context)
{
    FSP_SERVICE *Service;
    PWSTR Args[2] = { 0, 0 };
    PWSTR *Argv;
    DWORD Argc;

    Service = FspServiceFromTable();
    if (0 == Service)
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": internal error: FspServiceFromTable = 0");
        return FALSE;
    }

    Argv = CommandLineToArgvW(GetCommandLineW(), &Argc);
    if (0 == Argv)
    {
        Argv = Args;
        Argc = 1;
    }
    Argv[0] = Service->ServiceName;

    FspServiceMain(Service, Argc, Argv);

    if (Args != Argv)
        LocalFree(Argv);

    return 0;
}

static BOOL WINAPI FspServiceConsoleCtrlHandler(DWORD CtrlType)
{
    FSP_SERVICE *Service;

    Service = FspServiceFromTable();
    if (0 == Service)
    {
        FspServiceLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": internal error: FspServiceFromTable = 0");
        return FALSE;
    }

    switch (CtrlType)
    {
    case CTRL_SHUTDOWN_EVENT:
        FspServiceCtrlHandler(SERVICE_CONTROL_SHUTDOWN, 0, 0, Service);
        return TRUE;
    default:
        FspServiceCtrlHandler(SERVICE_CONTROL_STOP, 0, 0, Service);
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
