/**
 * @file winfsp/launch.h
 * WinFsp Launch API.
 *
 * In order to use the WinFsp Launch API a program must include &lt;winfsp/launch.h&gt;
 * and link with the winfsp_x64.dll (or winfsp_x86.dll) library.
 *
 * @copyright 2015-2022 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef WINFSP_LAUNCH_H_INCLUDED
#define WINFSP_LAUNCH_H_INCLUDED

#include <winfsp/winfsp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSP_LAUNCH_REGKEY               FSP_FSCTL_PRODUCT_REGKEY "\\Services"
#define FSP_LAUNCH_REGKEY_WOW64         FSP_FSCTL_PRODUCT_REGKEY_WOW64
#define FSP_LAUNCH_FULL_REGKEY          FSP_FSCTL_PRODUCT_FULL_REGKEY "\\Services"

#define FSP_LAUNCH_PIPE_NAME            "\\\\.\\pipe\\" FSP_FSCTL_PRODUCT_NAME ".{14E7137D-22B4-437A-B0C1-D21D1BDF3767}"
#define FSP_LAUNCH_PIPE_BUFFER_SIZE     4096
#define FSP_LAUNCH_PIPE_OWNER           ((PSID)WinLocalSystemSid)

/*
 * The launcher named pipe SDDL gives full access to LocalSystem and Administrators and
 * GENERIC_READ and FILE_WRITE_DATA access to Everyone. We are careful not to give the
 * FILE_CREATE_PIPE_INSTANCE right to Everyone to disallow the creation of additional
 * pipe instances.
 */
#define FSP_LAUNCH_PIPE_SDDL            "O:SYG:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRDCCR;;;WD)"

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
#define FSP_LAUNCH_SERVICE_DEFAULT_SDDL "D:P(A;;RPWPLC;;;SY)(A;;RPWPLC;;;BA)"
#define FSP_LAUNCH_SERVICE_WORLD_SDDL   "D:P(A;;RPWPLC;;;WD)"

enum
{
    FspLaunchCmdStart                   = 'S',  /* requires: SERVICE_START */
    FspLaunchCmdStartWithSecret         = 'X',  /* requires: SERVICE_START */
    FspLaunchCmdStop                    = 'T',  /* requires: SERVICE_STOP */
    FspLaunchCmdGetInfo                 = 'I',  /* requires: SERVICE_QUERY_STATUS */
    FspLaunchCmdGetNameList             = 'L',  /* requires: none*/
    FspLaunchCmdDefineDosDevice         = 'D',  /* internal: do not use! */
    FspLaunchCmdQuit                    = 'Q',  /* DEBUG version only */
};

enum
{
    FspLaunchCmdSuccess                 = '$',
    FspLaunchCmdFailure                 = '!',
};

/**
 * @group Launch Control
 */
/**
 * Call launcher pipe.
 *
 * This function is used to send a command to the launcher and receive a response.
 *
 * @param Command
 *     Launcher command to send. For example, the 'L' launcher command instructs
 *     the launcher to list all running service instances.
 * @param Argc
 *     Command argument count. May be 0.
 * @param Argv
 *     Command argument array. May be NULL.
 * @param Argl
 *     Command argument length array. May be NULL. If this is NULL all command arguments
 *     are assumed to be NULL-terminated strings. It is also possible for specific arguments
 *     to be NULL-terminated; in this case pass -1 in the corresponding Argl position.
 * @param Buffer
 *     Buffer that receives the command response. May be NULL.
 * @param PSize
 *     Pointer to a ULONG. On input it contains the size of the Buffer. On output it
 *     contains the number of bytes transferred. May be NULL.
 * @param PLauncherError
 *     Receives the launcher error if any. This is always a Win32 error code. May not be NULL.
 * @return
 *     STATUS_SUCCESS if the command is sent successfully to the launcher, even if the launcher
 *     returns an error. Other status codes indicate a communication error. Launcher errors are
 *     reported through PLauncherError.
 */
FSP_API NTSTATUS FspLaunchCallLauncherPipe(
    WCHAR Command, ULONG Argc, PWSTR *Argv, ULONG *Argl,
    PWSTR Buffer, PULONG PSize,
    PULONG PLauncherError);
/**
 * Call launcher pipe.
 *
 * This function is used to send a command to the launcher and receive a response.
 *
 * @param Command
 *     Launcher command to send. For example, the 'L' launcher command instructs
 *     the launcher to list all running service instances.
 * @param Argc
 *     Command argument count. May be 0.
 * @param Argv
 *     Command argument array. May be NULL.
 * @param Argl
 *     Command argument length array. May be NULL. If this is NULL all command arguments
 *     are assumed to be NULL-terminated strings. It is also possible for specific arguments
 *     to be NULL-terminated; in this case pass -1 in the corresponding Argl position.
 * @param Buffer
 *     Buffer that receives the command response. May be NULL.
 * @param PSize
 *     Pointer to a ULONG. On input it contains the size of the Buffer. On output it
 *     contains the number of bytes transferred. May be NULL.
 * @param AllowImpersonation
 *     Allow caller to be impersonated by launcher.
 * @param PLauncherError
 *     Receives the launcher error if any. This is always a Win32 error code. May not be NULL.
 * @return
 *     STATUS_SUCCESS if the command is sent successfully to the launcher, even if the launcher
 *     returns an error. Other status codes indicate a communication error. Launcher errors are
 *     reported through PLauncherError.
 */
FSP_API NTSTATUS FspLaunchCallLauncherPipeEx(
    WCHAR Command, ULONG Argc, PWSTR *Argv, ULONG *Argl,
    PWSTR Buffer, PULONG PSize,
    BOOLEAN AllowImpersonation,
    PULONG PLauncherError);
/**
 * Start a service instance.
 *
 * @param ClassName
 *     Class name of the service instance to start.
 * @param InstanceName
 *     Instance name of the service instance to start.
 * @param Argc
 *     Service instance argument count. May be 0.
 * @param Argv
 *     Service instance argument array. May be NULL.
 * @param HasSecret
 *     Whether the last argument in Argv is assumed to be a secret (e.g. password) or not.
 *     Secrets are passed to service instances through standard input rather than the command
 *     line.
 * @param PLauncherError
 *     Receives the launcher error if any. This is always a Win32 error code. May not be NULL.
 * @return
 *     STATUS_SUCCESS if the command is sent successfully to the launcher, even if the launcher
 *     returns an error. Other status codes indicate a communication error. Launcher errors are
 *     reported through PLauncherError.
 */
FSP_API NTSTATUS FspLaunchStart(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv,
    BOOLEAN HasSecret,
    PULONG PLauncherError);
/**
 * Start a service instance.
 *
 * @param ClassName
 *     Class name of the service instance to start.
 * @param InstanceName
 *     Instance name of the service instance to start.
 * @param Argc
 *     Service instance argument count. May be 0.
 * @param Argv
 *     Service instance argument array. May be NULL.
 * @param HasSecret
 *     Whether the last argument in Argv is assumed to be a secret (e.g. password) or not.
 *     Secrets are passed to service instances through standard input rather than the command
 *     line.
 * @param AllowImpersonation
 *     Allow caller to be impersonated by launcher.
 * @param PLauncherError
 *     Receives the launcher error if any. This is always a Win32 error code. May not be NULL.
 * @return
 *     STATUS_SUCCESS if the command is sent successfully to the launcher, even if the launcher
 *     returns an error. Other status codes indicate a communication error. Launcher errors are
 *     reported through PLauncherError.
 */
FSP_API NTSTATUS FspLaunchStartEx(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv,
    BOOLEAN HasSecret,
    BOOLEAN AllowImpersonation,
    PULONG PLauncherError);
/**
 * Stop a service instance.
 *
 * @param ClassName
 *     Class name of the service instance to stop.
 * @param InstanceName
 *     Instance name of the service instance to stop.
 * @param PLauncherError
 *     Receives the launcher error if any. This is always a Win32 error code. May not be NULL.
 * @return
 *     STATUS_SUCCESS if the command is sent successfully to the launcher, even if the launcher
 *     returns an error. Other status codes indicate a communication error. Launcher errors are
 *     reported through PLauncherError.
 */
FSP_API NTSTATUS FspLaunchStop(
    PWSTR ClassName, PWSTR InstanceName,
    PULONG PLauncherError);
/**
 * Get information about a service instance.
 *
 * The information is a list of NULL-terminated strings: the class name of the service instance,
 * the instance name of the service instance and the full command line used to start the service
 * instance.
 *
 * @param ClassName
 *     Class name of the service instance to stop.
 * @param InstanceName
 *     Instance name of the service instance to stop.
 * @param Buffer
 *     Buffer that receives the command response. May be NULL.
 * @param PSize
 *     Pointer to a ULONG. On input it contains the size of the Buffer. On output it
 *     contains the number of bytes transferred. May be NULL.
 * @param PLauncherError
 *     Receives the launcher error if any. This is always a Win32 error code. May not be NULL.
 * @return
 *     STATUS_SUCCESS if the command is sent successfully to the launcher, even if the launcher
 *     returns an error. Other status codes indicate a communication error. Launcher errors are
 *     reported through PLauncherError.
 */
FSP_API NTSTATUS FspLaunchGetInfo(
    PWSTR ClassName, PWSTR InstanceName,
    PWSTR Buffer, PULONG PSize,
    PULONG PLauncherError);
/**
 * List service instances.
 *
 * The information is a list of pairs of NULL-terminated strings. Each pair contains the class
 * name and instance name of a service instance. All currently running service instances are
 * listed.
 *
 * @param Buffer
 *     Buffer that receives the command response. May be NULL.
 * @param PSize
 *     Pointer to a ULONG. On input it contains the size of the Buffer. On output it
 *     contains the number of bytes transferred. May be NULL.
 * @param PLauncherError
 *     Receives the launcher error if any. This is always a Win32 error code. May not be NULL.
 * @return
 *     STATUS_SUCCESS if the command is sent successfully to the launcher, even if the launcher
 *     returns an error. Other status codes indicate a communication error. Launcher errors are
 *     reported through PLauncherError.
 */
FSP_API NTSTATUS FspLaunchGetNameList(
    PWSTR Buffer, PULONG PSize,
    PULONG PLauncherError);

/**
 * @group Service Registry
 */
#pragma warning(push)
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
/**
 * Service registry record.
 */
typedef struct _FSP_LAUNCH_REG_RECORD
{
    PWSTR Agent;
    PWSTR Executable;
    PWSTR CommandLine;
    PWSTR WorkDirectory;
    PWSTR RunAs;
    PWSTR Security;
    PWSTR AuthPackage;
    PWSTR Stderr;
    PVOID Reserved0[4];
    ULONG JobControl;
    ULONG Credentials;
    ULONG AuthPackageId;
    ULONG Recovery;
    ULONG Reserved1[4];
    UINT8 Buffer[];
} FSP_LAUNCH_REG_RECORD;
#pragma warning(pop)
/**
 * Add/change/delete a service registry record.
 *
 * @param ClassName
 *     The service class name.
 * @param Record
 *     The record to set in the registry. If NULL, the registry record is deleted.
 * @return
 *     STATUS_SUCCESS or error code.
 */
FSP_API NTSTATUS FspLaunchRegSetRecord(
    PWSTR ClassName,
    const FSP_LAUNCH_REG_RECORD *Record);
/**
 * Get a service registry record.
 *
 * @param ClassName
 *     The service class name.
 * @param Agent
 *     The name of the agent that is retrieving the service record. This API matches
 *     the supplied Agent against the Agent in the service record and it only returns
 *     the record if they match. Pass NULL to match any Agent.
 * @param PRecord
 *     Pointer to a record pointer. Memory for the service record will be allocated
 *     and a pointer to it will be stored at this address. This memory must be later
 *     freed using FspLaunchRegFreeRecord.
 * @return
 *     STATUS_SUCCESS or error code.
 * @see
 *     FspLaunchRegFreeRecord
 */
FSP_API NTSTATUS FspLaunchRegGetRecord(
    PWSTR ClassName, PWSTR Agent,
    FSP_LAUNCH_REG_RECORD **PRecord);
/**
 * Free a service registry record.
 *
 * @param Record
 *     The service record to free.
 * @see
 *     FspLaunchRegGetRecord
 */
FSP_API VOID FspLaunchRegFreeRecord(
    FSP_LAUNCH_REG_RECORD *Record);

#ifdef __cplusplus
}
#endif

#endif
