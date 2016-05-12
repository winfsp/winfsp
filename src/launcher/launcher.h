/**
 * @file launcher/launcher.h
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

#ifndef WINFSP_LAUNCHER_LAUNCHER_H_INCLUDED
#define WINFSP_LAUNCHER_LAUNCHER_H_INCLUDED

#include <winfsp/winfsp.h>
#include <shared/minimal.h>

#define PIPE_NAME                       "\\\\.\\pipe\\WinFsp.{14E7137D-22B4-437A-B0C1-D21D1BDF3767}"
#define PIPE_BUFFER_SIZE                2048
#define PIPE_DEFAULT_TIMEOUT            3000

enum
{
    LauncherSvcInstanceStart            = 'S',
    LauncherSvcInstanceStop             = 'T',
    LauncherSvcInstanceList             = 'L',
    LauncherSvcInstanceInfo             = 'I',
};

#endif
