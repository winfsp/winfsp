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

/*
 * The launcher named pipe SDDL gives full access to LocalSystem and Administrators.
 * It also gives generic read access and FILE_WRITE_DATA (SC) to Everyone. Note that
 * we cannot give generic write access or equivalently FILE_GENERIC_WRITE (FW) because
 * we would also grant the FILE_CREATE_PIPE_INSTANCE right.
 */
#define PIPE_SDDL                       "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRSC;;;WD)"

 /*
 * The default service instance SDDL gives full access to LocalSystem and Administrators.
 * The only possible service instance rights are as follows:
 *     RP   SERVICE_START
 *     WP   SERVICE_STOP
 *     SC   SERVICE_QUERY_STATUS
 *
 * To create a service that can be started, stopped or queried by Everyone, you can set
 * the following SDDL:
 *     D:P(A;;RPWPSC;;;WD)
 */
#define SVC_INSTANCE_DEFAULT_SDDL       "O:SYG:SYD:P(A;;RPWPSC;;;SY)(A;;RPWPSC;;;BA)"

enum
{
    LauncherSvcInstanceStart            = 'S',  /* requires: SERVICE_START */
    LauncherSvcInstanceStop             = 'T',  /* requires: SERVICE_STOP */
    LauncherSvcInstanceList             = 'L',  /* requires: none*/
    LauncherSvcInstanceInfo             = 'I',  /* requires: SERVICE_QUERY_STATUS */
};

#endif
