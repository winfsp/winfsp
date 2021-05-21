/**
 * @file dll/eventlog.c
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
#include <stdarg.h>
#include "eventlog/eventlog.h"

#define FSP_EVENTLOG_NAME               LIBRARY_NAME

static INIT_ONCE FspEventLogInitOnce = INIT_ONCE_STATIC_INIT;
static HANDLE FspEventLogHandle;

static BOOL WINAPI FspEventLogInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    FspEventLogHandle = RegisterEventSourceW(0, L"" FSP_EVENTLOG_NAME);
    if (0 == FspEventLogHandle)
        FspEventLogHandle = RegisterEventSourceW(0, FspDiagIdent());
    return TRUE;
}

VOID FspEventLogFinalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     *
     * We must deregister our event source (if any). We only do so if the library
     * is being explicitly unloaded (rather than the process exiting).
     */

    if (Dynamic && 0 != FspEventLogHandle)
        DeregisterEventSource(FspEventLogHandle);
}

FSP_API VOID FspEventLog(ULONG Type, PWSTR Format, ...)
{
    va_list ap;

    va_start(ap, Format);
    FspEventLogV(Type, Format, ap);
    va_end(ap);
}

FSP_API VOID FspEventLogV(ULONG Type, PWSTR Format, va_list ap)
{
    InitOnceExecuteOnce(&FspEventLogInitOnce, FspEventLogInitialize, 0, 0);
    if (0 == FspEventLogHandle)
        return;

    WCHAR Buf[1024], *Strings[2];
        /* wvsprintfW is only safe with a 1024 WCHAR buffer */
    DWORD EventId;

    Strings[0] = FspDiagIdent();

    wvsprintfW(Buf, Format, ap);
    Buf[(sizeof Buf / sizeof Buf[0]) - 1] = L'\0';
    Strings[1] = Buf;

    switch (Type)
    {
    default:
    case EVENTLOG_INFORMATION_TYPE:
    case EVENTLOG_SUCCESS:
        EventId = FSP_EVENTLOG_INFORMATION;
        break;
    case EVENTLOG_WARNING_TYPE:
        EventId = FSP_EVENTLOG_WARNING;
        break;
    case EVENTLOG_ERROR_TYPE:
        EventId = FSP_EVENTLOG_ERROR;
        break;
    }

    ReportEventW(FspEventLogHandle, (WORD)Type, 0, EventId, 0, 2, 0, Strings, 0);
}

NTSTATUS FspEventLogRegister(VOID)
{
    extern HINSTANCE DllInstance;
    WCHAR Path[MAX_PATH];
    DWORD RegResult, DwordValue;
    HKEY RegKey;

    if (0 == GetModuleFileNameW(DllInstance, Path, MAX_PATH))
        return FspNtStatusFromWin32(GetLastError());

    RegResult = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" FSP_EVENTLOG_NAME,
        0, 0, 0, KEY_ALL_ACCESS, 0, &RegKey, 0);
    if (ERROR_SUCCESS != RegResult)
        return FspNtStatusFromWin32(RegResult);

    RegResult = RegSetValueExW(RegKey,
        L"EventMessageFile", 0, REG_SZ, (PVOID)Path, (lstrlenW(Path) + 1) * sizeof(WCHAR));
    if (ERROR_SUCCESS != RegResult)
        goto close_and_exit;

    DwordValue = EVENTLOG_INFORMATION_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_ERROR_TYPE;
    RegResult = RegSetValueExW(RegKey,
        L"TypesSupported", 0, REG_DWORD, (PVOID)&DwordValue, sizeof DwordValue);
    if (ERROR_SUCCESS != RegResult)
        goto close_and_exit;

    RegCloseKey(RegKey);

    return STATUS_SUCCESS;

close_and_exit:
    RegCloseKey(RegKey);
    return FspNtStatusFromWin32(RegResult);
}

NTSTATUS FspEventLogUnregister(VOID)
{
    DWORD RegResult;

    RegResult = RegDeleteTreeW(
        HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" FSP_EVENTLOG_NAME);
    if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
        return FspNtStatusFromWin32(RegResult);

    return STATUS_SUCCESS;
}
