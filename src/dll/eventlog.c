/**
 * @file dll/eventlog.c
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
#include <stdarg.h>

static HANDLE FspEventLogHandle;
static INIT_ONCE FspEventLogInitOnce = INIT_ONCE_STATIC_INIT;
static BOOL WINAPI FspEventLogRegisterEventSource(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context);

FSP_API VOID FspEventLog(ULONG Type, PWSTR Format, ...)
{
    va_list ap;

    va_start(ap, Format);
    FspEventLogV(Type, Format, ap);
    va_end(ap);
}

FSP_API VOID FspEventLogV(ULONG Type, PWSTR Format, va_list ap)
{
    InitOnceExecuteOnce(&FspEventLogInitOnce, FspEventLogRegisterEventSource, 0, 0);
    if (0 == FspEventLogHandle)
        return;

    WCHAR Buf[1024], *Strings[1];
        /* wvsprintfW is only safe with a 1024 WCHAR buffer */
    DWORD EventId;

    wvsprintfW(Buf, Format, ap);
    Buf[(sizeof Buf / sizeof Buf[0]) - 1] = L'\0';
    Strings[0] = Buf;

    /*
     * Event Identifier Format:
     *
     *      3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
     *      1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
     *     +---+-+-+-----------------------+-------------------------------+
     *     |Sev|C|R|     Facility          |               Code            |
     *     +---+-+-+-----------------------+-------------------------------+
     *
     * Sev - Severity:
     *     00 - Success
     *     01 - Informational
     *     10 - Warning
     *     11 - Error
     *
     * C - Customer:
     *     0 - System code
     *     1 - Customer code
     *
     * R - Reserved
     *
     * See https://msdn.microsoft.com/en-us/library/windows/desktop/aa363651(v=vs.85).aspx
     */
    switch (Type)
    {
    case EVENTLOG_ERROR_TYPE:
        EventId = 0xd0000001;
        break;
    case EVENTLOG_WARNING_TYPE:
        EventId = 0xc0000001;
        break;
    case EVENTLOG_INFORMATION_TYPE:
    case EVENTLOG_SUCCESS:
    default:
        EventId = 0x60000001;
        break;
    }

    ReportEventW(FspEventLogHandle, (WORD)Type, 0, EventId, 0, 1, 0, Strings, 0);
}

static BOOL WINAPI FspEventLogRegisterEventSource(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    FspEventLogHandle = RegisterEventSourceW(0, L"" LIBRARY_NAME);
    return TRUE;
}
