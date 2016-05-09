/**
 * @file launcher.c
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

#include <winfsp/winfsp.h>

#define PROGNAME                        "winfsp-launcher"

NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    return STATUS_SUCCESS;
}

NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}

int wmainCRTStartup(void)
{
    return wmain(0, 0);
}
