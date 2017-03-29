/**
 * @file passthrough-cpp.cpp
 *
 * @copyright 2015-2017 Bill Zissimopoulos
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

#include <winfsp/winfsp.hpp>
#include <strsafe.h>

#define PROGNAME                        "passthrough-cpp"

#define ALLOCATION_UNIT                 4096
#define FULLPATH_SIZE                   (MAX_PATH + FSP_FSCTL_TRANSACT_PATH_SIZEMAX / sizeof(WCHAR))

#define info(format, ...)               Fsp::Service::Log(EVENTLOG_INFORMATION_TYPE, format, __VA_ARGS__)
#define warn(format, ...)               Fsp::Service::Log(EVENTLOG_WARNING_TYPE, format, __VA_ARGS__)
#define fail(format, ...)               Fsp::Service::Log(EVENTLOG_ERROR_TYPE, format, __VA_ARGS__)

#define ConcatPath(FN, FP)              (0 == StringCbPrintfW(FP, sizeof FP, L"%s%s", _Path, FN))
#define HandleFromContext(FC)           ((PTFS_FILE_DESC *)FileContext->FileDesc)->Handle

class PTFS : public Fsp::FileSystem
{
public:
    PTFS();
    ~PTFS();
    NTSTATUS SetPath(PWSTR Path);

protected:
    static NTSTATUS GetFileInfoInternal(HANDLE Handle, FILE_INFO *FileInfo);
    NTSTATUS GetVolumeInfo(VOLUME_INFO *VolumeInfo);
    NTSTATUS GetSecurityByName(
        PWSTR FileName, PUINT32 PFileAttributes/* or ReparsePointIndex */,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS Create(
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
        FILE_CONTEXT *FileContext, OPEN_FILE_INFO *OpenFileInfo);
    NTSTATUS Open(
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        FILE_CONTEXT *FileContext, OPEN_FILE_INFO *OpenFileInfo);
    NTSTATUS Overwrite(
        const FILE_CONTEXT *FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
        FILE_INFO *FileInfo);
    VOID Cleanup(
        const FILE_CONTEXT *FileContext, PWSTR FileName, ULONG Flags);
    VOID Close(
        const FILE_CONTEXT *FileContext);
    NTSTATUS Read(
        const FILE_CONTEXT *FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        PULONG PBytesTransferred);
    NTSTATUS Write(
        const FILE_CONTEXT *FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred, FILE_INFO *FileInfo);
    NTSTATUS Flush(
        const FILE_CONTEXT *FileContext,
        FILE_INFO *FileInfo);
    NTSTATUS GetFileInfo(
        const FILE_CONTEXT *FileContext,
        FILE_INFO *FileInfo);
    NTSTATUS SetBasicInfo(
        const FILE_CONTEXT *FileContext, UINT32 FileAttributes,
        UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
        FILE_INFO *FileInfo);
    NTSTATUS SetFileSize(
        const FILE_CONTEXT *FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
        FILE_INFO *FileInfo);
    NTSTATUS CanDelete(
        const FILE_CONTEXT *FileContext, PWSTR FileName);
    NTSTATUS Rename(
        const FILE_CONTEXT *FileContext,
        PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists);
    NTSTATUS GetSecurity(
        const FILE_CONTEXT *FileContext,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS SetSecurity(
        const FILE_CONTEXT *FileContext,
        SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor);
    NTSTATUS ReadDirectory(
        const FILE_CONTEXT *FileContext, PWSTR Pattern, PWSTR Marker,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred);

private:
    PWSTR _Path;
};

struct PTFS_FILE_DESC
{
    PTFS_FILE_DESC() : Handle(INVALID_HANDLE_VALUE), DirBuffer()
    {
    }
    ~PTFS_FILE_DESC()
    {
        CloseHandle(Handle);
        FspFileSystemDeleteDirectoryBuffer(&DirBuffer);
    }
    HANDLE Handle;
    PVOID DirBuffer;
};

PTFS::PTFS() : _Path()
{
    SetSectorSize(ALLOCATION_UNIT);
    SetSectorsPerAllocationUnit(1);
    SetFileInfoTimeout(1000);
    SetCaseSensitiveSearch(FALSE);
    SetCasePreservedNames(TRUE);
    SetUnicodeOnDisk(TRUE);
    SetPersistentAcls(TRUE);
    SetPostCleanupWhenModifiedOnly(TRUE);
    SetPassQueryDirectoryPattern(TRUE);
}

PTFS::~PTFS()
{
    delete[] _Path;
}

NTSTATUS PTFS::SetPath(PWSTR Path)
{
    WCHAR FullPath[MAX_PATH];
    ULONG Length;
    HANDLE Handle;
    FILETIME CreationTime;
    DWORD LastError;

    Handle = CreateFileW(
        Path, FILE_READ_ATTRIBUTES, 0, 0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == Handle)
        return FspNtStatusFromWin32(GetLastError());

    Length = GetFinalPathNameByHandleW(Handle, FullPath, FULLPATH_SIZE - 1, 0);
    if (0 == Length)
    {
        LastError = GetLastError();
        CloseHandle(Handle);
        return FspNtStatusFromWin32(LastError);
    }
    if (L'\\' == FullPath[Length - 1])
        FullPath[--Length] = L'\0';

    if (!GetFileTime(Handle, &CreationTime, 0, 0))
    {
        LastError = GetLastError();
        CloseHandle(Handle);
        return FspNtStatusFromWin32(LastError);
    }

    CloseHandle(Handle);

    Length++;
    _Path = new WCHAR[Length];
    memcpy(_Path, FullPath, Length * sizeof(WCHAR));

    SetVolumeCreationTime(((PLARGE_INTEGER)&CreationTime)->QuadPart);
    SetVolumeSerialNumber(0);

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::GetFileInfoInternal(HANDLE Handle, FILE_INFO *FileInfo)
{
    BY_HANDLE_FILE_INFORMATION ByHandleFileInfo;

    if (!GetFileInformationByHandle(Handle, &ByHandleFileInfo))
        return FspNtStatusFromWin32(GetLastError());

    FileInfo->FileAttributes = ByHandleFileInfo.dwFileAttributes;
    FileInfo->ReparseTag = 0;
    FileInfo->FileSize =
        ((UINT64)ByHandleFileInfo.nFileSizeHigh << 32) | (UINT64)ByHandleFileInfo.nFileSizeLow;
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1)
        / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = ((PLARGE_INTEGER)&ByHandleFileInfo.ftCreationTime)->QuadPart;
    FileInfo->LastAccessTime = ((PLARGE_INTEGER)&ByHandleFileInfo.ftLastAccessTime)->QuadPart;
    FileInfo->LastWriteTime = ((PLARGE_INTEGER)&ByHandleFileInfo.ftLastWriteTime)->QuadPart;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::GetVolumeInfo(VOLUME_INFO *VolumeInfo)
{
    WCHAR Root[MAX_PATH];
    ULARGE_INTEGER TotalSize, FreeSize;

    if (!GetVolumePathName(_Path, Root, MAX_PATH))
        return FspNtStatusFromWin32(GetLastError());

    if (!GetDiskFreeSpaceEx(Root, 0, &TotalSize, &FreeSize))
        return FspNtStatusFromWin32(GetLastError());

    VolumeInfo->TotalSize = TotalSize.QuadPart;
    VolumeInfo->FreeSize = FreeSize.QuadPart;

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::GetSecurityByName(
    PWSTR FileName, PUINT32 PFileAttributes/* or ReparsePointIndex */,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    WCHAR FullPath[FULLPATH_SIZE];
    HANDLE Handle;
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;
    DWORD SecurityDescriptorSizeNeeded;
    NTSTATUS Result;

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    Handle = CreateFileW(FullPath,
        FILE_READ_ATTRIBUTES | READ_CONTROL, 0, 0,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == Handle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    if (0 != PFileAttributes)
    {
        if (!GetFileInformationByHandleEx(Handle,
            FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo))
        {
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        *PFileAttributes = AttributeTagInfo.FileAttributes;
    }

    if (0 != PSecurityDescriptorSize)
    {
        if (!GetKernelObjectSecurity(Handle,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            SecurityDescriptor, (DWORD)*PSecurityDescriptorSize, &SecurityDescriptorSizeNeeded))
        {
            *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
            Result = FspNtStatusFromWin32(GetLastError());
            goto exit;
        }

        *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
    }

    Result = STATUS_SUCCESS;

exit:
    if (INVALID_HANDLE_VALUE != Handle)
        CloseHandle(Handle);

    return Result;
}

NTSTATUS PTFS::Create(
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    FILE_CONTEXT *FileContext, OPEN_FILE_INFO *OpenFileInfo)
{
    WCHAR FullPath[FULLPATH_SIZE];
    SECURITY_ATTRIBUTES SecurityAttributes;
    ULONG CreateFlags;
    PTFS_FILE_DESC *FileDesc;

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    FileDesc = new PTFS_FILE_DESC;

    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
    SecurityAttributes.bInheritHandle = FALSE;

    CreateFlags = FILE_FLAG_BACKUP_SEMANTICS;
    if (CreateOptions & FILE_DELETE_ON_CLOSE)
        CreateFlags |= FILE_FLAG_DELETE_ON_CLOSE;

    if (CreateOptions & FILE_DIRECTORY_FILE)
    {
        /*
         * It is not widely known but CreateFileW can be used to create directories!
         * It requires the specification of both FILE_FLAG_BACKUP_SEMANTICS and
         * FILE_FLAG_POSIX_SEMANTICS. It also requires that FileAttributes has
         * FILE_ATTRIBUTE_DIRECTORY set.
         */
        CreateFlags |= FILE_FLAG_POSIX_SEMANTICS;
        FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    }
    else
        FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;

    if (0 == FileAttributes)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

    FileDesc->Handle = CreateFileW(FullPath,
        GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &SecurityAttributes,
        CREATE_NEW, CreateFlags | FileAttributes, 0);
    if (INVALID_HANDLE_VALUE == FileDesc->Handle)
    {
        delete FileDesc;
        return FspNtStatusFromWin32(GetLastError());
    }

    FileContext->FileDesc = FileDesc;

    return GetFileInfoInternal(FileDesc->Handle, &OpenFileInfo->FileInfo);
}

NTSTATUS PTFS::Open(
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    FILE_CONTEXT *FileContext, OPEN_FILE_INFO *OpenFileInfo)
{
    WCHAR FullPath[FULLPATH_SIZE];
    ULONG CreateFlags;
    PTFS_FILE_DESC *FileDesc;

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    FileDesc = new PTFS_FILE_DESC;

    CreateFlags = FILE_FLAG_BACKUP_SEMANTICS;
    if (CreateOptions & FILE_DELETE_ON_CLOSE)
        CreateFlags |= FILE_FLAG_DELETE_ON_CLOSE;

    FileDesc->Handle = CreateFileW(FullPath,
        GrantedAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
        OPEN_EXISTING, CreateFlags, 0);
    if (INVALID_HANDLE_VALUE == FileDesc->Handle)
    {
        delete FileDesc;
        return FspNtStatusFromWin32(GetLastError());
    }

    FileContext->FileDesc = FileDesc;

    return GetFileInfoInternal(FileDesc->Handle, &OpenFileInfo->FileInfo);
}

NTSTATUS PTFS::Overwrite(
    const FILE_CONTEXT *FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
    FILE_INFO *FileInfo)
{
    HANDLE Handle = HandleFromContext(FileContext);
    FILE_BASIC_INFO BasicInfo = { 0 };
    FILE_ALLOCATION_INFO AllocationInfo = { 0 };
    FILE_ATTRIBUTE_TAG_INFO AttributeTagInfo;

    if (ReplaceFileAttributes)
    {
        if (0 == FileAttributes)
            FileAttributes = FILE_ATTRIBUTE_NORMAL;

        BasicInfo.FileAttributes = FileAttributes;
        if (!SetFileInformationByHandle(Handle,
            FileBasicInfo, &BasicInfo, sizeof BasicInfo))
            return FspNtStatusFromWin32(GetLastError());
    }
    else if (0 != FileAttributes)
    {
        if (!GetFileInformationByHandleEx(Handle,
            FileAttributeTagInfo, &AttributeTagInfo, sizeof AttributeTagInfo))
            return FspNtStatusFromWin32(GetLastError());

        BasicInfo.FileAttributes = FileAttributes | AttributeTagInfo.FileAttributes;
        if (BasicInfo.FileAttributes ^ FileAttributes)
        {
            if (!SetFileInformationByHandle(Handle,
                FileBasicInfo, &BasicInfo, sizeof BasicInfo))
                return FspNtStatusFromWin32(GetLastError());
        }
    }

    if (!SetFileInformationByHandle(Handle,
        FileAllocationInfo, &AllocationInfo, sizeof AllocationInfo))
        return FspNtStatusFromWin32(GetLastError());

    return GetFileInfoInternal(Handle, FileInfo);
}

VOID PTFS::Cleanup(
    const FILE_CONTEXT *FileContext, PWSTR FileName, ULONG Flags)
{
    HANDLE Handle = HandleFromContext(FileContext);

    if (Flags & CleanupDelete)
    {
        CloseHandle(Handle);

        /* this will make all future uses of Handle to fail with STATUS_INVALID_HANDLE */
        HandleFromContext(FileContext) = INVALID_HANDLE_VALUE;
    }
}

VOID PTFS::Close(
    const FILE_CONTEXT *FileContext)
{
    PTFS_FILE_DESC *FileDesc = (PTFS_FILE_DESC *)FileContext->FileDesc;

    delete FileDesc;
}

NTSTATUS PTFS::Read(
    const FILE_CONTEXT *FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    HANDLE Handle = HandleFromContext(FileContext);
    OVERLAPPED Overlapped = { 0 };

    Overlapped.Offset = (DWORD)Offset;
    Overlapped.OffsetHigh = (DWORD)(Offset >> 32);

    if (!ReadFile(Handle, Buffer, Length, PBytesTransferred, &Overlapped))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::Write(
    const FILE_CONTEXT *FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FILE_INFO *FileInfo)
{
    HANDLE Handle = HandleFromContext(FileContext);
    LARGE_INTEGER FileSize;
    OVERLAPPED Overlapped = { 0 };

    if (ConstrainedIo)
    {
        if (!GetFileSizeEx(Handle, &FileSize))
            return FspNtStatusFromWin32(GetLastError());

        if (Offset >= (UINT64)FileSize.QuadPart)
            return STATUS_SUCCESS;
        if (Offset + Length > (UINT64)FileSize.QuadPart)
            Length = (ULONG)((UINT64)FileSize.QuadPart - Offset);
    }

    Overlapped.Offset = (DWORD)Offset;
    Overlapped.OffsetHigh = (DWORD)(Offset >> 32);

    if (!WriteFile(Handle, Buffer, Length, PBytesTransferred, &Overlapped))
        return FspNtStatusFromWin32(GetLastError());

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS PTFS::Flush(
    const FILE_CONTEXT *FileContext,
    FILE_INFO *FileInfo)
{
    HANDLE Handle = HandleFromContext(FileContext);

    /* we do not flush the whole volume, so just return SUCCESS */
    if (0 == Handle)
        return STATUS_SUCCESS;

    if (!FlushFileBuffers(Handle))
        return FspNtStatusFromWin32(GetLastError());

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS PTFS::GetFileInfo(
    const FILE_CONTEXT *FileContext,
    FILE_INFO *FileInfo)
{
    HANDLE Handle = HandleFromContext(FileContext);

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS PTFS::SetBasicInfo(
    const FILE_CONTEXT *FileContext, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FILE_INFO *FileInfo)
{
    HANDLE Handle = HandleFromContext(FileContext);
    FILE_BASIC_INFO BasicInfo = { 0 };

    if (INVALID_FILE_ATTRIBUTES == FileAttributes)
        FileAttributes = 0;
    else if (0 == FileAttributes)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

    BasicInfo.FileAttributes = FileAttributes;
    BasicInfo.CreationTime.QuadPart = CreationTime;
    BasicInfo.LastAccessTime.QuadPart = LastAccessTime;
    BasicInfo.LastWriteTime.QuadPart = LastWriteTime;
    //BasicInfo.ChangeTime = ChangeTime;

    if (!SetFileInformationByHandle(Handle,
        FileBasicInfo, &BasicInfo, sizeof BasicInfo))
        return FspNtStatusFromWin32(GetLastError());

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS PTFS::SetFileSize(
    const FILE_CONTEXT *FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FILE_INFO *FileInfo)
{
    HANDLE Handle = HandleFromContext(FileContext);
    FILE_ALLOCATION_INFO AllocationInfo;
    FILE_END_OF_FILE_INFO EndOfFileInfo;

    if (SetAllocationSize)
    {
        /*
         * This file system does not maintain AllocationSize, although NTFS clearly can.
         * However it must always be FileSize <= AllocationSize and NTFS will make sure
         * to truncate the FileSize if it sees an AllocationSize < FileSize.
         *
         * If OTOH a very large AllocationSize is passed, the call below will increase
         * the AllocationSize of the underlying file, although our file system does not
         * expose this fact. This AllocationSize is only temporary as NTFS will reset
         * the AllocationSize of the underlying file when it is closed.
         */

        AllocationInfo.AllocationSize.QuadPart = NewSize;

        if (!SetFileInformationByHandle(Handle,
            FileAllocationInfo, &AllocationInfo, sizeof AllocationInfo))
            return FspNtStatusFromWin32(GetLastError());
    }
    else
    {
        EndOfFileInfo.EndOfFile.QuadPart = NewSize;

        if (!SetFileInformationByHandle(Handle,
            FileEndOfFileInfo, &EndOfFileInfo, sizeof EndOfFileInfo))
            return FspNtStatusFromWin32(GetLastError());
    }

    return GetFileInfoInternal(Handle, FileInfo);
}

NTSTATUS PTFS::CanDelete(
    const FILE_CONTEXT *FileContext, PWSTR FileName)
{
    HANDLE Handle = HandleFromContext(FileContext);
    FILE_DISPOSITION_INFO DispositionInfo;

    DispositionInfo.DeleteFile = TRUE;

    if (!SetFileInformationByHandle(Handle,
        FileDispositionInfo, &DispositionInfo, sizeof DispositionInfo))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::Rename(
    const FILE_CONTEXT *FileContext,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    WCHAR FullPath[FULLPATH_SIZE], NewFullPath[FULLPATH_SIZE];

    if (!ConcatPath(FileName, FullPath))
        return STATUS_OBJECT_NAME_INVALID;

    if (!ConcatPath(NewFileName, NewFullPath))
        return STATUS_OBJECT_NAME_INVALID;

    if (!MoveFileExW(FullPath, NewFullPath, ReplaceIfExists ? MOVEFILE_REPLACE_EXISTING : 0))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::GetSecurity(
    const FILE_CONTEXT *FileContext,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    HANDLE Handle = HandleFromContext(FileContext);
    DWORD SecurityDescriptorSizeNeeded;

    if (!GetKernelObjectSecurity(Handle,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        SecurityDescriptor, (DWORD)*PSecurityDescriptorSize, &SecurityDescriptorSizeNeeded))
    {
        *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
        return FspNtStatusFromWin32(GetLastError());
    }

    *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::SetSecurity(
    const FILE_CONTEXT *FileContext,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    HANDLE Handle = HandleFromContext(FileContext);

    if (!SetKernelObjectSecurity(Handle, SecurityInformation, ModificationDescriptor))
        return FspNtStatusFromWin32(GetLastError());

    return STATUS_SUCCESS;
}

NTSTATUS PTFS::ReadDirectory(
    const FILE_CONTEXT *FileContext, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG BufferLength, PULONG PBytesTransferred)
{
    PTFS_FILE_DESC *FileDesc = (PTFS_FILE_DESC *)FileContext->FileDesc;
    HANDLE Handle = FileDesc->Handle;
    WCHAR FullPath[FULLPATH_SIZE];
    ULONG Length, PatternLength;
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
    union
    {
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_DIR_INFO D;
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.D;
    NTSTATUS DirBufferResult;

    DirBufferResult = STATUS_SUCCESS;
    if (FspFileSystemAcquireDirectoryBuffer(&FileDesc->DirBuffer, 0 == Marker, &DirBufferResult))
    {
        if (0 == Pattern)
            Pattern = L"*";
        PatternLength = (ULONG)wcslen(Pattern);

        Length = GetFinalPathNameByHandleW(Handle, FullPath, FULLPATH_SIZE - 1, 0);
        if (0 == Length)
            DirBufferResult = FspNtStatusFromWin32(GetLastError());
        else if (Length + 1 + PatternLength >= FULLPATH_SIZE)
            DirBufferResult = STATUS_OBJECT_NAME_INVALID;
        if (!NT_SUCCESS(DirBufferResult))
        {
            FspFileSystemReleaseDirectoryBuffer(&FileDesc->DirBuffer);
            return DirBufferResult;
        }

        if (L'\\' != FullPath[Length - 1])
            FullPath[Length++] = L'\\';
        memcpy(FullPath + Length, Pattern, PatternLength * sizeof(WCHAR));
        FullPath[Length + PatternLength] = L'\0';

        FindHandle = FindFirstFileW(FullPath, &FindData);
        if (INVALID_HANDLE_VALUE != FindHandle)
        {
            do
            {
                memset(DirInfo, 0, sizeof *DirInfo);
                Length = (ULONG)wcslen(FindData.cFileName);
                DirInfo->Size = (UINT16)(FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + Length * sizeof(WCHAR));
                DirInfo->FileInfo.FileAttributes = FindData.dwFileAttributes;
                DirInfo->FileInfo.ReparseTag = 0;
                DirInfo->FileInfo.FileSize =
                    ((UINT64)FindData.nFileSizeHigh << 32) | (UINT64)FindData.nFileSizeLow;
                DirInfo->FileInfo.AllocationSize = (DirInfo->FileInfo.FileSize + ALLOCATION_UNIT - 1)
                    / ALLOCATION_UNIT * ALLOCATION_UNIT;
                DirInfo->FileInfo.CreationTime = ((PLARGE_INTEGER)&FindData.ftCreationTime)->QuadPart;
                DirInfo->FileInfo.LastAccessTime = ((PLARGE_INTEGER)&FindData.ftLastAccessTime)->QuadPart;
                DirInfo->FileInfo.LastWriteTime = ((PLARGE_INTEGER)&FindData.ftLastWriteTime)->QuadPart;
                DirInfo->FileInfo.ChangeTime = DirInfo->FileInfo.LastWriteTime;
                DirInfo->FileInfo.IndexNumber = 0;
                DirInfo->FileInfo.HardLinks = 0;
                memcpy(DirInfo->FileNameBuf, FindData.cFileName, Length * sizeof(WCHAR));

                if (!FspFileSystemFillDirectoryBuffer(&FileDesc->DirBuffer, DirInfo, &DirBufferResult))
                    break;
            } while (FindNextFileW(FindHandle, &FindData));

            FindClose(FindHandle);
        }

        FspFileSystemReleaseDirectoryBuffer(&FileDesc->DirBuffer);
    }

    if (!NT_SUCCESS(DirBufferResult))
        return DirBufferResult;

    FspFileSystemReadDirectoryBuffer(&FileDesc->DirBuffer,
        Marker, Buffer, BufferLength, PBytesTransferred);

    return STATUS_SUCCESS;
}

class PTFS_SERVICE : public Fsp::Service
{
public:
    PTFS_SERVICE();

protected:
    NTSTATUS OnStart(ULONG Argc, PWSTR *Argv);
    NTSTATUS OnStop();

private:
    PTFS *_Ptfs;
};

static NTSTATUS EnableBackupRestorePrivileges(VOID)
{
    union
    {
        TOKEN_PRIVILEGES P;
        UINT8 B[sizeof(TOKEN_PRIVILEGES) + sizeof(LUID_AND_ATTRIBUTES)];
    } Privileges;
    HANDLE Token;

    Privileges.P.PrivilegeCount = 2;
    Privileges.P.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    Privileges.P.Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(0, SE_BACKUP_NAME, &Privileges.P.Privileges[0].Luid) ||
        !LookupPrivilegeValueW(0, SE_RESTORE_NAME, &Privileges.P.Privileges[1].Luid))
        return FspNtStatusFromWin32(GetLastError());

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token))
        return FspNtStatusFromWin32(GetLastError());

    if (!AdjustTokenPrivileges(Token, FALSE, &Privileges.P, 0, 0, 0))
    {
        CloseHandle(Token);

        return FspNtStatusFromWin32(GetLastError());
    }

    CloseHandle(Token);

    return STATUS_SUCCESS;
}

static ULONG wcstol_deflt(wchar_t *w, ULONG deflt)
{
    wchar_t *endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

PTFS_SERVICE::PTFS_SERVICE() : Fsp::Service(L"" PROGNAME)
{
}

NTSTATUS PTFS_SERVICE::OnStart(ULONG argc, PWSTR *argv)
{
#define argtos(v)                       if (arge > ++argp) v = *argp; else goto usage
#define argtol(v)                       if (arge > ++argp) v = wcstol_deflt(*argp, v); else goto usage

    wchar_t **argp, **arge;
    PWSTR DebugLogFile = 0;
    ULONG DebugFlags = 0;
    PWSTR VolumePrefix = 0;
    PWSTR PassThrough = 0;
    PWSTR MountPoint = 0;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
    WCHAR PassThroughBuf[MAX_PATH];
    PTFS *Ptfs = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'd':
            argtol(DebugFlags);
            break;
        case L'D':
            argtos(DebugLogFile);
            break;
        case L'm':
            argtos(MountPoint);
            break;
        case L'p':
            argtos(PassThrough);
            break;
        case L'u':
            argtos(VolumePrefix);
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (0 == PassThrough && 0 != VolumePrefix)
    {
        PWSTR P;

        P = wcschr(VolumePrefix, L'\\');
        if (0 != P && L'\\' != P[1])
        {
            P = wcschr(P + 1, L'\\');
            if (0 != P &&
                (
                (L'A' <= P[1] && P[1] <= L'Z') ||
                (L'a' <= P[1] && P[1] <= L'z')
                ) &&
                L'$' == P[2])
            {
                StringCbPrintf(PassThroughBuf, sizeof PassThroughBuf, L"%c:%s", P[1], P + 3);
                PassThrough = PassThroughBuf;
            }
        }
    }

    if (0 == PassThrough || 0 == MountPoint)
        goto usage;

    EnableBackupRestorePrivileges();

    if (0 != DebugLogFile)
    {
        if (0 == wcscmp(L"-", DebugLogFile))
            DebugLogHandle = GetStdHandle(STD_ERROR_HANDLE);
        else
            DebugLogHandle = CreateFileW(
                DebugLogFile,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == DebugLogHandle)
        {
            fail(L"cannot open debug log file");
            goto usage;
        }

        FspDebugLogSetHandle(DebugLogHandle);
    }

    Ptfs = new PTFS;

    Ptfs->SetPrefix(VolumePrefix);

    Result = Ptfs->SetPath(PassThrough);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create file system");
        goto exit;
    }

    Result = Ptfs->Mount(MountPoint, 0, FALSE, DebugFlags);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot mount file system");
        goto exit;
    }

    MountPoint = Ptfs->MountPoint();

    info(L"%s%s%s -p %s -m %s",
        L"" PROGNAME,
        0 != VolumePrefix && L'\0' != VolumePrefix[0] ? L" -u " : L"",
            0 != VolumePrefix && L'\0' != VolumePrefix[0] ? VolumePrefix : L"",
        PassThrough,
        MountPoint);

    _Ptfs = Ptfs;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != Ptfs)
        delete Ptfs;

    return Result;

usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -d DebugFlags       [-1: enable all debug logs]\n"
        "    -D DebugLogFile     [file path; use - for stderr]\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -p Directory        [directory to expose as pass through file system]\n"
        "    -m MountPoint       [X:|*|directory]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;

#undef argtos
#undef argtol
}

NTSTATUS PTFS_SERVICE::OnStop()
{
    _Ptfs->Unmount();
    delete _Ptfs;
    _Ptfs = 0;

    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    return PTFS_SERVICE().Run();
}
