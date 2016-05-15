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

#define STOP_TIMEOUT                    5500
#define KILL_TIMEOUT                    5000

#define PIPE_NAME                       "\\\\.\\pipe\\WinFsp.{14E7137D-22B4-437A-B0C1-D21D1BDF3767}"
#define PIPE_BUFFER_SIZE                2048
#define PIPE_DEFAULT_TIMEOUT            3000

/*
 * The launcher named pipe SDDL gives full access to LocalSystem and Administrators.
 * It also gives GENERIC_READ and GENERIC_WRITE access to Everyone. This includes the
 * FILE_CREATE_PIPE_INSTANCE right which should not normally be granted to any process
 * that is not the pipe server. The reason that the GENERIC_WRITE is required is to allow
 * clients to use CallNamedPipeW which opens the pipe handle using CreateFileW and the
 * GENERIC_READ | GENERIC_WRITE access right. The reason that it should be safe to grant
 * the FILE_CREATE_PIPE_INSTANCE right is that the server creates the named pipe with
 * MaxInstances == 1 (and therefore no client can create additional instances).
 */
#define PIPE_SDDL                       "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;WD)"

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
    LauncherSvcInstanceStop             = 'T',  /* requires: SERVICE_STOP */
    LauncherSvcInstanceInfo             = 'I',  /* requires: SERVICE_QUERY_STATUS */
    LauncherSvcInstanceList             = 'L',  /* requires: none*/
    LauncherQuit                        = 'Q',  /* DEBUG version only */
};

#endif
