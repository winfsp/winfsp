/**
 * @file winfsp/winfsp.hpp
 * WinFsp C++ Layer.
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

class FileSystemBase
{
public:
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
    FileSystemBase()
    {
    }
    virtual ~FileSystemBase()
    {
    }

    /* operations */
    virtual NTSTATUS ExceptionHandler()
    {
        return STATUS_UNEXPECTED_IO_ERROR;
    }
    virtual NTSTATUS Init(PVOID Host)
    {
        return STATUS_SUCCESS;
    }
    virtual NTSTATUS Mounted(PVOID Host)
    {
        return STATUS_SUCCESS;
    }
    virtual VOID Unmounted(PVOID Host)
    {
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
        return SeekableReadDirectory(
            FileNode,
            FileDesc,
            Pattern,
            Marker,
            Buffer,
            Length,
            PBytesTransferred);
    }
    virtual NTSTATUS ReadDirectoryEntry(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR Pattern,
        PWSTR Marker,
        PVOID *PContext,
        DirInfo *DirInfo)
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
        return FspFileSystemResolveReparsePoints(
            0,
            GetReparsePointByName,
            this,
            FileName,
            ReparsePointIndex,
            ResolveLastPathComponent,
            PIoStatus,
            Buffer,
            PSize);
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

    /* helpers */
    static NTSTATUS NtStatusFromWin32(DWORD Error)
    {
        return FspNtStatusFromWin32(Error);
    }
    static DWORD Win32FromNtStatus(NTSTATUS Status)
    {
        return FspWin32FromNtStatus(Status);
    }
    static VOID DeleteDirectoryBuffer(PVOID *PDirBuffer)
    {
        FspFileSystemDeleteDirectoryBuffer(PDirBuffer);
    }
    NTSTATUS SeekableReadDirectory(
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR Pattern,
        PWSTR Marker,
        PVOID Buffer,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        PVOID Context = 0;
        union
        {
            UINT8 B[FIELD_OFFSET(FileSystemBase::DirInfo, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
            FileSystemBase::DirInfo D;
        } DirInfoBuf;
        FileSystemBase::DirInfo *DirInfo = &DirInfoBuf.D;
        NTSTATUS Result = STATUS_SUCCESS;
        *PBytesTransferred = 0;
        for (;;)
        {
            Result = ReadDirectoryEntry(FileNode, FileDesc, Pattern, Marker, &Context, DirInfo);
            if (STATUS_NO_MORE_FILES == Result)
            {
                Result = STATUS_SUCCESS;
                break;
            }
            if (!NT_SUCCESS(Result))
                break;
            if (!FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred))
                break;
        }
        if (!NT_SUCCESS(Result))
            return Result;
        return STATUS_SUCCESS;
    }
    NTSTATUS BufferedReadDirectory(
        PVOID *PDirBuffer,
        PVOID FileNode,
        PVOID FileDesc,
        PWSTR Pattern,
        PWSTR Marker,
        PVOID Buffer,
        ULONG Length,
        PULONG PBytesTransferred)
    {
        PVOID Context = 0;
        union
        {
            UINT8 B[FIELD_OFFSET(FileSystemBase::DirInfo, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
            FileSystemBase::DirInfo D;
        } DirInfoBuf;
        FileSystemBase::DirInfo *DirInfo = &DirInfoBuf.D;
        NTSTATUS Result = STATUS_SUCCESS;
        *PBytesTransferred = 0;
        if (FspFileSystemAcquireDirectoryBuffer(PDirBuffer, 0 == Marker, &Result))
        {
            try
            {
                for (;;)
                {
                    Result = ReadDirectoryEntry(FileNode, FileDesc, Pattern, Marker, &Context, DirInfo);
                    if (STATUS_NO_MORE_FILES == Result)
                    {
                        Result = STATUS_SUCCESS;
                        break;
                    }
                    if (!NT_SUCCESS(Result))
                        break;
                    if (!FspFileSystemFillDirectoryBuffer(PDirBuffer, DirInfo, &Result))
                        break;
                }
            }
            catch (...)
            {
                FspFileSystemReleaseDirectoryBuffer(PDirBuffer);
                throw;
            }
            FspFileSystemReleaseDirectoryBuffer(PDirBuffer);
        }
        if (!NT_SUCCESS(Result))
            return Result;
        FspFileSystemReadDirectoryBuffer(PDirBuffer, Marker, Buffer, Length, PBytesTransferred);
        return STATUS_SUCCESS;
    }
    BOOLEAN FindReparsePoint(
        PWSTR FileName, PUINT32 PReparsePointIndex)
    {
        return FspFileSystemFindReparsePoint(
            0,
            GetReparsePointByName,
            this,
            FileName,
            PReparsePointIndex);
    }
    static NTSTATUS CanReplaceReparsePoint(
        PVOID CurrentReparseData, SIZE_T CurrentReparseDataSize,
        PVOID ReplaceReparseData, SIZE_T ReplaceReparseDataSize)
    {
        return FspFileSystemCanReplaceReparsePoint(
            CurrentReparseData, CurrentReparseDataSize,
            ReplaceReparseData, ReplaceReparseDataSize);
    }
    static BOOLEAN AddStreamInfo(StreamInfo *StreamInfo,
        PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
    {
        return FspFileSystemAddStreamInfo(StreamInfo, Buffer, Length, PBytesTransferred);
    }

private:
    static NTSTATUS GetReparsePointByName(FSP_FILE_SYSTEM *FileSystem,
        PVOID Context,
        PWSTR FileName,
        BOOLEAN IsDirectory,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        FileSystemBase *self = (FileSystemBase *)Context;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetReparsePointByName(
                FileName,
                IsDirectory,
                Buffer,
                PSize);
        )
    }

private:
    /* disallow copy and assignment */
    FileSystemBase(const FileSystemBase &);
    FileSystemBase &operator=(const FileSystemBase &);
};

class FileSystemHost
{
public:
    /* ctor/dtor */
    FileSystemHost(FileSystemBase &FileSystem) :
        _VolumeParams(), _FileSystemPtr(0), _FileSystem(&FileSystem)
    {
        Initialize();
        _VolumeParams.UmFileContextIsFullContext = 1;
    }
    virtual ~FileSystemHost()
    {
        if (0 != _FileSystemPtr)
            FspFileSystemDelete(_FileSystemPtr);
    }

    /* properties */
    UINT16 SectorSize()
    {
        return _VolumeParams.SectorSize;
    }
    VOID SetSectorSize(UINT16 SectorSize)
    {
        _VolumeParams.SectorSize = SectorSize;
    }
    UINT16 SectorsPerAllocationUnit()
    {
        return _VolumeParams.SectorsPerAllocationUnit;
    }
    VOID SetSectorsPerAllocationUnit(UINT16 SectorsPerAllocationUnit)
    {
        _VolumeParams.SectorsPerAllocationUnit = SectorsPerAllocationUnit;
    }
    UINT16 MaxComponentLength()
    {
        return _VolumeParams.MaxComponentLength;
    }
    VOID SetMaxComponentLength(UINT16 MaxComponentLength)
    {
        _VolumeParams.MaxComponentLength = MaxComponentLength;
    }
    UINT64 VolumeCreationTime()
    {
        return _VolumeParams.VolumeCreationTime;
    }
    VOID SetVolumeCreationTime(UINT64 VolumeCreationTime)
    {
        _VolumeParams.VolumeCreationTime = VolumeCreationTime;
    }
    UINT32 VolumeSerialNumber()
    {
        return _VolumeParams.VolumeSerialNumber;
    }
    VOID SetVolumeSerialNumber(UINT32 VolumeSerialNumber)
    {
        _VolumeParams.VolumeSerialNumber = VolumeSerialNumber;
    }
    UINT32 FileInfoTimeout()
    {
        return _VolumeParams.FileInfoTimeout;
    }
    VOID SetFileInfoTimeout(UINT32 FileInfoTimeout)
    {
        _VolumeParams.FileInfoTimeout = FileInfoTimeout;
    }
    BOOLEAN CaseSensitiveSearch()
    {
        return _VolumeParams.CaseSensitiveSearch;
    }
    VOID SetCaseSensitiveSearch(BOOLEAN CaseSensitiveSearch)
    {
        _VolumeParams.CaseSensitiveSearch = !!CaseSensitiveSearch;
    }
    BOOLEAN CasePreservedNames()
    {
        return _VolumeParams.CasePreservedNames;
    }
    VOID SetCasePreservedNames(BOOLEAN CasePreservedNames)
    {
        _VolumeParams.CasePreservedNames = !!CasePreservedNames;
    }
    BOOLEAN UnicodeOnDisk()
    {
        return _VolumeParams.UnicodeOnDisk;
    }
    VOID SetUnicodeOnDisk(BOOLEAN UnicodeOnDisk)
    {
        _VolumeParams.UnicodeOnDisk = !!UnicodeOnDisk;
    }
    BOOLEAN PersistentAcls()
    {
        return _VolumeParams.PersistentAcls;
    }
    VOID SetPersistentAcls(BOOLEAN PersistentAcls)
    {
        _VolumeParams.PersistentAcls = !!PersistentAcls;
    }
    BOOLEAN ReparsePoints()
    {
        return _VolumeParams.ReparsePoints;
    }
    VOID SetReparsePoints(BOOLEAN ReparsePoints)
    {
        _VolumeParams.ReparsePoints = !!ReparsePoints;
    }
    BOOLEAN ReparsePointsAccessCheck()
    {
        return _VolumeParams.ReparsePointsAccessCheck;
    }
    VOID SetReparsePointsAccessCheck(BOOLEAN ReparsePointsAccessCheck)
    {
        _VolumeParams.ReparsePointsAccessCheck = !!ReparsePointsAccessCheck;
    }
    BOOLEAN NamedStreams()
    {
        return _VolumeParams.NamedStreams;
    }
    VOID SetNamedStreams(BOOLEAN NamedStreams)
    {
        _VolumeParams.NamedStreams = !!NamedStreams;
    }
    BOOLEAN PostCleanupWhenModifiedOnly()
    {
        return _VolumeParams.PostCleanupWhenModifiedOnly;
    }
    VOID SetPostCleanupWhenModifiedOnly(BOOLEAN PostCleanupWhenModifiedOnly)
    {
        _VolumeParams.PostCleanupWhenModifiedOnly = !!PostCleanupWhenModifiedOnly;
    }
    BOOLEAN PassQueryDirectoryPattern()
    {
        return _VolumeParams.PassQueryDirectoryPattern;
    }
    VOID SetPassQueryDirectoryPattern(BOOLEAN PassQueryDirectoryPattern)
    {
        _VolumeParams.PassQueryDirectoryPattern = !!PassQueryDirectoryPattern;
    }
    BOOLEAN FlushAndPurgeOnCleanup()
    {
        return _VolumeParams.FlushAndPurgeOnCleanup;
    }
    VOID SetFlushAndPurgeOnCleanup(BOOLEAN FlushAndPurgeOnCleanup)
    {
        _VolumeParams.FlushAndPurgeOnCleanup = !!FlushAndPurgeOnCleanup;
    }
    PWSTR Prefix()
    {
        return _VolumeParams.Prefix;
    }
    VOID SetPrefix(PWSTR Prefix)
    {
        int Size = lstrlenW(Prefix) * sizeof(WCHAR);
        if (Size > sizeof _VolumeParams.Prefix - sizeof(WCHAR))
            Size = sizeof _VolumeParams.Prefix - sizeof(WCHAR);
        RtlCopyMemory(_VolumeParams.Prefix, Prefix, Size);
        _VolumeParams.Prefix[Size / sizeof(WCHAR)] = L'\0';
    }
    PWSTR FileSystemName()
    {
        return _VolumeParams.FileSystemName;
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
            _VolumeParams.Prefix[0] ? L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME,
            MountPoint);
    }
    NTSTATUS Mount(PWSTR MountPoint,
        PSECURITY_DESCRIPTOR SecurityDescriptor = 0,
        BOOLEAN Synchronized = FALSE,
        UINT32 DebugLog = 0)
    {
        NTSTATUS Result;
        try
        {
            Result = _FileSystem->Init(this);
        }
        catch (...)
        {
            Result = _FileSystem->ExceptionHandler();
        }
        if (!NT_SUCCESS(Result))
            return Result;
        Result = FspFileSystemCreate(
            _VolumeParams.Prefix[0] ? L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME,
            &_VolumeParams, Interface(), &_FileSystemPtr);
        if (!NT_SUCCESS(Result))
            return Result;
        _FileSystemPtr->UserContext = _FileSystem;
        FspFileSystemSetOperationGuardStrategy(_FileSystemPtr, Synchronized ?
            FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE :
            FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE);
        FspFileSystemSetDebugLog(_FileSystemPtr, DebugLog);
        Result = FspFileSystemSetMountPointEx(_FileSystemPtr, MountPoint, SecurityDescriptor);
        if (NT_SUCCESS(Result))
        {
            try
            {
                Result = _FileSystem->Mounted(this);
            }
            catch (...)
            {
                Result = _FileSystem->ExceptionHandler();
            }
            if (NT_SUCCESS(Result))
            {
                Result = FspFileSystemStartDispatcher(_FileSystemPtr, 0);
                if (!NT_SUCCESS(Result))
                    try
                    {
                        _FileSystem->Unmounted(this);
                    }
                    catch (...)
                    {
                        _FileSystem->ExceptionHandler();
                    }
            }
        }
        if (!NT_SUCCESS(Result))
        {
            FspFileSystemDelete(_FileSystemPtr);
            _FileSystemPtr = 0;
        }
        return Result;
    }
    VOID Unmount()
    {
        FspFileSystemStopDispatcher(_FileSystemPtr);
        try
        {
            _FileSystem->Unmounted(this);
        }
        catch (...)
        {
            _FileSystem->ExceptionHandler();
        }
        _FileSystemPtr->UserContext = 0;
        FspFileSystemDelete(_FileSystemPtr);
        _FileSystemPtr = 0;
    }
    PWSTR MountPoint()
    {
        return 0 != _FileSystemPtr ? FspFileSystemMountPoint(_FileSystemPtr) : 0;
    }
    FSP_FILE_SYSTEM *FileSystemHandle()
    {
        return _FileSystemPtr;
    }
    FileSystemBase &FileSystem()
    {
        return *_FileSystem;
    }
    static NTSTATUS SetDebugLogFile(PWSTR FileName)
    {
        HANDLE Handle;
        if ('-' == FileName[0] && '\0' == FileName[1])
            Handle = GetStdHandle(STD_ERROR_HANDLE);
        else
            Handle = CreateFileW(
                FileName,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == Handle)
            return FspNtStatusFromWin32(GetLastError());
        FspDebugLogSetHandle(Handle);
        return STATUS_SUCCESS;
    }

private:
    /* FSP_FILE_SYSTEM_INTERFACE */
    static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem0,
        FSP_FSCTL_VOLUME_INFO *VolumeInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->GetVolumeInfo(
                VolumeInfo);
        )
    }
    static NTSTATUS SetVolumeLabel_(FSP_FILE_SYSTEM *FileSystem0,
        PWSTR VolumeLabel,
        FSP_FSCTL_VOLUME_INFO *VolumeInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
        FSP_CPP_EXCEPTION_GUARD(
            return self->Flush(
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext,
                (PVOID)(UINT_PTR)((FSP_FSCTL_TRANSACT_FULL_CONTEXT *)FullContext)->UserContext2,
                FileInfo);
        )
    }
    static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FSP_FSCTL_FILE_INFO *FileInfo)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
    static NTSTATUS GetReparsePoint(FSP_FILE_SYSTEM *FileSystem0,
        PVOID FullContext,
        PWSTR FileName,
        PVOID Buffer,
        PSIZE_T PSize)
    {
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
        FileSystemBase *self = (FileSystemBase *)FileSystem0->UserContext;
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
    FileSystemHost(const FileSystemHost &);
    FileSystemHost &operator=(const FileSystemHost &);

private:
    FSP_FSCTL_VOLUME_PARAMS _VolumeParams;
    FSP_FILE_SYSTEM *_FileSystemPtr;
    FileSystemBase *_FileSystem;
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
