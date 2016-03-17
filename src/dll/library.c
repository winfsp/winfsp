/**
 * @file dll/library.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#include <dll/library.h>

HANDLE ProcessHeap;

BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        ProcessHeap = GetProcessHeap();
        if (0 == ProcessHeap)
            return FALSE;
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
