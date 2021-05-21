/**
 * @file dll/wksid.c
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

static INIT_ONCE FspWksidInitOnce = INIT_ONCE_STATIC_INIT;
static PSID FspWksidWorld;
static PSID FspWksidAuthenticatedUser;
static PSID FspWksidLocalSystem;
static PSID FspWksidService;

static BOOL WINAPI FspWksidInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    FspWksidWorld = FspWksidNew(WinWorldSid, 0);
    FspWksidAuthenticatedUser = FspWksidNew(WinAuthenticatedUserSid, 0);
    FspWksidLocalSystem = FspWksidNew(WinLocalSystemSid, 0);
    FspWksidService = FspWksidNew(WinServiceSid, 0);

    //DEBUGLOGSID("FspWksidWorld=%s", FspWksidWorld);
    //DEBUGLOGSID("FspWksidAuthenticatedUser=%s", FspWksidAuthenticatedUser);
    //DEBUGLOGSID("FspWksidLocalSystem=%s", FspWksidLocalSystem);
    //DEBUGLOGSID("FspWksidService=%s", FspWksidService);

    return TRUE;
}

VOID FspWksidFinalize(BOOLEAN Dynamic)
{
    /*
     * This function is called during DLL_PROCESS_DETACH. We must therefore keep
     * finalization tasks to a minimum.
     *
     * We must deregister our event source (if any). We only do so if the library
     * is being explicitly unloaded (rather than the process exiting).
     */

    if (Dynamic)
    {
        MemFree(FspWksidWorld); FspWksidWorld = 0;
        MemFree(FspWksidAuthenticatedUser); FspWksidAuthenticatedUser = 0;
        MemFree(FspWksidLocalSystem); FspWksidLocalSystem = 0;
        MemFree(FspWksidService); FspWksidService = 0;
    }
}

PSID FspWksidNew(WELL_KNOWN_SID_TYPE WellKnownSidType, PNTSTATUS PResult)
{
    NTSTATUS Result;
    PSID Sid;
    DWORD Size;

    Size = SECURITY_MAX_SID_SIZE;
    Sid = MemAlloc(Size);
    if (0 == Sid)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!CreateWellKnownSid(WellKnownSidType, 0, Sid, &Size))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        MemFree(Sid); Sid = 0;
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    if (0 != PResult)
        *PResult = Result;

    return Sid;
}

PSID FspWksidGet(WELL_KNOWN_SID_TYPE WellKnownSidType)
{
    InitOnceExecuteOnce(&FspWksidInitOnce, FspWksidInitialize, 0, 0);

    switch (WellKnownSidType)
    {
    case WinWorldSid:
        return FspWksidWorld;
    case WinAuthenticatedUserSid:
        return FspWksidAuthenticatedUser;
    case WinLocalSystemSid:
        return FspWksidLocalSystem;
    case WinServiceSid:
        return FspWksidService;
    default:
        return 0;
    }
}
