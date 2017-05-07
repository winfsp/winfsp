/**
 * @file dotnet/FileSystemHost.cs
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

    public class FileSystemHost : IDisposable
    {
        /* ctor/dtor */
        public FileSystemHost(FileSystemBase FileSystem)
        {
            _VolumeParams.Flags = VolumeParams.UmFileContextIsFullContext;
            _FileSystem = FileSystem;
        }
        ~FileSystemHost()
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
            if (IntPtr.Zero != _FileSystemPtr)
            {
                Api.FspFileSystemStopDispatcher(_FileSystemPtr);
                if (disposing)
                    try
                    {
                        _FileSystem.Unmounted(this);
                    }
                    catch (Exception ex)
                    {
                        ExceptionHandler(_FileSystem, ex);
                    }
                Api.DisposeUserContext(_FileSystemPtr);
                Api.FspFileSystemDelete(_FileSystemPtr);
                _FileSystemPtr = IntPtr.Zero;
            }
        }

        /* properties */
        public UInt16 SectorSize
        {
            get { return _VolumeParams.SectorSize; }
            set { _VolumeParams.SectorSize = value; }
        }
        public UInt16 SectorsPerAllocationUnit
        {
            get { return _VolumeParams.SectorsPerAllocationUnit; }
            set { _VolumeParams.SectorsPerAllocationUnit = value; }
        }
        public UInt16 MaxComponentLength
        {
            get { return _VolumeParams.MaxComponentLength; }
            set { _VolumeParams.MaxComponentLength = value; }
        }
        public UInt64 VolumeCreationTime
        {
            get { return _VolumeParams.VolumeCreationTime; }
            set { _VolumeParams.VolumeCreationTime = value; }
        }
        public UInt32 VolumeSerialNumber
        {
            get { return _VolumeParams.VolumeSerialNumber; }
            set { _VolumeParams.VolumeSerialNumber = value; }
        }
        public UInt32 FileInfoTimeout
        {
            get { return _VolumeParams.FileInfoTimeout; }
            set { _VolumeParams.FileInfoTimeout = value; }
        }
        public Boolean CaseSensitiveSearch
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.CaseSensitiveSearch); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.CaseSensitiveSearch : 0); }
        }
        public Boolean CasePreservedNames
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.CasePreservedNames); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.CasePreservedNames : 0); }
        }
        public Boolean UnicodeOnDisk
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.UnicodeOnDisk); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.UnicodeOnDisk : 0); }
        }
        public Boolean PersistentAcls
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.PersistentAcls); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.PersistentAcls : 0); }
        }
        public Boolean ReparsePoints
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.ReparsePoints); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.ReparsePoints : 0); }
        }
        public Boolean ReparsePointsAccessCheck
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.ReparsePointsAccessCheck); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.ReparsePointsAccessCheck : 0); }
        }
        public Boolean NamedStreams
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.NamedStreams); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.NamedStreams : 0); }
        }
        public Boolean PostCleanupWhenModifiedOnly
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.PostCleanupWhenModifiedOnly); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.PostCleanupWhenModifiedOnly : 0); }
        }
        public Boolean PassQueryDirectoryPattern
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.PassQueryDirectoryPattern); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.PassQueryDirectoryPattern : 0); }
        }
        public String Prefix
        {
            get { return _VolumeParams.GetPrefix(); }
            set {  _VolumeParams.SetPrefix(value); }
        }
        public String FileSystemName
        {
            get { return _VolumeParams.GetFileSystemName(); }
            set {  _VolumeParams.SetFileSystemName(value); }
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
            try
            {
                Result = _FileSystem.Init(this);
            }
            catch (Exception ex)
            {
                Result = ExceptionHandler(_FileSystem, ex);
            }
            if (0 > Result)
                return Result;
            Result = Api.FspFileSystemCreate(
                _VolumeParams.IsPrefixEmpty() ? "WinFsp.Disk" : "WinFsp.Net",
                ref _VolumeParams, _FileSystemInterfacePtr, out _FileSystemPtr);
            if (0 > Result)
                return Result;
            Api.SetUserContext(_FileSystemPtr, _FileSystem);
            Api.FspFileSystemSetOperationGuardStrategy(_FileSystemPtr, Synchronized ?
                1/*FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE*/ :
                0/*FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE*/);
            Api.FspFileSystemSetDebugLog(_FileSystemPtr, DebugLog);
            Result = Api.FspFileSystemSetMountPointEx(_FileSystemPtr, MountPoint,
                SecurityDescriptor);
            if (0 <= Result)
            {
                try
                {
                    Result = _FileSystem.Mounted(this);
                }
                catch (Exception ex)
                {
                    Result = ExceptionHandler(_FileSystem, ex);
                }
                if (0 <= Result)
                {
                    Result = Api.FspFileSystemStartDispatcher(_FileSystemPtr, 0);
                    if (0 > Result)
                        try
                        {
                            _FileSystem.Unmounted(this);
                        }
                        catch (Exception ex)
                        {
                            ExceptionHandler(_FileSystem, ex);
                        }
                }
            }
            if (0 > Result)
            {
                Api.DisposeUserContext(_FileSystemPtr);
                Api.FspFileSystemDelete(_FileSystemPtr);
                _FileSystemPtr = IntPtr.Zero;
            }
            return Result;
        }
        public void Unmount()
        {
            Dispose();
        }
        public String MountPoint()
        {
            return IntPtr.Zero != _FileSystemPtr ?
                Marshal.PtrToStringUni(Api.FspFileSystemMountPoint(_FileSystemPtr)) : null;
        }
        public IntPtr FileSystemHandle()
        {
            return _FileSystemPtr;
        }
        public FileSystemBase FileSystem()
        {
            return _FileSystem;
        }
        public static Int32 SetDebugLogFile(String FileName)
        {
            return Api.SetDebugLogFile(FileName);
        }

        /* FSP_FILE_SYSTEM_INTERFACE */
        private static Byte[] ByteBufferNotNull = new Byte[0];
        private static Int32 ExceptionHandler(
            FileSystemBase FileSystem,
            Exception ex)
        {
            try
            {
                return FileSystem.ExceptionHandler(ex);
            }
            catch
            {
                return unchecked((Int32)0xc00000e9)/*STATUS_UNEXPECTED_IO_ERROR*/;
            }
        }
        private static Int32 GetVolumeInfo(
            IntPtr FileSystemPtr,
            out VolumeInfo VolumeInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                return FileSystem.GetVolumeInfo(
                    out VolumeInfo);
            }
            catch (Exception ex)
            {
                VolumeInfo = default(VolumeInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetVolumeLabel(
            IntPtr FileSystemPtr,
            String VolumeLabel,
            out VolumeInfo VolumeInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                return FileSystem.SetVolumeLabel(
                    VolumeLabel,
                    out VolumeInfo);
            }
            catch (Exception ex)
            {
                VolumeInfo = default(VolumeInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetSecurityByName(
            IntPtr FileSystemPtr,
            String FileName,
            IntPtr PFileAttributes/* or ReparsePointIndex */,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                UInt32 FileAttributes;
                Byte[] SecurityDescriptorBytes = null;
                Int32 Result;
                if (IntPtr.Zero != PSecurityDescriptorSize)
                    SecurityDescriptorBytes = ByteBufferNotNull;
                Result = FileSystem.GetSecurityByName(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Create(
            IntPtr FileSystemPtr,
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            IntPtr SecurityDescriptor,
            UInt64 AllocationSize,
            ref FullContext FullContext,
            ref OpenFileInfo OpenFileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Int32 Result;
                Result = FileSystem.Create(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Open(
            IntPtr FileSystemPtr,
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            ref FullContext FullContext,
            ref OpenFileInfo OpenFileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Int32 Result;
                Result = FileSystem.Open(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Overwrite(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Overwrite(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static void Cleanup(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            UInt32 Flags)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                FileSystem.Cleanup(
                    FileNode,
                    FileDesc,
                    FileName,
                    Flags);
            }
            catch (Exception ex)
            {
                ExceptionHandler(FileSystem, ex);
            }
        }
        private static void Close(
            IntPtr FileSystemPtr,
            ref FullContext FullContext)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                FileSystem.Close(
                    FileNode,
                    FileDesc);
                Api.DisposeFullContext(ref FullContext);
            }
            catch (Exception ex)
            {
                ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Read(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Read(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Write(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 PBytesTransferred,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Write(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Flush(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Flush(
                    FileNode,
                    FileDesc,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetFileInfo(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.GetFileInfo(
                    FileNode,
                    FileDesc,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetBasicInfo(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 FileAttributes,
            UInt64 CreationTime,
            UInt64 LastAccessTime,
            UInt64 LastWriteTime,
            UInt64 ChangeTime,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetBasicInfo(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetFileSize(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetFileSize(
                    FileNode,
                    FileDesc,
                    NewSize,
                    SetAllocationSize,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 CanDelete(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.CanDelete(
                    FileNode,
                    FileDesc,
                    FileName);
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Rename(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Rename(
                    FileNode,
                    FileDesc,
                    FileName,
                    NewFileName,
                    ReplaceIfExists);
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetSecurity(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Byte[] SecurityDescriptorBytes;
                Int32 Result;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                SecurityDescriptorBytes = ByteBufferNotNull;
                Result = FileSystem.GetSecurity(
                    FileNode,
                    FileDesc,
                    ref SecurityDescriptorBytes);
                if (0 <= Result)
                    Result = Api.CopySecurityDescriptor(SecurityDescriptorBytes,
                        SecurityDescriptor, PSecurityDescriptorSize);
                return Result;
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetSecurity(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 SecurityInformation,
            IntPtr ModificationDescriptor)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
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
                return FileSystem.SetSecurity(
                    FileNode,
                    FileDesc,
                    Sections,
                    Api.MakeSecurityDescriptor(ModificationDescriptor));
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 ReadDirectory(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.ReadDirectory(
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
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 ResolveReparsePoints(
            IntPtr FileSystemPtr,
            String FileName,
            UInt32 ReparsePointIndex,
            Boolean ResolveLastPathComponent,
            out IoStatusBlock PIoStatus,
            IntPtr Buffer,
            IntPtr PSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                return FileSystem.ResolveReparsePoints(
                    FileName,
                    ReparsePointIndex,
                    ResolveLastPathComponent,
                    out PIoStatus,
                    Buffer,
                    PSize);
            }
            catch (Exception ex)
            {
                PIoStatus = default(IoStatusBlock);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetReparsePoint(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            IntPtr PSize)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Byte[] ReparseData;
                Object FileNode, FileDesc;
                Int32 Result;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                ReparseData = null;
                Result = FileSystem.GetReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    ref ReparseData);
                if (0 <= Result)
                    Result = Api.CopyReparsePoint(ReparseData, Buffer, PSize);
                return Result;
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetReparsePoint(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    Api.MakeReparsePoint(Buffer, Size));
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 DeleteReparsePoint(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.DeleteReparsePoint(
                    FileNode,
                    FileDesc,
                    FileName,
                    Api.MakeReparsePoint(Buffer, Size));
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetStreamInfo(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.GetStreamInfo(
                    FileNode,
                    FileDesc,
                    Buffer,
                    Length,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return ExceptionHandler(FileSystem, ex);
            }
        }

        static FileSystemHost()
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
        private FileSystemBase _FileSystem;
        private IntPtr _FileSystemPtr;
    }

}
