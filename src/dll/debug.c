/**
 * @file dll/debug.c
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

#include <dll/library.h>
#include <sddl.h>
#include <stdarg.h>

FSP_API VOID FspDebugLog(const char *format, ...)
{
    char buf[1024];
        /* DbgPrint has a 512 byte limit, but wvsprintf is only safe with a 1024 byte buffer */
    va_list ap;
    va_start(ap, format);
    wvsprintfA(buf, format, ap);
    va_end(ap);
    buf[sizeof buf - 1] = '\0';
    OutputDebugStringA(buf);
}

FSP_API VOID FspDebugLogSD(const char *format, PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    char *Sddl;

    if (0 == SecurityDescriptor)
        FspDebugLog(format, "null security descriptor");
    else if (ConvertSecurityDescriptorToStringSecurityDescriptorA(SecurityDescriptor,
        SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
        DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
        &Sddl, 0))
    {
        FspDebugLog(format, Sddl);
        LocalFree(Sddl);
    }
    else
        FspDebugLog(format, "invalid security descriptor");
}

FSP_API VOID FspDebugLogFT(const char *format, PFILETIME FileTime)
{
    SYSTEMTIME SystemTime;
    char buf[32];

    if (FileTimeToSystemTime(FileTime, &SystemTime))
    {
        wsprintfA(buf, "%04hu-%02hu-%02huT%02hu:%02hu:%02hu.%03huZ",
            SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
            SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
            SystemTime.wMilliseconds);
        FspDebugLog(format, buf);
    }
    else
        FspDebugLog(format, "invalid file time");
}

#define MAKE_UINT32_PAIR(v)             (((v) >> 32) & 0xffffffff), ((v) & 0xffffffff)

static const char *FspDebugLogDispositionString(UINT32 CreateOptions)
{
    switch ((CreateOptions >> 24) & 0xff)
    {
    case FILE_CREATE:
        return "FILE_CREATE";
    case FILE_OPEN:
        return "FILE_OPEN";
    case FILE_OPEN_IF:
        return "FILE_OPEN_IF";
    case FILE_OVERWRITE:
        return "FILE_OVERWRITE";
    case FILE_SUPERSEDE:
        return "FILE_SUPERSEDE";
    case FILE_OVERWRITE_IF:
        return "FILE_OVERWRITE_IF";
    default:
        return "INVALID";
    }
}

static const char *FspDebugLogUserContextString(UINT64 UserContext, UINT64 UserContext2, char *Buf)
{
    wsprintfA(Buf, 0 == UserContext2 ? "%p" : "%p:%p", UserContext, UserContext2);

    return Buf;
}

static const char *FspDebugLogFileTimeString(UINT64 FileTime, char *Buf)
{
    SYSTEMTIME SystemTime;

    if (0 == FileTime)
        lstrcpyA(Buf, "0");
    else if (FileTimeToSystemTime((PFILETIME)&FileTime, &SystemTime))
    {
        wsprintfA(Buf, "%04hu-%02hu-%02huT%02hu:%02hu:%02hu.%03huZ",
            SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
            SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
            SystemTime.wMilliseconds);
    }
    else
        lstrcpyA(Buf, "INVALID");

    return Buf;
}

static const char *FspDebugLogFileInfoString(FSP_FSCTL_FILE_INFO *FileInfo, char *Buf)
{
    char CreationTimeBuf[32], LastAccessTimeBuf[32], LastWriteTimeBuf[32], ChangeTimeBuf[32];

    wsprintfA(Buf,
        "{"
        "FileAttributes=%lx, "
        "ReparseTag=%lx, "
        "AllocationSize=%lx:%lx, "
        "FileSize=%lx:%lx, "
        "CreationTime=%s, "
        "LastAccessTime=%s, "
        "LastWriteTime=%s, "
        "ChangeTime=%s, "
        "IndexNumber=%lx:%lx"
        "}",
        FileInfo->FileAttributes,
        FileInfo->ReparseTag,
        MAKE_UINT32_PAIR(FileInfo->AllocationSize),
        MAKE_UINT32_PAIR(FileInfo->FileSize),
        FspDebugLogFileTimeString(FileInfo->CreationTime, CreationTimeBuf),
        FspDebugLogFileTimeString(FileInfo->LastAccessTime, LastAccessTimeBuf),
        FspDebugLogFileTimeString(FileInfo->LastWriteTime, LastWriteTimeBuf),
        FspDebugLogFileTimeString(FileInfo->ChangeTime, ChangeTimeBuf),
        MAKE_UINT32_PAIR(FileInfo->IndexNumber));

    return Buf;
}

static const char *FspDebugLogVolumeInfoString(FSP_FSCTL_VOLUME_INFO *VolumeInfo, char *Buf)
{
    wsprintfA(Buf,
        "{"
        "TotalSize=%lx:%lx, "
        "FreeSize=%lx:%lx, "
        "VolumeLabel=\"%.32S\""
        "}",
        MAKE_UINT32_PAIR(VolumeInfo->TotalSize),
        MAKE_UINT32_PAIR(VolumeInfo->FreeSize),
        &VolumeInfo->VolumeLabel);

    return Buf;
}

static VOID FspDebugLogRequestVoid(FSP_FSCTL_TRANSACT_REQ *Request, const char *Name)
{
    FspDebugLog("%S[TID=%ld]: %p: >>%s\n",
        FspDiagIdent(), GetCurrentThreadId(), Request->Hint, Name);
}

static VOID FspDebugLogResponseStatus(FSP_FSCTL_TRANSACT_RSP *Response, const char *Name)
{
    FspDebugLog("%S[TID=%ld]: %p: <<%s IoStatus=%lx[%ld]\n",
        FspDiagIdent(), GetCurrentThreadId(), Response->Hint, Name,
        Response->IoStatus.Status, Response->IoStatus.Information);
}

FSP_API VOID FspDebugLogRequest(FSP_FSCTL_TRANSACT_REQ *Request)
{
    char UserContextBuf[40];
    char CreationTimeBuf[32], LastAccessTimeBuf[32], LastWriteTimeBuf[32];
    char *Sddl = 0;

    switch (Request->Kind)
    {
    case FspFsctlTransactReservedKind:
        FspDebugLogRequestVoid(Request, "RESERVED");
        break;
    case FspFsctlTransactCreateKind:
        if (0 != Request->Req.Create.SecurityDescriptor.Offset)
            ConvertSecurityDescriptorToStringSecurityDescriptorA(
                Request->Buffer + Request->Req.Create.SecurityDescriptor.Offset,
                SDDL_REVISION_1,
                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                &Sddl, 0);
        FspDebugLog("%S[TID=%ld]: %p: >>Create [%c%c%c%c] \"%S\", "
            "%s, CreateOptions=%lx, FileAttributes=%lx, Security=%s%s%s, "
            "AllocationSize=%lx:%lx, AccessToken=%lx, DesiredAccess=%lx, ShareAccess=%lx\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->Req.Create.UserMode ? 'U' : 'K',
            Request->Req.Create.HasTraversePrivilege ? 'T' : '-',
            Request->Req.Create.OpenTargetDirectory ? 'D' : '-',
            Request->Req.Create.CaseSensitive ? 'C' : '-',
            (PWSTR)Request->Buffer,
            FspDebugLogDispositionString(Request->Req.Create.CreateOptions),
            Request->Req.Create.CreateOptions & 0xffffff,
            Request->Req.Create.FileAttributes,
            Sddl ? "\"" : "",
            Sddl ? Sddl : "NULL",
            Sddl ? "\"" : "",
            MAKE_UINT32_PAIR(Request->Req.Create.AllocationSize),
            Request->Req.Create.AccessToken,
            Request->Req.Create.DesiredAccess,
            Request->Req.Create.ShareAccess);
        LocalFree(Sddl);
        break;
    case FspFsctlTransactOverwriteKind:
        FspDebugLog("%S[TID=%ld]: %p: >>Overwrite%s %s%S%s%s, "
            "FileAttributes=%lx\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->Req.Overwrite.Supersede ? " [Supersede]" : "",
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.Overwrite.UserContext, Request->Req.Overwrite.UserContext2,
                UserContextBuf),
            Request->Req.Overwrite.FileAttributes);
        break;
    case FspFsctlTransactCleanupKind:
        FspDebugLog("%S[TID=%ld]: %p: >>Cleanup%s %s%S%s%s\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->Req.Cleanup.Delete ? " [Delete]" : "",
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.Cleanup.UserContext, Request->Req.Cleanup.UserContext2,
                UserContextBuf));
        break;
    case FspFsctlTransactCloseKind:
        FspDebugLog("%S[TID=%ld]: %p: >>Close %s%S%s%s\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.Close.UserContext, Request->Req.Close.UserContext2,
                UserContextBuf));
        break;
    case FspFsctlTransactReadKind:
        FspDebugLog("%S[TID=%ld]: %p: >>Read %s%S%s%s, "
            "Address=%p, Offset=%lx:%lx, Length=%ld, Key=%lx\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.Read.UserContext, Request->Req.Read.UserContext2,
                UserContextBuf),
            Request->Req.Read.Address,
            MAKE_UINT32_PAIR(Request->Req.Read.Offset),
            Request->Req.Read.Length,
            Request->Req.Read.Key);
        break;
    case FspFsctlTransactWriteKind:
        FspDebugLog("%S[TID=%ld]: %p: >>Write%s %s%S%s%s, "
            "Address=%p, Offset=%lx:%lx, Length=%ld, Key=%lx\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->Req.Write.ConstrainedIo ? " [C]" : "",
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.Write.UserContext, Request->Req.Write.UserContext2,
                UserContextBuf),
            Request->Req.Write.Address,
            MAKE_UINT32_PAIR(Request->Req.Write.Offset),
            Request->Req.Write.Length,
            Request->Req.Write.Key);
        break;
    case FspFsctlTransactQueryInformationKind:
        FspDebugLog("%S[TID=%ld]: %p: >>QueryInformation %s%S%s%s\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.QueryInformation.UserContext, Request->Req.QueryInformation.UserContext2,
                UserContextBuf));
        break;
    case FspFsctlTransactSetInformationKind:
        switch (Request->Req.SetInformation.FileInformationClass)
        {
        case 4/*FileBasicInformation*/:
            FspDebugLog("%S[TID=%ld]: %p: >>SetInformation [Basic] %s%S%s%s, "
                "FileAttributes=%lx, CreationTime=%s, LastAccessTime=%s, LastWriteTime=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
                Request->FileName.Size ? "\"" : "",
                Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
                Request->FileName.Size ? "\", " : "",
                FspDebugLogUserContextString(
                    Request->Req.SetInformation.UserContext, Request->Req.SetInformation.UserContext2,
                    UserContextBuf),
                Request->Req.SetInformation.Info.Basic.FileAttributes,
                FspDebugLogFileTimeString(Request->Req.SetInformation.Info.Basic.CreationTime,
                    CreationTimeBuf),
                FspDebugLogFileTimeString(Request->Req.SetInformation.Info.Basic.LastAccessTime,
                    LastAccessTimeBuf),
                FspDebugLogFileTimeString(Request->Req.SetInformation.Info.Basic.LastWriteTime,
                    LastWriteTimeBuf));
            break;
        case 19/*FileAllocationInformation*/:
            FspDebugLog("%S[TID=%ld]: %p: >>SetInformation [Allocation] %s%S%s%s, "
                "AllocationSize=%lx:%lx\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
                Request->FileName.Size ? "\"" : "",
                Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
                Request->FileName.Size ? "\", " : "",
                FspDebugLogUserContextString(
                    Request->Req.SetInformation.UserContext, Request->Req.SetInformation.UserContext2,
                    UserContextBuf),
                MAKE_UINT32_PAIR(Request->Req.SetInformation.Info.Allocation.AllocationSize));
            break;
        case 20/*FileEndOfFileInformation*/:
            FspDebugLog("%S[TID=%ld]: %p: >>SetInformation [EndOfFile] %s%S%s%s, "
                "FileSize = %lx:%lx\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
                Request->FileName.Size ? "\"" : "",
                Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
                Request->FileName.Size ? "\", " : "",
                FspDebugLogUserContextString(
                    Request->Req.SetInformation.UserContext, Request->Req.SetInformation.UserContext2,
                    UserContextBuf),
                MAKE_UINT32_PAIR(Request->Req.SetInformation.Info.EndOfFile.FileSize));
            break;
        case 13/*FileDispositionInformation*/:
            FspDebugLog("%S[TID=%ld]: %p: >>SetInformation [Disposition] %s%S%s%s, "
                "%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
                Request->FileName.Size ? "\"" : "",
                Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
                Request->FileName.Size ? "\", " : "",
                FspDebugLogUserContextString(
                    Request->Req.SetInformation.UserContext, Request->Req.SetInformation.UserContext2,
                    UserContextBuf),
                Request->Req.SetInformation.Info.Disposition.Delete ? "Delete" : "Undelete");
            break;
        case 10/*FileRenameInformation*/:
            FspDebugLog("%S[TID=%ld]: %p: >>SetInformation [Rename] %s%S%s%s, "
                "NewFileName=\"%S\"%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
                Request->FileName.Size ? "\"" : "",
                Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
                Request->FileName.Size ? "\", " : "",
                FspDebugLogUserContextString(
                    Request->Req.SetInformation.UserContext, Request->Req.SetInformation.UserContext2,
                    UserContextBuf),
                (PWSTR)(Request->Buffer + Request->Req.SetInformation.Info.Rename.NewFileName.Offset),
                Request->Req.SetInformation.Info.Rename.ReplaceIfExists ? " ReplaceIfExists" : "");
            break;
        default:
            FspDebugLog("%S[TID=%ld]: %p: >>SetInformation [INVALID] %s%S%s%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
                Request->FileName.Size ? "\"" : "",
                Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
                Request->FileName.Size ? "\", " : "",
                FspDebugLogUserContextString(
                    Request->Req.SetInformation.UserContext, Request->Req.SetInformation.UserContext2,
                    UserContextBuf));
        }
        break;
    case FspFsctlTransactQueryEaKind:
        FspDebugLogRequestVoid(Request, "QUERYEA");
        break;
    case FspFsctlTransactSetEaKind:
        FspDebugLogRequestVoid(Request, "SETEA");
        break;
    case FspFsctlTransactFlushBuffersKind:
        FspDebugLog("%S[TID=%ld]: %p: >>FlushBuffers %s%S%s%s\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.FlushBuffers.UserContext, Request->Req.FlushBuffers.UserContext2,
                UserContextBuf));
        break;
    case FspFsctlTransactQueryVolumeInformationKind:
        FspDebugLogRequestVoid(Request, "QueryVolumeInformation");
        break;
    case FspFsctlTransactSetVolumeInformationKind:
        switch (Request->Req.SetVolumeInformation.FsInformationClass)
        {
        case 2/*FileFsLabelInformation*/:
            FspDebugLog("%S[TID=%ld]: %p: >>SetVolumeInformation [FsLabel] "
                "Label=\"%S\"\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
                (PWSTR)Request->Buffer);
            break;
        default:
            FspDebugLog("%S[TID=%ld]: %p: >>SetVolumeInformation [INVALID]\n",
                FspDiagIdent(), GetCurrentThreadId(), Request->Hint);
            break;
        }
        break;
    case FspFsctlTransactQueryDirectoryKind:
        FspDebugLog("%S[TID=%ld]: %p: >>QueryDirectory %s%S%s%s, "
            "Address=%p, Offset=%lx:%lx, Length=%ld, Pattern=%s%S%s\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.QueryDirectory.UserContext, Request->Req.QueryDirectory.UserContext2,
                UserContextBuf),
            Request->Req.QueryDirectory.Address,
            MAKE_UINT32_PAIR(Request->Req.QueryDirectory.Offset),
            Request->Req.QueryDirectory.Length,
            Request->Req.QueryDirectory.Pattern.Size ? "\"" : "",
            Request->Req.QueryDirectory.Pattern.Size ?
                (PWSTR)(Request->Buffer + Request->Req.QueryDirectory.Pattern.Offset) : L"NULL",
            Request->Req.QueryDirectory.Pattern.Size ? "\"" : "");
        break;
    case FspFsctlTransactFileSystemControlKind:
        FspDebugLogRequestVoid(Request, "FILESYSTEMCONTROL");
        break;
    case FspFsctlTransactDeviceControlKind:
        FspDebugLogRequestVoid(Request, "DEVICECONTROL");
        break;
    case FspFsctlTransactShutdownKind:
        FspDebugLogRequestVoid(Request, "SHUTDOWN");
        break;
    case FspFsctlTransactLockControlKind:
        FspDebugLogRequestVoid(Request, "LOCKCONTROL");
        break;
    case FspFsctlTransactQuerySecurityKind:
        FspDebugLog("%S[TID=%ld]: %p: >>QuerySecurity %s%S%s%s\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.QuerySecurity.UserContext, Request->Req.QuerySecurity.UserContext2,
                UserContextBuf));
        break;
    case FspFsctlTransactSetSecurityKind:
        if (0 != Request->Req.SetSecurity.SecurityDescriptor.Size)
            ConvertSecurityDescriptorToStringSecurityDescriptorA(
                Request->Buffer + Request->Req.SetSecurity.SecurityDescriptor.Offset,
                SDDL_REVISION_1,
                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                &Sddl, 0);
        FspDebugLog("%S[TID=%ld]: %p: >>SetSecurity %s%S%s%s, "
            "SecurityInformation=%lx, AccessToken=%lx, Security=%s%s%s\n",
            FspDiagIdent(), GetCurrentThreadId(), Request->Hint,
            Request->FileName.Size ? "\"" : "",
            Request->FileName.Size ? (PWSTR)Request->Buffer : L"",
            Request->FileName.Size ? "\", " : "",
            FspDebugLogUserContextString(
                Request->Req.SetSecurity.UserContext, Request->Req.SetSecurity.UserContext2,
                UserContextBuf),
            Request->Req.SetSecurity.SecurityInformation,
            Request->Req.SetSecurity.AccessToken,
            Sddl ? "\"" : "",
            Sddl ? Sddl : "NULL",
            Sddl ? "\"" : "");
        LocalFree(Sddl);
        break;
    default:
        FspDebugLogRequestVoid(Request, "INVALID");
        break;
    }
}

FSP_API VOID FspDebugLogResponse(FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (STATUS_PENDING == Response->IoStatus.Status)
        return;

    char UserContextBuf[40];
    char InfoBuf[256];
    char *Sddl = 0;

    switch (Response->Kind)
    {
    case FspFsctlTransactReservedKind:
        FspDebugLogResponseStatus(Response, "RESERVED");
        break;
    case FspFsctlTransactCreateKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "Create");
        else if (STATUS_REPARSE == Response->IoStatus.Status)
            FspDebugLog("%S[TID=%ld]: %p: <<Create IoStatus=%lx[%ld] "
                "Reparse.FileName=\"%S\"\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                (PWSTR)(Response->Buffer + Response->Rsp.Create.Reparse.FileName.Offset));
        else
            FspDebugLog("%S[TID=%ld]: %p: <<Create IoStatus=%lx[%ld] "
                "UserContext=%s, GrantedAccess=%lx, FileInfo=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                FspDebugLogUserContextString(
                    Response->Rsp.Create.Opened.UserContext, Response->Rsp.Create.Opened.UserContext2,
                    UserContextBuf),
                Response->Rsp.Create.Opened.GrantedAccess,
                FspDebugLogFileInfoString(&Response->Rsp.Create.Opened.FileInfo, InfoBuf));
        break;
    case FspFsctlTransactOverwriteKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "Overwrite");
        else
            FspDebugLog("%S[TID=%ld]: %p: <<Overwrite IoStatus=%lx[%ld] "
                "FileInfo=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                FspDebugLogFileInfoString(&Response->Rsp.Overwrite.FileInfo, InfoBuf));
        break;
    case FspFsctlTransactCleanupKind:
        FspDebugLogResponseStatus(Response, "Cleanup");
        break;
    case FspFsctlTransactCloseKind:
        FspDebugLogResponseStatus(Response, "Close");
        break;
    case FspFsctlTransactReadKind:
        FspDebugLogResponseStatus(Response, "Read");
        break;
    case FspFsctlTransactWriteKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "Write");
        else
            FspDebugLog("%S[TID=%ld]: %p: <<Write IoStatus=%lx[%ld] "
                "FileInfo=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                FspDebugLogFileInfoString(&Response->Rsp.Write.FileInfo, InfoBuf));
        break;
    case FspFsctlTransactQueryInformationKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "QueryInformation");
        else
            FspDebugLog("%S[TID=%ld]: %p: <<QueryInformation IoStatus=%lx[%ld] "
                "FileInfo=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                FspDebugLogFileInfoString(&Response->Rsp.QueryInformation.FileInfo, InfoBuf));
        break;
    case FspFsctlTransactSetInformationKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "SetInformation");
        else
            FspDebugLog("%S[TID=%ld]: %p: <<SetInformation IoStatus=%lx[%ld] "
                "FileInfo=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                FspDebugLogFileInfoString(&Response->Rsp.SetInformation.FileInfo, InfoBuf));
        break;
    case FspFsctlTransactQueryEaKind:
        FspDebugLogResponseStatus(Response, "QUERYEA");
        break;
    case FspFsctlTransactSetEaKind:
        FspDebugLogResponseStatus(Response, "SETEA");
        break;
    case FspFsctlTransactFlushBuffersKind:
        FspDebugLogResponseStatus(Response, "FlushBuffers");
        break;
    case FspFsctlTransactQueryVolumeInformationKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "QueryVolumeInformation");
        else
            FspDebugLog("%S[TID=%ld]: %p: <<QueryVolumeInformation IoStatus=%lx[%ld] "
                "VolumeInfo=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                FspDebugLogVolumeInfoString(&Response->Rsp.QueryVolumeInformation.VolumeInfo, InfoBuf));
        break;
    case FspFsctlTransactSetVolumeInformationKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "SetVolumeInformation");
        else
            FspDebugLog("%S[TID=%ld]: %p: <<SetVolumeInformation IoStatus=%lx[%ld] "
                "VolumeInfo=%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                FspDebugLogVolumeInfoString(&Response->Rsp.SetVolumeInformation.VolumeInfo, InfoBuf));
        break;
    case FspFsctlTransactQueryDirectoryKind:
        FspDebugLogResponseStatus(Response, "QueryDirectory");
        break;
    case FspFsctlTransactFileSystemControlKind:
        FspDebugLogResponseStatus(Response, "FILESYSTEMCONTROL");
        break;
    case FspFsctlTransactDeviceControlKind:
        FspDebugLogResponseStatus(Response, "DEVICECONTROL");
        break;
    case FspFsctlTransactShutdownKind:
        FspDebugLogResponseStatus(Response, "SHUTDOWN");
        break;
    case FspFsctlTransactLockControlKind:
        FspDebugLogResponseStatus(Response, "LOCKCONTROL");
        break;
    case FspFsctlTransactQuerySecurityKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "QuerySecurity");
        else
        {
            if (0 != Response->Rsp.QuerySecurity.SecurityDescriptor.Size)
                ConvertSecurityDescriptorToStringSecurityDescriptorA(
                    Response->Buffer + Response->Rsp.QuerySecurity.SecurityDescriptor.Offset,
                    SDDL_REVISION_1,
                    OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                    DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                    &Sddl, 0);
            FspDebugLog("%S[TID=%ld]: %p: <<QuerySecurity IoStatus=%lx[%ld] "
                "Security=%s%s%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                Sddl ? "\"" : "",
                Sddl ? Sddl : "NULL",
                Sddl ? "\"" : "");
            LocalFree(Sddl);
        }
        break;
    case FspFsctlTransactSetSecurityKind:
        if (!NT_SUCCESS(Response->IoStatus.Status))
            FspDebugLogResponseStatus(Response, "SetSecurity");
        else
        {
            if (0 != Response->Rsp.SetSecurity.SecurityDescriptor.Size)
                ConvertSecurityDescriptorToStringSecurityDescriptorA(
                    Response->Buffer + Response->Rsp.SetSecurity.SecurityDescriptor.Offset,
                    SDDL_REVISION_1,
                    OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                    DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                    &Sddl, 0);
            FspDebugLog("%S[TID=%ld]: %p: <<SetSecurity IoStatus=%lx[%ld] "
                "Security=%s%s%s\n",
                FspDiagIdent(), GetCurrentThreadId(), Response->Hint,
                Response->IoStatus.Status, Response->IoStatus.Information,
                Sddl ? "\"" : "",
                Sddl ? Sddl : "NULL",
                Sddl ? "\"" : "");
            LocalFree(Sddl);
        }
        break;
    default:
        FspDebugLogResponseStatus(Response, "INVALID");
        break;
    }
}
