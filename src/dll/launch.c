/**
 * @file dll/launch.c
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

#include <dll/library.h>

FSP_API NTSTATUS FspLaunchCallLauncherPipe(
    WCHAR Command, ULONG Argc, PWSTR *Argv, ULONG *Argl,
    PWSTR Buffer, PULONG PSize, PULONG PLauncherError)
{
    PWSTR PipeBuf = 0, P;
    ULONG Length, BytesTransferred;
    NTSTATUS Result;
    ULONG ErrorCode;

    if (0 != PSize)
        *PSize = 0;
    *PLauncherError = 0;

    PipeBuf = MemAlloc(FSP_LAUNCH_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    P = PipeBuf;
    *P++ = Command;
    for (ULONG I = 0; Argc > I; I++)
        if (0 != Argv[I])
        {
            Length = 0 == Argl || -1 == Argl[I] ? lstrlenW(Argv[I]) : Argl[I];
            if (FSP_LAUNCH_PIPE_BUFFER_SIZE < ((ULONG)(P - PipeBuf) + Length + 1) * sizeof(WCHAR))
            {
                Result = STATUS_INVALID_PARAMETER;
                goto exit;
            }
            memcpy(P, Argv[I], Length * sizeof(WCHAR)); P += Length; *P++ = L'\0';
        }

    Result = FspCallNamedPipeSecurely(L"" FSP_LAUNCH_PIPE_NAME,
        PipeBuf, (ULONG)(P - PipeBuf) * sizeof(WCHAR), PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT, FSP_LAUNCH_PIPE_OWNER);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;
    ErrorCode = ERROR_BROKEN_PIPE; /* protocol error! */
    if (sizeof(WCHAR) <= BytesTransferred)
    {
        if (FspLaunchCmdSuccess == PipeBuf[0])
        {
            ErrorCode = 0;

            if (0 != PSize)
            {
                BytesTransferred -= sizeof(WCHAR);
                memcpy(Buffer, PipeBuf + 1, *PSize < BytesTransferred ? *PSize : BytesTransferred);
                *PSize = BytesTransferred;
            }
        }
        else if (FspLaunchCmdFailure == PipeBuf[0])
        {
            ErrorCode = 0;

            for (PWSTR P = PipeBuf + 1, EndP = PipeBuf + BytesTransferred / sizeof(WCHAR); EndP > P; P++)
            {
                if (L'0' > *P || *P > L'9')
                    break;
                ErrorCode = 10 * ErrorCode + (*P - L'0');
            }

            if (0 == ErrorCode)
                ErrorCode = ERROR_BROKEN_PIPE; /* protocol error! */
        }
    }

    *PLauncherError = ErrorCode;

exit:
    MemFree(PipeBuf);

    return Result;
}

FSP_API NTSTATUS FspLaunchStart(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv0,
    BOOLEAN HasSecret,
    PULONG PLauncherError)
{
    PWSTR Argv[9 + 2];

    if (9 < Argc)
        return STATUS_INVALID_PARAMETER;

    Argv[0] = ClassName;
    Argv[1] = InstanceName;
    memcpy(Argv + 2, Argv, Argc * sizeof(PWSTR));

    return FspLaunchCallLauncherPipe(
        HasSecret ? FspLaunchCmdStartWithSecret : FspLaunchCmdStart,
        Argc + 2, Argv, 0, 0, 0, PLauncherError);
}

FSP_API NTSTATUS FspLaunchStop(
    PWSTR ClassName, PWSTR InstanceName,
    PULONG PLauncherError)
{
    PWSTR Argv[2];

    Argv[0] = ClassName;
    Argv[1] = InstanceName;

    return FspLaunchCallLauncherPipe(FspLaunchCmdStop,
        2, Argv, 0, 0, 0, PLauncherError);
}

FSP_API NTSTATUS FspLaunchGetInfo(
    PWSTR ClassName, PWSTR InstanceName,
    PWSTR Buffer, PULONG PSize,
    PULONG PLauncherError)
{
    PWSTR Argv[2];

    Argv[0] = ClassName;
    Argv[1] = InstanceName;

    return FspLaunchCallLauncherPipe(FspLaunchCmdGetInfo,
        2, Argv, 0, Buffer, PSize, PLauncherError);
}

FSP_API NTSTATUS FspLaunchGetNameList(
    PWSTR Buffer, PULONG PSize,
    PULONG PLauncherError)
{
    return FspLaunchCallLauncherPipe(FspLaunchCmdGetNameList,
        0, 0, 0, Buffer, PSize, PLauncherError);
}
