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

// !!!: NOTIMPLEMENTED
#define FspEventLog(Type, Message, ...)

enum
{
    SetStatus_ServiceType               = 0x0001,
    SetStatus_CurrentState              = 0x0002,
    SetStatus_ControlsAccepted          = 0x0004,
    SetStatus_Win32ExitCode             = 0x0008,
    SetStatus_ServiceSpecificExitCode   = 0x0010,
    SetStatus_CheckPoint                = 0x0020,
    SetStatus_WaitHint                  = 0x0040,
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
    {
        Service = (PVOID)((PUINT8)FspServiceTable[0].lpServiceName -
            FIELD_OFFSET(FSP_SERVICE, ServiceName));
        FspServiceTable = 0;
    }

    return Service;
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
    Service->ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    Service->ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    Service->ServiceStatus.dwControlsAccepted = 0;
    Service->ServiceStatus.dwWin32ExitCode = NO_ERROR;
    Service->ServiceStatus.dwServiceSpecificExitCode = 0;
    Service->ServiceStatus.dwCheckPoint = 0;
    Service->ServiceStatus.dwWaitHint = 0;

    *PService = Service;

    return STATUS_SUCCESS;
}

FSP_API VOID FspServiceDelete(FSP_SERVICE *Service)
{
    if (0 != Service->InteractiveEvent)
        CloseHandle(Service->InteractiveEvent);

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
    if (Flags & SetStatus_CheckPoint)
    {
        DWORD Temp = ServiceStatus->dwCheckPoint;
        if (Flags & GetStatus_CheckPoint)
            ServiceStatus->dwCheckPoint = Service->ServiceStatus.dwCheckPoint;

        /* treat CheckPoint specially! */
        if (0 == Temp)
            Service->ServiceStatus.dwCheckPoint = 0;
        else
            Service->ServiceStatus.dwCheckPoint += Temp;
    }
    XCHG(WaitHint);

    if (0 != Service->StatusHandle)
    {
        if (!SetServiceStatus(Service->StatusHandle, &Service->ServiceStatus))
            FspEventLog(EVENTLOG_ERROR_TYPE,
                L"" __FUNCTION__ ": error = %ld", GetLastError());
    }
    else if (0 != Service->InteractiveEvent &&
        SERVICE_STOPPED == Service->ServiceStatus.dwCurrentState)
    {
        SetEvent(Service->InteractiveEvent);
    }

    LeaveCriticalSection(&Service->ServiceStatusGuard);

#undef XCHG
}

FSP_API VOID FspServiceAllowInteractive(FSP_SERVICE *Service)
{
    Service->AllowInteractive = TRUE;
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
        SetStatus_CheckPoint | SetStatus_WaitHint, &ServiceStatus);
}

FSP_API VOID FspServiceSetExitCode(FSP_SERVICE *Service, ULONG ExitCode)
{
    Service->ExitCode = ExitCode;
}

FSP_API NTSTATUS FspServiceRun(FSP_SERVICE *Service)
{
    SERVICE_TABLE_ENTRYW ServiceTable[2];

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
        if (!Service->AllowInteractive || ERROR_FAILED_SERVICE_CONTROLLER_CONNECT != LastError)
            return FspNtStatusFromWin32(LastError);

        /* enter INTERACTIVE (DEBUG) mode! */

        Service->InteractiveEvent = CreateEventW(0, TRUE, FALSE, 0);
        if (0 == Service->InteractiveEvent)
            return FspNtStatusFromWin32(GetLastError());

        Thread = CreateThread(0, 0, FspServiceInteractiveThread, Service, 0, 0);
        if (0 == Thread)
            return FspNtStatusFromWin32(GetLastError());
        WaitResult = WaitForSingleObject(Thread, INFINITE);
        CloseHandle(Thread);
        if (WAIT_OBJECT_0 != WaitResult)
            return FspNtStatusFromWin32(GetLastError());

        if (!SetConsoleCtrlHandler(FspServiceConsoleCtrlHandler, TRUE))
            return FspNtStatusFromWin32(GetLastError());

        WaitResult = WaitForSingleObject(Service->InteractiveEvent, INFINITE);
        if (WAIT_OBJECT_0 != WaitResult)
            return FspNtStatusFromWin32(GetLastError());
    }

    return STATUS_SUCCESS;
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
    }
    else
    {
        /* revert the service status */
        FspServiceSetStatus(Service,
            SetStatus_CurrentState | SetStatus_CheckPoint | SetStatus_WaitHint,
            &ServiceStatus);
    }
}

static VOID WINAPI FspServiceEntry(DWORD Argc, PWSTR *Argv)
{
    FSP_SERVICE *Service;

    Service = FspServiceFromTable();
    if (0 == Service)
    {
        FspEventLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": internal error: FspServiceFromTable = 0");
        return;
    }

    Service->StatusHandle = RegisterServiceCtrlHandlerExW(Service->ServiceName,
        FspServiceCtrlHandler, Service);
    if (0 == Service->StatusHandle)
    {
        FspEventLog(EVENTLOG_ERROR_TYPE,
            L"" __FUNCTION__ ": RegisterServiceCtrlHandlerW = %ld", GetLastError());
        return;
    }

    FspServiceMain(Service, Argc, Argv);
}

static VOID FspServiceMain(FSP_SERVICE *Service, DWORD Argc, PWSTR *Argv)
{
    SERVICE_STATUS ServiceStatus;
    NTSTATUS Result;

    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;
    FspServiceSetStatus(Service,
        SetStatus_CurrentState | SetStatus_ControlsAccepted | SetStatus_CheckPoint | SetStatus_WaitHint,
        &ServiceStatus);

    Result = STATUS_SUCCESS;
    if (0 != Service->OnStart)
        Result = Service->OnStart(Service, Argc, Argv);

    if (NT_SUCCESS(Result))
    {
        ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        ServiceStatus.dwControlsAccepted = Service->AcceptControl;
        FspServiceSetStatus(Service,
            SetStatus_CurrentState | SetStatus_ControlsAccepted, &ServiceStatus);
    }
    else
    {
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode = FspWin32FromNtStatus(Result);
        FspServiceSetStatus(Service,
            SetStatus_CurrentState | SetStatus_Win32ExitCode, &ServiceStatus);
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
        FspEventLog(EVENTLOG_ERROR_TYPE,
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
        FspEventLog(EVENTLOG_ERROR_TYPE,
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
