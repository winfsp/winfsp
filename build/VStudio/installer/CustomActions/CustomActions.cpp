/**
 * @file CustomActions.cpp
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <msiquery.h>
#include <wcautil.h>
#include <strutil.h>

UINT __stdcall ServiceRunning(MSIHANDLE MsiHandle)
{
#if 0
    WCHAR MessageBuf[64];
    wsprintfW(MessageBuf, L"PID=%ld", GetCurrentProcessId());
    MessageBoxW(0, MessageBuf, L"" __FUNCTION__ " Break", MB_OK);
#endif

    HRESULT hr = S_OK;
    UINT err = ERROR_SUCCESS;
    PWSTR ServiceName = 0;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    SERVICE_STATUS ServiceStatus;
    int Result = 0;

    hr = WcaInitialize(MsiHandle, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    WcaGetProperty(L"" __FUNCTION__, &ServiceName);
    ExitOnFailure(hr, "Failed to get ServiceName");

    WcaLog(LOGMSG_STANDARD, "Initialized: \"%S\"", ServiceName);

    ScmHandle = OpenSCManagerW(0, 0, 0);
    ExitOnNullWithLastError(ScmHandle, hr, "Failed to open SCM");

    SvcHandle = OpenServiceW(ScmHandle, ServiceName, SERVICE_QUERY_STATUS);
    if (0 != SvcHandle && QueryServiceStatus(SvcHandle, &ServiceStatus))
        Result = SERVICE_STOPPED != ServiceStatus.dwCurrentState;

    WcaSetIntProperty(L"" __FUNCTION__, Result);

LExit:
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    ReleaseStr(ServiceName);

    err = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(err);
}

extern "C"
BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    switch(Reason)
    {
    case DLL_PROCESS_ATTACH:
        WcaGlobalInitialize(Instance);
        break;
    case DLL_PROCESS_DETACH:
        WcaGlobalFinalize();
        break;
    }

    return TRUE;
}
