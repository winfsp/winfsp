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

FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error)
{
    switch (Error)
    {
    #include "ntstatus.i"
    default:
        return STATUS_ACCESS_DENIED;
    }
}
