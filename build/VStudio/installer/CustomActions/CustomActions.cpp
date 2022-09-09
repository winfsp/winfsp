/**
 * @file CustomActions.cpp
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <msiquery.h>
#include <wcautil.h>
#include <strutil.h>

static HINSTANCE DllInstance;

UINT __stdcall InstanceID(MSIHANDLE MsiHandle)
{
#if 0
    WCHAR MessageBuf[64];
    wsprintfW(MessageBuf, L"PID=%ld", GetCurrentProcessId());
    MessageBoxW(0, MessageBuf, L"" __FUNCTION__ " Break", MB_OK);
#endif

    HRESULT hr = S_OK;
    UINT err = ERROR_SUCCESS;
    SYSTEMTIME SystemTime;
    WCHAR Result[32+1];

    hr = WcaInitialize(MsiHandle, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Initialized");

    GetSystemTime(&SystemTime);
    wsprintfW(Result, L"%04u%02u%02uT%02u%02u%02uZ",
        SystemTime.wYear,
        SystemTime.wMonth,
        SystemTime.wDay,
        SystemTime.wHour,
        SystemTime.wMinute,
        SystemTime.wSecond);

    /*
     * Sleep 1 second to ensure timestamp uniqueness.
     *
     * Note that this assumes that time is monotonic and users do not change time.
     * Disable for now as it is assumed that the installation takes more than 1 second to complete.
     */
    //Sleep(1000);

    WcaSetProperty(L"" __FUNCTION__, Result);

LExit:
    err = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(err);
}

UINT __stdcall ServiceRunning(MSIHANDLE MsiHandle)
{
#if 0
    WCHAR MessageBuf[64];
    wsprintfW(MessageBuf, L"PID=%ld", GetCurrentProcessId());
    MessageBoxW(0, MessageBuf, L"" __FUNCTION__ " Break", MB_OK);
#endif

    HRESULT hr = S_OK;
    UINT err = ERROR_SUCCESS;
    PWSTR ServiceName = 0;
    SC_HANDLE ScmHandle = 0;
    SC_HANDLE SvcHandle = 0;
    SERVICE_STATUS ServiceStatus;
    int Result = 0;

    hr = WcaInitialize(MsiHandle, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    hr = WcaGetProperty(L"" __FUNCTION__, &ServiceName);
    ExitOnFailure(hr, "Failed to get ServiceName");

    WcaLog(LOGMSG_STANDARD, "Initialized: \"%S\"", ServiceName);

    ScmHandle = OpenSCManagerW(0, 0, 0);
    ExitOnNullWithLastError(ScmHandle, hr, "Failed to open SCM");

    SvcHandle = OpenServiceW(ScmHandle, ServiceName, SERVICE_QUERY_STATUS);
    if (0 != SvcHandle && QueryServiceStatus(SvcHandle, &ServiceStatus))
        Result = SERVICE_STOPPED != ServiceStatus.dwCurrentState;

    WcaSetIntProperty(L"" __FUNCTION__, Result);

LExit:
    if (0 != SvcHandle)
        CloseServiceHandle(SvcHandle);
    if (0 != ScmHandle)
        CloseServiceHandle(ScmHandle);

    ReleaseStr(ServiceName);

    err = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(err);
}

UINT __stdcall DeferredAction(MSIHANDLE MsiHandle)
{
#if 0
    WCHAR MessageBuf[64];
    wsprintfW(MessageBuf, L"PID=%ld", GetCurrentProcessId());
    MessageBoxW(0, MessageBuf, L"" __FUNCTION__ " Break", MB_OK);
#endif

    HRESULT hr = S_OK;
    UINT err = ERROR_SUCCESS;
    PWSTR CommandLine = 0;
    PWSTR *Argv;
    int Argc;
    CHAR ProcName[64];
    FARPROC Proc;

    hr = WcaInitialize(MsiHandle, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    hr = WcaGetProperty(L"CustomActionData", &CommandLine);
    ExitOnFailure(hr, "Failed to get CommandLine");

    WcaLog(LOGMSG_STANDARD, "Initialized: \"%S\"", CommandLine);

    Argv = CommandLineToArgvW(CommandLine, &Argc);
    ExitOnNullWithLastError(Argv, hr, "Failed to CommandLineToArgvW");

    if (0 < Argc)
    {
        if (0 == WideCharToMultiByte(CP_UTF8, 0, Argv[0], -1, ProcName, sizeof ProcName, 0, 0))
            ExitWithLastError(hr, "Failed to WideCharToMultiByte");

        Proc = GetProcAddress(DllInstance, ProcName);
        ExitOnNullWithLastError(Proc, hr, "Failed to GetProcAddress");

        err = ((HRESULT (*)(int, PWSTR *))Proc)(Argc, Argv);
        ExitOnWin32Error(err, hr, "Failed to %s", ProcName);
    }
    else
    {
        hr = E_INVALIDARG;
        ExitOnFailure(hr, "Failed to get arguments");
    }

LExit:
    LocalFree(Argv);
    ReleaseStr(CommandLine);

    err = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
    return WcaFinalize(err);
}

static DWORD MakeSymlink(PWSTR Symlink, PWSTR Target);
static DWORD MakeJunction(PWSTR Junction, PWSTR Target);
static DWORD CreateJunction(PWSTR Junction, PWSTR Target);
static DWORD RemoveFile(PWSTR FileName);

DWORD InstallSymlinks(int Argc, PWSTR *Argv)
{
    /* usage: InstallSymlinks/InstallJunctions SourceDir TargetDir Name... */

    DWORD Result;
    BOOL Junctions;
    PWSTR SourceDir, TargetDir;
    WCHAR SourcePath[MAX_PATH], TargetPath[MAX_PATH];
    int SourceDirLen, TargetDirLen, Len;

    if (4 > Argc)
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Junctions = 0 == lstrcmpW(L"InstallJunctions", Argv[0]);
    SourceDir = Argv[1];
    TargetDir = Argv[2];
    SourceDirLen = lstrlenW(SourceDir);
    TargetDirLen = lstrlenW(TargetDir);

    for (int Argi = 3; Argc > Argi; Argi++)
    {
        Len = lstrlenW(Argv[Argi]);
        if (MAX_PATH < SourceDirLen + Len + 1 || MAX_PATH < TargetDirLen + Len + 1)
        {
            Result = ERROR_FILENAME_EXCED_RANGE;
            goto exit;
        }

        memcpy(SourcePath, SourceDir, SourceDirLen * sizeof(WCHAR));
        memcpy(SourcePath + SourceDirLen, Argv[Argi], Len * sizeof(WCHAR));
        SourcePath[SourceDirLen + Len] = L'\0';

        memcpy(TargetPath, TargetDir, TargetDirLen * sizeof(WCHAR));
        memcpy(TargetPath + TargetDirLen, Argv[Argi], Len * sizeof(WCHAR));
        TargetPath[TargetDirLen + Len] = L'\0';

        if (!Junctions)
            Result = MakeSymlink(SourcePath, TargetPath);
        else
            Result = MakeJunction(SourcePath, TargetPath);
#if 0
    WCHAR MessageBuf[1024];
    wsprintfW(MessageBuf, L"MakeSymlink(\"%s\", \"%s\") = %lu", SourcePath, TargetPath, Result);
    MessageBoxW(0, MessageBuf, L"TRACE", MB_OK);
#endif
        if (ERROR_SUCCESS != Result)
            goto exit;
    }

    Result = ERROR_SUCCESS;

exit:
    return Result;
}

DWORD InstallJunctions(int Argc, PWSTR *Argv)
{
    return InstallSymlinks(Argc, Argv);
}

DWORD RemoveFiles(int Argc, PWSTR *Argv)
{
    /* usage: RemoveFiles Dir Name... */

    DWORD Result;
    PWSTR Dir;
    WCHAR Path[MAX_PATH];
    int DirLen, Len;

    if (3 > Argc)
    {
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    Dir = Argv[1];
    DirLen = lstrlenW(Dir);

    for (int Argi = 2; Argc > Argi; Argi++)
    {
        Len = lstrlenW(Argv[Argi]);
        if (MAX_PATH < DirLen + Len + 1)
        {
            Result = ERROR_FILENAME_EXCED_RANGE;
            goto exit;
        }

        memcpy(Path, Dir, DirLen * sizeof(WCHAR));
        memcpy(Path + DirLen, Argv[Argi], Len * sizeof(WCHAR));
        Path[DirLen + Len] = L'\0';

        Result = RemoveFile(Path);
#if 0
    WCHAR MessageBuf[1024];
    wsprintfW(MessageBuf, L"RemoveFile(\"%s\") = %lu", Path, Result);
    MessageBoxW(0, MessageBuf, L"TRACE", MB_OK);
#endif
        if (ERROR_SUCCESS != Result)
            goto exit;
    }

    Result = ERROR_SUCCESS;

exit:
    return Result;
}

static DWORD MakeSymlink(PWSTR Symlink, PWSTR Target)
{
    DWORD Result;
    DWORD FileAttributes, Flags;

    RemoveFile(Symlink);

    FileAttributes = GetFileAttributesW(Target);
    if (INVALID_FILE_ATTRIBUTES == FileAttributes)
    {
        Result = GetLastError();
        goto exit;
    }
    Flags = 0 != (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;

    if (!CreateSymbolicLinkW(Symlink, Target, Flags))
    {
        Result = GetLastError();
        RemoveFile(Symlink);
        goto exit;
    }

    Result = ERROR_SUCCESS;

exit:
    return Result;
}

static DWORD MakeJunction(PWSTR Junction, PWSTR Target)
{
    DWORD Result;
    DWORD FileAttributes;

    RemoveFile(Junction);

    FileAttributes = GetFileAttributesW(Target);
    if (INVALID_FILE_ATTRIBUTES == FileAttributes)
    {
        Result = GetLastError();
        goto exit;
    }
    if (0 == (FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        Result = ERROR_DIRECTORY;
        goto exit;
    }

    Result = CreateJunction(Junction, Target);
    if (ERROR_SUCCESS != Result)
    {
        RemoveFile(Junction);
        goto exit;
    }

    Result = ERROR_SUCCESS;

exit:
    return Result;
}

static DWORD CreateJunction(PWSTR Junction, PWSTR Target)
{
    /*
     * The REPARSE_DATA_BUFFER definitions appear to be missing from the user mode headers.
     */
    typedef struct _REPARSE_DATA_BUFFER
    {
        ULONG ReparseTag;
        USHORT ReparseDataLength;
        USHORT Reserved;
        union
        {
            struct
            {
                USHORT SubstituteNameOffset;
                USHORT SubstituteNameLength;
                USHORT PrintNameOffset;
                USHORT PrintNameLength;
                ULONG Flags;
                WCHAR PathBuffer[1];
            } SymbolicLinkReparseBuffer;
            struct
            {
                USHORT SubstituteNameOffset;
                USHORT SubstituteNameLength;
                USHORT PrintNameOffset;
                USHORT PrintNameLength;
                WCHAR PathBuffer[1];
            } MountPointReparseBuffer;
            struct
            {
                UCHAR DataBuffer[1];
            } GenericReparseBuffer;
        } DUMMYUNIONNAME;
    } REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
    const LONG REPARSE_DATA_BUFFER_HEADER_SIZE =
        FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer);
    const DWORD FSCTL_SET_REPARSE_POINT = 0x000900a4;

    DWORD Result;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    USHORT TargetLength, ReparseDataLength;
    PREPARSE_DATA_BUFFER ReparseData = 0;
    PWSTR PathBuffer;
    DWORD Bytes;

    if (!(
        ((L'A' <= Target[0] && Target[0] <= L'Z') || (L'a' <= Target[0] && Target[0] <= L'z')) &&
        L':' == Target[1]
        ))
    {
        Result = ERROR_INVALID_NAME;
        goto exit;
    }

    Handle = CreateFileW(Junction,
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        CREATE_NEW,
        FILE_ATTRIBUTE_DIRECTORY |
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS,
        0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Result = GetLastError();
        goto exit;
    }

    TargetLength = (USHORT)lstrlenW(Target) * sizeof(WCHAR);
    ReparseDataLength = (USHORT)(
        FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer) -
        FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer)) +
        4 * sizeof(WCHAR) + 2 * (TargetLength + sizeof(WCHAR));
    ReparseData = (PREPARSE_DATA_BUFFER)
        HeapAlloc(GetProcessHeap(), 0, REPARSE_DATA_BUFFER_HEADER_SIZE + ReparseDataLength);
    if (0 == ReparseData)
    {
        Result = ERROR_NO_SYSTEM_RESOURCES;
        goto exit;
    }

    ReparseData->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    ReparseData->ReparseDataLength = ReparseDataLength;
    ReparseData->Reserved = 0;
    ReparseData->MountPointReparseBuffer.SubstituteNameOffset = 0;
    ReparseData->MountPointReparseBuffer.SubstituteNameLength =
        4 * sizeof(WCHAR) + TargetLength;
    ReparseData->MountPointReparseBuffer.PrintNameOffset =
        ReparseData->MountPointReparseBuffer.SubstituteNameLength + sizeof(WCHAR);
    ReparseData->MountPointReparseBuffer.PrintNameLength =
        TargetLength;

    PathBuffer = ReparseData->MountPointReparseBuffer.PathBuffer;
    PathBuffer[0] = L'\\';
    PathBuffer[1] = L'?';
    PathBuffer[2] = L'?';
    PathBuffer[3] = L'\\';
    memcpy(PathBuffer + 4, Target, TargetLength);
    PathBuffer[4 + TargetLength / sizeof(WCHAR)] = L'\0';

    PathBuffer = ReparseData->MountPointReparseBuffer.PathBuffer +
        (ReparseData->MountPointReparseBuffer.PrintNameOffset) / sizeof(WCHAR);
    memcpy(PathBuffer, Target, TargetLength);
    PathBuffer[TargetLength / sizeof(WCHAR)] = L'\0';

    if (!DeviceIoControl(Handle, FSCTL_SET_REPARSE_POINT,
        ReparseData, REPARSE_DATA_BUFFER_HEADER_SIZE + ReparseData->ReparseDataLength,
        0, 0,
        &Bytes, 0))
    {
        Result = GetLastError();
        goto exit;
    }

    Result = ERROR_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    if (0 != ReparseData)
        HeapFree(GetProcessHeap(), 0, ReparseData);

    return Result;
}

static DWORD RemoveFile(PWSTR FileName)
{
    DWORD Result;

    if (!RemoveDirectoryW(FileName))
    {
        Result = GetLastError();
        if (ERROR_DIRECTORY != Result)
            goto exit;

        if (!DeleteFileW(FileName))
        {
            Result = GetLastError();
            goto exit;
        }
    }

    Result = ERROR_SUCCESS;

exit:
    return Result;
}

extern "C"
BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    switch(Reason)
    {
    case DLL_PROCESS_ATTACH:
        DllInstance = Instance;
        WcaGlobalInitialize(Instance);
        break;
    case DLL_PROCESS_DETACH:
        WcaGlobalFinalize();
        break;
    }

    return TRUE;
}
