/**
 * @file dll/library.c
 *
 * @copyright 2015 Bill Zissimopoulos
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
