/**
 * @file dll/library.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <dll/library.h>

HINSTANCE DllInstance;
HANDLE ProcessHeap;

BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        DllInstance = Instance;
        ProcessHeap = GetProcessHeap();
        if (0 == ProcessHeap)
            return FALSE;
        FspFileSystemInitialize();
        break;
    case DLL_PROCESS_DETACH:
        FspFileSystemFinalize();
        break;
    }

    return TRUE;
}

/* see comments in library.h */
#if defined(WINFSP_DLL_NODEFAULTLIB)
BOOL WINAPI _DllMainCRTStartup(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    return DllMain(Instance, Reason, Reserved);
}
#endif

HRESULT WINAPI DllRegisterServer(VOID)
{
    NTSTATUS Result;

    Result = FspNpRegister();

    return NT_SUCCESS(Result) ? S_OK : 0x80040201/*SELFREG_E_CLASS*/;
}

HRESULT WINAPI DllUnregisterServer(VOID)
{
    NTSTATUS Result;

    Result = FspNpUnregister();

    return NT_SUCCESS(Result) ? S_OK : 0x80040201/*SELFREG_E_CLASS*/;
}
