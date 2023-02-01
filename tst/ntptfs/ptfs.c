/**
 * @file ptfs.c
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

#include "ptfs.h"

#define FileSystemContext               ((PTFS *)(FileSystem)->UserContext)
#define FileContextHandle               (((FILE_CONTEXT *)(FileContext))->Handle)
#define FileContextIsDirectory          (((FILE_CONTEXT *)(FileContext))->IsDirectory)
#define FileContextDirFileSize          (((FILE_CONTEXT *)(FileContext))->DirFileSize)
#define FileContextDirBuffer            (&((FILE_CONTEXT *)(FileContext))->DirBuffer)

typedef struct
{
    HANDLE Handle;
    BOOLEAN IsDirectory;
    ULONG DirFileSize;
    PVOID DirBuffer;
} FILE_CONTEXT;

static NTSTATUS GetReparsePointByName(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize);
static NTSTATUS SetDisposition(
    HANDLE Handle, BOOLEAN DeleteFile);

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    PTFS *Ptfs = FileSystemContext;
    IO_STATUS_BLOCK Iosb;
    FILE_FS_SIZE_INFORMATION FsSizeInfo;
    NTSTATUS Result;

    Result = NtQueryVolumeInformationFile(
        Ptfs->RootHandle,
        &Iosb,
        &FsSizeInfo,
        sizeof FsSizeInfo,
        3/*FileFsSizeInformation*/);
    if (!NT_SUCCESS(Result))
        goto exit;

    VolumeInfo->TotalSize = FsSizeInfo.TotalAllocationUnits.QuadPart * Ptfs->AllocationUnit;
    VolumeInfo->FreeSize = FsSizeInfo.AvailableAllocationUnits.QuadPart * Ptfs->AllocationUnit;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS SetVolumeLabel_(FSP_FILE_SYSTEM *FileSystem,
    PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes/* or ReparsePointIndex */,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    PTFS *Ptfs = FileSystemContext;
    HANDLE Handle = 0;
    IO_STATUS_BLOCK Iosb;
    FILE_ATTRIBUTE_TAG_INFORMATION FileAttrInfo;
    ULONG SecurityDescriptorSizeNeeded;
    NTSTATUS Result;

    if (0 != (FILE_SUPPORTS_REPARSE_POINTS & Ptfs->FsAttributes) &&
        FspFileSystemFindReparsePoint(FileSystem, GetReparsePointByName, 0, FileName, PFileAttributes))
    {
        Result = STATUS_REPARSE;
        goto exit;
    }

    Result = LfsOpenFile(
        &Handle,
        READ_CONTROL |
            (Ptfs->HasSecurityPrivilege ? ACCESS_SYSTEM_SECURITY : 0),
        Ptfs->RootHandle,
        FileName,
        FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
    if (!NT_SUCCESS(Result))
        goto exit;

    if (0 != PFileAttributes)
    {
        Result = NtQueryInformationFile(
            Handle,
            &Iosb,
            &FileAttrInfo,
            sizeof FileAttrInfo,
            35/*FileAttributeTagInformation*/);
        if (!NT_SUCCESS(Result))
            goto exit;

        *PFileAttributes = FileAttrInfo.FileAttributes;

        /* cache FileAttributes for Open */
        FspFileSystemGetOperationContext()->Response->Rsp.Create.Opened.FileInfo.FileAttributes =
            FileAttrInfo.FileAttributes;
    }

    if (0 != PSecurityDescriptorSize)
    {
        Result = NtQuerySecurityObject(
            Handle,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
                (Ptfs->HasSecurityPrivilege ? SACL_SECURITY_INFORMATION : 0),
            SecurityDescriptor,
            (ULONG)*PSecurityDescriptorSize,
            &SecurityDescriptorSizeNeeded);
        if (!NT_SUCCESS(Result))
            goto exit;

        *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;
    }

    Result = STATUS_SUCCESS;

exit:
    if (0 != Handle)
        NtClose(Handle);

    return Result;
}

static NTSTATUS CreateEx(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    PVOID ExtraBuffer, ULONG ExtraLength, BOOLEAN ExtraBufferIsReparsePoint,
    PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
    PTFS *Ptfs = FileSystemContext;
    FILE_CONTEXT *FileContext = 0;
    HANDLE Handle = 0;
    BOOLEAN IsDirectory = !!(CreateOptions & FILE_DIRECTORY_FILE);
    UINT32 MaximumAccess = IsDirectory ? GrantedAccess : MAXIMUM_ALLOWED;
    NTSTATUS Result;

    CreateOptions &=
        FILE_DIRECTORY_FILE |
        FILE_NON_DIRECTORY_FILE |
        FILE_NO_EA_KNOWLEDGE;

    /* WORKAROUND:
     *
     * WOW64 appears to have a bug in some versions of the OS (seen on Win10 1909 and
     * Server 2012 R2), where NtQueryDirectoryFile may produce garbage if called on a
     * directory that has been opened without FILE_SYNCHRONOUS_IO_NONALERT. (Garbage:
     * after a STATUS_PENDING has been waited, Iosb.Information reports bytes transferred
     * but the buffer does not get filled).
     *
     * So make sure to always open directories in a synchronous manner.
     */
    if (IsDirectory)
    {
        MaximumAccess |= SYNCHRONIZE;
        //GrantedAccess |= SYNCHRONIZE;
        CreateOptions |= FILE_SYNCHRONOUS_IO_NONALERT;
    }

    Result = LfsCreateFile(
        &Handle,
        MaximumAccess |
            (Ptfs->HasSecurityPrivilege ? ACCESS_SYSTEM_SECURITY : 0),
        Ptfs->RootHandle,
        FileName,
        SecurityDescriptor,
        0 != AllocationSize ? (PLARGE_INTEGER)&AllocationSize : 0,
        0 != FileAttributes ? FileAttributes : FILE_ATTRIBUTE_NORMAL,
        FILE_CREATE,
        FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT | CreateOptions,
        ExtraBufferIsReparsePoint ? 0 : ExtraBuffer,
        ExtraBufferIsReparsePoint ? 0 : ExtraLength);
    if (!NT_SUCCESS(Result) && MAXIMUM_ALLOWED == MaximumAccess)
        switch (Result)
        {
        case STATUS_INVALID_PARAMETER:
            Result = LfsCreateFile(
                &Handle,
                GrantedAccess |
                    (Ptfs->HasSecurityPrivilege ? ACCESS_SYSTEM_SECURITY : 0),
                Ptfs->RootHandle,
                FileName,
                SecurityDescriptor,
                0 != AllocationSize ? (PLARGE_INTEGER)&AllocationSize : 0,
                0 != FileAttributes ? FileAttributes : FILE_ATTRIBUTE_NORMAL,
                FILE_CREATE,
                FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT | CreateOptions,
                ExtraBufferIsReparsePoint ? 0 : ExtraBuffer,
                ExtraBufferIsReparsePoint ? 0 : ExtraLength);
            break;
        }
    if (!NT_SUCCESS(Result))
        goto exit;

    if (ExtraBufferIsReparsePoint)
    {
        /* this can happen on a WSL mount */
        Result = LfsFsControlFile(
            Handle,
            FSCTL_SET_REPARSE_POINT,
            ExtraBuffer,
            (ULONG)ExtraLength,
            0,
            0,
            &ExtraLength);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    Result = LfsGetFileInfo(Handle, Ptfs->RootPrefixLength, FileInfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    FileContext = malloc(sizeof *FileContext);
    if (0 == FileContext)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    memset(FileContext, 0, sizeof *FileContext);
    FileContext->Handle = Handle;
    FileContext->IsDirectory = IsDirectory;
    FileContext->DirFileSize = (ULONG)FileInfo->FileSize;
    *PFileContext = FileContext;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (0 != Handle)
            NtClose(Handle);

        free(FileContext);
    }

    return Result;
}

static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    PVOID *PFileContext, FSP_FSCTL_FILE_INFO *FileInfo)
{
    PTFS *Ptfs = FileSystemContext;
    FILE_CONTEXT *FileContext = 0;
    BOOLEAN IsDirectory = !!(FILE_ATTRIBUTE_DIRECTORY &
        FspFileSystemGetOperationContext()->Response->Rsp.Create.Opened.FileInfo.FileAttributes);
    UINT32 MaximumAccess = IsDirectory ? GrantedAccess : MAXIMUM_ALLOWED;
    HANDLE Handle = 0;
    NTSTATUS Result;

    CreateOptions &=
        FILE_DIRECTORY_FILE |
        FILE_NON_DIRECTORY_FILE |
        FILE_NO_EA_KNOWLEDGE;

    /* WORKAROUND:
     *
     * WOW64 appears to have a bug in some versions of the OS (seen on Win10 1909 and
     * Server 2012 R2), where NtQueryDirectoryFile may produce garbage if called on a
     * directory that has been opened without FILE_SYNCHRONOUS_IO_NONALERT. (Garbage:
     * after a STATUS_PENDING has been waited, Iosb.Information reports bytes transferred
     * but the buffer does not get filled).
     *
     * So make sure to always open directories in a synchronous manner.
     */
    if (IsDirectory)
    {
        MaximumAccess |= SYNCHRONIZE;
        //GrantedAccess |= SYNCHRONIZE;
        CreateOptions |= FILE_SYNCHRONOUS_IO_NONALERT;
    }

    Result = LfsOpenFile(
        &Handle,
        MaximumAccess |
            (Ptfs->HasSecurityPrivilege ? ACCESS_SYSTEM_SECURITY : 0),
        Ptfs->RootHandle,
        FileName,
        FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT | CreateOptions);
    if (!NT_SUCCESS(Result) && MAXIMUM_ALLOWED == MaximumAccess)
        switch (Result)
        {
        case STATUS_ACCESS_DENIED:
        case STATUS_MEDIA_WRITE_PROTECTED:
        case STATUS_SHARING_VIOLATION:
        case STATUS_INVALID_PARAMETER:
            Result = LfsOpenFile(
                &Handle,
                GrantedAccess |
                    (Ptfs->HasSecurityPrivilege ? ACCESS_SYSTEM_SECURITY : 0),
                Ptfs->RootHandle,
                FileName,
                FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT | CreateOptions);
            break;
        }
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LfsGetFileInfo(Handle, Ptfs->RootPrefixLength, FileInfo);
    if (!NT_SUCCESS(Result))
        goto exit;

    FileContext = malloc(sizeof *FileContext);
    if (0 == FileContext)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    memset(FileContext, 0, sizeof *FileContext);
    FileContext->Handle = Handle;
    FileContext->IsDirectory = IsDirectory;
    FileContext->DirFileSize = (ULONG)FileInfo->FileSize;
    *PFileContext = FileContext;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (0 != Handle)
            NtClose(Handle);

        free(FileContext);
    }

    return Result;
}

static NTSTATUS OverwriteEx(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    HANDLE Handle = FileContextHandle;
    HANDLE NewHandle;
    NTSTATUS Result;

    Result = LfsCreateFile(
        &NewHandle,
        ReplaceFileAttributes ? DELETE : FILE_WRITE_DATA,
        Handle,
        L"",
        0,
        0 != AllocationSize ? (PLARGE_INTEGER)&AllocationSize : 0,
        ReplaceFileAttributes ?
            (0 != FileAttributes ? FileAttributes : FILE_ATTRIBUTE_NORMAL) :
            FileAttributes,
        ReplaceFileAttributes ? FILE_SUPERSEDE : FILE_OVERWRITE,
        FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT,
        Ea,
        EaLength);
    if (!NT_SUCCESS(Result))
        goto exit;
    NtClose(NewHandle);

    Result = LfsGetFileInfo(Handle, -1, FileInfo);

exit:
    return Result;
}

static VOID Cleanup(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR FileName, ULONG Flags)
{
    PTFS *Ptfs = FileSystemContext;
    HANDLE Handle = FileContextHandle;

    if (Flags & FspCleanupDelete)
    {
        SetDisposition(Handle, TRUE);
        NtClose(Handle);

        /* this will make all future uses of Handle to fail with STATUS_INVALID_HANDLE */
        FileContextHandle = 0;
    }
    else if ((Flags & FspCleanupSetAllocationSize) &&
        (Ptfs->FsAttributeMask & PtfsSetAllocationSizeOnCleanup))
    {
        IO_STATUS_BLOCK Iosb;
        FILE_STANDARD_INFORMATION FileStdInfo;
        FILE_ALLOCATION_INFORMATION FileAllocInfo;
        NTSTATUS Result;

        Result = NtQueryInformationFile(
            Handle,
            &Iosb,
            &FileStdInfo,
            sizeof FileStdInfo,
            5/*FileStandardInformation*/);
        if (NT_SUCCESS(Result))
        {
            FileAllocInfo.AllocationSize.QuadPart = FileStdInfo.EndOfFile.QuadPart;

            Result = NtSetInformationFile(
                Handle,
                &Iosb,
                &FileAllocInfo,
                sizeof FileAllocInfo,
                19/*FileAllocationInformation*/);
        }
    }
}

static VOID Close(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext)
{
    HANDLE Handle = FileContextHandle;

    if (0 != Handle)
        NtClose(Handle);

    FspFileSystemDeleteDirectoryBuffer(FileContextDirBuffer);

    free(FileContext);
}

static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    HANDLE Handle = FileContextHandle;

    return LfsReadFile(
        Handle,
        Buffer,
        Offset,
        Length,
        PBytesTransferred);
}

static NTSTATUS Write(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    HANDLE Handle = FileContextHandle;
    IO_STATUS_BLOCK Iosb;
    FILE_STANDARD_INFORMATION FileStdInfo;
    NTSTATUS Result;

    if (ConstrainedIo)
    {
        Result = NtQueryInformationFile(
            Handle,
            &Iosb,
            &FileStdInfo,
            sizeof FileStdInfo,
            5/*FileStandardInformation*/);
        if (!NT_SUCCESS(Result))
            goto exit;

        if (Offset >= (UINT64)FileStdInfo.EndOfFile.QuadPart)
            return STATUS_SUCCESS;
        if (Offset + Length > (UINT64)FileStdInfo.EndOfFile.QuadPart)
            Length = (ULONG)((UINT64)FileStdInfo.EndOfFile.QuadPart - Offset);
    }

    Result = LfsWriteFile(
        Handle,
        Buffer,
        Offset,
        Length,
        PBytesTransferred);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LfsGetFileInfo(Handle, -1, FileInfo);

exit:
    return Result;
}

static NTSTATUS Flush(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    /* we do not flush the whole volume, so just return SUCCESS */
    if (0 == FileContext)
        return STATUS_SUCCESS;

    HANDLE Handle = FileContextHandle;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    Result = NtFlushBuffersFile(
        Handle,
        &Iosb);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LfsGetFileInfo(Handle, -1, FileInfo);

exit:
    return Result;
}

static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    HANDLE Handle = FileContextHandle;

    return LfsGetFileInfo(Handle, -1, FileInfo);
}

static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    HANDLE Handle = FileContextHandle;
    IO_STATUS_BLOCK Iosb;
    FILE_BASIC_INFORMATION FileBasicInfo;
    NTSTATUS Result;

    if (INVALID_FILE_ATTRIBUTES == FileAttributes)
        FileAttributes = 0;
    else if (0 == FileAttributes)
        FileAttributes = FILE_ATTRIBUTE_NORMAL;

    memset(&FileBasicInfo, 0, sizeof FileBasicInfo);
    FileBasicInfo.CreationTime.QuadPart = CreationTime;
    FileBasicInfo.LastAccessTime.QuadPart = LastAccessTime;
    FileBasicInfo.LastWriteTime.QuadPart = LastWriteTime;
    FileBasicInfo.ChangeTime.QuadPart = ChangeTime;
    FileBasicInfo.FileAttributes = FileAttributes;

    Result = NtSetInformationFile(
        Handle,
        &Iosb,
        &FileBasicInfo,
        sizeof FileBasicInfo,
        4/*FileBasicInformation*/);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LfsGetFileInfo(Handle, -1, FileInfo);

exit:
    return Result;
}

static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    HANDLE Handle = FileContextHandle;
    IO_STATUS_BLOCK Iosb;
    FILE_ALLOCATION_INFORMATION FileAllocInfo;
    FILE_END_OF_FILE_INFORMATION FileEofInfo;
    NTSTATUS Result;

    if (SetAllocationSize)
    {
        FileAllocInfo.AllocationSize.QuadPart = NewSize;

        Result = NtSetInformationFile(
            Handle,
            &Iosb,
            &FileAllocInfo,
            sizeof FileAllocInfo,
            19/*FileAllocationInformation*/);
    }
    else
    {
        FileEofInfo.EndOfFile.QuadPart = NewSize;

        Result = NtSetInformationFile(
            Handle,
            &Iosb,
            &FileEofInfo,
            sizeof FileEofInfo,
            20/*FileEndOfFileInformation*/);
    }
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LfsGetFileInfo(Handle, -1, FileInfo);

exit:
    return Result;
}

static NTSTATUS SetDelete(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR FileName, BOOLEAN DeleteFile)
{
    return SetDisposition(FileContextHandle, DeleteFile);
}

static NTSTATUS SetDisposition(
    HANDLE Handle, BOOLEAN DeleteFile)
{
    IO_STATUS_BLOCK Iosb;
    FILE_DISPOSITION_INFORMATION_EX FileDispInfoEx;
    FILE_DISPOSITION_INFORMATION FileDispInfo;
    NTSTATUS Result;

    FileDispInfoEx.Flags = DeleteFile ?
        0x17/*DELETE | POSIX_SEMANTICS | IGNORE_READONLY_ATTRIBUTE | FORCE_IMAGE_SECTION_CHECK*/ :
        0;

    Result = NtSetInformationFile(
        Handle,
        &Iosb,
        &FileDispInfoEx,
        sizeof FileDispInfoEx,
        64/*FileDispositionInformationEx*/);
    if (!NT_SUCCESS(Result))
    {
        switch (Result)
        {
        case STATUS_ACCESS_DENIED:
        case STATUS_DIRECTORY_NOT_EMPTY:
        case STATUS_CANNOT_DELETE:
        case STATUS_FILE_DELETED:
            goto exit;
        }

        FileDispInfo.DeleteFile = DeleteFile;

        Result = NtSetInformationFile(
            Handle,
            &Iosb,
            &FileDispInfo,
            sizeof FileDispInfo,
            13/*FileDispositionInformation*/);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    PTFS *Ptfs = FileSystemContext;
    HANDLE Handle = FileContextHandle;
    BOOLEAN PosixReplaceIfExists;
    IO_STATUS_BLOCK Iosb;
    union
    {
        FILE_RENAME_INFORMATION V;
        UINT8 B[FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + FSP_FSCTL_TRANSACT_PATH_SIZEMAX];
    } FileRenInfo;
    NTSTATUS Result;

    FileRenInfo.V.RootDirectory = Ptfs->RootHandle;
    FileRenInfo.V.FileNameLength = (ULONG)(wcslen(NewFileName + 1) * sizeof(WCHAR));
    if (FSP_FSCTL_TRANSACT_PATH_SIZEMAX < FileRenInfo.V.FileNameLength)
    {
        Result = STATUS_INVALID_PARAMETER;
        goto exit;
    }
    memcpy(FileRenInfo.V.FileName, NewFileName + 1, FileRenInfo.V.FileNameLength);

    /* POSIX rename is able to replace existing (empty) directories; verify this is what caller wants! */
    PosixReplaceIfExists = ReplaceIfExists && (!FileContextIsDirectory || 0 != (2/**POSIX_SEMANTICS*/ &
        FspFileSystemGetOperationContext()->Request->Req.SetInformation.Info.RenameEx.Flags));

    FileRenInfo.V.Flags = (PosixReplaceIfExists ? 1/*REPLACE_IF_EXISTS*/ : 0) |
        0x42 /*POSIX_SEMANTICS | IGNORE_READONLY_ATTRIBUTE*/;

    Result = NtSetInformationFile(
        Handle,
        &Iosb,
        &FileRenInfo,
        FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + FileRenInfo.V.FileNameLength,
        65/*FileRenameInformationEx*/);
    if (!NT_SUCCESS(Result))
    {
        switch (Result)
        {
        case STATUS_OBJECT_NAME_COLLISION:
            if (ReplaceIfExists && !PosixReplaceIfExists)
                Result = STATUS_ACCESS_DENIED;
            goto exit;
        case STATUS_ACCESS_DENIED:
            goto exit;
        }

        FileRenInfo.V.Flags = 0;
        FileRenInfo.V.ReplaceIfExists = ReplaceIfExists;

        Result = NtSetInformationFile(
            Handle,
            &Iosb,
            &FileRenInfo,
            FIELD_OFFSET(FILE_RENAME_INFORMATION, FileName) + FileRenInfo.V.FileNameLength,
            10/*FileRenameInformation*/);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    PTFS *Ptfs = FileSystemContext;
    HANDLE Handle = FileContextHandle;
    ULONG SecurityDescriptorSizeNeeded;
    NTSTATUS Result;

    Result = NtQuerySecurityObject(
        Handle,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION |
            (Ptfs->HasSecurityPrivilege ? SACL_SECURITY_INFORMATION : 0),
        SecurityDescriptor,
        (ULONG)*PSecurityDescriptorSize,
        &SecurityDescriptorSizeNeeded);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PSecurityDescriptorSize = SecurityDescriptorSizeNeeded;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS SetSecurity(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    HANDLE Handle = FileContextHandle;

    return NtSetSecurityObject(Handle, SecurityInformation, ModificationDescriptor);
}

static inline VOID CopyQueryInfoToDirInfo(
    FILE_ID_BOTH_DIR_INFORMATION *QueryInfo,
    FSP_FSCTL_DIR_INFO *DirInfo)
{
    memset(DirInfo, 0, sizeof *DirInfo);
    DirInfo->Size = (UINT16)(FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) +
        QueryInfo->FileNameLength);
    DirInfo->FileInfo.FileAttributes = QueryInfo->FileAttributes;
    DirInfo->FileInfo.ReparseTag = 0 != (FILE_ATTRIBUTE_REPARSE_POINT & QueryInfo->FileAttributes) ?
        QueryInfo->EaSize : 0;
    DirInfo->FileInfo.AllocationSize = QueryInfo->AllocationSize.QuadPart;
    DirInfo->FileInfo.FileSize = QueryInfo->EndOfFile.QuadPart;
    DirInfo->FileInfo.CreationTime = QueryInfo->CreationTime.QuadPart;
    DirInfo->FileInfo.LastAccessTime = QueryInfo->LastAccessTime.QuadPart;
    DirInfo->FileInfo.LastWriteTime = QueryInfo->LastWriteTime.QuadPart;
    DirInfo->FileInfo.ChangeTime = QueryInfo->ChangeTime.QuadPart;
    DirInfo->FileInfo.IndexNumber = QueryInfo->FileId.QuadPart;
    DirInfo->FileInfo.HardLinks = 0;
    DirInfo->FileInfo.EaSize = 0 != (FILE_ATTRIBUTE_REPARSE_POINT & QueryInfo->FileAttributes) ?
        0 : LfsGetEaSize(QueryInfo->EaSize);
}

static NTSTATUS BufferedReadDirectory(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    PTFS *Ptfs = FileSystemContext;
    HANDLE Handle = FileContextHandle;
    ULONG CapacityHint = FileContextDirFileSize;
        /*
         * An analysis of the relationship between the required buffer capacity and the
         * NTFS directory size (as reported by FileStandardInformation) showed the ratio
         * between the two to be between 0.5 and 1 with some outliers outside those bounds.
         * (For this analysis file names of average length of 4 or 24 were used and NTFS
         * had 8.3 file names disabled.)
         *
         * We use the NTFS directory size as our capacity hint (i.e. ratio of 1), which
         * means that we may overestimate the required buffer capacity in most cases.
         * This is ok since our goal is to improve performance.
         */
    PVOID PDirBuffer = FileContextDirBuffer;
    BOOLEAN RestartScan;
    ULONG BytesTransferred;
    UINT8 QueryBuffer[16 * 1024];
    FILE_ID_BOTH_DIR_INFORMATION *QueryInfo;
    ULONG QueryNext;
    union
    {
        FSP_FSCTL_DIR_INFO V;
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + 255 * sizeof(WCHAR)];
    } DirInfo;
    NTSTATUS Result, DirBufferResult;

    DirBufferResult = STATUS_SUCCESS;
    if (FspFileSystemAcquireDirectoryBufferEx(PDirBuffer, 0 == Marker, CapacityHint, &DirBufferResult))
    {
        for (RestartScan = TRUE;; RestartScan = FALSE)
        {
            Result = LfsQueryDirectoryFile(
                Handle,
                QueryBuffer,
                sizeof QueryBuffer,
                37/*FileIdBothDirectoryInformation*/,
                FALSE,
                Pattern,
                RestartScan,
                &BytesTransferred);
            if (!NT_SUCCESS(Result))
                break;

            for (QueryInfo = (FILE_ID_BOTH_DIR_INFORMATION *)QueryBuffer;
                ;
                QueryInfo = (FILE_ID_BOTH_DIR_INFORMATION *)((PUINT8)QueryInfo + QueryNext))
            {
                if (QueryBuffer + BytesTransferred <
                    (PUINT8)QueryInfo + FIELD_OFFSET(FILE_ID_BOTH_DIR_INFORMATION, FileName))
                    goto done;

                QueryNext = QueryInfo->NextEntryOffset;

                CopyQueryInfoToDirInfo(QueryInfo, &DirInfo.V);
                memcpy(DirInfo.V.FileNameBuf, QueryInfo->FileName, QueryInfo->FileNameLength);

                if (!FspFileSystemFillDirectoryBuffer(PDirBuffer, &DirInfo.V, &DirBufferResult))
                    goto done;

                if (0 == QueryNext)
                    break;
            }
        }

    done:
        FspFileSystemReleaseDirectoryBuffer(PDirBuffer);
    }

    if (!NT_SUCCESS(DirBufferResult))
        return DirBufferResult;

    FspFileSystemReadDirectoryBuffer(PDirBuffer,
        Marker, Buffer, Length, PBytesTransferred);

    return STATUS_SUCCESS;
}

static NTSTATUS ResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem,
    PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    return FspFileSystemResolveReparsePoints(FileSystem, GetReparsePointByName, 0,
        FileName, ReparsePointIndex, ResolveLastPathComponent,
        PIoStatus, Buffer, PSize);
}

static NTSTATUS GetReparsePointByName(
    FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize)
{
    PTFS *Ptfs = FileSystemContext;
    HANDLE Handle = 0;
    union
    {
        REPARSE_DATA_BUFFER V;
        UINT8 B[FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX];
    } ReparseBuffer;
    SIZE_T ReparseBufferSize;
    ULONG BytesTransferred;
    NTSTATUS Result;

    if (0 == Buffer)
    {
        Buffer = &ReparseBuffer;
        PSize = &ReparseBufferSize;
        ReparseBufferSize = sizeof ReparseBuffer;
    }

    Result = LfsOpenFile(
        &Handle,
        0,
        Ptfs->RootHandle,
        FileName,
        FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT |
            (IsDirectory ? FILE_DIRECTORY_FILE : 0));
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LfsFsControlFile(
        Handle,
        FSCTL_GET_REPARSE_POINT,
        0,
        0,
        Buffer,
        (ULONG)*PSize,
        &BytesTransferred);
    if (STATUS_BUFFER_OVERFLOW == Result)
    {
        Result = STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }
    else if (!NT_SUCCESS(Result))
        goto exit;

    *PSize = BytesTransferred;

    Result = STATUS_SUCCESS;

exit:
    if (0 != Handle)
        NtClose(Handle);

    return Result;
}

static NTSTATUS GetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PWSTR FileName, PVOID Buffer, PSIZE_T PSize)
{
    HANDLE Handle = FileContextHandle;
    ULONG BytesTransferred;
    NTSTATUS Result;

    Result = LfsFsControlFile(
        Handle,
        FSCTL_GET_REPARSE_POINT,
        0,
        0,
        Buffer,
        (ULONG)*PSize,
        &BytesTransferred);
    if (STATUS_BUFFER_OVERFLOW == Result)
    {
        Result = STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }
    else if (!NT_SUCCESS(Result))
        goto exit;

    *PSize = BytesTransferred;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS SetReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    HANDLE Handle = FileContextHandle;
    ULONG BytesTransferred;
    NTSTATUS Result;

    Result = LfsFsControlFile(
        Handle,
        FSCTL_SET_REPARSE_POINT,
        Buffer,
        (ULONG)Size,
        0,
        0,
        &BytesTransferred);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS DeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PWSTR FileName, PVOID Buffer, SIZE_T Size)
{
    HANDLE Handle = FileContextHandle;
    ULONG BytesTransferred;
    NTSTATUS Result;

    Result = LfsFsControlFile(
        Handle,
        FSCTL_DELETE_REPARSE_POINT,
        Buffer,
        (ULONG)Size,
        0,
        0,
        &BytesTransferred);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS GetStreamInfo(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, PVOID Buffer, ULONG Length,
    PULONG PBytesTransferred)
{
    HANDLE Handle = FileContextHandle;
    IO_STATUS_BLOCK Iosb;
    UINT8 QueryBuffer[16 * 1024];
    FILE_STREAM_INFORMATION *QueryInfo;
    ULONG QueryNext;
    union
    {
        FSP_FSCTL_STREAM_INFO V;
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_STREAM_INFO, StreamNameBuf) + 255 * sizeof(WCHAR)];
    } StreamInfo;
    PWCHAR P, EndP, NameP;
    NTSTATUS Result;

    Result = NtQueryInformationFile(
        Handle,
        &Iosb,
        QueryBuffer,
        sizeof QueryBuffer,
        22/*FileStreamInformation*/);
    if (!NT_SUCCESS(Result) && STATUS_BUFFER_OVERFLOW != Result)
        goto exit;

    for (QueryInfo = (FILE_STREAM_INFORMATION *)QueryBuffer;
        ;
        QueryInfo = (FILE_STREAM_INFORMATION *)((PUINT8)QueryInfo + QueryNext))
    {
        if (QueryBuffer + Iosb.Information <
            (PUINT8)QueryInfo + FIELD_OFFSET(FILE_STREAM_INFORMATION, StreamName))
            break;

        QueryNext = QueryInfo->NextEntryOffset;

        NameP = 0;
        for (P = QueryInfo->StreamName, EndP = P + QueryInfo->StreamNameLength / sizeof(WCHAR);; P++)
            if (L':' == *P)
            {
                if (0 == NameP)
                    NameP = P + 1;
                else
                    break;
            }
        if (0 == NameP)
            NameP = P;

        memset(&StreamInfo.V, 0, sizeof StreamInfo.V);
        StreamInfo.V.Size = (UINT16)(FIELD_OFFSET(FSP_FSCTL_STREAM_INFO, StreamNameBuf) +
            ((PUINT8)P - (PUINT8)NameP));
        StreamInfo.V.StreamSize = QueryInfo->StreamSize.QuadPart;
        StreamInfo.V.StreamAllocationSize = QueryInfo->StreamAllocationSize.QuadPart;
        memcpy(StreamInfo.V.StreamNameBuf, NameP, ((PUINT8)P - (PUINT8)NameP));

        if (!FspFileSystemAddStreamInfo(&StreamInfo.V, Buffer, Length, PBytesTransferred))
            goto done;

        if (0 == QueryNext)
            break;
    }

    FspFileSystemAddStreamInfo(0, Buffer, Length, PBytesTransferred);

done:

    Result = STATUS_SUCCESS;

exit:
    return Result;
}

static NTSTATUS GetEa(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength, PULONG PBytesTransferred)
{
    HANDLE Handle = FileContextHandle;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    Result = NtQueryEaFile(
        Handle,
        &Iosb,
        Ea,
        EaLength,
        FALSE,
        0,
        0,
        0,
        TRUE);
    if (!NT_SUCCESS(Result) && STATUS_BUFFER_OVERFLOW != Result)
    {
        /* on error report an empty EA list */
        Iosb.Information = 0;
    }

    *PBytesTransferred = (ULONG)Iosb.Information;

    Result = STATUS_SUCCESS;

    return Result;
}

static NTSTATUS SetEa(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext,
    PFILE_FULL_EA_INFORMATION Ea, ULONG EaLength,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    HANDLE Handle = FileContextHandle;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    Result = NtSetEaFile(
        Handle,
        &Iosb,
        Ea,
        EaLength);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = LfsGetFileInfo(Handle, -1, FileInfo);

exit:
    return Result;
}

static VOID DispatcherStopped(FSP_FILE_SYSTEM *FileSystem,
    BOOLEAN Normally)
{
    FspFileSystemStopServiceIfNecessary(FileSystem, Normally);
}

static FSP_FILE_SYSTEM_INTERFACE PtfsInterface =
{
    .GetVolumeInfo = GetVolumeInfo,
    .SetVolumeLabel = SetVolumeLabel_,
    .GetSecurityByName = GetSecurityByName,
    .CreateEx = CreateEx,
    .Open = Open,
    .OverwriteEx = OverwriteEx,
    .Cleanup = Cleanup,
    .Close = Close,
    .Read = Read,
    .Write = Write,
    .Flush = Flush,
    .GetFileInfo = GetFileInfo,
    .SetBasicInfo = SetBasicInfo,
    .SetFileSize = SetFileSize,
    .SetDelete = SetDelete,
    .Rename = Rename,
    .GetSecurity = GetSecurity,
    .SetSecurity = SetSecurity,
    .ReadDirectory = BufferedReadDirectory,
    .ResolveReparsePoints = ResolveReparsePoints,
    .GetReparsePoint = GetReparsePoint,
    .SetReparsePoint = SetReparsePoint,
    .DeleteReparsePoint = DeleteReparsePoint,
    .GetStreamInfo = GetStreamInfo,
    .GetEa = GetEa,
    .SetEa = SetEa,
    .DispatcherStopped = DispatcherStopped,
};

NTSTATUS PtfsCreate(
    PWSTR RootPath,
    ULONG FileInfoTimeout,
    ULONG FsAttributeMask,
    PWSTR VolumePrefix,
    PWSTR MountPoint,
    UINT32 DebugFlags,
    PTFS **PPtfs)
{
    PTFS *Ptfs = 0;
    FSP_FILE_SYSTEM *FileSystem = 0;
    BOOL HasSecurityPrivilege = FALSE;
    PRIVILEGE_SET PrivilegeSet;
    HANDLE ProcessToken;
    HANDLE RootHandle = INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK Iosb;
    union
    {
        FILE_FS_ATTRIBUTE_INFORMATION V;
        UINT8 B[FIELD_OFFSET(FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName) + MAX_PATH * sizeof(WCHAR)];
    } FsAttrInfo;
    FILE_FS_SIZE_INFORMATION FsSizeInfo;
    FILE_ALL_INFORMATION FileAllInfo;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    NTSTATUS Result;

    *PPtfs = 0;

    if (LookupPrivilegeValueW(0, SE_SECURITY_NAME, &PrivilegeSet.Privilege[0].Luid) &&
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &ProcessToken))
    {
        PrivilegeSet.PrivilegeCount = 1;
        PrivilegeSet.Control = PRIVILEGE_SET_ALL_NECESSARY;
        PrivilegeSet.Privilege[0].Attributes = 0;
        PrivilegeCheck(ProcessToken, &PrivilegeSet, &HasSecurityPrivilege);
        CloseHandle(ProcessToken);
    }

    RootHandle = CreateFileW(
        RootPath,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        0);
    if (INVALID_HANDLE_VALUE == RootHandle)
    {
        Result = FspNtStatusFromWin32(GetLastError());
        goto exit;
    }

    Result = NtQueryVolumeInformationFile(
        RootHandle,
        &Iosb,
        &FsAttrInfo,
        sizeof FsAttrInfo,
        5/*FileFsAttributeInformation*/);
    if (!NT_SUCCESS(Result))
        goto exit;
    Result = NtQueryVolumeInformationFile(
        RootHandle,
        &Iosb,
        &FsSizeInfo,
        sizeof FsSizeInfo,
        3/*FileFsSizeInformation*/);
    if (!NT_SUCCESS(Result))
        goto exit;
    Result = NtQueryInformationFile(
        RootHandle,
        &Iosb,
        &FileAllInfo,
        sizeof FileAllInfo,
        18/*FileAllInformation*/);
    if (!NT_SUCCESS(Result) && STATUS_BUFFER_OVERFLOW != Result)
        goto exit;

    FsAttributeMask &= PtfsAttributesMask;
    FsAttrInfo.V.FileSystemAttributes &=
        FILE_CASE_PRESERVED_NAMES |
        FILE_UNICODE_ON_DISK |
        FILE_PERSISTENT_ACLS |
        ((FsAttributeMask & PtfsReparsePoints) ? FILE_SUPPORTS_REPARSE_POINTS : 0) |
        ((FsAttributeMask & PtfsNamedStreams) ? FILE_NAMED_STREAMS : 0) |
        ((FsAttributeMask & PtfsExtendedAttributes) ? FILE_SUPPORTS_EXTENDED_ATTRIBUTES : 0) |
        0x00000400/*FILE_SUPPORTS_POSIX_UNLINK_RENAME*/ |
        FILE_READ_ONLY_VOLUME;

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.SectorSize = (UINT16)FsSizeInfo.BytesPerSector;
    VolumeParams.SectorsPerAllocationUnit = (UINT16)FsSizeInfo.SectorsPerAllocationUnit;
    VolumeParams.MaxComponentLength = (UINT16)FsAttrInfo.V.MaximumComponentNameLength;
    VolumeParams.VolumeCreationTime = FileAllInfo.BasicInformation.CreationTime.QuadPart;
    VolumeParams.VolumeSerialNumber = 0;
    VolumeParams.FileInfoTimeout = FileInfoTimeout;
    VolumeParams.CaseSensitiveSearch = 0;
    VolumeParams.CasePreservedNames = !!(FsAttrInfo.V.FileSystemAttributes & FILE_CASE_PRESERVED_NAMES);
    VolumeParams.UnicodeOnDisk = !!(FsAttrInfo.V.FileSystemAttributes & FILE_UNICODE_ON_DISK);
    VolumeParams.PersistentAcls = !!(FsAttrInfo.V.FileSystemAttributes & FILE_PERSISTENT_ACLS);
    VolumeParams.ReparsePoints = !!(FsAttrInfo.V.FileSystemAttributes & FILE_SUPPORTS_REPARSE_POINTS);
    VolumeParams.NamedStreams = !!(FsAttrInfo.V.FileSystemAttributes & FILE_NAMED_STREAMS);
    VolumeParams.ExtendedAttributes = !!(FsAttrInfo.V.FileSystemAttributes & FILE_SUPPORTS_EXTENDED_ATTRIBUTES);
    VolumeParams.SupportsPosixUnlinkRename = !!(FsAttrInfo.V.FileSystemAttributes & 0x00000400
        /*FILE_SUPPORTS_POSIX_UNLINK_RENAME*/);
    VolumeParams.ReadOnlyVolume = !!(FsAttrInfo.V.FileSystemAttributes & FILE_READ_ONLY_VOLUME);
    VolumeParams.PostCleanupWhenModifiedOnly = 1;
    VolumeParams.PostDispositionWhenNecessaryOnly = 1;
    VolumeParams.PassQueryDirectoryPattern = 1;
    VolumeParams.FlushAndPurgeOnCleanup = !!(FsAttributeMask & PtfsFlushAndPurgeOnCleanup);
    VolumeParams.WslFeatures = !!(FsAttributeMask & PtfsWslFeatures);
    VolumeParams.AllowOpenInKernelMode = 1;
    VolumeParams.RejectIrpPriorToTransact0 = 1;
    VolumeParams.UmFileContextIsUserContext2 = 1;
    if (0 != VolumePrefix)
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), VolumePrefix);
    memcpy(VolumeParams.FileSystemName, FsAttrInfo.V.FileSystemName,
        sizeof VolumeParams.FileSystemName <= FsAttrInfo.V.FileSystemNameLength ?
            sizeof VolumeParams.FileSystemName : FsAttrInfo.V.FileSystemNameLength);

    Result = FspFileSystemCreate(
        VolumeParams.Prefix[0] ?
            L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME,
        &VolumeParams,
        &PtfsInterface,
        &FileSystem);
    if (!NT_SUCCESS(Result))
        goto exit;
    FspFileSystemSetDebugLog(FileSystem, DebugFlags);

    Ptfs = malloc(sizeof *Ptfs);
    if (0 == Ptfs)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    memset(Ptfs, 0, sizeof *Ptfs);

    Ptfs->FileSystem = FileSystem;
    Ptfs->HasSecurityPrivilege = HasSecurityPrivilege;
    Ptfs->RootHandle = RootHandle;
    Ptfs->RootPrefixLength = FileAllInfo.NameInformation.FileNameLength;
    Ptfs->FsAttributeMask = FsAttributeMask;
    Ptfs->FsAttributes = FsAttrInfo.V.FileSystemAttributes;
    Ptfs->AllocationUnit = VolumeParams.SectorSize * VolumeParams.SectorsPerAllocationUnit;
    FileSystem->UserContext = Ptfs;

    Result = FspFileSystemSetMountPoint(FileSystem, MountPoint);
    if (!NT_SUCCESS(Result))
        goto exit;

    *PPtfs = Ptfs;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        free(Ptfs);

        if (0 != FileSystem)
            FspFileSystemDelete(FileSystem);

        if (INVALID_HANDLE_VALUE != RootHandle)
            CloseHandle(RootHandle);
    }

    return Result;
}

VOID PtfsDelete(PTFS *Ptfs)
{
    FspFileSystemDelete(Ptfs->FileSystem);
    CloseHandle(Ptfs->RootHandle);

    free(Ptfs);
}
