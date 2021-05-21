/**
 * @file dll/library.c
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

HINSTANCE DllInstance;

BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    BOOLEAN Dynamic;

    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        DllInstance = Instance;
        break;

    case DLL_PROCESS_DETACH:
        /*
         * These functions are called during DLL_PROCESS_DETACH. We must therefore keep
         * finalization tasks to a minimum.
         *
         * See the following documents:
         *     https://msdn.microsoft.com/en-us/library/windows/desktop/dn633971(v=vs.85).aspx
         *     https://blogs.msdn.microsoft.com/oldnewthing/20070503-00/?p=27003/
         *     https://blogs.msdn.microsoft.com/oldnewthing/20100122-00/?p=15193/
         */
        Dynamic = 0 == Reserved;
        fsp_fuse_finalize(Dynamic);
        FspServiceFinalize(Dynamic);
        FspFileSystemFinalize(Dynamic);
        FspEventLogFinalize(Dynamic);
        FspPosixFinalize(Dynamic);
        FspWksidFinalize(Dynamic);
        break;

    case DLL_THREAD_DETACH:
        fsp_fuse_finalize_thread();
        break;
    }

    return TRUE;
}

/* see comments in shared/minimal.h */
BOOL WINAPI _DllMainCRTStartup(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    return DllMain(Instance, Reason, Reserved);
}

HRESULT WINAPI DllRegisterServer(VOID)
{
    NTSTATUS Result;

    Result = FspFsctlRegister();
    FspDebugLog("FspFsctlRegister = %lx\n", Result);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* ignore errors below; these are non-critical */

    Result = FspNpRegister();
    FspDebugLog("FspNpRegister = %lx\n", Result);

    Result = FspEventLogRegister();
    FspDebugLog("FspEventLogRegister = %lx\n", Result);

    Result = STATUS_SUCCESS;

exit:
    return NT_SUCCESS(Result) ? S_OK : 0x80040201/*SELFREG_E_CLASS*/;
}

HRESULT WINAPI DllUnregisterServer(VOID)
{
    NTSTATUS Result;

    Result = FspFsctlUnregister();
    FspDebugLog("FspFsctlUnregister = %lx\n", Result);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* ignore errors below; these are non-critical */

    Result = FspNpUnregister();
    FspDebugLog("FspNpUnregister = %lx\n", Result);

    Result = FspEventLogUnregister();
    FspDebugLog("FspEventLogUnregister = %lx\n", Result);

    Result = STATUS_SUCCESS;

exit:
    return NT_SUCCESS(Result) ? S_OK : 0x80040201/*SELFREG_E_CLASS*/;
}
