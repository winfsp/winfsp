/**
 * @file launcher/launchctl.c
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

#include <launcher/launcher.h>

#define PROGNAME                        "launchctl"

int wmain(int argc, wchar_t **argv)
{
    return 0;
}

int wmainCRTStartup(void)
{
    return wmain(0, 0);
}
