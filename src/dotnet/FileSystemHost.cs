/*
 * dotnet/FileSystemHost.cs
 *
 * Copyright 2015-2021 Bill Zissimopoulos
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

using System;
using System.Runtime.InteropServices;
using System.Security.AccessControl;

using Fsp.Interop;

namespace Fsp
{

    /// <summary>
    /// Provides a means to host (mount) a file system.
    /// </summary>
    public class FileSystemHost : IDisposable
    {
        /* ctor/dtor */
        /// <summary>
        /// Creates an instance of the FileSystemHost class.
        /// </summary>
        /// <param name="FileSystem">The file system to host.</param>
        public FileSystemHost(FileSystemBase FileSystem)
        {
            _VolumeParams.Version = (UInt16)Marshal.SizeOf(_VolumeParams);
            _VolumeParams.Flags = VolumeParams.UmFileContextIsFullContext;
            _FileSystem = FileSystem;
        }
        ~FileSystemHost()
        {
            Dispose(false);
        }
        /// <summary>
        /// Unmounts the file system and releases all associated resources.
        /// </summary>
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
        /// <summary>
        /// Gets or sets the sector size used by the file system.
        /// </summary>
        public UInt16 SectorSize
        {
            get { return _VolumeParams.SectorSize; }
            set { _VolumeParams.SectorSize = value; }
        }
        /// <summary>
        /// Gets or sets the sectors per allocation unit used by the file system.
        /// </summary>
        public UInt16 SectorsPerAllocationUnit
        {
            get { return _VolumeParams.SectorsPerAllocationUnit; }
            set { _VolumeParams.SectorsPerAllocationUnit = value; }
        }
        /// <summary>
        /// Gets or sets the maximum path component length used by the file system.
        /// </summary>
        public UInt16 MaxComponentLength
        {
            get { return _VolumeParams.MaxComponentLength; }
            set { _VolumeParams.MaxComponentLength = value; }
        }
        /// <summary>
        /// Gets or sets the volume creation time.
        /// </summary>
        public UInt64 VolumeCreationTime
        {
            get { return _VolumeParams.VolumeCreationTime; }
            set { _VolumeParams.VolumeCreationTime = value; }
        }
        /// <summary>
        /// Gets or sets the volume serial number.
        /// </summary>
        public UInt32 VolumeSerialNumber
        {
            get { return _VolumeParams.VolumeSerialNumber; }
            set { _VolumeParams.VolumeSerialNumber = value; }
        }
        /// <summary>
        /// Gets or sets the file information timeout.
        /// </summary>
        public UInt32 FileInfoTimeout
        {
            get { return _VolumeParams.FileInfoTimeout; }
            set { _VolumeParams.FileInfoTimeout = value; }
        }
        /// <summary>
        /// Gets or sets the volume information timeout.
        /// </summary>
        public UInt32 VolumeInfoTimeout
        {
            get
            {
                return 0 != (_VolumeParams.AdditionalFlags & VolumeParams.VolumeInfoTimeoutValid) ?
                    _VolumeParams.VolumeInfoTimeout : _VolumeParams.FileInfoTimeout;
            }
            set
            {
                _VolumeParams.AdditionalFlags |= VolumeParams.VolumeInfoTimeoutValid;
                _VolumeParams.VolumeInfoTimeout = value;
            }
        }
        /// <summary>
        /// Gets or sets the directory information timeout.
        /// </summary>
        public UInt32 DirInfoTimeout
        {
            get
            {
                return 0 != (_VolumeParams.AdditionalFlags & VolumeParams.DirInfoTimeoutValid) ?
                    _VolumeParams.DirInfoTimeout : _VolumeParams.FileInfoTimeout;
            }
            set
            {
                _VolumeParams.AdditionalFlags |= VolumeParams.DirInfoTimeoutValid;
                _VolumeParams.DirInfoTimeout = value;
            }
        }
        /// <summary>
        /// Gets or sets the security information timeout.
        /// </summary>
        public UInt32 SecurityTimeout
        {
            get
            {
                return 0 != (_VolumeParams.AdditionalFlags & VolumeParams.SecurityTimeoutValid) ?
                    _VolumeParams.SecurityTimeout : _VolumeParams.FileInfoTimeout;
            }
            set
            {
                _VolumeParams.AdditionalFlags |= VolumeParams.SecurityTimeoutValid;
                _VolumeParams.SecurityTimeout = value;
            }
        }
        /// <summary>
        /// Gets or sets the stream information timeout.
        /// </summary>
        public UInt32 StreamInfoTimeout
        {
            get
            {
                return 0 != (_VolumeParams.AdditionalFlags & VolumeParams.StreamInfoTimeoutValid) ?
                    _VolumeParams.StreamInfoTimeout : _VolumeParams.FileInfoTimeout;
            }
            set
            {
                _VolumeParams.AdditionalFlags |= VolumeParams.StreamInfoTimeoutValid;
                _VolumeParams.StreamInfoTimeout = value;
            }
        }
        /// <summary>
        /// Gets or sets the EA information timeout.
        /// </summary>
        public UInt32 EaTimeout
        {
            get
            {
                return 0 != (_VolumeParams.AdditionalFlags & VolumeParams.EaTimeoutValid) ?
                    _VolumeParams.EaTimeout : _VolumeParams.FileInfoTimeout;
            }
            set
            {
                _VolumeParams.AdditionalFlags |= VolumeParams.EaTimeoutValid;
                _VolumeParams.EaTimeout = value;
            }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the file system is case sensitive.
        /// </summary>
        public Boolean CaseSensitiveSearch
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.CaseSensitiveSearch); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.CaseSensitiveSearch : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether a case insensitive file system
        /// preserves case in file names.
        /// </summary>
        public Boolean CasePreservedNames
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.CasePreservedNames); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.CasePreservedNames : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether file names support unicode characters.
        /// </summary>
        public Boolean UnicodeOnDisk
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.UnicodeOnDisk); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.UnicodeOnDisk : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the file system supports ACL security.
        /// </summary>
        public Boolean PersistentAcls
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.PersistentAcls); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.PersistentAcls : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the file system supports reparse points.
        /// </summary>
        public Boolean ReparsePoints
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.ReparsePoints); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.ReparsePoints : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the file system allows creation of
        /// symbolic links without additional privileges.
        /// </summary>
        public Boolean ReparsePointsAccessCheck
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.ReparsePointsAccessCheck); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.ReparsePointsAccessCheck : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the file system supports named streams.
        /// </summary>
        public Boolean NamedStreams
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.NamedStreams); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.NamedStreams : 0); }
        }
        /// <summary>
        /// Gets or sets a value that determines whether the file system supports extended attributes.
        /// </summary>
        public Boolean ExtendedAttributes
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.ExtendedAttributes); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.ExtendedAttributes : 0); }
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
        public Boolean PassQueryDirectoryFileName
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.PassQueryDirectoryFileName); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.PassQueryDirectoryFileName : 0); }
        }
        public Boolean FlushAndPurgeOnCleanup
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.FlushAndPurgeOnCleanup); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.FlushAndPurgeOnCleanup : 0); }
        }
        public Boolean DeviceControl
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.DeviceControl); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.DeviceControl : 0); }
        }
        public Boolean AllowOpenInKernelMode
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.AllowOpenInKernelMode); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.AllowOpenInKernelMode : 0); }
        }
        public Boolean WslFeatures
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.WslFeatures); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.WslFeatures : 0); }
        }
        public Boolean RejectIrpPriorToTransact0
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.RejectIrpPriorToTransact0); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.RejectIrpPriorToTransact0 : 0); }
        }
        public Boolean SupportsPosixUnlinkRename
        {
            get { return 0 != (_VolumeParams.Flags & VolumeParams.SupportsPosixUnlinkRename); }
            set { _VolumeParams.Flags |= (value ? VolumeParams.SupportsPosixUnlinkRename : 0); }
        }
        /// <summary>
        /// Gets or sets the prefix for a network file system.
        /// </summary>
        public String Prefix
        {
            get { return _VolumeParams.GetPrefix(); }
            set {  _VolumeParams.SetPrefix(value); }
        }
        /// <summary>
        /// Gets or sets the file system name.
        /// </summary>
        public String FileSystemName
        {
            get { return _VolumeParams.GetFileSystemName(); }
            set {  _VolumeParams.SetFileSystemName(value); }
        }

        /* control */
        /// <summary>
        /// Checks whether mounting a file system is possible.
        /// </summary>
        /// <param name="MountPoint">
        /// The mount point for the new file system. A value of null means that
        /// the file system should use the next available drive letter counting
        /// downwards from Z: as its mount point.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public Int32 Preflight(String MountPoint)
        {
            return Api.FspFileSystemPreflight(
                _VolumeParams.IsPrefixEmpty() ? Api.ProductName + ".Disk" : Api.ProductName + ".Net",
                MountPoint);
        }
        /// <summary>
        /// Mounts a file system.
        /// </summary>
        /// <param name="MountPoint">
        /// The mount point for the new file system. A value of null means that
        /// the file system should use the next available drive letter counting
        /// downwards from Z: as its mount point.
        /// </param>
        /// <param name="SecurityDescriptor">
        /// Security descriptor to use if mounting on (newly created) directory.
        /// A value of null means the directory should be created with default
        /// security.
        /// </param>
        /// <param name="Synchronized">
        /// If true file system operations are synchronized using an exclusive lock.
        /// </param>
        /// <param name="DebugLog">
        /// A value of 0 disables all debug logging.
        /// A value of -1 enables all debug logging.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public Int32 Mount(String MountPoint,
            Byte[] SecurityDescriptor = null,
            Boolean Synchronized = false,
            UInt32 DebugLog = 0)
        {
            return MountEx(MountPoint, 0, SecurityDescriptor, Synchronized, DebugLog);
        }
        /// <summary>
        /// Mounts a file system.
        /// </summary>
        /// <param name="MountPoint">
        /// The mount point for the new file system. A value of null means that
        /// the file system should use the next available drive letter counting
        /// downwards from Z: as its mount point.
        /// </param>
        /// <param name="ThreadCount">
        /// Number of threads to use to service file system requests. A value
        /// of 0 means that the default number of threads should be used.
        /// </param>
        /// <param name="SecurityDescriptor">
        /// Security descriptor to use if mounting on (newly created) directory.
        /// A value of null means the directory should be created with default
        /// security.
        /// </param>
        /// <param name="Synchronized">
        /// If true file system operations are synchronized using an exclusive lock.
        /// </param>
        /// <param name="DebugLog">
        /// A value of 0 disables all debug logging.
        /// A value of -1 enables all debug logging.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public Int32 MountEx(String MountPoint,
            UInt32 ThreadCount,
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
                _VolumeParams.IsPrefixEmpty() ? Api.ProductName + ".Disk" : Api.ProductName + ".Net",
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
                    Result = Api.FspFileSystemStartDispatcher(_FileSystemPtr, ThreadCount);
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
        /// <summary>
        /// Unmounts the file system and releases all associated resources.
        /// </summary>
        public void Unmount()
        {
            Dispose();
        }
        /// <summary>
        /// Gets the file system mount point.
        /// </summary>
        /// <returns>The file system mount point.</returns>
        public String MountPoint()
        {
            return IntPtr.Zero != _FileSystemPtr ?
                Marshal.PtrToStringUni(Api.FspFileSystemMountPoint(_FileSystemPtr)) : null;
        }
        public IntPtr FileSystemHandle()
        {
            return _FileSystemPtr;
        }
        /// <summary>
        /// Gets the hosted file system.
        /// </summary>
        /// <returns>The hosted file system.</returns>
        public FileSystemBase FileSystem()
        {
            return _FileSystem;
        }
        /// <summary>
        /// Sets the debug log file to use when debug logging is enabled.
        /// </summary>
        /// <param name="FileName">
        /// The debug log file name. A value of "-" means standard error output.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public static Int32 SetDebugLogFile(String FileName)
        {
            return Api.SetDebugLogFile(FileName);
        }
        /// <summary>
        /// Return the installed version of WinFsp.
        /// </summary>
        public static Version Version()
        {
            return Api.GetVersion();
        }
        /// <summary>
        /// Returns a RequestHint to reference the current operation asynchronously.
        /// </summary>
        public UInt64 GetOperationRequestHint()
        {
            return Api.FspFileSystemGetOperationRequestHint();
        }
        /// <summary>
        /// Asynchronously complete a Read operation.
        /// </summary>
        /// <param name="RequestHint">
        /// A reference to the operation to complete.
        /// </param>
        /// <param name="Status">
        /// STATUS_SUCCESS or error code.
        /// </param>
        /// <param name="BytesTransferred">
        /// Number of bytes read.
        /// </param>
        public void SendReadResponse(UInt64 RequestHint, Int32 Status, UInt32 BytesTransferred)
        {
            FspFsctlTransactRsp Response = default(FspFsctlTransactRsp);
            Response.Size = 128;
            Response.Kind = (UInt32)FspFsctlTransact.ReadKind;
            Response.Hint = RequestHint;
            Response.IoStatus.Information = BytesTransferred;
            Response.IoStatus.Status = (UInt32)Status;
            Api.FspFileSystemSendResponse(_FileSystemPtr, ref Response);
        }
        /// <summary>
        /// Asynchronously complete a Write operation.
        /// </summary>
        /// <param name="RequestHint">
        /// A reference to the operation to complete.
        /// </param>
        /// <param name="Status">
        /// STATUS_SUCCESS or error code.
        /// </param>
        /// <param name="BytesTransferred">
        /// The number of bytes written.
        /// </param>
        /// <param name="FileInfo">
        /// Updated file information.
        /// </param>
        public void SendWriteResponse(UInt64 RequestHint, Int32 Status, UInt32 BytesTransferred, ref FileInfo FileInfo)
        {
            FspFsctlTransactRsp Response = default(FspFsctlTransactRsp);
            Response.Size = 128;
            Response.Kind = (UInt32)FspFsctlTransact.WriteKind;
            Response.Hint = RequestHint;
            Response.IoStatus.Information = BytesTransferred;
            Response.IoStatus.Status = (UInt32)Status;
            Response.WriteFileInfo = FileInfo;
            Api.FspFileSystemSendResponse(_FileSystemPtr, ref Response);
        }
        /// <summary>
        /// Asynchronously complete a ReadDirectory operation.
        /// </summary>
        /// <param name="RequestHint">
        /// A reference to the operation to complete.
        /// </param>
        /// <param name="Status">
        /// STATUS_SUCCESS or error code.
        /// </param>
        /// <param name="BytesTransferred">
        /// Number of bytes read.
        /// </param>
        public void SendReadDirectoryResponse(UInt64 RequestHint, Int32 Status, UInt32 BytesTransferred)
        {
            FspFsctlTransactRsp Response = default(FspFsctlTransactRsp);
            Response.Size = 128;
            Response.Kind = (UInt32)FspFsctlTransact.QueryDirectoryKind;
            Response.Hint = RequestHint;
            Response.IoStatus.Information = BytesTransferred;
            Response.IoStatus.Status = (UInt32)Status;
            Api.FspFileSystemSendResponse(_FileSystemPtr, ref Response);
        }
        /// <summary>
        /// Begin notifying Windows that the file system has file changes.
        /// </summary>
        /// <remarks>
        /// <para>
        /// A file system that wishes to notify Windows about file changes must
        /// first issue an FspFileSystemBegin call, followed by 0 or more
        /// FspFileSystemNotify calls, followed by an FspFileSystemNotifyEnd call.
        /// </para><para>
        /// This operation blocks concurrent file rename operations. File rename
        /// operations may interfere with file notification, because a file being
        /// notified may also be concurrently renamed. After all file change
        /// notifications have been issued, you must make sure to call
        /// FspFileSystemNotifyEnd to allow file rename operations to proceed.
        /// </para>
        /// </remarks>
        /// <returns>
        /// STATUS_SUCCESS or error code. The error code STATUS_CANT_WAIT means that
        /// a file rename operation is currently in progress and the operation must be
        /// retried at a later time.
        /// </returns>
        public Int32 NotifyBegin(UInt32 Timeout)
        {
            return Api.FspFileSystemNotifyBegin(_FileSystemPtr, Timeout);
        }
        /// <summary>
        /// End notifying Windows that the file system has file changes.
        /// </summary>
        /// <remarks>
        /// <para>
        /// A file system that wishes to notify Windows about file changes must
        /// first issue an FspFileSystemBegin call, followed by 0 or more
        /// FspFileSystemNotify calls, followed by an FspFileSystemNotifyEnd call.
        /// </para><para>
        /// This operation allows any blocked file rename operations to proceed.
        /// </para>
        /// </remarks>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public Int32 NotifyEnd()
        {
            return Api.FspFileSystemNotifyEnd(_FileSystemPtr);
        }
        /// <summary>
        /// Notify Windows that the file system has file changes.
        /// </summary>
        /// <remarks>
        /// <para>
        /// A file system that wishes to notify Windows about file changes must
        /// first issue an FspFileSystemBegin call, followed by 0 or more
        /// FspFileSystemNotify calls, followed by an FspFileSystemNotifyEnd call.
        /// </para><para>
        /// Note that FspFileSystemNotify requires file names to be normalized. A
        /// normalized file name is one that contains the correct case of all characters
        /// in the file name.
        /// </para><para>
        /// For case-sensitive file systems all file names are normalized by definition.
        /// For case-insensitive file systems that implement file name normalization,
        /// a normalized file name is the one that the file system specifies in the
        /// response to Create or Open (see also FspFileSystemGetOpenFileInfo). For
        /// case-insensitive file systems that do not implement file name normalization
        /// a normalized file name is the upper case version of the file name used
        /// to open the file.
        /// </para>
        /// </remarks>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public Int32 Notify(NotifyInfo[] NotifyInfoArray)
        {
            return Api.FspFileSystemNotify(_FileSystemPtr, NotifyInfoArray);
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
                if (0 <= Result && 260/*STATUS_REPARSE*/ != Result)
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
            IntPtr ExtraBuffer,
            UInt32 ExtraLength,
            Boolean ExtraBufferIsReparsePoint,
            ref FullContext FullContext,
            ref OpenFileInfo OpenFileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Int32 Result;
                Result = FileSystem.CreateEx(
                    FileName,
                    CreateOptions,
                    GrantedAccess,
                    FileAttributes,
                    Api.MakeSecurityDescriptor(SecurityDescriptor),
                    AllocationSize,
                    ExtraBuffer,
                    ExtraLength,
                    ExtraBufferIsReparsePoint,
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
            IntPtr Ea,
            UInt32 EaLength,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.OverwriteEx(
                    FileNode,
                    FileDesc,
                    FileAttributes,
                    ReplaceFileAttributes,
                    AllocationSize,
                    Ea,
                    EaLength,
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
        private static Int32 GetDirInfoByName(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            out DirInfo DirInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                String NormalizedName;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                DirInfo = default(DirInfo);
                Int32 Result = FileSystem.GetDirInfoByName(
                    FileNode,
                    FileDesc,
                    FileName,
                    out NormalizedName,
                    out DirInfo.FileInfo);
                DirInfo.SetFileNameBuf(NormalizedName);
                return Result;
            }
            catch (Exception ex)
            {
                DirInfo = default(DirInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 Control(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            UInt32 ControlCode,
            IntPtr InputBuffer, UInt32 InputBufferLength,
            IntPtr OutputBuffer, UInt32 OutputBufferLength,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.Control(
                    FileNode,
                    FileDesc,
                    ControlCode,
                    InputBuffer,
                    InputBufferLength,
                    OutputBuffer,
                    OutputBufferLength,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetDelete(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            String FileName,
            Boolean DeleteFile)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetDelete(
                    FileNode,
                    FileDesc,
                    FileName,
                    DeleteFile);
            }
            catch (Exception ex)
            {
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 GetEa(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Ea,
            UInt32 EaLength,
            out UInt32 PBytesTransferred)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.GetEa(
                    FileNode,
                    FileDesc,
                    Ea,
                    EaLength,
                    out PBytesTransferred);
            }
            catch (Exception ex)
            {
                PBytesTransferred = default(UInt32);
                return ExceptionHandler(FileSystem, ex);
            }
        }
        private static Int32 SetEa(
            IntPtr FileSystemPtr,
            ref FullContext FullContext,
            IntPtr Ea,
            UInt32 EaLength,
            out FileInfo FileInfo)
        {
            FileSystemBase FileSystem = (FileSystemBase)Api.GetUserContext(FileSystemPtr);
            try
            {
                Object FileNode, FileDesc;
                Api.GetFullContext(ref FullContext, out FileNode, out FileDesc);
                return FileSystem.SetEa(
                    FileNode,
                    FileDesc,
                    Ea,
                    EaLength,
                    out FileInfo);
            }
            catch (Exception ex)
            {
                FileInfo = default(FileInfo);
                return ExceptionHandler(FileSystem, ex);
            }
        }

        static FileSystemHost()
        {
            _FileSystemInterface.GetVolumeInfo = GetVolumeInfo;
            _FileSystemInterface.SetVolumeLabel = SetVolumeLabel;
            _FileSystemInterface.GetSecurityByName = GetSecurityByName;
            _FileSystemInterface.CreateEx = Create;
            _FileSystemInterface.Open = Open;
            _FileSystemInterface.OverwriteEx = Overwrite;
            _FileSystemInterface.Cleanup = Cleanup;
            _FileSystemInterface.Close = Close;
            _FileSystemInterface.Read = Read;
            _FileSystemInterface.Write = Write;
            _FileSystemInterface.Flush = Flush;
            _FileSystemInterface.GetFileInfo = GetFileInfo;
            _FileSystemInterface.SetBasicInfo = SetBasicInfo;
            _FileSystemInterface.SetFileSize = SetFileSize;
            _FileSystemInterface.Rename = Rename;
            _FileSystemInterface.GetSecurity = GetSecurity;
            _FileSystemInterface.SetSecurity = SetSecurity;
            _FileSystemInterface.ReadDirectory = ReadDirectory;
            _FileSystemInterface.ResolveReparsePoints = ResolveReparsePoints;
            _FileSystemInterface.GetReparsePoint = GetReparsePoint;
            _FileSystemInterface.SetReparsePoint = SetReparsePoint;
            _FileSystemInterface.DeleteReparsePoint = DeleteReparsePoint;
            _FileSystemInterface.GetStreamInfo = GetStreamInfo;
            _FileSystemInterface.GetDirInfoByName = GetDirInfoByName;
            _FileSystemInterface.Control = Control;
            _FileSystemInterface.SetDelete = SetDelete;
            _FileSystemInterface.GetEa = GetEa;
            _FileSystemInterface.SetEa = SetEa;

            _FileSystemInterfacePtr = Marshal.AllocHGlobal(FileSystemInterface.Size);
            /* Marshal.AllocHGlobal does not zero memory; we must do it ourselves! */
            for (int Offset = 0; FileSystemInterface.Size > Offset; Offset += IntPtr.Size)
                Marshal.WriteIntPtr(_FileSystemInterfacePtr, Offset, IntPtr.Zero);
            Marshal.StructureToPtr(_FileSystemInterface, _FileSystemInterfacePtr, false);
        }

        private static FileSystemInterface _FileSystemInterface;
        private static IntPtr _FileSystemInterfacePtr;
        private VolumeParams _VolumeParams;
        private FileSystemBase _FileSystem;
        private IntPtr _FileSystemPtr;
    }

}
