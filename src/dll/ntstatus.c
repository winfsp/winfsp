/**
 * @file dll/ntstatus.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error)
{
    switch (Error)
    {
    #include "ntstatus.i"
    default:
        return STATUS_ACCESS_DENIED;
    }
}
