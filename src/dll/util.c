/**
 * @file dll/util.c
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

#include <dll/library.h>
#include <aclapi.h>
#include <shlwapi.h>

static INIT_ONCE FspDiagIdentInitOnce = INIT_ONCE_STATIC_INIT;
static WCHAR FspDiagIdentBuf[20] = L"UNKNOWN";

static BOOL WINAPI FspDiagIdentInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    WCHAR ModuleFileName[MAX_PATH];
    WCHAR Root[2] = L"\\";
    PWSTR Parent, ModuleBaseName;

    if (0 != GetModuleFileNameW(0, ModuleFileName, sizeof ModuleFileName / sizeof(WCHAR)))
        FspPathSuffix(ModuleFileName, &Parent, &ModuleBaseName, Root);
    else
        lstrcpyW(ModuleBaseName = ModuleFileName, L"UNKNOWN");

    for (PWSTR P = ModuleBaseName, Dot = 0;; P++)
    {
        if (L'\0' == *P)
        {
            if (0 != Dot)
                *Dot = L'\0';
            break;
        }
        if (L'.' == *P)
            Dot = P;
    }

    memcpy(FspDiagIdentBuf, ModuleBaseName, sizeof FspDiagIdentBuf);
    FspDiagIdentBuf[(sizeof FspDiagIdentBuf / sizeof(WCHAR)) - 1] = L'\0';

    return TRUE;
}

PWSTR FspDiagIdent(VOID)
{
    /* internal only: get a diagnostic identifier (eventlog, debug) */

    InitOnceExecuteOnce(&FspDiagIdentInitOnce, FspDiagIdentInitialize, 0, 0);
    return FspDiagIdentBuf;
}

static INIT_ONCE FspCreateDirectoryFileInitOnce = INIT_ONCE_STATIC_INIT;
static NTSTATUS (NTAPI *FspNtCreateFile)(
    PHANDLE FileHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PIO_STATUS_BLOCK IoStatusBlock,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG ShareAccess,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength);

static BOOL WINAPI FspCreateDirectoryFileInitialize(
    PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
    HANDLE Handle;

    Handle = GetModuleHandleW(L"ntdll.dll");
    if (0 != Handle)
        FspNtCreateFile = (PVOID)GetProcAddress(Handle, "NtCreateFile");

    return TRUE;
}

HANDLE FspCreateDirectoryFileW(
    PWSTR FileName,
    DWORD DesiredAccess,
    DWORD ShareAccess,
    PSECURITY_ATTRIBUTES SecurityAttributes,
    DWORD FlagsAndAttributes)
{
    InitOnceExecuteOnce(&FspCreateDirectoryFileInitOnce, FspCreateDirectoryFileInitialize, 0, 0);
    if (0 == FspNtCreateFile)
    {
        SetLastError(ERROR_INVALID_FUNCTION);
        return INVALID_HANDLE_VALUE;
    }

    HANDLE Handle = INVALID_HANDLE_VALUE, ParentHandle = INVALID_HANDLE_VALUE;
    PWSTR FullFileName = 0;
    WCHAR *FilePart, FilePartChar;
    DWORD Length;
    UNICODE_STRING UFullFileName;
    OBJECT_ATTRIBUTES Obja;
    ULONG CreateFlags;
    IO_STATUS_BLOCK IoStatus;

    Length = GetFullPathNameW(FileName, 0, 0, 0);
    if (0 == Length)
        goto exit;

    FullFileName = MemAlloc((Length + 1) * sizeof(WCHAR));
    if (0 == FullFileName)
    {
        SetLastError(ERROR_NO_SYSTEM_RESOURCES);
        goto exit;
    }

    Length = GetFullPathNameW(FileName, Length + 1, FullFileName, &FilePart);
    if (0 == Length)
        goto exit;

    FilePartChar = *FilePart;
    *FilePart = L'\0';

    ParentHandle = CreateFileW(
        FullFileName,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        0);
    if (INVALID_HANDLE_VALUE == ParentHandle)
        goto exit;

    *FilePart = FilePartChar;

    UFullFileName.Length = UFullFileName.MaximumLength = (USHORT)(lstrlenW(FilePart) * sizeof(WCHAR));
    UFullFileName.Buffer = FilePart;

    memset(&Obja, 0, sizeof Obja);
    Obja.Length = sizeof Obja;
    Obja.ObjectName = &UFullFileName;
    Obja.Attributes =
        (!(FlagsAndAttributes & FILE_FLAG_POSIX_SEMANTICS) ? OBJ_CASE_INSENSITIVE : 0) |
        (SecurityAttributes->bInheritHandle ? OBJ_INHERIT : 0);
    Obja.RootDirectory = ParentHandle;
    Obja.SecurityDescriptor = SecurityAttributes->lpSecurityDescriptor;

    DesiredAccess |=
        SYNCHRONIZE |
        FILE_READ_ATTRIBUTES |
        ((FlagsAndAttributes & FILE_FLAG_DELETE_ON_CLOSE) ? DELETE : 0);

    CreateFlags =
        FILE_DIRECTORY_FILE |
        ((FlagsAndAttributes & FILE_FLAG_WRITE_THROUGH) ? FILE_WRITE_THROUGH : 0) |
        ((FlagsAndAttributes & FILE_FLAG_OVERLAPPED) ? FILE_SYNCHRONOUS_IO_NONALERT : 0) |
        ((FlagsAndAttributes & FILE_FLAG_NO_BUFFERING) ? FILE_NO_INTERMEDIATE_BUFFERING : 0) |
        ((FlagsAndAttributes & FILE_FLAG_RANDOM_ACCESS) ? FILE_RANDOM_ACCESS : 0) |
        ((FlagsAndAttributes & FILE_FLAG_SEQUENTIAL_SCAN) ? FILE_SEQUENTIAL_ONLY : 0) |
        ((FlagsAndAttributes & FILE_FLAG_DELETE_ON_CLOSE) ? FILE_DELETE_ON_CLOSE : 0) |
        ((FlagsAndAttributes & FILE_FLAG_BACKUP_SEMANTICS) ? FILE_OPEN_FOR_BACKUP_INTENT : 0) |
        ((FlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) ? FILE_OPEN_REPARSE_POINT : 0) |
        ((FlagsAndAttributes & FILE_FLAG_OPEN_NO_RECALL) ? FILE_OPEN_NO_RECALL : 0);

    IoStatus.Status = FspNtCreateFile(
        &Handle,
        DesiredAccess,
        &Obja,
        &IoStatus,
        0,
        FlagsAndAttributes & (0x7fff & ~FILE_ATTRIBUTE_DIRECTORY),
        ShareAccess,
        FILE_CREATE,
        CreateFlags,
        0,
        0);
    if (!NT_SUCCESS(IoStatus.Status))
    {
        SetLastError(FspWin32FromNtStatus(IoStatus.Status));
        Handle = INVALID_HANDLE_VALUE;
        goto exit;
    }

    SetLastError(0);

exit:
    if (INVALID_HANDLE_VALUE != ParentHandle)
        CloseHandle(ParentHandle);

    MemFree(FullFileName);

    return Handle;
}

FSP_API NTSTATUS FspCallNamedPipeSecurely(PWSTR PipeName,
    PVOID InBuffer, ULONG InBufferSize, PVOID OutBuffer, ULONG OutBufferSize,
    PULONG PBytesTransferred, ULONG Timeout,
    PSID Sid)
{
    return FspCallNamedPipeSecurelyEx(PipeName,
        InBuffer, InBufferSize, OutBuffer, OutBufferSize, PBytesTransferred, Timeout,
        FALSE, Sid);
}

FSP_API NTSTATUS FspCallNamedPipeSecurelyEx(PWSTR PipeName,
    PVOID InBuffer, ULONG InBufferSize, PVOID OutBuffer, ULONG OutBufferSize,
    PULONG PBytesTransferred, ULONG Timeout, BOOLEAN AllowImpersonation,
    PSID Sid)
{
    NTSTATUS Result;
    HANDLE Pipe = INVALID_HANDLE_VALUE;
    DWORD PipeMode;

    Pipe = CreateFileW(PipeName,
        GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | (AllowImpersonation ? SECURITY_IMPERSONATION : SECURITY_IDENTIFICATION),
        0);
    if (INVALID_HANDLE_VALUE == Pipe)
    {
        if (ERROR_PIPE_BUSY != GetLastError())
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        WaitNamedPipeW(PipeName, Timeout);

        Pipe = CreateFileW(PipeName,
            GENERIC_READ | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
            SECURITY_SQOS_PRESENT | (AllowImpersonation ? SECURITY_IMPERSONATION : SECURITY_IDENTIFICATION),
            0);
        if (INVALID_HANDLE_VALUE == Pipe)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }
    }

    if (0 != Sid)
    {
        PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
        PSID OwnerSid, WellKnownSid = 0;
        DWORD LastError;

        /* if it is a small number treat it like a well known SID */
        if (1024 > (INT_PTR)Sid)
        {
            WellKnownSid = FspWksidNew((INT_PTR)Sid, &Result);
            if (0 == WellKnownSid)
                goto sid_exit;
        }

        LastError = GetSecurityInfo(Pipe, SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION, &OwnerSid, 0, 0, 0, &SecurityDescriptor);
        if (0 != LastError)
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto sid_exit;
        }

        if (!EqualSid(OwnerSid, WellKnownSid ? WellKnownSid : Sid))
        {
            Result = STATUS_ACCESS_DENIED;
            goto sid_exit;
        }

        Result = STATUS_SUCCESS;

    sid_exit:
        MemFree(WellKnownSid);
        LocalFree(SecurityDescriptor);

        if (!NT_SUCCESS(Result))
            goto exit;
    }

    PipeMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
    if (!SetNamedPipeHandleState(Pipe, &PipeMode, 0, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (!TransactNamedPipe(Pipe, InBuffer, InBufferSize, OutBuffer, OutBufferSize,
        PBytesTransferred, 0))
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != Pipe)
        CloseHandle(Pipe);

    return Result;
}

FSP_API NTSTATUS FspVersion(PUINT32 PVersion)
{
    static UINT32 Version;

    if (0 == Version)
    {
        /*
         * This check is not thread-safe, but that should be ok.
         * Two threads competing to read the version will read
         * the same value from the Version resource.
         */
        *PVersion = 0;

        extern HINSTANCE DllInstance;
        WCHAR ModuleFileName[MAX_PATH];
        PVOID VersionInfo;
        DWORD Size;
        VS_FIXEDFILEINFO *FixedFileInfo = 0;

        if (0 != GetModuleFileNameW(DllInstance, ModuleFileName, MAX_PATH))
        {
            Size = GetFileVersionInfoSizeW(ModuleFileName, &Size/*dummy*/);
            if (0 < Size)
            {
                VersionInfo = MemAlloc(Size);
                if (0 != VersionInfo &&
                    GetFileVersionInfoW(ModuleFileName, 0, Size, VersionInfo) &&
                    VerQueryValueW(VersionInfo, L"\\", &FixedFileInfo, &Size))
                {
                    /* 32-bit store should be atomic! */
                    Version = FixedFileInfo->dwFileVersionMS;
                }

                MemFree(VersionInfo);
            }
        }

        if (0 == FixedFileInfo)
            return STATUS_UNSUCCESSFUL;
    }

    *PVersion = Version;

    return STATUS_SUCCESS;
}

NTSTATUS FspGetModuleVersion(PWSTR ModuleFileName, PUINT32 PVersion)
{
    /* internal only: get version of any module */

    PVOID VersionInfo;
    DWORD Size;
    VS_FIXEDFILEINFO *FixedFileInfo = 0;
    UINT32 Version;

    *PVersion = 0;

    Size = GetFileVersionInfoSizeW(ModuleFileName, &Size/*dummy*/);
    if (0 < Size)
    {
        VersionInfo = MemAlloc(Size);
        if (0 != VersionInfo &&
            GetFileVersionInfoW(ModuleFileName, 0, Size, VersionInfo) &&
            VerQueryValueW(VersionInfo, L"\\", &FixedFileInfo, &Size))
        {
            Version = FixedFileInfo->dwFileVersionMS;
        }

        MemFree(VersionInfo);
    }

    if (0 == FixedFileInfo)
        return STATUS_UNSUCCESSFUL;

    *PVersion = Version;

    return STATUS_SUCCESS;
}

NTSTATUS FspGetModuleFileName(
    HMODULE Module,
    PWSTR FileName,
    ULONG Size,
    PWSTR RelativePath)
{
    if (MAX_PATH > Size)
        return STATUS_BUFFER_TOO_SMALL;

    if (0 != RelativePath &&
        L'\0' != RelativePath[0] &&
        (L'.' != RelativePath[0] || L'\0' != RelativePath[1]))
    {
        WCHAR Temp[MAX_PATH];

        if (0 == GetModuleFileNameW(Module, Temp, MAX_PATH))
            return FspNtStatusFromWin32(GetLastError());

        if (0 == PathCombineW(FileName, Temp, RelativePath))
            return STATUS_UNSUCCESSFUL;
    }
    else
    {
        if (0 == GetModuleFileNameW(Module, FileName, MAX_PATH))
            return FspNtStatusFromWin32(GetLastError());
    }

    return STATUS_SUCCESS;
}
