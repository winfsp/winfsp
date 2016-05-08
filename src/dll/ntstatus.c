/**
 * @file dll/ntstatus.c
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

static ULONG (WINAPI *FspRtlNtStatusToDosError)(NTSTATUS Status);

VOID FspNtStatusInitialize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_ATTACH. We must therefore keep
     * initialization tasks to a minimum.
     *
     * GetModuleHandle/GetProcAddress is allowed (because they are kernel32 API's)! See:
     *     https://msdn.microsoft.com/en-us/library/windows/desktop/dn633971(v=vs.85).aspx
     */

    HANDLE Handle;

    Handle = GetModuleHandleW(L"ntdll.dll");
    if (0 != Handle)
        FspRtlNtStatusToDosError = (PVOID)GetProcAddress(Handle, "RtlNtStatusToDosError");
}

VOID FspNtStatusFinalize(BOOLEAN Dynamic)
{
}

FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error)
{
    switch (Error)
    {
    #include "ntstatus.i"
    default:
        return STATUS_ACCESS_DENIED;
    }
}

FSP_API DWORD FspWin32FromNtStatus(NTSTATUS Status)
{
    if (0 == FspRtlNtStatusToDosError)
        return ERROR_MR_MID_NOT_FOUND;

    return FspRtlNtStatusToDosError(Status);
}
