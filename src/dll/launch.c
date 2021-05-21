/**
 * @file dll/launch.c
 *
 * @copyright 2015-2021 Bill Zissimopoulos
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

#include <dll/library.h>

FSP_API NTSTATUS FspLaunchCallLauncherPipe(
    WCHAR Command, ULONG Argc, PWSTR *Argv, ULONG *Argl,
    PWSTR Buffer, PULONG PSize,
    PULONG PLauncherError)
{
    return FspLaunchCallLauncherPipeEx(
        Command, Argc, Argv, Argl, Buffer, PSize, FALSE, PLauncherError);
}

FSP_API NTSTATUS FspLaunchCallLauncherPipeEx(
    WCHAR Command, ULONG Argc, PWSTR *Argv, ULONG *Argl,
    PWSTR Buffer, PULONG PSize,
    BOOLEAN AllowImpersonation,
    PULONG PLauncherError)
{
    PWSTR PipeBuf = 0, P;
    ULONG Length, BytesTransferred;
    NTSTATUS Result;
    ULONG ErrorCode;

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

    Result = FspCallNamedPipeSecurelyEx(L"" FSP_LAUNCH_PIPE_NAME,
        PipeBuf, (ULONG)(P - PipeBuf) * sizeof(WCHAR), PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE,
        &BytesTransferred, NMPWAIT_USE_DEFAULT_WAIT, AllowImpersonation, FSP_LAUNCH_PIPE_OWNER);
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
    if (!NT_SUCCESS(Result) && 0 != PSize)
        *PSize = 0;

    MemFree(PipeBuf);

    return Result;
}

FSP_API NTSTATUS FspLaunchStart(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv,
    BOOLEAN HasSecret,
    PULONG PLauncherError)
{
    return FspLaunchStartEx(ClassName, InstanceName, Argc, Argv, HasSecret, FALSE, PLauncherError);
}

FSP_API NTSTATUS FspLaunchStartEx(
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv0,
    BOOLEAN HasSecret,
    BOOLEAN AllowImpersonation,
    PULONG PLauncherError)
{
    PWSTR Argv[9 + 2];

    if (9 < Argc)
        return STATUS_INVALID_PARAMETER;

    Argv[0] = ClassName;
    Argv[1] = InstanceName;
    memcpy(Argv + 2, Argv0, Argc * sizeof(PWSTR));

    return FspLaunchCallLauncherPipeEx(
        HasSecret ? FspLaunchCmdStartWithSecret : FspLaunchCmdStart,
        Argc + 2, Argv, 0, 0, 0, AllowImpersonation, PLauncherError);
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

FSP_API NTSTATUS FspLaunchRegSetRecord(
    PWSTR ClassName,
    const FSP_LAUNCH_REG_RECORD *Record)
{
#define SETFIELD(FieldName)             \
    do                                  \
    {                                   \
        if (0 != Record->FieldName)     \
        {                               \
            RegResult = RegSetValueExW(RegKey,\
                L"" #FieldName, 0, REG_SZ,\
                (PVOID)Record->FieldName, (lstrlenW(Record->FieldName) + 1) * sizeof(WCHAR));\
            if (ERROR_SUCCESS != RegResult)\
            {                           \
                Result = FspNtStatusFromWin32(RegResult);\
                goto exit;              \
            }                           \
        }                               \
        else                            \
        {                               \
            RegResult = RegDeleteValueW(RegKey,\
                L"" #FieldName);\
            if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)\
            {                           \
                Result = FspNtStatusFromWin32(RegResult);\
                goto exit;              \
            }                           \
        }                               \
    } while (0,0)
#define SETFIELDI(FieldName, Deflt)     \
    do                                  \
    {                                   \
        if (Deflt != Record->FieldName) \
        {                               \
            RegResult = RegSetValueExW(RegKey,\
                L"" #FieldName, 0, REG_DWORD,\
                (PVOID)&Record->FieldName, sizeof Record->FieldName);\
            if (ERROR_SUCCESS != RegResult)\
            {                           \
                Result = FspNtStatusFromWin32(RegResult);\
                goto exit;              \
            }                           \
        }                               \
        else                            \
        {                               \
            RegResult = RegDeleteValueW(RegKey,\
                L"" #FieldName);\
            if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)\
            {                           \
                Result = FspNtStatusFromWin32(RegResult);\
                goto exit;              \
            }                           \
        }                               \
    } while (0,0)

    NTSTATUS Result;
    ULONG ClassNameLen;
    WCHAR RegPath[MAX_PATH];
    HKEY RegKey = 0;
    DWORD RegResult;

    if (0 != Record && 0 == Record->Executable)
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    ClassNameLen = lstrlenW(ClassName);
    if (sizeof RegPath - sizeof L"" FSP_LAUNCH_REGKEY <= (ClassNameLen + 1) * sizeof(WCHAR))
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    memcpy(RegPath, L"" FSP_LAUNCH_REGKEY, sizeof L"" FSP_LAUNCH_REGKEY - sizeof(WCHAR));
    RegPath[sizeof L"" FSP_LAUNCH_REGKEY / sizeof(WCHAR) - 1] = L'\\';
    memcpy(RegPath + sizeof L"" FSP_LAUNCH_REGKEY / sizeof(WCHAR),
        ClassName, (ClassNameLen + 1) * sizeof(WCHAR));

    if (0 != Record)
    {
        RegResult = RegCreateKeyExW(HKEY_LOCAL_MACHINE, RegPath,
            0, 0, 0, FSP_LAUNCH_REGKEY_WOW64 | KEY_SET_VALUE, 0, &RegKey, 0);
        if (ERROR_SUCCESS != RegResult)
        {
            Result = FspNtStatusFromWin32(RegResult);
            goto exit;
        }

        SETFIELD(Agent);
        SETFIELD(Executable);
        SETFIELD(CommandLine);
        SETFIELD(WorkDirectory);
        SETFIELD(RunAs);
        SETFIELD(Security);
        SETFIELD(AuthPackage);
        SETFIELD(Stderr);
        SETFIELDI(JobControl, ~0); /* JobControl default is 1; but we treat as without default */
        SETFIELDI(Credentials, 0);
        SETFIELDI(AuthPackageId, 0);
        SETFIELDI(Recovery, 0);
    }
    else
    {
        RegResult = RegDeleteKeyEx(HKEY_LOCAL_MACHINE, RegPath,
            FSP_LAUNCH_REGKEY_WOW64, 0);
        if (ERROR_SUCCESS != RegResult && ERROR_FILE_NOT_FOUND != RegResult)
        {
            Result = FspNtStatusFromWin32(RegResult);
            goto exit;
        }
    }

    Result = STATUS_SUCCESS;

exit:
    if (0 != RegKey)
        RegCloseKey(RegKey);

    return Result;

#undef SETFIELD
#undef SETFIELDI
}

FSP_API NTSTATUS FspLaunchRegGetRecord(
    PWSTR ClassName, PWSTR Agent,
    FSP_LAUNCH_REG_RECORD **PRecord)
{
#define GETFIELD(FieldName)             \
    do                                  \
    {                                   \
        RegSize = sizeof RegBuf - RegMark;\
        RegResult = RegQueryValueEx(RegKey,\
            L"" #FieldName, 0, &RegType,\
            (PVOID)(RegBuf + RegMark), &RegSize);\
        if (ERROR_SUCCESS != RegResult) \
        {                               \
            if (ERROR_FILE_NOT_FOUND != RegResult)\
            {                           \
                Result = FspNtStatusFromWin32(RegResult);\
                goto exit;              \
            }                           \
        }                               \
        else if (REG_SZ != RegType ||   \
            sizeof(WCHAR) > RegSize ||  \
            L'\0' != *(PWSTR)(RegBuf + RegMark + RegSize - sizeof(WCHAR)))\
        {                               \
            Result = STATUS_OBJECT_NAME_NOT_FOUND;\
            goto exit;                  \
        }                               \
        else                            \
        {                               \
            Record->FieldName = (PWSTR)(RegBuf + RegMark);\
            RegMark += RegSize;         \
        }                               \
    } while (0,0)
#define GETFIELDI(FieldName)            \
    do                                  \
    {                                   \
        RegSize = sizeof RegDword;      \
        RegResult = RegQueryValueEx(RegKey,\
            L"" #FieldName, 0, &RegType,\
            (PVOID)&RegDword, &RegSize);\
        if (ERROR_SUCCESS != RegResult) \
        {                               \
            if (ERROR_FILE_NOT_FOUND != RegResult)\
            {                           \
                Result = FspNtStatusFromWin32(RegResult);\
                goto exit;              \
            }                           \
        }                               \
        else if (REG_DWORD != RegType)  \
        {                               \
            Result = STATUS_OBJECT_NAME_NOT_FOUND;\
            goto exit;                  \
        }                               \
        else                            \
            Record->FieldName = RegDword;\
    } while (0,0)

    NTSTATUS Result;
    ULONG ClassNameLen;
    WCHAR RegPath[MAX_PATH];
    FSP_LAUNCH_REG_RECORD RecordBuf, *Record = &RecordBuf;
    HKEY RegKey = 0;
    DWORD RegResult, RegDword, RegType, RegSize, RegMark;
    UINT8 RegBuf[2 * 1024];
    PWSTR P, Part;
    BOOLEAN FoundAgent;

    *PRecord = 0;

    ClassNameLen = lstrlenW(ClassName);
    if (sizeof RegPath - sizeof L"" FSP_LAUNCH_REGKEY <= (ClassNameLen + 1) * sizeof(WCHAR))
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    memcpy(RegPath, L"" FSP_LAUNCH_REGKEY, sizeof L"" FSP_LAUNCH_REGKEY - sizeof(WCHAR));
    RegPath[sizeof L"" FSP_LAUNCH_REGKEY / sizeof(WCHAR) - 1] = L'\\';
    memcpy(RegPath + sizeof L"" FSP_LAUNCH_REGKEY / sizeof(WCHAR),
        ClassName, (ClassNameLen + 1) * sizeof(WCHAR));

    RegResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, RegPath,
        0, FSP_LAUNCH_REGKEY_WOW64 | KEY_QUERY_VALUE, &RegKey);
    if (ERROR_SUCCESS != RegResult)
    {
        Result = FspNtStatusFromWin32(RegResult);
        goto exit;
    }

    memset(Record, 0, sizeof *Record);
    Record->JobControl = 1; /* default is YES! */
    RegMark = 0;

    GETFIELD(Agent);
    if (0 != Agent && L'\0' != Agent[0] &&
        0 != Record->Agent && L'\0' != Record->Agent[0])
    {
        FoundAgent = FALSE;
        P = Record->Agent, Part = P;
        do
        {
            if (L',' == *P || '\0' == *P)
            {
                if (0 == invariant_wcsnicmp(Part, Agent, P - Part))
                {
                    FoundAgent = TRUE;
                    break;
                }
                else
                    Part = P + 1;
            }
        } while (L'\0' != *P++);

        if (!FoundAgent)
        {
            Result = STATUS_OBJECT_NAME_NOT_FOUND;
            goto exit;
        }
    }

    GETFIELD(Executable);
    GETFIELD(CommandLine);
    GETFIELD(WorkDirectory);
    GETFIELD(RunAs);
    GETFIELD(Security);
    GETFIELD(AuthPackage);
    GETFIELD(Stderr);
    GETFIELDI(JobControl);
    GETFIELDI(Credentials);
    GETFIELDI(AuthPackageId);
    GETFIELDI(Recovery);

    if (0 == Record->Executable)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        goto exit;
    }

    Record = MemAlloc(FIELD_OFFSET(FSP_LAUNCH_REG_RECORD, Buffer) + RegMark);
    if (0 == Record)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    memset(Record, 0, sizeof *Record);
    memcpy(Record->Buffer, RegBuf, RegMark);
    Record->Agent = 0 != RecordBuf.Agent ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.Agent - RegBuf)) : 0;
    Record->Executable = 0 != RecordBuf.Executable ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.Executable - RegBuf)) : 0;
    Record->CommandLine = 0 != RecordBuf.CommandLine ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.CommandLine - RegBuf)) : 0;
    Record->WorkDirectory = 0 != RecordBuf.WorkDirectory ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.WorkDirectory - RegBuf)) : 0;
    Record->RunAs = 0 != RecordBuf.RunAs ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.RunAs - RegBuf)) : 0;
    Record->Security = 0 != RecordBuf.Security ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.Security - RegBuf)) : 0;
    Record->AuthPackage = 0 != RecordBuf.AuthPackage ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.AuthPackage - RegBuf)) : 0;
    Record->Stderr = 0 != RecordBuf.Stderr ?
        (PVOID)(Record->Buffer + ((PUINT8)RecordBuf.Stderr - RegBuf)) : 0;
    Record->JobControl = RecordBuf.JobControl;
    Record->Credentials = RecordBuf.Credentials;
    Record->AuthPackageId = RecordBuf.AuthPackageId;
    Record->Recovery = RecordBuf.Recovery;

    *PRecord = Record;
    Result = STATUS_SUCCESS;

exit:
    if (0 != RegKey)
        RegCloseKey(RegKey);

    return Result;

#undef GETFIELDI
#undef GETFIELD
}

FSP_API VOID FspLaunchRegFreeRecord(
    FSP_LAUNCH_REG_RECORD *Record)
{
    MemFree(Record);
}
