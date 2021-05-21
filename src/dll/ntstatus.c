/**
 * @file dll/ntstatus.c
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

static INIT_ONCE FspNtStatusInitOnce = INIT_ONCE_STATIC_INIT;
static ULONG (WINAPI *FspRtlNtStatusToDosError)(NTSTATUS Status);

static BOOL WINAPI FspNtStatusInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    HANDLE Handle;

    Handle = GetModuleHandleW(L"ntdll.dll");
    if (0 != Handle)
        FspRtlNtStatusToDosError = (PVOID)GetProcAddress(Handle, "RtlNtStatusToDosError");

    return TRUE;
}

FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error)
{
    switch (Error)
    {
    #include "ntstatus.i"
    default:
        /* use FACILITY_NTWIN32 if able, else STATUS_ACCESS_DENIED */
        return 0xffff >= Error ? (0x80070000 | Error) : STATUS_ACCESS_DENIED;
    }
}

FSP_API DWORD FspWin32FromNtStatus(NTSTATUS Status)
{
    InitOnceExecuteOnce(&FspNtStatusInitOnce, FspNtStatusInitialize, 0, 0);
    if (0 == FspRtlNtStatusToDosError)
        return ERROR_MR_MID_NOT_FOUND;

    return FspRtlNtStatusToDosError(Status);
}
