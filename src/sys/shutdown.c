/**
 * @file sys/shutdown.c
 *
 * @copyright 2015-2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <sys/driver.h>

FSP_DRIVER_DISPATCH FspShutdown;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspShutdown)
#endif

NTSTATUS FspShutdown(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);

    FSP_LEAVE_MJ("%s", "");
}
