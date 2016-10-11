/**
 * @file launcher/launcher.h
 *
 * @copyright 2015-2016 Bill Zissimopoulos
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

#ifndef WINFSP_LAUNCHER_LAUNCHER_H_INCLUDED
#define WINFSP_LAUNCHER_LAUNCHER_H_INCLUDED

#include <winfsp/winfsp.h>
#include <shared/minimal.h>

#define LAUNCHER_REGKEY                 "SYSTEM\\CurrentControlSet\\Services\\WinFsp.Launcher\\Services"

#define LAUNCHER_STOP_TIMEOUT           5500
#define LAUNCHER_KILL_TIMEOUT           5000

#define LAUNCHER_PIPE_NAME              "\\\\.\\pipe\\WinFsp.{14E7137D-22B4-437A-B0C1-D21D1BDF3767}"
#define LAUNCHER_PIPE_BUFFER_SIZE       4096
#define LAUNCHER_PIPE_DEFAULT_TIMEOUT   3000

#define LAUNCHER_START_WITH_SECRET_TIMEOUT 15000

/*
 * The launcher named pipe SDDL gives full access to LocalSystem and Administrators and
 * GENERIC_READ and FILE_WRITE_DATA access to Everyone. We are careful not to give the
 * FILE_CREATE_PIPE_INSTANCE right to Everyone to disallow the creation of additional
 * pipe instances.
 */
#define LAUNCHER_PIPE_SDDL              "O:SYG:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRDCCR;;;WD)"
#define LAUNCHER_PIPE_OWNER             ((PSID)WinLocalSystemSid)

 /*
 * The default service instance SDDL gives full access to LocalSystem and Administrators.
 * The only possible service instance rights are as follows:
 *     RP   SERVICE_START
 *     WP   SERVICE_STOP
 *     LC   SERVICE_QUERY_STATUS
 *
 * To create a service that can be started, stopped or queried by Everyone, you can set
 * the following SDDL:
 *     D:P(A;;RPWPLC;;;WD)
 */
#define SVC_INSTANCE_DEFAULT_SDDL       "D:P(A;;RPWPLC;;;SY)(A;;RPWPLC;;;BA)"

enum
{
    LauncherSvcInstanceStart            = 'S',  /* requires: SERVICE_START */
    LauncherSvcInstanceStartWithSecret  = 'X',  /* requires: SERVICE_START */
    LauncherSvcInstanceStop             = 'T',  /* requires: SERVICE_STOP */
    LauncherSvcInstanceInfo             = 'I',  /* requires: SERVICE_QUERY_STATUS */
    LauncherSvcInstanceList             = 'L',  /* requires: none*/
    LauncherQuit                        = 'Q',  /* DEBUG version only */

    LauncherSuccess                     = '$',
    LauncherFailure                     = '!',
};

#endif
