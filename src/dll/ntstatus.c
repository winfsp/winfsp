/**
 * @file dll/ntstatus.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <winfsp/winfsp.h>

NTSTATUS FspNtStatusFromWin32(DWORD Error)
{
    return STATUS_ACCESS_DENIED;
}
