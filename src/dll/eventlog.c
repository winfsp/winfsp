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

    wvsprintfW(Buf, Format, ap);
    Buf[(sizeof Buf / sizeof Buf[0]) - 1] = L'\0';

    Strings[0] = Buf;
    ReportEventW(FspEventLogHandle, (WORD)Type, 0, 1, 0, 1, 0, Strings, 0);
}

static BOOL WINAPI FspEventLogRegisterEventSource(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    FspEventLogHandle = RegisterEventSourceW(0, L"" LIBRARY_NAME);
    return TRUE;
}
