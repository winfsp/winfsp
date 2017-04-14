/**
 * @file winfsp/winfsp.hpp
 * WinFsp C++ Layer.
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

#ifndef WINFSP_WINFSP_HPP_INCLUDED
#define WINFSP_WINFSP_HPP_INCLUDED

#ifndef __cplusplus
#error this header requires a C++ compiler
#endif

#include <winfsp/winfsp.h>

#define FSP_CPP_EXCEPTION_GUARD(...)\
    try { __VA_ARGS__ } catch (...) { return self->ExceptionHandler(); }
#define FSP_CPP_EXCEPTION_GUARD_VOID(...)\
    try { __VA_ARGS__ } catch (...) { self->ExceptionHandler(); return; }

namespace Fsp {

inline NTSTATUS Initialize()
{
    static NTSTATUS LoadResult = FspLoad(0);
    return LoadResult;
}

inline NTSTATUS Version(PUINT32 PVersion)
{
    return FspVersion(PVersion);
}

inline NTSTATUS NtStatusFromWin32(DWORD Error)
{
    return FspNtStatusFromWin32(Error);
}

inline DWORD Win32FromNtStatus(NTSTATUS Status)
{
    return FspWin32FromNtStatus(Status);
}

class FileSystem
{
public:
    typedef FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    typedef FSP_FSCTL_VOLUME_INFO VolumeInfo;
    typedef FSP_FSCTL_FILE_INFO FileInfo;
    typedef FSP_FSCTL_OPEN_FILE_INFO OpenFileInfo;
    typedef FSP_FSCTL_DIR_INFO DirInfo;
    typedef FSP_FSCTL_STREAM_INFO StreamInfo;
    enum CleanupFlags
    {
        CleanupDelete                   = FspCleanupDelete,
        CleanupSetAllocationSize        = FspCleanupSetAllocationSize,
        CleanupSetArchiveBit            = FspCleanupSetArchiveBit,
        CleanupSetLastAccessTime        = FspCleanupSetLastAccessTime,
        CleanupSetLastWriteTime         = FspCleanupSetLastWriteTime,
        CleanupSetChangeTime            = FspCleanupSetChangeTime,
    };

public:
    /* ctor/dtor */
    FileSystem() : _VolumeParams(), _FileSystem(0)
    {
        Initialize();
        _VolumeParams.UmFileContextIsFullContext = 1;
    }
    virtual ~FileSystem()
    {
        if (0 != _FileSystem)
            FspFileSystemDelete(_FileSystem);
    }

    /* properties */
    const VolumeParams *GetVolumeParams()
    {
        return &_VolumeParams;
    }
    VOID SetSectorSize(UINT16 SectorSize)
    {
        _VolumeParams.SectorSize = SectorSize;
    }
    VOID SetSectorsPerAllocationUnit(UINT16 SectorsPerAllocationUnit)
    {
        _VolumeParams.SectorsPerAllocationUnit = SectorsPerAllocationUnit;
    }
    VOID SetMaxComponentLength(UINT16 MaxComponentLength)
    {
        _VolumeParams.MaxComponentLength = MaxComponentLength;
    }
    VOID SetVolumeCreationTime(UINT64 VolumeCreationTime)
    {
        _VolumeParams.VolumeCreationTime = VolumeCreationTime;
    }
    VOID SetVolumeSerialNumber(UINT32 VolumeSerialNumber)
    {
        _VolumeParams.VolumeSerialNumber = VolumeSerialNumber;
    }
    VOID SetFileInfoTimeout(UINT32 FileInfoTimeout)
    {
        _VolumeParams.FileInfoTimeout = FileInfoTimeout;
    }
    VOID SetCaseSensitiveSearch(BOOLEAN CaseSensitiveSearch)
    {
        _VolumeParams.CaseSensitiveSearch = !!CaseSensitiveSearch;
    }
    VOID SetCasePreservedNames(BOOLEAN CasePreservedNames)
    {
        _VolumeParams.CasePreservedNames = !!CasePreservedNames;
    }
    VOID SetUnicodeOnDisk(BOOLEAN UnicodeOnDisk)
    {
        _VolumeParams.UnicodeOnDisk = !!UnicodeOnDisk;
    }
    VOID SetPersistentAcls(BOOLEAN PersistentAcls)
    {
        _VolumeParams.PersistentAcls = !!PersistentAcls;
    }
    VOID SetReparsePoints(BOOLEAN ReparsePoints)
    {
        _VolumeParams.ReparsePoints = !!ReparsePoints;
    }
    VOID SetReparsePointsAccessCheck(BOOLEAN ReparsePointsAccessCheck)
    {
        _VolumeParams.ReparsePointsAccessCheck = !!ReparsePointsAccessCheck;
    }
    VOID SetNamedStreams(BOOLEAN NamedStreams)
    {
        _VolumeParams.NamedStreams = !!NamedStreams;
    }
    VOID SetPostCleanupWhenModifiedOnly(BOOLEAN PostCleanupWhenModifiedOnly)
    {
        _VolumeParams.PostCleanupWhenModifiedOnly = !!PostCleanupWhenModifiedOnly;
    }
    VOID SetPassQueryDirectoryPattern(BOOLEAN PassQueryDirectoryPattern)
    {
        _VolumeParams.PassQueryDirectoryPattern = !!PassQueryDirectoryPattern;
    }
    VOID SetPrefix(PWSTR Prefix)
    {
        int Size = lstrlenW(Prefix) * sizeof(WCHAR);
        if (Size > sizeof _VolumeParams.Prefix - sizeof(WCHAR))
            Size = sizeof _VolumeParams.Prefix - sizeof(WCHAR);
        RtlCopyMemory(_VolumeParams.Prefix, Prefix, Size);
        _VolumeParams.Prefix[Size / sizeof(WCHAR)] = L'\0';
    }
    VOID SetFileSystemName(PWSTR FileSystemName)
    {
        int Size = lstrlenW(FileSystemName) * sizeof(WCHAR);
        if (Size > sizeof _VolumeParams.FileSystemName - sizeof(WCHAR))
            Size = sizeof _VolumeParams.FileSystemName - sizeof(WCHAR);
        RtlCopyMemory(_VolumeParams.FileSystemName, FileSystemName, Size);
        _VolumeParams.FileSystemName[Size / sizeof(WCHAR)] = L'\0';
    }

    /* control */
    NTSTATUS Preflight(PWSTR MountPoint)
    {
        return FspFileSystemPreflight(
            _VolumeParams.Prefix[0] ? L"WinFsp.Net" : L"WinFsp.Disk",
            MountPoint);
    }
    NTSTATUS Mount(PWSTR MountPoint,
        PSECURITY_DESCRIPTOR SecurityDescriptor = 0,
        BOOLEAN Synchronized = FALSE,
        UINT32 DebugLog = 0)
    {
        NTSTATUS Result;
        Result = FspFileSystemCreate(
            _VolumeParams.Prefix[0] ? L"WinFsp.Net" : L"WinFsp.Disk",
            &_VolumeParams, Interface(), &_FileSystem);
        if (NT_SUCCESS(Result))
        {
            _FileSystem->UserContext = this;
            FspFileSystemSetOperationGuardStrategy(_FileSystem, Synchronized ?
                FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE :
                FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE);
            FspFileSystemSetDebugLog(_FileSystem, DebugLog);
            Result = FspFileSystemSetMountPointEx(_FileSystem, MountPoint, SecurityDescriptor);
            if (NT_SUCCESS(Result))
                Result = FspFileSystemStartDispatcher(_FileSystem, 0);
        }
        if (!NT_SUCCESS(Result) && 0 != _FileSystem)
        {
            FspFileSystemDelete(_FileSystem);
            _FileSystem = 0;
        }
        return Result;
    }
    VOID Unmount()
    {
        FspFileSystemStopDispatcher(_FileSystem);
        FspFileSystemDelete(_FileSystem);
        _FileSystem = 0;
    }
    PWSTR MountPoint()
    {
        return 0 != _FileSystem ? FspFileSystemMountPoint(_FileSystem) : 0;
    }
    FSP_FILE_SYSTEM *FileSystemHandle()
    {
        return _FileSystem;
    }

    /* helpers: directories/streams */
    static BOOLEAN AcquireDirectoryBuffer(PVOID *PDirBuffer,
        BOOLEAN Reset, PNTSTATUS PResult)
    {
        return FspFileSystemAcquireDirectoryBuffer(PDirBuffer, Reset, PResult);
    }
    static BOOLEAN FillDirectoryBuffer(PVOID *PDirBuffer,
        DirInfo *DirInfo, PNTSTATUS PResult)
    {
        return FspFileSystemFillDirectoryBuffer(PDirBuffer, DirInfo, PResult);
    }
    static VOID ReleaseDirectoryBuffer(PVOID *PDirBuffer)
    {
        FspFileSystemReleaseDirectoryBuffer(PDirBuffer);
    }
    static VOID ReadDirectoryBuffer(PVOID *PDirBuffer,
        PWSTR Marker,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
    {
        FspFileSystemReadDirectoryBuffer(PDirBuffer,
            Marker, Buffer, Length, PBytesTransferred);
    }
    static VOID DeleteDirectoryBuffer(PVOID *PDirBuffer)
    {
        FspFileSystemDeleteDirectoryBuffer(PDirBuffer);
    }
    static BOOLEAN AddDirInfo(DirInfo *DirInfo,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
    {
        return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
    }
    static BOOLEAN AddStreamInfo(StreamInfo *StreamInfo,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
    {
        return FspFileSystemAddStreamInfo(StreamInfo, Buffer, Length, PBytesTransferred);
    }

    /* helpers: reparse points */
    BOOLEAN FindReparsePoint(
        PWSTR FileName, PUINT32 PReparsePointIndex)
    {
        return FspFileSystemFindReparsePoint(_FileSystem, GetReparsePointByName, 0,
            FileName, PReparsePointIndex);
    }
    static NTSTATUS CanReplaceReparsePoint(
        PVOID CurrentReparseData, SIZE_T CurrentReparseDataSize,
        PVOID ReplaceReparseData, SIZE_T ReplaceReparseDataSize)
    {
        return FspFileSystemCanReplaceReparsePoint(
            CurrentReparseData, CurrentReparseDataSize,
            ReplaceReparseData, ReplaceReparseDataSize);
    }

protected:
    /* operations */
    virtual NTSTATUS ExceptionHandler()
    {
        return STATUS_UNEXPECTED_IO_ERROR;
    }
    virtual NTSTATUS GetVolumeInfo(
        VolumeInfo *VolumeInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetVolumeLabel_(
        PWSTR VolumeLabel,
        VolumeInfo *VolumeInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetSecurityByName(
        PWSTR FileName,
        PUINT32 PFileAttributes/* or ReparsePointIndex */,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        SIZE_T *PSecurityDescriptorSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Create(
        PWSTR FileName,
        UINT32 CreateOptions,
        UINT32 GrantedAccess,
        UINT32 FileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        UINT64 AllocationSize,
        PVOID *PFileNode,
        PVOID *PFileDesc,
        OpenFileInfo *OpenFileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Open(
        PWSTR FileName,
        UINT32 CreateOptions,
        UINT32 GrantedAccess,
        PVOID *PFileNode,
        PVOID *PFileDesc,
        OpenFileInfo *OpenFileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Overwrite(
        PVOID FileNode,
        PVOID FileDesc,
        UINT32 FileAttributes,
        BOOLEAN ReplaceFileAttributes,
        UINT64 AllocationSize,
        FileInfo *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual VOID Cleanup(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName,
        ULONG Flags)
    {
    }
    virtual VOID Close(
        PVOID FileNode,
        PVOID FileDesc)
    {
    }
    virtual NTSTATUS Read(
        PVOID FileNode,
        PVOID FileDesc,
        PVOID Buffer,
        UINT64 Offset,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Write(
        PVOID FileNode,
        PVOID FileDesc,
        PVOID Buffer,
        UINT64 Offset,
        ULONG Length,
        BOOLEAN WriteToEndOfFile,
        BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred,
        FileInfo *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Flush(
        PVOID FileNode,
        PVOID FileDesc,
        FileInfo *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetFileInfo(
        PVOID FileNode,
        PVOID FileDesc,
        FileInfo *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetBasicInfo(
        PVOID FileNode,
        PVOID FileDesc,
        UINT32 FileAttributes,
        UINT64 CreationTime,
        UINT64 LastAccessTime,
        UINT64 LastWriteTime,
        UINT64 ChangeTime,
        FileInfo *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetFileSize(
        PVOID FileNode,
        PVOID FileDesc,
        UINT64 NewSize,
        BOOLEAN SetAllocationSize,
        FileInfo *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS CanDelete(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Rename(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName,
        PWSTR NewFileName,
        BOOLEAN ReplaceIfExists)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetSecurity(
        PVOID FileNode,
        PVOID FileDesc,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        SIZE_T *PSecurityDescriptorSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetSecurity(
        PVOID FileNode,
        PVOID FileDesc,
        SECURITY_INFORMATION SecurityInformation,
        PSECURITY_DESCRIPTOR ModificationDescriptor)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS ReadDirectory(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR Pattern,
        PWSTR Marker,
        PVOID Buffer,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS ResolveReparsePoints(
        PWSTR FileName,
        UINT32 ReparsePointIndex,
        BOOLEAN ResolveLastPathComponent,
        PIO_STATUS_BLOCK PIoStatus,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        return FspFileSystemResolveReparsePoints(_FileSystem, GetReparsePointByName, 0,
            FileName, ReparsePointIndex, ResolveLastPathComponent,
            PIoStatus, Buffer, PSize);
    }
    virtual NTSTATUS GetReparsePointByName(
        PWSTR FileName,
        BOOLEAN IsDirectory,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetReparsePoint(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetReparsePoint(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName,
        PVOID Buffer,
        SIZE_T Size)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS DeleteReparsePoint(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR FileName,
        PVOID Buffer,
        SIZE_T Size)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetStreamInfo(
        PVOID FileNode,
        PVOID FileDesc,
        PVOID Buffer,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

private:
    /* FSP_FILE_SYSTEM_INTERFACE */
    static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem0,
        VolumeInfo *VolumeInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetVolumeInfo(
                VolumeInfo);
        )
    }
    static NTSTATUS SetVolumeLabel_(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR VolumeLabel,
        VolumeInfo *VolumeInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->SetVolumeLabel_(
                VolumeLabel,
                VolumeInfo);
        )
    }
    static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName,
        PUINT32 PFileAttributes/* or ReparsePointIndex */,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        SIZE_T *PSecurityDescriptorSize)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetSecurityByName(
                FileName,
                PFileAttributes,
                SecurityDescriptor,
                PSecurityDescriptorSize);
        )
    }
    static NTSTATUS Create(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName,
        UINT32 CreateOptions,
        UINT32 GrantedAccess,
        UINT32 FileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        UINT64 AllocationSize,
        PVOID *FullContext,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        PVOID FileNode, FileDesc;
        NTSTATUS Result;
        FSP_CPP_EXCEPTION_GUARD(
            Result = self->Create(
                FileName,
                CreateOptions,
                GrantedAccess,
                FileAttributes,
                SecurityDescriptor,
                AllocationSize,
                &FileNode,
                &FileDesc,
                FspFileSystemGetOpenFileInfo(FileInfo));
        )
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext = (UINT64)(UINT_PTR)FileNode;
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2 = (UINT64)(UINT_PTR)FileDesc;
        return Result;
    }
    static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName,
        UINT32 CreateOptions,
        UINT32 GrantedAccess,
        PVOID *FullContext,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        PVOID FileNode, FileDesc;
        NTSTATUS Result;
        FSP_CPP_EXCEPTION_GUARD(
            Result = self->Open(
                FileName,
                CreateOptions,
                GrantedAccess,
                &FileNode,
                &FileDesc,
                FspFileSystemGetOpenFileInfo(FileInfo));
        )
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext = (UINT64)(UINT_PTR)FileNode;
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2 = (UINT64)(UINT_PTR)FileDesc;
        return Result;
    }
    static NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        UINT32 FileAttributes,
        BOOLEAN ReplaceFileAttributes,
        UINT64 AllocationSize,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->Overwrite(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileAttributes,
                ReplaceFileAttributes,
                AllocationSize,
                FileInfo);
        )
    }
    static VOID Cleanup(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName,
        ULONG Flags)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD_VOID(
            return self->Cleanup(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileName,
                Flags);
        )
    }
    static VOID Close(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD_VOID(
            return self->Close(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2);
        )
    }
    static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PVOID Buffer,
        UINT64 Offset,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->Read(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                Buffer,
                Offset,
                Length,
                PBytesTransferred);
        )
    }
    static NTSTATUS Write(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PVOID Buffer,
        UINT64 Offset,
        ULONG Length,
        BOOLEAN WriteToEndOfFile,
        BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->Write(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                Buffer,
                Offset,
                Length,
                WriteToEndOfFile,
                ConstrainedIo,
                PBytesTransferred,
                FileInfo);
        )
    }
    static NTSTATUS Flush(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->Flush(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileInfo);
        )
    }
    static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetFileInfo(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileInfo);
        )
    }
    static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        UINT32 FileAttributes,
        UINT64 CreationTime,
        UINT64 LastAccessTime,
        UINT64 LastWriteTime,
        UINT64 ChangeTime,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->SetBasicInfo(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileAttributes,
                CreationTime,
                LastAccessTime,
                LastWriteTime,
                ChangeTime,
                FileInfo);
        )
    }
    static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        UINT64 NewSize,
        BOOLEAN SetAllocationSize,
        FileInfo *FileInfo)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->SetFileSize(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                NewSize,
                SetAllocationSize,
                FileInfo);
        )
    }
    static NTSTATUS CanDelete(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->CanDelete(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileName);
        )
    }
    static NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName,
        PWSTR NewFileName,
        BOOLEAN ReplaceIfExists)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->Rename(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileName,
                NewFileName,
                ReplaceIfExists);
        )
    }
    static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PSECURITY_DESCRIPTOR SecurityDescriptor,
        SIZE_T *PSecurityDescriptorSize)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetSecurity(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                SecurityDescriptor,
                PSecurityDescriptorSize);
        )
    }
    static NTSTATUS SetSecurity(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        SECURITY_INFORMATION SecurityInformation,
        PSECURITY_DESCRIPTOR ModificationDescriptor)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->SetSecurity(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                SecurityInformation,
                ModificationDescriptor);
        )
    }
    static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR Pattern,
        PWSTR Marker,
        PVOID Buffer,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->ReadDirectory(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                Pattern,
                Marker,
                Buffer,
                Length,
                PBytesTransferred);
        )
    }
    static NTSTATUS ResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName,
        UINT32 ReparsePointIndex,
        BOOLEAN ResolveLastPathComponent,
        PIO_STATUS_BLOCK PIoStatus,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->ResolveReparsePoints(
                FileName,
                ReparsePointIndex,
                ResolveLastPathComponent,
                PIoStatus,
                Buffer,
                PSize);
        )
    }
    static NTSTATUS GetReparsePointByName(FSP_FILE_SYSTEM *FileSystem0,
        PVOID Context,
        PWSTR FileName,
        BOOLEAN IsDirectory,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetReparsePointByName(
                FileName,
                IsDirectory,
                Buffer,
                PSize);
        )
    }
    static NTSTATUS GetReparsePoint(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetReparsePoint(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileName,
                Buffer,
                PSize);
        )
    }
    static NTSTATUS SetReparsePoint(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName,
        PVOID Buffer,
        SIZE_T Size)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->SetReparsePoint(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileName,
                Buffer,
                Size);
        )
    }
    static NTSTATUS DeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName,
        PVOID Buffer,
        SIZE_T Size)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->DeleteReparsePoint(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileName,
                Buffer,
                Size);
        )
    }
    static NTSTATUS GetStreamInfo(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PVOID Buffer,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetStreamInfo(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                Buffer,
                Length,
                PBytesTransferred);
        )
    }
    static FSP_FILE_SYSTEM_INTERFACE *Interface()
    {
        static FSP_FILE_SYSTEM_INTERFACE _Interface =
        {
            GetVolumeInfo,
            SetVolumeLabel_,
            GetSecurityByName,
            Create,
            Open,
            Overwrite,
            Cleanup,
            Close,
            Read,
            Write,
            Flush,
            GetFileInfo,
            SetBasicInfo,
            SetFileSize,
            CanDelete,
            Rename,
            GetSecurity,
            SetSecurity,
            ReadDirectory,
            ResolveReparsePoints,
            GetReparsePoint,
            SetReparsePoint,
            DeleteReparsePoint,
            GetStreamInfo,
        };
        return &_Interface;
    }

private:
    /* disallow copy and assignment */
    FileSystem(const FileSystem &);
    FileSystem &operator=(const FileSystem &);

private:
    VolumeParams _VolumeParams;
    FSP_FILE_SYSTEM *_FileSystem;
};

class Service
{
public:
    /* ctor/dtor */
    Service(PWSTR ServiceName) : _Service(0)
    {
        Initialize();
        FspServiceCreate(ServiceName, OnStart, OnStop, 0, &_Service);
        if (0 != _Service)
            _Service->UserContext = this;
    }
    virtual ~Service()
    {
        if (0 != _Service)
            FspServiceDelete(_Service);
    }

    /* control */
    ULONG Run()
    {
        if (0 == _Service)
        {
            FspServiceLog(EVENTLOG_ERROR_TYPE,
                L"The service cannot be created (Status=%lx).",
                STATUS_INSUFFICIENT_RESOURCES);
            return FspWin32FromNtStatus(STATUS_INSUFFICIENT_RESOURCES);
        }
        FspServiceAllowConsoleMode(_Service);
        NTSTATUS Result = FspServiceLoop(_Service);
        ULONG ExitCode = FspServiceGetExitCode(_Service);
        if (!NT_SUCCESS(Result))
        {
            FspServiceLog(EVENTLOG_ERROR_TYPE,
                L"The service has failed to run (Status=%lx).",
                Result);
            return FspWin32FromNtStatus(Result);
        }
        return ExitCode;
    }
    VOID Stop()
    {
        if (0 == _Service)
            return;
        FspServiceStop(_Service);
    }
    VOID RequestTime(ULONG Time)
    {
        if (0 == _Service)
            return;
        FspServiceRequestTime(_Service, Time);
    }
    ULONG GetExitCode()
    {
        return 0 != _Service ? FspServiceGetExitCode(_Service) : ERROR_NO_SYSTEM_RESOURCES;
    }
    VOID SetExitCode(ULONG ExitCode)
    {
        if (0 == _Service)
            return;
        FspServiceSetExitCode(_Service, ExitCode);
    }
    FSP_SERVICE *ServiceHandle()
    {
        return _Service;
    }
    static VOID Log(ULONG Type, PWSTR Format, ...)
    {
        va_list ap;
        va_start(ap, Format);
        FspServiceLogV(Type, Format, ap);
        va_end(ap);
    }
    static VOID LogV(ULONG Type, PWSTR Format, va_list ap)
    {
        FspServiceLogV(Type, Format, ap);
    }

protected:
    /* start/stop */
    virtual NTSTATUS ExceptionHandler()
    {
        return 0xE06D7363/*STATUS_CPP_EH_EXCEPTION*/;
    }
    virtual NTSTATUS OnStart(ULONG Argc, PWSTR *Argv)
    {
        return STATUS_SUCCESS;
    }
    virtual NTSTATUS OnStop()
    {
        return STATUS_SUCCESS;
    }

private:
    /* callbacks */
    static NTSTATUS OnStart(FSP_SERVICE *Service0, ULONG Argc, PWSTR *Argv)
    {
        Service *self = (Service *)Service0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->OnStart(Argc, Argv);
        )
    }
    static NTSTATUS OnStop(FSP_SERVICE *Service0)
    {
        Service *self = (Service *)Service0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->OnStop();
        )
    }

private:
    /* disallow copy and assignment */
    Service(const Service &);
    Service &operator=(const Service &);

private:
    FSP_SERVICE *_Service;
};

}

#undef FSP_CPP_EXCEPTION_GUARD
#undef FSP_CPP_EXCEPTION_GUARD_VOID

#endif
