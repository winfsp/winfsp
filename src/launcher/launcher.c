/**
 * @file launcher/launcher.c
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

#include <winfsp/launch.h>
#include <shared/um/minimal.h>
#include <aclapi.h>
#include <sddl.h>
#include <userenv.h>

#define PROGNAME                        FSP_FSCTL_PRODUCT_NAME ".Launcher"

static NTSTATUS (NTAPI *SvcNtOpenSymbolicLinkObject)(
    PHANDLE LinkHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes);
static NTSTATUS (NTAPI *SvcNtClose)(
    HANDLE Handle);

static BOOL CreateOverlappedPipe(
    PHANDLE PReadPipe, PHANDLE PWritePipe,
    DWORD Size,
    BOOL ReadInherit, BOOL WriteInherit,
    DWORD ReadMode, DWORD WriteMode)
{
    RPC_STATUS RpcStatus;
    UUID Uuid;
    WCHAR PipeNameBuf[MAX_PATH];
    SECURITY_ATTRIBUTES ReadSecurityAttributes = { sizeof(SECURITY_ATTRIBUTES), 0, ReadInherit };
    SECURITY_ATTRIBUTES WriteSecurityAttributes = { sizeof(SECURITY_ATTRIBUTES), 0, WriteInherit };
    HANDLE ReadPipe, WritePipe;
    DWORD LastError;

    RpcStatus = UuidCreate(&Uuid);
    if (S_OK != RpcStatus && RPC_S_UUID_LOCAL_ONLY != RpcStatus)
    {
        SetLastError(ERROR_INTERNAL_ERROR);
        return FALSE;
    }

    wsprintfW(PipeNameBuf, L"\\\\.\\pipe\\"
        "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        Uuid.Data1, Uuid.Data2, Uuid.Data3,
        Uuid.Data4[0], Uuid.Data4[1], Uuid.Data4[2], Uuid.Data4[3],
        Uuid.Data4[4], Uuid.Data4[5], Uuid.Data4[6], Uuid.Data4[7]);

    ReadPipe = CreateNamedPipeW(PipeNameBuf,
        PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | ReadMode,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, Size, Size, 120 * 1000, &ReadSecurityAttributes);
    if (INVALID_HANDLE_VALUE == ReadPipe)
        return FALSE;

    WritePipe = CreateFileW(PipeNameBuf,
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &WriteSecurityAttributes, OPEN_EXISTING, WriteMode, 0);
    if (INVALID_HANDLE_VALUE == WritePipe)
    {
        LastError = GetLastError();
        CloseHandle(ReadPipe);
        SetLastError(LastError);
        return FALSE;
    }

    *PReadPipe = ReadPipe;
    *PWritePipe = WritePipe;

    return TRUE;
}

static NTSTATUS GetTokenUserName(HANDLE Token, PWSTR *PUserName)
{
    union
    {
        TOKEN_USER V;
        UINT8 B[128];
    } UserInfoBuf;
    PTOKEN_USER UserInfo = &UserInfoBuf.V;
    WCHAR Name[256], Domn[256];
    DWORD UserSize, NameSize, DomnSize;
    SID_NAME_USE Use;
    PWSTR P;
    NTSTATUS Result;

    *PUserName = 0;

    if (!GetTokenInformation(Token, TokenUser, UserInfo, sizeof UserInfoBuf, &UserSize))
    {
        if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        UserInfo = MemAlloc(UserSize);
        if (0 == UserInfo)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        if (!GetTokenInformation(Token, TokenUser, UserInfo, UserSize, &UserSize))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    NameSize = sizeof Name / sizeof Name[0];
    DomnSize = sizeof Domn / sizeof Domn[0];
    if (!LookupAccountSidW(0, UserInfo->User.Sid, Name, &NameSize, Domn, &DomnSize, &Use))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    NameSize = lstrlenW(Name);
    DomnSize = lstrlenW(Domn);

    P = *PUserName = MemAlloc((DomnSize + 1 + NameSize + 1) * sizeof(WCHAR));
    if (0 == P)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (0 < DomnSize)
    {
        memcpy(P, Domn, DomnSize * sizeof(WCHAR));
        P[DomnSize] = L'\\';
        P += DomnSize + 1;
    }
    memcpy(P, Name, NameSize * sizeof(WCHAR));
    P[NameSize] = L'\0';

    Result = STATUS_SUCCESS;

exit:
    if (UserInfo != &UserInfoBuf.V)
        MemFree(UserInfo);

    return Result;
}

static NTSTATUS AddAccessForTokenUser(HANDLE Handle, DWORD Access, HANDLE Token)
{
    union
    {
        TOKEN_USER V;
        UINT8 B[128];
    } UserInfoBuf;
    PTOKEN_USER UserInfo = &UserInfoBuf.V;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    PSECURITY_DESCRIPTOR NewSecurityDescriptor = 0;
    EXPLICIT_ACCESSW AccessEntry;
    DWORD Size, LastError;
    NTSTATUS Result;

    if (!GetTokenInformation(Token, TokenUser, UserInfo, sizeof UserInfoBuf, &Size))
    {
        if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        UserInfo = MemAlloc(Size);
        if (0 == UserInfo)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        if (!GetTokenInformation(Token, TokenUser, UserInfo, Size, &Size))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    if (GetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION, 0, 0, &Size))
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }
    if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    SecurityDescriptor = MemAlloc(Size);
    if (0 == SecurityDescriptor)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!GetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION, SecurityDescriptor, Size, &Size))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    AccessEntry.grfAccessPermissions = Access;
    AccessEntry.grfAccessMode = GRANT_ACCESS;
    AccessEntry.grfInheritance = NO_INHERITANCE;
    AccessEntry.Trustee.pMultipleTrustee = 0;
    AccessEntry.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    AccessEntry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    AccessEntry.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    AccessEntry.Trustee.ptstrName = UserInfo->User.Sid;

    LastError = BuildSecurityDescriptorW(0, 0, 1, &AccessEntry, 0, 0, SecurityDescriptor,
        &Size, &NewSecurityDescriptor);
    if (0 != LastError)
    {
        Result = FspNtStatusFromWin32(LastError);
        goto exit;
    }

    if (!SetKernelObjectSecurity(Handle, DACL_SECURITY_INFORMATION, NewSecurityDescriptor))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    LocalFree(NewSecurityDescriptor);
    MemFree(SecurityDescriptor);
    if (UserInfo != &UserInfoBuf.V)
        MemFree(UserInfo);

    return Result;
}

static BOOL LogonCreateProcess(
    PWSTR UserName,
    HANDLE Token,
    LPCWSTR ApplicationName,
    LPWSTR CommandLine,
    LPSECURITY_ATTRIBUTES ProcessAttributes,
    LPSECURITY_ATTRIBUTES ThreadAttributes,
    BOOL InheritHandles,
    DWORD CreationFlags,
    LPVOID Environment,
    LPCWSTR CurrentDirectory,
    LPSTARTUPINFOW StartupInfo,
    LPPROCESS_INFORMATION ProcessInformation)
{
    PWSTR DomainName = 0;

    if (0 != UserName)
    {
        if (0 == invariant_wcsicmp(UserName, L"LocalSystem"))
        {
            UserName = 0;
            Token = 0;
        }
        else
        if (0 == invariant_wcsicmp(UserName, L"LocalService") ||
            0 == invariant_wcsicmp(UserName, L"NetworkService"))
        {
            DomainName = L"NT AUTHORITY";
            Token = 0;
        }
        else
        if (0 == invariant_wcsicmp(UserName, L"."))
            ;
        else
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
    }

    if (0 == UserName)
        /* without a user name go ahead and call CreateProcessW */
        return CreateProcessW(
            ApplicationName,
            CommandLine,
            ProcessAttributes,
            ThreadAttributes,
            InheritHandles,
            CreationFlags,
            Environment,
            CurrentDirectory,
            StartupInfo,
            ProcessInformation);

    HANDLE LogonToken = 0;
    PVOID EnvironmentBlock = 0;
    DWORD SessionId;
    DWORD LastError;
    BOOL Success;

    if (0 == Token)
    {
        Success = LogonUserW(
            UserName,
            DomainName,
            0,
            LOGON32_LOGON_SERVICE,
            LOGON32_PROVIDER_DEFAULT,
            &LogonToken);
        if (!Success)
            goto exit;
    }
    else
    {
        /* convert the impersonation token to a primary token */
        Success = DuplicateTokenEx(Token,
            TOKEN_ALL_ACCESS,
            0,
            SecurityAnonymous,
            TokenPrimary,
            &LogonToken);
        if (!Success)
            goto exit;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &SessionId))
            SessionId = 0;
        /* place the duplicated token in the service session (session 0) */
        Success = SetTokenInformation(LogonToken, TokenSessionId, &SessionId, sizeof SessionId);
        if (!Success)
            goto exit;
    }

    if (0 == Environment)
    {
        Success = CreateEnvironmentBlock(&EnvironmentBlock, LogonToken, FALSE);
        if (!Success)
            goto exit;

        CreationFlags |= CREATE_UNICODE_ENVIRONMENT;
        Environment = EnvironmentBlock;
    }

    Success = ImpersonateLoggedOnUser(LogonToken);
    if (!Success)
        goto exit;

    Success = CreateProcessAsUserW(
        LogonToken,
        ApplicationName,
        CommandLine,
        ProcessAttributes,
        ThreadAttributes,
        InheritHandles,
        CreationFlags,
        Environment,
        CurrentDirectory,
        StartupInfo,
        ProcessInformation);

    if (!RevertToSelf())
        /* should not happen! */
        ExitProcess(GetLastError());

exit:
    if (!Success)
        LastError = GetLastError();

    if (0 != EnvironmentBlock)
        DestroyEnvironmentBlock(EnvironmentBlock);
    if (0 != LogonToken)
        CloseHandle(LogonToken);

    if (!Success)
        SetLastError(LastError);

    return Success;
}

typedef struct
{
    HANDLE Process;
    HANDLE ProcessWait;
} KILL_PROCESS_DATA;

static VOID CALLBACK KillProcessWait(PVOID Context, BOOLEAN Timeout);

static VOID KillProcess(ULONG ProcessId, HANDLE Process, ULONG Timeout)
{
    if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, ProcessId))
    {
        /*
         * If GenerateConsoleCtrlEvent succeeds, but the child process does not exit
         * timely we will terminate it with extreme prejudice. This is done by calling
         * RegisterWaitForSingleObject with timeout on a duplicated process handle.
         *
         * If GenerateConsoleCtrlEvent succeeds, but we are not able to successfully call
         * RegisterWaitForSingleObject, we do NOT terminate the child process forcibly.
         * This is by design as it is not the child process's fault and the child process
         * should (we hope in this case) respond to the console control event timely.
         */

        KILL_PROCESS_DATA *KillProcessData;

        KillProcessData = MemAlloc(sizeof *KillProcessData);
        if (0 != KillProcessData)
        {
            if (DuplicateHandle(GetCurrentProcess(), Process, GetCurrentProcess(), &KillProcessData->Process,
                0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                if (RegisterWaitForSingleObject(&KillProcessData->ProcessWait, KillProcessData->Process,
                    KillProcessWait, KillProcessData, Timeout, WT_EXECUTEONLYONCE))
                    KillProcessData = 0;
                else
                    CloseHandle(KillProcessData->Process);
            }

            MemFree(KillProcessData);
        }
    }
    else
        TerminateProcess(Process, 0);
}

static VOID CALLBACK KillProcessWait(PVOID Context, BOOLEAN Timeout)
{
    KILL_PROCESS_DATA *KillProcessData = Context;

    if (Timeout)
        TerminateProcess(KillProcessData->Process, 0);

    UnregisterWaitEx(KillProcessData->ProcessWait, 0);
    CloseHandle(KillProcessData->Process);
    MemFree(KillProcessData);
}

#define LAUNCHER_PIPE_DEFAULT_TIMEOUT   (2 * 15000 + 1000)
#define LAUNCHER_START_WITH_SECRET_TIMEOUT 15000
#define LAUNCHER_STOP_TIMEOUT           5500
#define LAUNCHER_KILL_TIMEOUT           5000

typedef struct
{
    LONG RefCount;
    LIST_ENTRY ListEntry;
    HANDLE ClientToken;
    PWSTR ClassName;
    PWSTR InstanceName;
    PWSTR CommandLine;
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    DWORD ProcessId;
    HANDLE Process;
    HANDLE ProcessWait;
    HANDLE StdioHandles[3];
    DWORD Recovery;
    ULONG Argc;
    PWSTR *Argv;
    BOOLEAN HasSecret;
    BOOLEAN Started, Stopped;
    WCHAR Buffer[];
} SVC_INSTANCE;

static HANDLE SvcJob;
static CRITICAL_SECTION SvcInstanceLock;
static HANDLE SvcInstanceEvent;
static LIST_ENTRY SvcInstanceList = { &SvcInstanceList, &SvcInstanceList };

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Timeout);
NTSTATUS SvcInstanceStart(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv, HANDLE Job,
    BOOLEAN HasSecret);

static SVC_INSTANCE *SvcInstanceLookup(PWSTR ClassName, PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        if (0 == invariant_wcsicmp(ClassName, SvcInstance->ClassName) &&
            0 == invariant_wcsicmp(InstanceName, SvcInstance->InstanceName))
            return SvcInstance;
    }

    return 0;
}

static inline ULONG SvcInstanceArgumentLength(PWSTR Arg, PWSTR Pattern, BOOLEAN Quote)
{
    PWSTR PathTransform(PWSTR Dest, PWSTR Arg, PWSTR Pattern);

    return (Quote ? 2 : 0) + (ULONG)((UINT_PTR)PathTransform(0, Arg, Pattern) / sizeof(WCHAR));
}

static inline PWSTR SvcInstanceArgumentCopy(PWSTR Dest, PWSTR Arg, PWSTR Pattern, BOOLEAN Quote)
{
    PWSTR PathTransform(PWSTR Dest, PWSTR Arg, PWSTR Pattern);

    if (Quote)
        *Dest++ = L'"';
    Dest = PathTransform(Dest, Arg, Pattern);
    if (Quote)
        *Dest++ = L'"';

    return Dest;
}

static NTSTATUS SvcInstanceReplaceArguments(PWSTR String,
    ULONG Argc, PWSTR *Argv, PWSTR *Varv, BOOLEAN Quote,
    PWSTR *PNewString)
{
    PWSTR NewString = 0, P, Q;
    PWSTR EmptyArg = L"";
    ULONG Length;
    PWSTR Pattern;

    *PNewString = 0;

    Length = 0;
    for (P = String; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            Pattern = 0;
            P++;
            if (L'\\' == *P)
            {
                Pattern = ++P;
                while (!(L'\0' == *P ||
                    (L'0' <= *P && *P <= L'9') ||
                    (L'A' <= *P && *P <= L'Z')))
                    P++;
            }
            if (L'0' <= *P && *P <= L'9')
            {
                if (Argc > (ULONG)(*P - L'0'))
                    Length += SvcInstanceArgumentLength(Argv[*P - L'0'], Pattern, Quote);
                else
                    Length += SvcInstanceArgumentLength(EmptyArg, 0, Quote);
            }
            else
            if (L'A' <= *P && *P <= L'Z')
            {
                if (0 != Varv[*P - L'A'])
                    Length += SvcInstanceArgumentLength(Varv[*P - L'A'], Pattern, Quote);
                else
                    Length += SvcInstanceArgumentLength(EmptyArg, 0, Quote);
            }
            else
            if (*P)
                Length++;
            else
                P--;
            break;
        default:
            Length++;
            break;
        }
    }

    NewString = MemAlloc((Length + 1) * sizeof(WCHAR));
    if (0 == NewString)
        return STATUS_INSUFFICIENT_RESOURCES;

    for (P = String, Q = NewString; *P; P++)
    {
        switch (*P)
        {
        case L'%':
            Pattern = 0;
            P++;
            if (L'\\' == *P)
            {
                Pattern = ++P;
                while (!(L'\0' == *P ||
                    (L'0' <= *P && *P <= L'9') ||
                    (L'A' <= *P && *P <= L'Z')))
                    P++;
            }
            if (L'0' <= *P && *P <= L'9')
            {
                if (Argc > (ULONG)(*P - L'0'))
                    Q = SvcInstanceArgumentCopy(Q, Argv[*P - L'0'], Pattern, Quote);
                else
                    Q = SvcInstanceArgumentCopy(Q, EmptyArg, 0, Quote);
            }
            else
            if (L'A' <= *P && *P <= L'Z')
            {
                if (0 != Varv[*P - L'A'])
                    Q = SvcInstanceArgumentCopy(Q, Varv[*P - L'A'], Pattern, Quote);
                else
                    Q = SvcInstanceArgumentCopy(Q, EmptyArg, 0, Quote);
            }
            else
            if (*P)
                *Q++ = *P;
            else
                P--;
            break;
        default:
            *Q++ = *P;
            break;
        }
    }
    *Q = L'\0';

    *PNewString = NewString;

    return STATUS_SUCCESS;
}

static NTSTATUS SvcInstanceAddUserRights(HANDLE Token,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PSECURITY_DESCRIPTOR *PNewSecurityDescriptor)
{
    PSECURITY_DESCRIPTOR NewSecurityDescriptor;
    TOKEN_USER *User = 0;
    EXPLICIT_ACCESSW AccessEntry;
    DWORD Size, LastError;
    NTSTATUS Result;

    *PNewSecurityDescriptor = 0;

    if (GetTokenInformation(Token, TokenUser, 0, 0, &Size))
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }
    if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    User = MemAlloc(Size);
    if (0 == User)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (!GetTokenInformation(Token, TokenUser, User, Size, &Size))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    AccessEntry.grfAccessPermissions = SERVICE_QUERY_STATUS | SERVICE_STOP;
    AccessEntry.grfAccessMode = GRANT_ACCESS;
    AccessEntry.grfInheritance = NO_INHERITANCE;
    AccessEntry.Trustee.pMultipleTrustee = 0;
    AccessEntry.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    AccessEntry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    AccessEntry.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    AccessEntry.Trustee.ptstrName = User->User.Sid;

    LastError = BuildSecurityDescriptorW(0, 0, 1, &AccessEntry, 0, 0, SecurityDescriptor,
        &Size, &NewSecurityDescriptor);
    if (0 != LastError)
    {
        Result = FspNtStatusFromWin32(LastError);
        goto exit;
    }

    *PNewSecurityDescriptor = NewSecurityDescriptor;
    Result = STATUS_SUCCESS;

exit:
    MemFree(User);

    return Result;
}

static NTSTATUS SvcInstanceAccessCheck(HANDLE ClientToken, ULONG DesiredAccess,
    PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    static GENERIC_MAPPING GenericMapping =
    {
        .GenericRead = STANDARD_RIGHTS_READ | SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS |
            SERVICE_INTERROGATE | SERVICE_ENUMERATE_DEPENDENTS,
        .GenericWrite = STANDARD_RIGHTS_WRITE | SERVICE_CHANGE_CONFIG,
        .GenericExecute = STANDARD_RIGHTS_EXECUTE | SERVICE_START | SERVICE_STOP |
            SERVICE_PAUSE_CONTINUE | SERVICE_USER_DEFINED_CONTROL,
        .GenericAll = SERVICE_ALL_ACCESS,
    };
    UINT8 PrivilegeSetBuf[sizeof(PRIVILEGE_SET) + 15 * sizeof(LUID_AND_ATTRIBUTES)];
    PPRIVILEGE_SET PrivilegeSet = (PVOID)PrivilegeSetBuf;
    DWORD PrivilegeSetLength = sizeof PrivilegeSetBuf;
    ULONG GrantedAccess;
    BOOL AccessStatus;
    NTSTATUS Result;

    if (AccessCheck(SecurityDescriptor, ClientToken, DesiredAccess,
        &GenericMapping, PrivilegeSet, &PrivilegeSetLength, &GrantedAccess, &AccessStatus))
        Result = AccessStatus ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
    else
        Result = FspNtStatusFromWin32(GetLastError());

    return Result;
}

static NTSTATUS SvcInstanceCreateProcess(PWSTR UserName, HANDLE ClientToken,
    PWSTR Executable, PWSTR CommandLine, PWSTR WorkDirectory,
    HANDLE StdioHandles[2], HANDLE StderrHandle,
    PPROCESS_INFORMATION ProcessInfo)
{
    WCHAR WorkDirectoryBuf[MAX_PATH];
    STARTUPINFOEXW StartupInfoEx;
    HANDLE ChildHandles[3] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE/* NO CLOSE!*/ };
    HANDLE ParentHandles[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
    PPROC_THREAD_ATTRIBUTE_LIST AttrList = 0;
    BOOLEAN InitDoneAttrList = FALSE;
    SIZE_T Size;
    NTSTATUS Result;

    if (0 != WorkDirectory && L'.' == WorkDirectory[0] && L'\0' == WorkDirectory[1])
    {
        PWSTR Backslash = 0, P;

        if (0 == GetModuleFileNameW(0, WorkDirectoryBuf, MAX_PATH))
            return FspNtStatusFromWin32(GetLastError());

        for (P = WorkDirectoryBuf; *P; P++)
            if (L'\\' == *P)
                Backslash = P;
        if (0 != Backslash && WorkDirectoryBuf < Backslash && L':' != Backslash[-1])
            *Backslash = L'\0';

        WorkDirectory = WorkDirectoryBuf;
    }

    memset(&StartupInfoEx, 0, sizeof StartupInfoEx);
    StartupInfoEx.StartupInfo.cb = sizeof StartupInfoEx.StartupInfo;

    if (0 != StdioHandles || INVALID_HANDLE_VALUE != StderrHandle)
    {
        /*
         * Create child process and redirect stdin/stdout. Do *not* inherit other handles.
         *
         * For explanation see:
         *     https://blogs.msdn.microsoft.com/oldnewthing/20111216-00/?p=8873/
         */

        if (0 != StdioHandles)
        {
            /* create stdin read/write ends; make them inheritable */
            if (!CreateOverlappedPipe(&ChildHandles[0], &ParentHandles[0],
                0, TRUE, FALSE, 0, 0))
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }
        }

        if (0 != StdioHandles)
        {
            /* create stdout read/write ends; make them inheritable */
            if (!CreateOverlappedPipe(&ParentHandles[1], &ChildHandles[1],
                0, FALSE, TRUE, FILE_FLAG_OVERLAPPED, 0))
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }
        }

        if (INVALID_HANDLE_VALUE != StderrHandle)
            ChildHandles[2] = StderrHandle;

        Size = 0;
        if (!InitializeProcThreadAttributeList(0, 1, 0, &Size) &&
            ERROR_INSUFFICIENT_BUFFER != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        AttrList = MemAlloc(Size);
        if (0 == AttrList)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        if (!InitializeProcThreadAttributeList(AttrList, 1, 0, &Size))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
        InitDoneAttrList = TRUE;

        /* only the child ends of stdin/stdout/stderr are actually inherited */
        if (!UpdateProcThreadAttribute(AttrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            0 != StdioHandles ? ChildHandles : ChildHandles + 2,
            ((0 != StdioHandles ? 2 : 0) + (INVALID_HANDLE_VALUE != StderrHandle ? 1 : 0)) *
                sizeof ChildHandles[0],
            0, 0))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        StartupInfoEx.StartupInfo.cb = sizeof StartupInfoEx;
        StartupInfoEx.lpAttributeList = AttrList;
        StartupInfoEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        StartupInfoEx.StartupInfo.hStdInput = ChildHandles[0];
        StartupInfoEx.StartupInfo.hStdOutput = ChildHandles[1];
        StartupInfoEx.StartupInfo.hStdError = ChildHandles[2];

        if (!LogonCreateProcess(UserName, ClientToken,
            Executable, CommandLine, 0, 0, TRUE,
            CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP | EXTENDED_STARTUPINFO_PRESENT,
            0, WorkDirectory,
            &StartupInfoEx.StartupInfo, ProcessInfo))
        {
            if (ERROR_NO_SYSTEM_RESOURCES != GetLastError())
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }

            /*
             * On Win7 CreateProcessW with EXTENDED_STARTUPINFO_PRESENT
             * may fail with ERROR_NO_SYSTEM_RESOURCES.
             *
             * In that case go ahead and retry with a CreateProcessW with
             * bInheritHandles==TRUE, but without EXTENDED_STARTUPINFO_PRESENT.
             * Not ideal, but...
             */
            StartupInfoEx.StartupInfo.cb = sizeof StartupInfoEx.StartupInfo;
            if (!LogonCreateProcess(UserName, ClientToken,
                Executable, CommandLine, 0, 0, TRUE,
                CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP,
                0, WorkDirectory,
                &StartupInfoEx.StartupInfo, ProcessInfo))
            {
                Result = FspNtStatusFromWin32(GetLastError());
                goto exit;
            }
        }
    }
    else
    {
        if (!LogonCreateProcess(UserName, ClientToken,
            Executable, CommandLine, 0, 0, FALSE,
            CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP,
            0, WorkDirectory,
            &StartupInfoEx.StartupInfo, ProcessInfo))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (INVALID_HANDLE_VALUE != ParentHandles[0])
            CloseHandle(ParentHandles[0]);
        if (INVALID_HANDLE_VALUE != ParentHandles[1])
            CloseHandle(ParentHandles[1]);
    }
    else if (0 != StdioHandles)
    {
        StdioHandles[0] = ParentHandles[0];
        StdioHandles[1] = ParentHandles[1];
    }

    if (INVALID_HANDLE_VALUE != ChildHandles[0])
        CloseHandle(ChildHandles[0]);
    if (INVALID_HANDLE_VALUE != ChildHandles[1])
        CloseHandle(ChildHandles[1]);

    if (InitDoneAttrList)
        DeleteProcThreadAttributeList(AttrList);
    MemFree(AttrList);

    return Result;
}

NTSTATUS SvcInstanceCreate(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv0, HANDLE Job,
    BOOLEAN RedirectStdio,
    SVC_INSTANCE **PSvcInstance)
{
    SVC_INSTANCE *SvcInstance = 0;
    PWSTR Argv[10];
    PWSTR Varv[26];
    SYSTEMTIME SystemTime;
    PWSTR ClientUserName = 0, StderrFileName = 0;
    DWORD ClientTokenInformation = -1;
    SECURITY_ATTRIBUTES StderrSecurityAttributes = { sizeof(SECURITY_ATTRIBUTES), 0, TRUE };
    FSP_LAUNCH_REG_RECORD *Record = 0;
    WCHAR CurrentTime[32], UserProfileDir[MAX_PATH], CommandLine[512], Security[512];
    DWORD Length, ClassNameSize, InstanceNameSize;
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0, NewSecurityDescriptor;
    PROCESS_INFORMATION ProcessInfo;
    NTSTATUS Result;

    *PSvcInstance = 0;

    if (Argc > sizeof Argv / sizeof Argv[0] - 1)
        Argc = sizeof Argv / sizeof Argv[0] - 1;
    memcpy(Argv + 1, Argv0, Argc * sizeof(PWSTR));
    Argv[0] = 0;
    Argc++;

    memset(Varv, 0, sizeof Varv);

    memset(&ProcessInfo, 0, sizeof ProcessInfo);

    EnterCriticalSection(&SvcInstanceLock);

    if (0 != SvcInstanceLookup(ClassName, InstanceName))
    {
        Result = STATUS_OBJECT_NAME_COLLISION;
        goto exit;
    }

    GetSystemTime(&SystemTime);
    wsprintfW(CurrentTime, L"%04hu%02hu%02huT%02hu%02hu%02hu.%03huZ",
        SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
        SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
        SystemTime.wMilliseconds);
    Varv[L'T' - L'A'] = CurrentTime;

    Result = GetTokenUserName(ClientToken, &ClientUserName);
    if (!NT_SUCCESS(Result))
        goto exit;
    Varv[L'U' - L'A'] = ClientUserName;

    Length = MAX_PATH;
    if (!GetUserProfileDirectoryW(ClientToken, UserProfileDir, &Length))
        /* store an invalid filename; any attempt to use it will fail */
        lstrcpyW(UserProfileDir, L":INVALID:");
    Varv[L'P' - L'A'] = UserProfileDir;

    Result = FspLaunchRegGetRecord(ClassName, 0, &Record);
    if (!NT_SUCCESS(Result))
        goto exit;

    if ((!RedirectStdio && 0 != Record->Credentials) ||
        ( RedirectStdio && 0 == Record->Credentials))
    {
        Result = STATUS_DEVICE_CONFIGURATION_ERROR;
        goto exit;
    }

    Argv[0] = Record->Executable;

    lstrcpyW(CommandLine, L"%0 ");
    if (0 != Record->CommandLine)
    {
        Length = lstrlenW(CommandLine);
        lstrcpynW(CommandLine + Length, Record->CommandLine,
            sizeof CommandLine / sizeof(WCHAR) - Length);
        CommandLine[sizeof CommandLine / sizeof(WCHAR) - 1] = L'\0';
    }

    lstrcpyW(Security, L"O:SYG:SY");
    if (0 != Record->Security)
    {
        if (L'D' == Record->Security[0] && L':' == Record->Security[1])
            Length = lstrlenW(Security);
        else
            Length = 0;
        lstrcpynW(Security + Length, Record->Security,
            sizeof Security / sizeof(WCHAR) - Length);
        Security[sizeof Security / sizeof(WCHAR) - 1] = L'\0';
    }
    else
    {
        Length = lstrlenW(Security);
        lstrcpyW(Security + Length, L"" FSP_LAUNCH_SERVICE_DEFAULT_SDDL);
    }

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(Security, SDDL_REVISION_1,
        &SecurityDescriptor, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = SvcInstanceAccessCheck(ClientToken, SERVICE_START, SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = SvcInstanceAddUserRights(ClientToken, SecurityDescriptor, &NewSecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;
    LocalFree(SecurityDescriptor);
    SecurityDescriptor = NewSecurityDescriptor;

    ClassNameSize = (lstrlenW(ClassName) + 1) * sizeof(WCHAR);
    InstanceNameSize = (lstrlenW(InstanceName) + 1) * sizeof(WCHAR);

    SvcInstance = MemAlloc(sizeof *SvcInstance + ClassNameSize + InstanceNameSize);
    if (0 == SvcInstance)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    /* ClientToken: protect from CloseHandle; do not inherit */
    if (!GetHandleInformation(ClientToken, &ClientTokenInformation) ||
        !SetHandleInformation(ClientToken,
            HANDLE_FLAG_PROTECT_FROM_CLOSE | HANDLE_FLAG_INHERIT,
            HANDLE_FLAG_PROTECT_FROM_CLOSE | 0))
    {
        ClientTokenInformation = -1;
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    memset(SvcInstance, 0, sizeof *SvcInstance);
    SvcInstance->RefCount = 2;
    SvcInstance->ClientToken = ClientToken;
    memcpy(SvcInstance->Buffer, ClassName, ClassNameSize);
    memcpy(SvcInstance->Buffer + ClassNameSize / sizeof(WCHAR), InstanceName, InstanceNameSize);
    SvcInstance->ClassName = SvcInstance->Buffer;
    SvcInstance->InstanceName = SvcInstance->Buffer + ClassNameSize / sizeof(WCHAR);
    SvcInstance->SecurityDescriptor = SecurityDescriptor;
    SvcInstance->StdioHandles[0] = INVALID_HANDLE_VALUE;
    SvcInstance->StdioHandles[1] = INVALID_HANDLE_VALUE;
    SvcInstance->StdioHandles[2] = INVALID_HANDLE_VALUE;
    SvcInstance->Recovery = Record->Recovery;

    Result = SvcInstanceReplaceArguments(CommandLine, Argc, Argv, Varv, TRUE,
        &SvcInstance->CommandLine);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (0 != Record->Stderr)
    {
        Result = SvcInstanceReplaceArguments(Record->Stderr, Argc, Argv, Varv, FALSE,
            &StderrFileName);
        if (!NT_SUCCESS(Result))
            goto exit;

        SvcInstance->StdioHandles[2] = CreateFileW(
            StderrFileName,
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &StderrSecurityAttributes,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            0);
        if (INVALID_HANDLE_VALUE == SvcInstance->StdioHandles[2])
            FspServiceLog(EVENTLOG_WARNING_TYPE,
                L"Ignorning error: cannot create stderr file = %ld", GetLastError());
    }

    Result = SvcInstanceCreateProcess(
        Record->RunAs,
        ClientToken,
        Record->Executable,
        SvcInstance->CommandLine,
        Record->WorkDirectory,
        RedirectStdio ? SvcInstance->StdioHandles : 0,
        SvcInstance->StdioHandles[2],
        &ProcessInfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    SvcInstance->ProcessId = ProcessInfo.dwProcessId;
    SvcInstance->Process = ProcessInfo.hProcess;

    if (!RegisterWaitForSingleObject(&SvcInstance->ProcessWait, SvcInstance->Process,
        SvcInstanceTerminated, SvcInstance, INFINITE, WT_EXECUTEONLYONCE))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (0 != Job && Record->JobControl)
    {
        if (!AssignProcessToJobObject(Job, SvcInstance->Process))
            FspServiceLog(EVENTLOG_WARNING_TYPE,
                L"Ignorning error: AssignProcessToJobObject = %ld", GetLastError());
    }

    /*
     * ONCE THE PROCESS IS RESUMED NO MORE FAILURES ALLOWED!
     */

    ResumeThread(ProcessInfo.hThread);
    CloseHandle(ProcessInfo.hThread);
    ProcessInfo.hThread = 0;

    InsertTailList(&SvcInstanceList, &SvcInstance->ListEntry);
    ResetEvent(SvcInstanceEvent);

    *PSvcInstance = SvcInstance;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        LocalFree(SecurityDescriptor);

        if (0 != ProcessInfo.hThread)
            CloseHandle(ProcessInfo.hThread);

        if (-1 != ClientTokenInformation)
            SetHandleInformation(ClientToken,
                HANDLE_FLAG_PROTECT_FROM_CLOSE | HANDLE_FLAG_INHERIT,
                ClientTokenInformation);

        if (0 != SvcInstance)
        {
            if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[0])
                CloseHandle(SvcInstance->StdioHandles[0]);
            if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[1])
                CloseHandle(SvcInstance->StdioHandles[1]);
            if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[2])
                CloseHandle(SvcInstance->StdioHandles[2]);

            if (0 != SvcInstance->ProcessWait)
                UnregisterWaitEx(SvcInstance->ProcessWait, 0);

            if (0 != SvcInstance->Process)
            {
                TerminateProcess(SvcInstance->Process, 0);
                CloseHandle(SvcInstance->Process);
            }

            MemFree(SvcInstance->CommandLine);
            MemFree(SvcInstance);
        }
    }

    if (0 != Record)
        FspLaunchRegFreeRecord(Record);

    MemFree(StderrFileName);
    MemFree(ClientUserName);

    LeaveCriticalSection(&SvcInstanceLock);

    FspServiceLog(EVENTLOG_INFORMATION_TYPE,
        L"create %s\\%s = %lx", ClassName, InstanceName, Result);

    return Result;
}

static VOID SvcInstanceRelease(SVC_INSTANCE *SvcInstance)
{
    if (0 != InterlockedDecrement(&SvcInstance->RefCount))
        return;

    EnterCriticalSection(&SvcInstanceLock);
    if (RemoveEntryList(&SvcInstance->ListEntry))
        SetEvent(SvcInstanceEvent);
    LeaveCriticalSection(&SvcInstanceLock);

    SetHandleInformation(SvcInstance->ClientToken,
        HANDLE_FLAG_PROTECT_FROM_CLOSE,
        0);

    if (1 == SvcInstance->Recovery && SvcInstance->Started && !SvcInstance->Stopped)
        SvcInstanceStart(SvcInstance->ClientToken,
            SvcInstance->ClassName, SvcInstance->InstanceName,
            SvcInstance->Argc, SvcInstance->Argv,
            SvcJob,
            SvcInstance->HasSecret);

    MemFree(SvcInstance->Argv);

    if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[0])
        CloseHandle(SvcInstance->StdioHandles[0]);
    if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[1])
        CloseHandle(SvcInstance->StdioHandles[1]);
    if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[2])
        CloseHandle(SvcInstance->StdioHandles[2]);

    if (0 != SvcInstance->ProcessWait)
        UnregisterWaitEx(SvcInstance->ProcessWait, 0);
    if (0 != SvcInstance->Process)
        CloseHandle(SvcInstance->Process);

    LocalFree(SvcInstance->SecurityDescriptor);

    MemFree(SvcInstance->CommandLine);

    /*
     * NOTE:
     * New instances store the ClientToken and protect it from CloseHandle.
     * This results in an unhandled exception when running under a debugger.
     * Such exceptions can be ignored.
     *
     * See MSDN CloseHandle for details:
     * https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
     */
    CloseHandle(SvcInstance->ClientToken);

    MemFree(SvcInstance);
}

static VOID CALLBACK SvcInstanceTerminated(PVOID Context, BOOLEAN Timeout)
{
    SVC_INSTANCE *SvcInstance = Context;

    FspServiceLog(EVENTLOG_INFORMATION_TYPE,
        L"terminated %s\\%s", SvcInstance->ClassName, SvcInstance->InstanceName);

    SvcInstanceRelease(SvcInstance);
}

static NTSTATUS SvcInstanceStartWithArgvCopy(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv, HANDLE Job,
    BOOLEAN HasSecret)
{
    SVC_INSTANCE *SvcInstance;
    NTSTATUS Result;

    if (HasSecret && (0 == Argc || L'\0' == Argv[Argc - 1][0]))
        return STATUS_INVALID_PARAMETER;
    HasSecret = !!HasSecret;

    Result = SvcInstanceCreate(ClientToken, ClassName, InstanceName,
        Argc - HasSecret, Argv, Job, HasSecret,
        &SvcInstance);
    if (!NT_SUCCESS(Result))
        return Result;

    if (!HasSecret)
        Result = STATUS_SUCCESS;
    else
    {
        PWSTR Secret = Argv[Argc - 1];
        UINT8 ReqBuf[256];
        UINT8 RspBuf[2];
        DWORD BytesTransferred;
        OVERLAPPED Overlapped;
        DWORD WaitResult;

        if (0 == (BytesTransferred =
            WideCharToMultiByte(CP_UTF8, 0, Secret, lstrlenW(Secret), ReqBuf, sizeof ReqBuf, 0, 0)))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        if (!WriteFile(SvcInstance->StdioHandles[0], ReqBuf, BytesTransferred, &BytesTransferred, 0))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        CloseHandle(SvcInstance->StdioHandles[0]);
        SvcInstance->StdioHandles[0] = INVALID_HANDLE_VALUE;

        memset(&Overlapped, 0, sizeof Overlapped);
        if (!ReadFile(SvcInstance->StdioHandles[1], RspBuf, sizeof RspBuf, 0, &Overlapped) &&
            ERROR_IO_PENDING != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        /*
         * We need to perform a GetOverlappedResult with a timeout. GetOverlappedResultEx would
         * be perfect except that it is a Windows 8 and above API. We will therefore replace with
         * WaitForSingleObject followed by GetOverlappedResult on success.
         */
        WaitResult = WaitForSingleObject(SvcInstance->StdioHandles[1],
            LAUNCHER_START_WITH_SECRET_TIMEOUT);
        if (WAIT_OBJECT_0 == WaitResult)
            Result = GetOverlappedResult(SvcInstance->StdioHandles[1], &Overlapped, &BytesTransferred, TRUE) ?
                STATUS_SUCCESS : FspNtStatusFromWin32(GetLastError());
        else if (WAIT_TIMEOUT == WaitResult)
            Result = STATUS_TIMEOUT;
        else
            Result = FspNtStatusFromWin32(GetLastError());
        if (!NT_SUCCESS(Result) || STATUS_TIMEOUT == Result)
        {
            CancelIoEx(SvcInstance->StdioHandles[1], &Overlapped);
            goto exit;
        }

        if (sizeof RspBuf <= BytesTransferred && 'O' == RspBuf[0] && 'K' == RspBuf[1])
            Result = STATUS_SUCCESS;
        else
            Result = STATUS_ACCESS_DENIED;
    }

exit:
    if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[0])
    {
        CloseHandle(SvcInstance->StdioHandles[0]);
        SvcInstance->StdioHandles[0] = INVALID_HANDLE_VALUE;
    }
    if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[1])
    {
        CloseHandle(SvcInstance->StdioHandles[1]);
        SvcInstance->StdioHandles[1] = INVALID_HANDLE_VALUE;
    }
    if (INVALID_HANDLE_VALUE != SvcInstance->StdioHandles[2])
    {
        CloseHandle(SvcInstance->StdioHandles[2]);
        SvcInstance->StdioHandles[2] = INVALID_HANDLE_VALUE;
    }

    if (NT_SUCCESS(Result) && STATUS_TIMEOUT != Result)
    {
        SvcInstance->Argc = Argc;
        SvcInstance->Argv = Argv;
        SvcInstance->HasSecret = HasSecret;
        SvcInstance->Started = TRUE;
    }

    SvcInstanceRelease(SvcInstance);

    if (STATUS_TIMEOUT == Result)
        /* convert to an error! */
        Result = 0x80070000 | ERROR_TIMEOUT;

    return Result;
}

NTSTATUS SvcInstanceStart(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, ULONG Argc, PWSTR *Argv, HANDLE Job,
    BOOLEAN HasSecret)
{
    DWORD ArgvSize;
    PWSTR *ArgvCopy;
    NTSTATUS Result;

    ArgvSize = 0;
    for (ULONG I = 0; Argc > I; I++)
        ArgvSize += (lstrlenW(Argv[I]) + 1) * sizeof(WCHAR);

    ArgvCopy = MemAlloc(Argc * sizeof(PWSTR) + ArgvSize);
    if (0 == ArgvCopy)
        return STATUS_INSUFFICIENT_RESOURCES;

    ArgvSize = 0;
    for (ULONG I = 0; Argc > I; I++)
    {
        ULONG L = (lstrlenW(Argv[I]) + 1) * sizeof(WCHAR);
        ArgvCopy[I] = (PWSTR)((PUINT8)ArgvCopy + Argc * sizeof(PWSTR) + ArgvSize);
        memcpy(ArgvCopy[I], Argv[I], L);
        ArgvSize += L;
    }

    Result = SvcInstanceStartWithArgvCopy(ClientToken,
        ClassName, InstanceName, Argc, ArgvCopy, Job,
        HasSecret);

    if (!NT_SUCCESS(Result))
        MemFree(ArgvCopy);

    return Result;
}

NTSTATUS SvcInstanceStop(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName)
{
    SVC_INSTANCE *SvcInstance;
    NTSTATUS Result;

    EnterCriticalSection(&SvcInstanceLock);

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName);
    if (0 == SvcInstance)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        goto exit;
    }

    Result = SvcInstanceAccessCheck(ClientToken, SERVICE_STOP, SvcInstance->SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    SvcInstance->Stopped = TRUE;
    KillProcess(SvcInstance->ProcessId, SvcInstance->Process, LAUNCHER_KILL_TIMEOUT);

    Result = STATUS_SUCCESS;

exit:
    LeaveCriticalSection(&SvcInstanceLock);

    return Result;
}

NTSTATUS SvcInstanceGetInfo(HANDLE ClientToken,
    PWSTR ClassName, PWSTR InstanceName, PWSTR Buffer, PULONG PSize)
{
    SVC_INSTANCE *SvcInstance;
    PWSTR P = Buffer;
    ULONG ClassNameSize, InstanceNameSize, CommandLineSize;
    NTSTATUS Result;

    EnterCriticalSection(&SvcInstanceLock);

    SvcInstance = SvcInstanceLookup(ClassName, InstanceName);
    if (0 == SvcInstance)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        goto exit;
    }

    Result = SvcInstanceAccessCheck(ClientToken, SERVICE_QUERY_STATUS, SvcInstance->SecurityDescriptor);
    if (!NT_SUCCESS(Result))
        goto exit;

    ClassNameSize = lstrlenW(SvcInstance->ClassName) + 1;
    InstanceNameSize = lstrlenW(SvcInstance->InstanceName) + 1;
    CommandLineSize = lstrlenW(SvcInstance->CommandLine) + 1;

    if (*PSize < (ClassNameSize + InstanceNameSize + CommandLineSize) * sizeof(WCHAR))
    {
        Result = STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }

    memcpy(P, SvcInstance->ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
    memcpy(P, SvcInstance->InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    memcpy(P, SvcInstance->CommandLine, CommandLineSize * sizeof(WCHAR)); P += CommandLineSize;

    *PSize = (ULONG)(P - Buffer) * sizeof(WCHAR);

    Result = STATUS_SUCCESS;

exit:
    LeaveCriticalSection(&SvcInstanceLock);

    return Result;
}

NTSTATUS SvcInstanceGetNameList(HANDLE ClientToken,
    PWSTR Buffer, PULONG PSize)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;
    PWSTR P = Buffer, BufferEnd = P + *PSize / sizeof(WCHAR);
    ULONG ClassNameSize, InstanceNameSize;

    EnterCriticalSection(&SvcInstanceLock);

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        ClassNameSize = lstrlenW(SvcInstance->ClassName) + 1;
        InstanceNameSize = lstrlenW(SvcInstance->InstanceName) + 1;

        if (BufferEnd < P + ClassNameSize + InstanceNameSize)
            break;

        memcpy(P, SvcInstance->ClassName, ClassNameSize * sizeof(WCHAR)); P += ClassNameSize;
        memcpy(P, SvcInstance->InstanceName, InstanceNameSize * sizeof(WCHAR)); P += InstanceNameSize;
    }

    LeaveCriticalSection(&SvcInstanceLock);

    *PSize = (ULONG)(P - Buffer) * sizeof(WCHAR);

    return STATUS_SUCCESS;
}

NTSTATUS SvcInstanceStopAndWaitAll(VOID)
{
    SVC_INSTANCE *SvcInstance;
    PLIST_ENTRY ListEntry;

    EnterCriticalSection(&SvcInstanceLock);

    for (ListEntry = SvcInstanceList.Flink;
        &SvcInstanceList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        SvcInstance = CONTAINING_RECORD(ListEntry, SVC_INSTANCE, ListEntry);

        SvcInstance->Stopped = TRUE;
        KillProcess(SvcInstance->ProcessId, SvcInstance->Process, LAUNCHER_KILL_TIMEOUT);
    }

    LeaveCriticalSection(&SvcInstanceLock);

    WaitForSingleObject(SvcInstanceEvent, LAUNCHER_STOP_TIMEOUT);

    return STATUS_SUCCESS;
}

NTSTATUS SvcDefineDosDevice(HANDLE ClientToken,
    PWSTR DeviceName, PWSTR TargetPath)
{
    NTSTATUS Result;

    if (L'+' != DeviceName[0] && L'-' != DeviceName[0])
        return STATUS_INVALID_PARAMETER;

    Result = FspServiceContextCheck(ClientToken, 0);
    if (!NT_SUCCESS(Result))
        return Result;

    if (!DefineDosDeviceW(
        DDD_RAW_TARGET_PATH |
            (L'+' == DeviceName[0] ? 0 : DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE),
        DeviceName + 1, TargetPath))
        return FspNtStatusFromWin32(GetLastError());

    if (L'+' == DeviceName[0] && 0 != SvcNtOpenSymbolicLinkObject)
    {
        /* The drive symlink now exists; add DELETE access to it for the ClientToken. */
        WCHAR SymlinkBuf[6];
        UNICODE_STRING Symlink;
        OBJECT_ATTRIBUTES Obja;
        HANDLE MountHandle;

        memcpy(SymlinkBuf, L"\\??\\X:", sizeof SymlinkBuf);
        SymlinkBuf[4] = DeviceName[1];
        Symlink.Length = Symlink.MaximumLength = sizeof SymlinkBuf;
        Symlink.Buffer = SymlinkBuf;

        memset(&Obja, 0, sizeof Obja);
        Obja.Length = sizeof Obja;
        Obja.ObjectName = &Symlink;
        Obja.Attributes = OBJ_CASE_INSENSITIVE;

        Result = SvcNtOpenSymbolicLinkObject(&MountHandle, READ_CONTROL | WRITE_DAC, &Obja);
        if (NT_SUCCESS(Result))
        {
            AddAccessForTokenUser(MountHandle, DELETE, ClientToken);
            SvcNtClose(MountHandle);
        }
    }

    return STATUS_SUCCESS;
}

static HANDLE SvcThread, SvcEvent;
static DWORD SvcThreadId;
static HANDLE SvcPipe = INVALID_HANDLE_VALUE;
static OVERLAPPED SvcOverlapped;

static DWORD WINAPI SvcPipeServer(PVOID Context);
static VOID SvcPipeTransact(HANDLE ClientToken, PWSTR PipeBuf, PULONG PSize);

static NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    SECURITY_ATTRIBUTES SecurityAttributes = { 0 };

    /*
     * Allocate a console in case we are running as a service without one.
     * This will ensure that we can send console control events to service instances.
     */
    if (AllocConsole())
        ShowWindow(GetConsoleWindow(), SW_HIDE);

    InitializeCriticalSection(&SvcInstanceLock);

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"" FSP_LAUNCH_PIPE_SDDL, SDDL_REVISION_1,
        &SecurityAttributes.lpSecurityDescriptor, 0))
        goto fail;

    //FspDebugLogSD(__FUNCTION__ ": SDDL = %s\n", SecurityAttributes.lpSecurityDescriptor);

    SvcInstanceEvent = CreateEventW(0, TRUE, TRUE, 0);
    if (0 == SvcInstanceEvent)
        goto fail;

    SvcJob = CreateJobObjectW(0, 0);
    if (0 != SvcJob)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION LimitInfo;

        memset(&LimitInfo, 0, sizeof LimitInfo);
        LimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(SvcJob, JobObjectExtendedLimitInformation,
            &LimitInfo, sizeof LimitInfo))
        {
            CloseHandle(SvcJob);
            SvcJob = 0;
        }
    }

    SvcEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == SvcEvent)
        goto fail;

    SvcOverlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);
    if (0 == SvcOverlapped.hEvent)
        goto fail;

    SvcPipe = CreateNamedPipeW(L"" FSP_LAUNCH_PIPE_NAME,
        PIPE_ACCESS_DUPLEX |
            FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, FSP_LAUNCH_PIPE_BUFFER_SIZE, FSP_LAUNCH_PIPE_BUFFER_SIZE, LAUNCHER_PIPE_DEFAULT_TIMEOUT,
        &SecurityAttributes);
    if (INVALID_HANDLE_VALUE == SvcPipe)
        goto fail;

    SvcThread = CreateThread(0, 0, SvcPipeServer, Service, 0, &SvcThreadId);
    if (0 == SvcThread)
        goto fail;

    LocalFree(SecurityAttributes.lpSecurityDescriptor);

    return STATUS_SUCCESS;

fail:
    DWORD LastError = GetLastError();

    /*
     * The OS will cleanup for us. So there is no need to explicitly release these resources.
     */
#if 0
    if (0 != SvcThread)
        CloseHandle(SvcThread);

    if (INVALID_HANDLE_VALUE != SvcPipe)
        CloseHandle(SvcPipe);

    if (0 != SvcOverlapped.hEvent)
        CloseHandle(SvcOverlapped.hEvent);

    if (0 != SvcEvent)
        CloseHandle(SvcEvent);

    if (0 != SvcJob)
        CloseHandle(SvcJob);

    if (0 != SvcInstanceEvent)
        CloseHandle(SvcInstanceEvent);

    LocalFree(SecurityAttributes.lpSecurityDescriptor);

    DeleteCriticalSection(&SvcInstanceLock);
#endif

    return FspNtStatusFromWin32(LastError);
}

static NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    if (GetCurrentThreadId() != SvcThreadId)
    {
        SetEvent(SvcEvent);
        FspServiceRequestTime(Service, LAUNCHER_STOP_TIMEOUT);
        WaitForSingleObject(SvcThread, LAUNCHER_STOP_TIMEOUT);
    }

    /*
     * The OS will cleanup for us. So there is no need to explicitly release these resources.
     *
     * This also protects us from scenarios where not all child processes terminate timely
     * and KillProcess decides to terminate them forcibly, thus creating racing conditions
     * with SvcInstanceTerminated.
     */
#if 0
    if (0 != SvcThread)
        CloseHandle(SvcThread);

    if (INVALID_HANDLE_VALUE != SvcPipe)
        CloseHandle(SvcPipe);

    if (0 != SvcOverlapped.hEvent)
        CloseHandle(SvcOverlapped.hEvent);

    if (0 != SvcEvent)
        CloseHandle(SvcEvent);

    if (0 != SvcJob)
        CloseHandle(SvcJob);

    if (0 != SvcInstanceEvent)
        CloseHandle(SvcInstanceEvent);

    DeleteCriticalSection(&SvcInstanceLock);
#endif

    return STATUS_SUCCESS;
}

static inline DWORD SvcPipeWaitResult(BOOL Success, HANDLE StopEvent,
    HANDLE Handle, OVERLAPPED *Overlapped, PDWORD PBytesTransferred)
{
    HANDLE WaitObjects[2];
    DWORD WaitResult;

    if (!Success && ERROR_IO_PENDING != GetLastError())
        return GetLastError();

    WaitObjects[0] = StopEvent;
    WaitObjects[1] = Overlapped->hEvent;
    WaitResult = WaitForMultipleObjects(2, WaitObjects, FALSE, INFINITE);
    if (WAIT_OBJECT_0 == WaitResult)
        return -1; /* special: stop thread */
    else if (WAIT_OBJECT_0 + 1 == WaitResult)
    {
        if (!GetOverlappedResult(Handle, Overlapped, PBytesTransferred, TRUE))
            return GetLastError();
        return 0;
    }
    else
        return GetLastError();
}

static DWORD WINAPI SvcPipeServer(PVOID Context)
{
    static PWSTR LoopErrorMessage =
        L"Error in service main loop (%s = %ld). Exiting...";
    static PWSTR LoopWarningMessage =
        L"Error in service main loop (%s = %ld). Continuing...";
    FSP_SERVICE *Service = Context;
    PWSTR PipeBuf = 0;
    HANDLE ClientToken;
    DWORD LastError, BytesTransferred;

    PipeBuf = MemAlloc(FSP_LAUNCH_PIPE_BUFFER_SIZE);
    if (0 == PipeBuf)
    {
        FspServiceSetExitCode(Service, ERROR_NO_SYSTEM_RESOURCES);
        goto exit;
    }

    for (;;)
    {
        LastError = SvcPipeWaitResult(
            ConnectNamedPipe(SvcPipe, &SvcOverlapped),
            SvcEvent, SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError &&
            ERROR_PIPE_CONNECTED != LastError && ERROR_NO_DATA != LastError)
        {
            FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                L"ConnectNamedPipe", LastError);
            continue;
        }

        LastError = SvcPipeWaitResult(
            ReadFile(SvcPipe, PipeBuf, FSP_LAUNCH_PIPE_BUFFER_SIZE, &BytesTransferred, &SvcOverlapped),
            SvcEvent, SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError || sizeof(WCHAR) > BytesTransferred)
        {
            DisconnectNamedPipe(SvcPipe);
            if (0 != LastError)
                FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                    L"ReadFile", LastError);
            continue;
        }

        ClientToken = 0;
        if (!ImpersonateNamedPipeClient(SvcPipe) ||
            (
                !OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_DUPLICATE, FALSE, &ClientToken) &&
                !OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &ClientToken)
            ) ||
            !RevertToSelf())
        {
            LastError = GetLastError();
            if (0 == ClientToken)
            {
                CloseHandle(ClientToken);
                DisconnectNamedPipe(SvcPipe);
                FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                    L"ImpersonateNamedPipeClient||OpenThreadToken", LastError);
                continue;
            }
            else
            {
                CloseHandle(ClientToken);
                DisconnectNamedPipe(SvcPipe);
                FspServiceLog(EVENTLOG_ERROR_TYPE, LoopErrorMessage,
                    L"RevertToSelf", LastError);
                break;
            }
        }

        SvcPipeTransact(ClientToken, PipeBuf, &BytesTransferred);

        /*
         * NOTE:
         * New instances store the ClientToken and protect it from CloseHandle.
         * This results in an unhandled exception when running under a debugger.
         * Such exceptions can be ignored.
         *
         * See MSDN CloseHandle for details:
         * https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
         */
        CloseHandle(ClientToken);

        LastError = SvcPipeWaitResult(
            WriteFile(SvcPipe, PipeBuf, BytesTransferred, &BytesTransferred, &SvcOverlapped),
            SvcEvent, SvcPipe, &SvcOverlapped, &BytesTransferred);
        if (-1 == LastError)
            break;
        else if (0 != LastError)
        {
            DisconnectNamedPipe(SvcPipe);
            FspServiceLog(EVENTLOG_WARNING_TYPE, LoopWarningMessage,
                L"WriteFile", LastError);
            continue;
        }

        DisconnectNamedPipe(SvcPipe);
    }

exit:
    MemFree(PipeBuf);

    SvcInstanceStopAndWaitAll();

    FspServiceStop(Service);

    return 0;
}

static inline PWSTR SvcPipeTransactGetPart(PWSTR *PP, PWSTR PipeBufEnd)
{
    PWSTR PipeBufBeg = *PP, P;

    for (P = PipeBufBeg; PipeBufEnd > P && *P; P++)
        ;

    if (PipeBufEnd > P)
    {
        *PP = P + 1;
        return PipeBufBeg;
    }
    else
    {
        *PP = P;
        return 0;
    }
}

static inline VOID SvcPipeTransactResult(NTSTATUS Result, PWSTR PipeBuf, PULONG PSize)
{
    if (NT_SUCCESS(Result))
    {
        *PipeBuf = FspLaunchCmdSuccess;
        *PSize += sizeof(WCHAR);
    }
    else
        *PSize = (wsprintfW(PipeBuf, L"%c%ld", FspLaunchCmdFailure, FspWin32FromNtStatus(Result)) + 1) *
            sizeof(WCHAR);
}

static VOID SvcPipeTransact(HANDLE ClientToken, PWSTR PipeBuf, PULONG PSize)
{
    if (sizeof(WCHAR) > *PSize)
        return;

    PWSTR P = PipeBuf, PipeBufEnd = PipeBuf + *PSize / sizeof(WCHAR);
    PWSTR ClassName, InstanceName;
    PWSTR DeviceName, TargetPath;
    ULONG Argc; PWSTR Argv[9];
    BOOLEAN HasSecret = FALSE;
    NTSTATUS Result;

    *PSize = 0;

    switch (*P++)
    {
    case FspLaunchCmdStartWithSecret:
        HasSecret = TRUE;
        /* fall through! */
    case FspLaunchCmdStart:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        for (Argc = 0; sizeof Argv / sizeof Argv[0] > Argc; Argc++)
            if (0 == (Argv[Argc] = SvcPipeTransactGetPart(&P, PipeBufEnd)))
                break;

        Result = STATUS_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Result = SvcInstanceStart(ClientToken, ClassName, InstanceName, Argc, Argv, SvcJob,
                HasSecret);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case FspLaunchCmdStop:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Result = STATUS_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
            Result = SvcInstanceStop(ClientToken, ClassName, InstanceName);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case FspLaunchCmdGetInfo:
        ClassName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        InstanceName = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Result = STATUS_INVALID_PARAMETER;
        if (0 != ClassName && 0 != InstanceName)
        {
            *PSize = FSP_LAUNCH_PIPE_BUFFER_SIZE - 1;
            Result = SvcInstanceGetInfo(ClientToken, ClassName, InstanceName, PipeBuf + 1, PSize);
        }

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case FspLaunchCmdGetNameList:
        *PSize = FSP_LAUNCH_PIPE_BUFFER_SIZE - 1;
        Result = SvcInstanceGetNameList(ClientToken, PipeBuf + 1, PSize);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

    case FspLaunchCmdDefineDosDevice:
        DeviceName = SvcPipeTransactGetPart(&P, PipeBufEnd);
        TargetPath = SvcPipeTransactGetPart(&P, PipeBufEnd);

        Result = STATUS_INVALID_PARAMETER;
        if (0 != DeviceName && 0 != TargetPath)
            Result = SvcDefineDosDevice(ClientToken, DeviceName, TargetPath);

        SvcPipeTransactResult(Result, PipeBuf, PSize);
        break;

#if !defined(NDEBUG)
    case FspLaunchCmdQuit:
        SetEvent(SvcEvent);

        SvcPipeTransactResult(STATUS_SUCCESS, PipeBuf, PSize);
        break;
#endif
    default:
        SvcPipeTransactResult(STATUS_INVALID_PARAMETER, PipeBuf, PSize);
        break;
    }
}

int wmain(int argc, wchar_t **argv)
{
    HANDLE Handle = GetModuleHandleW(L"ntdll.dll");
    if (0 != Handle)
    {
        SvcNtOpenSymbolicLinkObject = (PVOID)GetProcAddress(Handle, "NtOpenSymbolicLinkObject");
        SvcNtClose = (PVOID)GetProcAddress(Handle, "NtClose");

        if (0 == SvcNtOpenSymbolicLinkObject || 0 == SvcNtClose)
        {
            SvcNtOpenSymbolicLinkObject = 0;
            SvcNtClose = 0;
        }
    }

    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}

void wmainCRTStartup(void)
{
    ExitProcess(wmain(0, 0));
}
