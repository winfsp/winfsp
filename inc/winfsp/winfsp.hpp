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

namespace Fsp
{

class FileSystem
{
public:
    typedef FSP_FSCTL_VOLUME_PARAMS VOLUME_PARAMS;
    typedef FSP_FSCTL_VOLUME_INFO VOLUME_INFO;
    typedef FSP_FSCTL_FILE_INFO FILE_INFO;
    typedef FSP_FSCTL_OPEN_FILE_INFO OPEN_FILE_INFO;
    typedef FSP_FSCTL_DIR_INFO DIR_INFO;
    typedef FSP_FSCTL_STREAM_INFO STREAM_INFO;
    struct FILE_CONTEXT
    {
        PVOID FileNode;
        PVOID FileDesc;
    };
    enum CLEANUP_FLAGS
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
        static NTSTATUS LoadResult = FspLoad(0);
        _VolumeParams.SectorSize = 4096;
        _VolumeParams.SectorsPerAllocationUnit = 1;
        _VolumeParams.MaxComponentLength = 255;
        _VolumeParams.FileInfoTimeout = 1000;
        GetSystemTimeAsFileTime((PFILETIME)&_VolumeParams.VolumeCreationTime);
        _VolumeParams.VolumeSerialNumber = (UINT32)(_VolumeParams.VolumeCreationTime / (10000 * 1000));
        _VolumeParams.UmFileContextIsUserContext2 = 1;
    }
    virtual ~FileSystem()
    {
        if (0 != _FileSystem)
            FspFileSystemDelete(_FileSystem);
    }

    /* properties */
    const VOLUME_PARAMS *VolumeParams()
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
            Result = FspFileSystemSetMountPointEx(_FileSystem, MountPoint, SecurityDescriptor);
            if (NT_SUCCESS(Result))
                Result = FspFileSystemStartDispatcher(_FileSystem, 0);
        }
        if (NT_SUCCESS(Result))
        {
            _FileSystem->UserContext = this;
            FspFileSystemSetOperationGuardStrategy(_FileSystem, Synchronized ?
                FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE :
                FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE);
            FspFileSystemSetDebugLog(_FileSystem, DebugLog);
        }
        else if (0 != _FileSystem)
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
        DIR_INFO *DirInfo, PNTSTATUS PResult)
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
    static BOOLEAN AddDirInfo(DIR_INFO *DirInfo,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
    {
        return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
    }
    static BOOLEAN AddStreamInfo(STREAM_INFO *StreamInfo,
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
    virtual NTSTATUS GetVolumeInfo(
        VOLUME_INFO *VolumeInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetVolumeLabel_(
        PWSTR VolumeLabel,
        VOLUME_INFO *VolumeInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetSecurityByName(
        PWSTR FileName, PUINT32 PFileAttributes/* or ReparsePointIndex */,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Create(
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
        FILE_CONTEXT *FileContext, OPEN_FILE_INFO *OpenFileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Open(
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        FILE_CONTEXT *FileContext, OPEN_FILE_INFO *OpenFileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Overwrite(
        const FILE_CONTEXT *FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
        FILE_INFO *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual VOID Cleanup(
        const FILE_CONTEXT *FileContext, PWSTR FileName, ULONG Flags)
    {
    }
    virtual VOID Close(
        const FILE_CONTEXT *FileContext)
    {
    }
    virtual NTSTATUS Read(
        const FILE_CONTEXT *FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        PULONG PBytesTransferred)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Write(
        const FILE_CONTEXT *FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred, FILE_INFO *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Flush(
        const FILE_CONTEXT *FileContext,
        FILE_INFO *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetFileInfo(
        const FILE_CONTEXT *FileContext,
        FILE_INFO *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetBasicInfo(
        const FILE_CONTEXT *FileContext, UINT32 FileAttributes,
        UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
        FILE_INFO *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetFileSize(
        const FILE_CONTEXT *FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
        FILE_INFO *FileInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS CanDelete(
        const FILE_CONTEXT *FileContext, PWSTR FileName)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS Rename(
        const FILE_CONTEXT *FileContext,
        PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetSecurity(
        const FILE_CONTEXT *FileContext,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetSecurity(
        const FILE_CONTEXT *FileContext,
        SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS ReadDirectory(
        const FILE_CONTEXT *FileContext, PWSTR Pattern, PWSTR Marker,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS ResolveReparsePoints(
        PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
        PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
    {
        return FspFileSystemResolveReparsePoints(_FileSystem, GetReparsePointByName, 0,
            FileName, ReparsePointIndex, ResolveLastPathComponent,
            PIoStatus, Buffer, PSize);
    }
    virtual NTSTATUS GetReparsePointByName(
        PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetReparsePoint(
        const FILE_CONTEXT *FileContext,
        PWSTR FileName, PVOID Buffer, PSIZE_T PSize)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS SetReparsePoint(
        const FILE_CONTEXT *FileContext,
        PWSTR FileName, PVOID Buffer, SIZE_T Size)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS DeleteReparsePoint(
        const FILE_CONTEXT *FileContext,
        PWSTR FileName, PVOID Buffer, SIZE_T Size)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    virtual NTSTATUS GetStreamInfo(
        const FILE_CONTEXT *FileContext, PVOID Buffer, ULONG Length,
        PULONG PBytesTransferred)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

private:
    /* FSP_FILE_SYSTEM_INTERFACE */
#define FSP_FSOP_PROLOG                 try {
#define FSP_FSOP_EPILOG                 } catch (...) { return STATUS_UNEXPECTED_IO_ERROR; }
#define FSP_FSOP_EPILOG_VOID            } catch (...) { return; }
    static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem0,
        VOLUME_INFO *VolumeInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        return self->GetVolumeInfo(VolumeInfo);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS SetVolumeLabel_(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR VolumeLabel,
        VOLUME_INFO *VolumeInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        return self->SetVolumeLabel_(VolumeLabel, VolumeInfo);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName, PUINT32 PFileAttributes/* or ReparsePointIndex */,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        return self->GetSecurityByName(
            FileName, PFileAttributes, SecurityDescriptor, PSecurityDescriptorSize);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS Create(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
        PVOID *FullContext, FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext = { 0 };
        NTSTATUS Result = self->Create(
            FileName, CreateOptions, GrantedAccess, FileAttributes, SecurityDescriptor, AllocationSize,
            &FileContext, FspFileSystemGetOpenFileInfo(FileInfo));
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext = (UINT64)(UINT_PTR)FileContext.FileNode;
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2 = (UINT64)(UINT_PTR)FileContext.FileDesc;
        return Result;
        FSP_FSOP_EPILOG
    }
    static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        PVOID *FullContext, FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext = { 0 };
        NTSTATUS Result = self->Open(
            FileName, CreateOptions, GrantedAccess,
            &FileContext, FspFileSystemGetOpenFileInfo(FileInfo));
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext = (UINT64)(UINT_PTR)FileContext.FileNode;
        ((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2 = (UINT64)(UINT_PTR)FileContext.FileDesc;
        return Result;
        FSP_FSOP_EPILOG
    }
    static NTSTATUS Overwrite(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
        FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->Overwrite(&FileContext, FileAttributes, ReplaceFileAttributes, AllocationSize,
            FileInfo);
        FSP_FSOP_EPILOG
    }
    static VOID Cleanup(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, PWSTR FileName, ULONG Flags)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->Cleanup(&FileContext, FileName, (CLEANUP_FLAGS)Flags);
        FSP_FSOP_EPILOG_VOID
    }
    static VOID Close(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->Close(&FileContext);
        FSP_FSOP_EPILOG_VOID
    }
    static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        PULONG PBytesTransferred)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->Read(&FileContext, Buffer, Offset, Length, PBytesTransferred);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS Write(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred, FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->Write(&FileContext, Buffer, Offset, Length, WriteToEndOfFile, ConstrainedIo,
            PBytesTransferred, FileInfo);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS Flush(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->Flush(&FileContext, FileInfo);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->GetFileInfo(&FileContext, FileInfo);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, UINT32 FileAttributes,
        UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
        FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->SetBasicInfo(&FileContext, FileAttributes,
            CreationTime, LastAccessTime, LastWriteTime, ChangeTime,
            FileInfo);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
        FILE_INFO *FileInfo)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->SetFileSize(&FileContext, NewSize, SetAllocationSize, FileInfo);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS CanDelete(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, PWSTR FileName)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->CanDelete(&FileContext, FileName);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS Rename(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->Rename(&FileContext, FileName, NewFileName, ReplaceIfExists);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS GetSecurity(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->GetSecurity(&FileContext, SecurityDescriptor, PSecurityDescriptorSize);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS SetSecurity(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->SetSecurity(&FileContext, SecurityInformation, ModificationDescriptor);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, PWSTR Pattern, PWSTR Marker,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->ReadDirectory(&FileContext, Pattern, Marker, Buffer, Length, PBytesTransferred);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS ResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR FileName, UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
        PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        return self->ResolveReparsePoints(FileName, ReparsePointIndex, ResolveLastPathComponent,
            PIoStatus, Buffer, PSize);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS GetReparsePointByName(
        FSP_FILE_SYSTEM *FileSystem0, PVOID Context,
        PWSTR FileName, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        return self->GetReparsePointByName(FileName, IsDirectory, Buffer, PSize);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS GetReparsePoint(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName, PVOID Buffer, PSIZE_T PSize)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->GetReparsePoint(&FileContext, FileName, Buffer, PSize);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS SetReparsePoint(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName, PVOID Buffer, SIZE_T Size)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->SetReparsePoint(&FileContext, FileName, Buffer, Size);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS DeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName, PVOID Buffer, SIZE_T Size)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->DeleteReparsePoint(&FileContext, FileName, Buffer, Size);
        FSP_FSOP_EPILOG
    }
    static NTSTATUS GetStreamInfo(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext, PVOID Buffer, ULONG Length,
        PULONG PBytesTransferred)
    {
        FSP_FSOP_PROLOG
        FileSystem *self = (FileSystem *)FileSystem0->UserContext;
        FILE_CONTEXT FileContext;
        FileContext.FileNode = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext;
        FileContext.FileDesc = (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2;
        return self->GetStreamInfo(&FileContext, Buffer, Length, PBytesTransferred);
        FSP_FSOP_EPILOG
    }
#undef FSP_FSOP_PROLOG
#undef FSP_FSOP_EPILOG
#undef FSP_FSOP_EPILOG_VOID
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
    VOLUME_PARAMS _VolumeParams;
    FSP_FILE_SYSTEM *_FileSystem;
};

}

#endif
