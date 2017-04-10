/**
 * @file dotnet/FileSystem.cs
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

using System;
using System.Runtime.InteropServices;
using System.Security.AccessControl;

using Fsp.Interop;

namespace Fsp
{

    public partial class FileSystem : IDisposable
    {
        /* types */
        public class DirectoryBuffer : IDisposable
        {
            ~DirectoryBuffer()
            {
                Dispose(false);
            }
            public void Dispose()
            {
                lock (this)
                    Dispose(true);
                GC.SuppressFinalize(true);
            }
            protected virtual void Dispose(bool disposing)
            {
                Api.FspFileSystemDeleteDirectoryBuffer(ref DirBuffer);
            }

            internal IntPtr DirBuffer;
        }

        /* ctor/dtor */
        public FileSystem()
        {
            _VolumeParams.Flags = VolumeParams.UmFileContextIsFullContext;
        }
        ~FileSystem()
        {
            Dispose(false);
        }
        public void Dispose()
        {
            lock (this)
                Dispose(true);
            GC.SuppressFinalize(true);
        }
        protected virtual void Dispose(bool disposing)
        {
            if (IntPtr.Zero != _FileSystem)
            {
                Api.FspFileSystemStopDispatcher(_FileSystem);
                Api.SetUserContext(_FileSystem, null);
                Api.FspFileSystemDelete(_FileSystem);
                _FileSystem = IntPtr.Zero;
            }
        }

        /* properties */
        public void SetSectorSize(UInt16 SectorSize)
        {
            _VolumeParams.SectorSize = SectorSize;
        }
        public void SetSectorsPerAllocationUnit(UInt16 SectorsPerAllocationUnit)
        {
            _VolumeParams.SectorsPerAllocationUnit = SectorsPerAllocationUnit;
        }
        public void SetMaxComponentLength(UInt16 MaxComponentLength)
        {
            _VolumeParams.MaxComponentLength = MaxComponentLength;
        }
        public void SetVolumeCreationTime(UInt64 VolumeCreationTime)
        {
            _VolumeParams.VolumeCreationTime = VolumeCreationTime;
        }
        public void SetVolumeSerialNumber(UInt32 VolumeSerialNumber)
        {
            _VolumeParams.VolumeSerialNumber = VolumeSerialNumber;
        }
        public void SetFileInfoTimeout(UInt32 FileInfoTimeout)
        {
            _VolumeParams.FileInfoTimeout = FileInfoTimeout;
        }
        public void SetCaseSensitiveSearch(Boolean CaseSensitiveSearch)
        {
            _VolumeParams.Flags |= CaseSensitiveSearch ? VolumeParams.CaseSensitiveSearch : 0;
        }
        public void SetCasePreservedNames(Boolean CasePreservedNames)
        {
            _VolumeParams.Flags |= CasePreservedNames ? VolumeParams.CasePreservedNames : 0;
        }
        public void SetUnicodeOnDisk(Boolean UnicodeOnDisk)
        {
            _VolumeParams.Flags |= UnicodeOnDisk ? VolumeParams.UnicodeOnDisk : 0;
        }
        public void SetPersistentAcls(Boolean PersistentAcls)
        {
            _VolumeParams.Flags |= PersistentAcls ? VolumeParams.PersistentAcls : 0;
        }
        public void SetReparsePoints(Boolean ReparsePoints)
        {
            _VolumeParams.Flags |= ReparsePoints ? VolumeParams.ReparsePoints : 0;
        }
        public void SetReparsePointsAccessCheck(Boolean ReparsePointsAccessCheck)
        {
            _VolumeParams.Flags |= ReparsePointsAccessCheck ? VolumeParams.ReparsePointsAccessCheck : 0;
        }
        public void SetNamedStreams(Boolean NamedStreams)
        {
            _VolumeParams.Flags |= NamedStreams ? VolumeParams.NamedStreams : 0;
        }
        public void SetPostCleanupWhenModifiedOnly(Boolean PostCleanupWhenModifiedOnly)
        {
            _VolumeParams.Flags |= PostCleanupWhenModifiedOnly ? VolumeParams.PostCleanupWhenModifiedOnly : 0;
        }
        public void SetPassQueryDirectoryPattern(Boolean PassQueryDirectoryPattern)
        {
            _VolumeParams.Flags |= PassQueryDirectoryPattern ? VolumeParams.PassQueryDirectoryPattern : 0;
        }
        public void SetPrefix(String Prefix)
        {
            _VolumeParams.SetPrefix(Prefix);
        }
        public void SetFileSystemName(String FileSystemName)
        {
            _VolumeParams.SetFileSystemName(FileSystemName);
        }

        /* control */
        public Int32 Preflight(String MountPoint)
        {
            return Api.FspFileSystemPreflight(
                _VolumeParams.IsPrefixEmpty() ? "WinFsp.Disk" : "WinFsp.Net",
                MountPoint);
        }
        public Int32 Mount(String MountPoint,
            Byte[] SecurityDescriptor = null,
            Boolean Synchronized = false,
            UInt32 DebugLog = 0)
        {
            Int32 Result;
            Result = Api.FspFileSystemCreate(
                _VolumeParams.IsPrefixEmpty() ? "WinFsp.Disk" : "WinFsp.Net",
                ref _VolumeParams, _FileSystemInterfacePtr, out _FileSystem);
            if (0 <= Result)
            {
                Api.SetUserContext(_FileSystem, this);
                Api.FspFileSystemSetOperationGuardStrategy(_FileSystem, Synchronized ?
                    1/*FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE*/ :
                    0/*FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE*/);
                Api.FspFileSystemSetDebugLog(_FileSystem, DebugLog);
                Result = Api.FspFileSystemSetMountPointEx(_FileSystem, MountPoint,
                    SecurityDescriptor);
                if (0 <= Result)
                    Result = Api.FspFileSystemStartDispatcher(_FileSystem, 0);
            }
            if (0 > Result && IntPtr.Zero != _FileSystem)
            {
                Api.SetUserContext(_FileSystem, null);
                Api.FspFileSystemDelete(_FileSystem);
                _FileSystem = IntPtr.Zero;
            }
            return Result;
        }
        public void Unmount()
        {
            Dispose();
        }
        public String MountPoint()
        {
            return IntPtr.Zero != _FileSystem ?
                Marshal.PtrToStringUni(Api.FspFileSystemMountPoint(_FileSystem)) : null;
        }
        public IntPtr FileSystemHandle()
        {
            return _FileSystem;
        }

        /* helpers */
        public Int32 SeekableReadDirectory(
            Object FileNode,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            Object Context = null;
            String FileName;
            DirInfo DirInfo = default(DirInfo);
            PBytesTransferred = default(UInt32);
            while (ReadDirectoryEntry(FileNode, FileDesc, Pattern, Marker,
                ref Context, out FileName, out DirInfo.FileInfo))
            {
                DirInfo.SetFileNameBuf(FileName);
                if (!Api.FspFileSystemAddDirInfo(ref DirInfo, Buffer, Length,
                    out PBytesTransferred))
                    break;
            }
            return STATUS_SUCCESS;
        }
        public Int32 BufferedReadDirectory(
            DirectoryBuffer DirectoryBuffer,
            Object FileNode,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            Object Context = null;
            String FileName;
            DirInfo DirInfo = default(DirInfo);
            Int32 DirBufferResult = STATUS_SUCCESS;
            PBytesTransferred = default(UInt32);
            if (Api.FspFileSystemAcquireDirectoryBuffer(ref DirectoryBuffer.DirBuffer, null == Marker,
                out DirBufferResult))
                try
                {
                    while (ReadDirectoryEntry(FileNode, FileDesc, Pattern, Marker,
                        ref Context, out FileName, out DirInfo.FileInfo))
                    {
                        DirInfo.SetFileNameBuf(FileName);
                        if (!Api.FspFileSystemFillDirectoryBuffer(
                            ref DirectoryBuffer.DirBuffer, ref DirInfo, out DirBufferResult))
                            break;
                    }
                }
                finally
                {
                    Api.FspFileSystemReleaseDirectoryBuffer(ref DirectoryBuffer.DirBuffer);
                }
            if (0 > DirBufferResult)
            {
                PBytesTransferred = default(UInt32);
                return DirBufferResult;
            }
            Api.FspFileSystemReadDirectoryBuffer(ref DirectoryBuffer.DirBuffer,
                Marker, Buffer, Length, out PBytesTransferred);
            return STATUS_SUCCESS;
        }
        public static Int32 NtStatusFromWin32(UInt32 Error)
        {
            return Api.FspNtStatusFromWin32(Error);
        }
        public static UInt32 Win32FromNtStatus(Int32 Status)
        {
            return Api.FspWin32FromNtStatus(Status);
        }
        public static Int32 SetDebugLogFile(String FileName)
        {
            return Api.SetDebugLogFile(FileName);
        }

        /* operations */
        protected virtual Int32 ExceptionHandler(Exception ex)
        {
            return STATUS_UNEXPECTED_IO_ERROR;
        }
        protected virtual Int32 GetVolumeInfo(
            out VolumeInfo VolumeInfo)
        {
            VolumeInfo = default(VolumeInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 SetVolumeLabel(
            String VolumeLabel,
            out VolumeInfo VolumeInfo)
        {
            VolumeInfo = default(VolumeInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 GetSecurityByName(
            String FileName,
            out UInt32 FileAttributes/* or ReparsePointIndex */,
            ref Byte[] SecurityDescriptor)
        {
            FileAttributes = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 Create(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            Byte[] SecurityDescriptor,
            UInt64 AllocationSize,
            out Object FileNode,
            out Object FileDesc,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileNode = default(Object);
            FileDesc = default(Object);
            FileInfo = default(FileInfo);
            NormalizedName = default(String);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 Open(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            out Object FileNode,
            out Object FileDesc,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileNode = default(Object);
            FileDesc = default(Object);
            FileInfo = default(FileInfo);
            NormalizedName = default(String);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 Overwrite(
            Object FileNode,
            Object FileDesc,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual void Cleanup(
            Object FileNode,
            Object FileDesc,
            String FileName,
            UInt32 Flags)
        {
        }
        protected virtual void Close(
            Object FileNode,
            Object FileDesc)
        {
        }
        protected virtual Int32 Read(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            PBytesTransferred = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 Write(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 PBytesTransferred,
            out FileInfo FileInfo)
        {
            PBytesTransferred = default(UInt32);
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 Flush(
            Object FileNode,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 GetFileInfo(
            Object FileNode,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 SetBasicInfo(
            Object FileNode,
            Object FileDesc,
            UInt32 FileAttributes,
            UInt64 CreationTime,
            UInt64 LastAccessTime,
            UInt64 LastWriteTime,
            UInt64 ChangeTime,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 SetFileSize(
            Object FileNode,
            Object FileDesc,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 CanDelete(
            Object FileNode,
            Object FileDesc,
            String FileName)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 Rename(
            Object FileNode,
            Object FileDesc,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 GetSecurity(
            Object FileNode,
            Object FileDesc,
            ref Byte[] SecurityDescriptor)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 SetSecurity(
            Object FileNode,
            Object FileDesc,
            AccessControlSections Sections,
            Byte[] SecurityDescriptor)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 ReadDirectory(
            Object FileNode,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            PBytesTransferred = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Boolean ReadDirectoryEntry(
            Object FileNode,
            Object FileDesc,
            String Pattern,
            String Marker,
            ref Object Context,
            out String FileName,
            out FileInfo FileInfo)
        {
            FileName = default(String);
            FileInfo = default(FileInfo);
            return false;
        }
        protected virtual Int32 ResolveReparsePoints(
            String FileName,
            UInt32 ReparsePointIndex,
            Boolean ResolveLastPathComponent,
            out IoStatusBlock PIoStatus,
            IntPtr Buffer,
            ref UIntPtr PSize)
        {
            return Api.FspFileSystemResolveReparsePoints(
                _FileSystem,
                GetReparsePointByName,
                IntPtr.Zero,
                FileName,
                ReparsePointIndex,
                ResolveLastPathComponent,
                out PIoStatus,
                Buffer,
                ref PSize);
        }
        protected virtual Int32 GetReparsePointByName(
            String FileName,
            Boolean IsDirectory,
            IntPtr Buffer,
            ref UIntPtr PSize)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 GetReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            IntPtr Buffer,
            out UIntPtr PSize)
        {
            PSize = default(UIntPtr);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 SetReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 DeleteReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        protected virtual Int32 GetStreamInfo(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            PBytesTransferred = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }

        /* FSP_FILE_SYSTEM_INTERFACE */
        private static Byte[] SecurityDescriptorNotNull = new Byte[0];
        private static Int32 GetVolumeInfo(
            IntPtr FileSystem,
            out VolumeInfo VolumeInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                return self.GetVolumeInfo(
                    out VolumeInfo);
            }
            catch (Exception ex)
            {
                VolumeInfo = default(VolumeInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 SetVolumeLabel(
            IntPtr FileSystem,
            String VolumeLabel,
            out VolumeInfo VolumeInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                return self.SetVolumeLabel(
                    VolumeLabel,
                    out VolumeInfo);
            }
            catch (Exception ex)
            {
                VolumeInfo = default(VolumeInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 GetSecurityByName(
            IntPtr FileSystem,
            String FileName,
            IntPtr PFileAttributes/* or ReparsePointIndex */,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                UInt32 FileAttributes;
                Byte[] SecurityDescriptorBytes = null;
                Int32 Result;
                if (IntPtr.Zero != PSecurityDescriptorSize)
                    SecurityDescriptorBytes = SecurityDescriptorNotNull;
                Result = self.GetSecurityByName(
                    FileName,
                    out FileAttributes,
                    ref SecurityDescriptorBytes);
                if (0 <= Result)
                {
                    if (IntPtr.Zero != PFileAttributes)
                        Marshal.WriteInt32(PFileAttributes, (Int32)FileAttributes);
                    Result = Api.CopySecurityDescriptor(SecurityDescriptorBytes,
                        SecurityDescriptor, PSecurityDescriptorSize);
                }
                return Result;
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 Create(
            IntPtr FileSystem,
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            IntPtr SecurityDescriptor,
            UInt64 AllocationSize,
            ref FullContext FullContext,
            ref OpenFileInfo OpenFileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Int32 Result;
                Result = self.Create(
                    FileName,
                    CreateOptions,
                    GrantedAccess,
                    FileAttributes,
                    Api.MakeSecurityDescriptor(SecurityDescriptor),
                    AllocationSize,
                    out FileNode,
                    out FileDesc,
                    out OpenFileInfo.FileInfo,
                    out NormalizedName);
                if (0 <= Result)
                {
                    if (null != NormalizedName)
                        OpenFileInfo.SetNormalizedName(NormalizedName);
                    Api.SetFullContext(ref FullContext, FileNode, FileDesc);
                }
                return Result;
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 Open(
            IntPtr FileSystem,
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            ref FullContext FullContext,
            ref OpenFileInfo OpenFileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Int32 Result;
                Result = self.Open(
                    FileName,
                    CreateOptions,
                    GrantedAccess,
                    out FileNode,
                    out FileDesc,
                    out OpenFileInfo.FileInfo,
                    out NormalizedName);
                if (0 <= Result)
                {
                    if (null != NormalizedName)
                        OpenFileInfo.SetNormalizedName(NormalizedName);
                    Api.SetFullContext(ref FullContext, FileNode, FileDesc);
                }
                return Result;
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 Overwrite(
            IntPtr FileSystem,
            ref FullContext FullContext,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.Overwrite(
                    FileNode,
                    FileDesc,
                    FileAttributes,
                    ReplaceFileAttributes,
                    AllocationSize,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static void Cleanup(
            IntPtr FileSystem,
            ref FullContext FullContext,
            String FileName,
            UInt32 Flags)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                self.Cleanup(
                    FileNode,
                    FileDesc,
                    FileName,
                    Flags);
            }
            catch (Exception ex)
            {
                self.ExceptionHandler(ex);
            }
        }
        private static void Close(
            IntPtr FileSystem,
            ref FullContext FullContext)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                self.Close(
                    FileNode,
                    FileDesc);
                Api.SetFullContext(ref FullContext, null, null);
            }
            catch (Exception ex)
            {
                self.ExceptionHandler(ex);
            }
        }
        private static Int32 Read(
            IntPtr FileSystem,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.Read(
                    FileNode,
                    FileDesc,
                    Buffer,
                    Offset,
                    Length,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 Write(
            IntPtr FileSystem,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 PBytesTransferred,
            out FileInfo FileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.Write(
                    FileNode,
                    FileDesc,
                    Buffer,
                    Offset,
                    Length,
                    WriteToEndOfFile,
                    ConstrainedIo,
                    out PBytesTransferred,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                FileInfo = default(FileInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 Flush(
            IntPtr FileSystem,
            ref FullContext FullContext,
            out FileInfo FileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.Flush(
                    FileNode,
                    FileDesc,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 GetFileInfo(
            IntPtr FileSystem,
            ref FullContext FullContext,
            out FileInfo FileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.GetFileInfo(
                    FileNode,
                    FileDesc,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 SetBasicInfo(
            IntPtr FileSystem,
            ref FullContext FullContext,
            UInt32 FileAttributes,
            UInt64 CreationTime,
            UInt64 LastAccessTime,
            UInt64 LastWriteTime,
            UInt64 ChangeTime,
            out FileInfo FileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.SetBasicInfo(
                    FileNode,
                    FileDesc,
                    FileAttributes,
                    CreationTime,
                    LastAccessTime,
                    LastWriteTime,
                    ChangeTime,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 SetFileSize(
            IntPtr FileSystem,
            ref FullContext FullContext,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.SetFileSize(
                    FileNode,
                    FileDesc,
                    NewSize,
                    SetAllocationSize,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 CanDelete(
            IntPtr FileSystem,
            ref FullContext FullContext,
            String FileName)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.CanDelete(
                    FileNode,
                    FileDesc,
                    FileName);
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 Rename(
            IntPtr FileSystem,
            ref FullContext FullContext,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.Rename(
                    FileNode,
                    FileDesc,
                    FileName,
                    NewFileName,
                    ReplaceIfExists);
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 GetSecurity(
            IntPtr FileSystem,
            ref FullContext FullContext,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Byte[] SecurityDescriptorBytes;
                Int32 Result;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                SecurityDescriptorBytes = SecurityDescriptorNotNull;
                Result = self.GetSecurity(
                    FileNode,
                    FileDesc,
                    ref SecurityDescriptorBytes);
                return Api.CopySecurityDescriptor(SecurityDescriptorBytes,
                    SecurityDescriptor, PSecurityDescriptorSize);
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 SetSecurity(
            IntPtr FileSystem,
            ref FullContext FullContext,
            UInt32 SecurityInformation,
            IntPtr ModificationDescriptor)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                AccessControlSections Sections;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                Sections = AccessControlSections.None;
                if (0 != (SecurityInformation & 1/*OWNER_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Owner;
                if (0 != (SecurityInformation & 2/*GROUP_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Group;
                if (0 != (SecurityInformation & 4/*DACL_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Access;
                if (0 != (SecurityInformation & 8/*SACL_SECURITY_INFORMATION*/))
                    Sections |= AccessControlSections.Audit;
                return self.SetSecurity(
                    FileNode,
                    FileDesc,
                    Sections,
                    Api.MakeSecurityDescriptor(ModificationDescriptor));
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 ReadDirectory(
            IntPtr FileSystem,
            ref FullContext FullContext,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.ReadDirectory(
                    FileNode,
                    FileDesc,
                    Pattern,
                    Marker,
                    Buffer,
                    Length,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 ResolveReparsePoints(
            IntPtr FileSystem,
            String FileName,
            UInt32 ReparsePointIndex,
            Boolean ResolveLastPathComponent,
            out IoStatusBlock PIoStatus,
            IntPtr Buffer,
            ref UIntPtr PSize)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                return self.ResolveReparsePoints(
                    FileName,
                    ReparsePointIndex,
                    ResolveLastPathComponent,
                    out PIoStatus,
                    Buffer,
                    ref PSize);
            }
            catch (Exception ex)
            {
                PIoStatus = default(IoStatusBlock);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 GetReparsePointByName(
            IntPtr FileSystem,
            IntPtr Context,
            String FileName,
            Boolean IsDirectory,
            IntPtr Buffer,
            ref UIntPtr PSize)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                return self.GetReparsePointByName(
                    FileName,
                    IsDirectory,
                    Buffer,
                    ref PSize);
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 GetReparsePoint(
            IntPtr FileSystem,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            out UIntPtr PSize)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.GetReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    Buffer,
                    out PSize);
            }
            catch (Exception ex)
            {
                PSize = default(UIntPtr);
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 SetReparsePoint(
            IntPtr FileSystem,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.SetReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    Buffer,
                    Size);
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 DeleteReparsePoint(
            IntPtr FileSystem,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.DeleteReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    Buffer,
                    Size);
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 GetStreamInfo(
            IntPtr FileSystem,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystem self = (FileSystem)Api.GetUserContext(FileSystem);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return self.GetStreamInfo(
                    FileNode,
                    FileDesc,
                    Buffer,
                    Length,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return self.ExceptionHandler(ex);
            }
        }

        static FileSystem()
        {
            _FileSystemInterface.GetVolumeInfo = GetVolumeInfo;
            _FileSystemInterface.SetVolumeLabel = SetVolumeLabel;
            _FileSystemInterface.GetSecurityByName = GetSecurityByName;
            _FileSystemInterface.Create = Create;
            _FileSystemInterface.Open = Open;
            _FileSystemInterface.Overwrite = Overwrite;
            _FileSystemInterface.Cleanup = Cleanup;
            _FileSystemInterface.Close = Close;
            _FileSystemInterface.Read = Read;
            _FileSystemInterface.Write = Write;
            _FileSystemInterface.Flush = Flush;
            _FileSystemInterface.GetFileInfo = GetFileInfo;
            _FileSystemInterface.SetBasicInfo = SetBasicInfo;
            _FileSystemInterface.SetFileSize = SetFileSize;
            _FileSystemInterface.CanDelete = CanDelete;
            _FileSystemInterface.Rename = Rename;
            _FileSystemInterface.GetSecurity = GetSecurity;
            _FileSystemInterface.SetSecurity = SetSecurity;
            _FileSystemInterface.ReadDirectory = ReadDirectory;
            _FileSystemInterface.ResolveReparsePoints = ResolveReparsePoints;
            _FileSystemInterface.GetReparsePoint = GetReparsePoint;
            _FileSystemInterface.SetReparsePoint = SetReparsePoint;
            _FileSystemInterface.DeleteReparsePoint = DeleteReparsePoint;
            _FileSystemInterface.GetStreamInfo = GetStreamInfo;

            _FileSystemInterfacePtr = Marshal.AllocHGlobal(FileSystemInterface.Size);
            Marshal.StructureToPtr(_FileSystemInterface, _FileSystemInterfacePtr, false);
        }

        private static FileSystemInterface _FileSystemInterface;
        private static IntPtr _FileSystemInterfacePtr;
        private VolumeParams _VolumeParams;
        private IntPtr _FileSystem;
    }

}
