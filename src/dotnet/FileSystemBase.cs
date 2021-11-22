/*
 * dotnet/FileSystemBase.cs
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
    /// Provides the base class that user mode file systems must inherit from.
    /// </summary>
    public partial class FileSystemBase
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

        /* operations */
        /// <summary>
        /// Provides a means to customize the returned status code when an exception happens.
        /// </summary>
        /// <param name="ex"></param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 ExceptionHandler(Exception ex)
        {
            Api.FspDebugLog("%s\n", ex.ToString());
            return STATUS_UNEXPECTED_IO_ERROR;
        }
        /// <summary>
        /// Occurs just before the file system is mounted.
        /// File systems may override this method to configure the file system host.
        /// </summary>
        /// <param name="Host">
        /// The file system host that is mounting this file system.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Init(Object Host)
        {
            return STATUS_SUCCESS;
        }
        /// <summary>
        /// Occurs just after the file system is mounted,
        /// but prior to receiving any file system operation.
        /// </summary>
        /// <param name="Host">
        /// The file system host that is mounting this file system.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Mounted(Object Host)
        {
            return STATUS_SUCCESS;
        }
        /// <summary>
        /// Occurs just after the file system is unmounted.
        /// No other file system operations will be received on this file system.
        /// </summary>
        /// <param name="Host">
        /// The file system host that is mounting this file system.
        /// </param>
        public virtual void Unmounted(Object Host)
        {
        }
        /// <summary>
        /// Gets the volume information.
        /// </summary>
        /// <param name="VolumeInfo">
        /// Receives the volume information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 GetVolumeInfo(
            out VolumeInfo VolumeInfo)
        {
            VolumeInfo = default(VolumeInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Sets the volume label.
        /// </summary>
        /// <param name="VolumeLabel">
        /// The new label for the volume.
        /// </param>
        /// <param name="VolumeInfo">
        /// Receives the updated volume information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 SetVolumeLabel(
            String VolumeLabel,
            out VolumeInfo VolumeInfo)
        {
            VolumeInfo = default(VolumeInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Gets file or directory attributes and security descriptor given a file name.
        /// </summary>
        /// <param name="FileName">
        /// The name of the file or directory to get the attributes and security descriptor for.
        /// </param>
        /// <param name="FileAttributes">
        /// Receives the file attributes on successful return.
        /// If this call returns STATUS_REPARSE, the file system may place here the index of the
        /// first reparse point within FileName.
        /// </param>
        /// <param name="SecurityDescriptor">
        /// Receives the file security descriptor. If the SecurityDescriptor parameter is null
        /// on input the file system should not fill this value.
        /// </param>
        /// <returns>
        /// STATUS_SUCCESS, STATUS_REPARSE or error code.
        /// STATUS_REPARSE should be returned by file systems that support reparse points when
        /// they encounter a FileName that contains reparse points anywhere but the final path
        /// component.
        /// </returns>
        public virtual Int32 GetSecurityByName(
            String FileName,
            out UInt32 FileAttributes/* or ReparsePointIndex */,
            ref Byte[] SecurityDescriptor)
        {
            FileAttributes = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Creates a new file or directory.
        /// </summary>
        /// <param name="FileName">
        /// The name of the file or directory to be created.
        /// </param>
        /// <param name="CreateOptions">
        /// Create options for this request.
        /// </param>
        /// <param name="GrantedAccess">
        /// Determines the specific access rights that have been granted for this request.
        /// </param>
        /// <param name="FileAttributes">
        /// File attributes to apply to the newly created file or directory.
        /// </param>
        /// <param name="SecurityDescriptor">
        /// Security descriptor to apply to the newly created file or directory.
        /// </param>
        /// <param name="AllocationSize">
        /// Allocation size for the newly created file.
        /// </param>
        /// <param name="FileNode">
        /// Receives the file node for the newly created file.
        /// </param>
        /// <param name="FileDesc">
        /// Receives the file descriptor for the newly created file.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the file information for the newly created file.
        /// </param>
        /// <param name="NormalizedName">
        /// Receives the normalized name for the newly created file.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Create(
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
        /// <summary>
        /// Opens a file or directory.
        /// </summary>
        /// <param name="FileName">
        /// The name of the file or directory to be opened.
        /// </param>
        /// <param name="CreateOptions">
        /// Create options for this request.
        /// </param>
        /// <param name="GrantedAccess">
        /// Determines the specific access rights that have been granted for this request.
        /// </param>
        /// <param name="FileNode">
        /// Receives the file node for the newly opened file.
        /// </param>
        /// <param name="FileDesc">
        /// Receives the file descriptor for the newly opened file.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the file information for the newly opened file.
        /// </param>
        /// <param name="NormalizedName">
        /// Receives the normalized name for the newly opened file.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Open(
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
        /// <summary>
        /// Overwrites a file.
        /// </summary>
        /// <param name="FileNode">
        /// The file node for the file to be overwritten.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor for the file to be overwritten.
        /// </param>
        /// <param name="FileAttributes">
        /// File attributes to apply to the overwritten file.
        /// </param>
        /// <param name="ReplaceFileAttributes">
        /// When true the existing file attributes should be replaced with the new ones.
        /// When false the existing file attributes should be merged (or'ed) with the new ones.
        /// </param>
        /// <param name="AllocationSize">
        /// Allocation size for the overwritten file.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the updated file information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Overwrite(
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
        /// <summary>
        /// Cleans up a file or directory.
        /// </summary>
        /// <remarks>
        /// <para>
        /// When CreateFile is used to open or create a file the kernel creates a kernel mode file
        /// object (type FILE_OBJECT) and a handle for it, which it returns to user-mode. The handle may
        /// be duplicated (using DuplicateHandle), but all duplicate handles always refer to the same
        /// file object. When all handles for a particular file object get closed (using CloseHandle)
        /// the system sends a Cleanup request to the file system.
        /// </para><para>
        /// There will be a Cleanup operation for every Create or Open operation posted to the user mode
        /// file system. However the Cleanup operation is not the final close operation on a file.
        /// The file system must be ready to receive additional operations until close time. This is true
        /// even when the file is being deleted!
        /// </para><para>
        /// The Flags parameter contains information about the cleanup operation:
        /// <list>
        /// <item>CleanupDelete -
        /// An important function of the Cleanup operation is to complete a delete operation. Deleting
        /// a file or directory in Windows is a three-stage process where the file is first opened, then
        /// tested to see if the delete can proceed and if the answer is positive the file is then
        /// deleted during Cleanup.
        /// When this flag is set, this is the last outstanding cleanup for this particular file node.
        /// </item>
        /// <item>CleanupSetAllocationSize -
        /// The NTFS and FAT file systems reset a file's allocation size when they receive the last
        /// outstanding cleanup for a particular file node. User mode file systems that implement
        /// allocation size and wish to duplicate the NTFS and FAT behavior can use this flag.
        /// </item>
        /// <item>CleanupSetArchiveBit -
        /// File systems that support the archive bit should set the file node's archive bit when this
        /// flag is set.
        /// </item>
        /// <item>CleanupSetLastAccessTime, CleanupSetLastWriteTime, CleanupSetChangeTime -
        /// File systems should set the corresponding file time when each one of these flags is set.
        /// Note that updating the last access time is expensive and a file system may choose to not
        /// implement it.
        /// </item>
        /// </list>
        /// </para><para>
        /// There is no way to report failure of this operation. This is a Windows limitation.
        /// </para>
        /// </remarks>
        /// <param name="FileNode">
        /// The file node of the file or directory to cleanup.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to cleanup.
        /// </param>
        /// <param name="FileName">
        /// The name of the file or directory to cleanup. Sent only when a Delete is requested.
        /// </param>
        /// <param name="Flags">
        /// These flags determine whether the file was modified and whether to delete the file.
        /// </param>
        /// <seealso cref="CanDelete"/>
        /// <seealso cref="SetDelete"/>
        /// <seealso cref="Close"/>
        public virtual void Cleanup(
            Object FileNode,
            Object FileDesc,
            String FileName,
            UInt32 Flags)
        {
        }
        /// <summary>
        /// Closes a file or directory.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file or directory to close.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to close.
        /// </param>
        public virtual void Close(
            Object FileNode,
            Object FileDesc)
        {
        }
        /// <summary>
        /// Reads a file.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file to read.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file to read.
        /// </param>
        /// <param name="Buffer">
        /// Pointer to a buffer that receives the results of the read operation.
        /// </param>
        /// <param name="Offset">
        /// Offset within the file to read from.
        /// </param>
        /// <param name="Length">
        /// Length of data to read.
        /// </param>
        /// <param name="BytesTransferred">
        /// Receives the actual number of bytes read.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Read(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            BytesTransferred = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Writes a file.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file to write.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file to write.
        /// </param>
        /// <param name="Buffer">
        /// Pointer to a buffer that receives the results of the write operation.
        /// </param>
        /// <param name="Offset">
        /// Offset within the file to write to.
        /// </param>
        /// <param name="Length">
        /// Length of data to write.
        /// </param>
        /// <param name="WriteToEndOfFile">
        /// When true the file system must write to the current end of file. In this case the Offset
        /// parameter will contain the value -1.
        /// </param>
        /// <param name="ConstrainedIo">
        /// When true the file system must not extend the file (i.e. change the file size).
        /// </param>
        /// <param name="BytesTransferred">
        /// Receives the actual number of bytes written.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the updated file information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Write(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 BytesTransferred,
            out FileInfo FileInfo)
        {
            BytesTransferred = default(UInt32);
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Flushes a file or volume.
        /// </summary>
        /// <remarks>
        /// Note that the FSD will also flush all file/volume caches prior to invoking this operation.
        /// </remarks>
        /// <param name="FileNode">
        /// The file node of the file to flush.
        /// When this and the FileDesc parameter are null the whole volume is being flushed.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file to flush.
        /// When this and the FileNode parameter are null the whole volume is being flushed.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the updated file information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Flush(
            Object FileNode,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Gets file or directory information.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file to get information for.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file to get information for.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the file information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 GetFileInfo(
            Object FileNode,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Sets file or directory basic information.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file to set information for.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file to set information for.
        /// </param>
        /// <param name="FileAttributes">
        /// File attributes to apply to the file or directory.
        /// If the value -1 is sent, the file attributes should not be changed.
        /// </param>
        /// <param name="CreationTime">
        /// Creation time to apply to the file or directory.
        /// If the value 0 is sent, the creation time should not be changed.
        /// </param>
        /// <param name="LastAccessTime">
        /// Last access time to apply to the file or directory.
        /// If the value 0 is sent, the last access time should not be changed.
        /// </param>
        /// <param name="LastWriteTime">
        /// Last write time to apply to the file or directory.
        /// If the value 0 is sent, the last write time should not be changed.
        /// </param>
        /// <param name="ChangeTime">
        /// Change time to apply to the file or directory.
        /// If the value 0 is sent, the change time should not be changed.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the updated file information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 SetBasicInfo(
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
        /// <summary>
        /// Sets file/allocation size.
        /// </summary>
        /// <remarks>
        /// <para>
        /// This function is used to change a file's sizes. Windows file systems maintain two kinds
        /// of sizes: the file size is where the End Of File (EOF) is, and the allocation size is the
        /// actual size that a file takes up on the "disk".
        /// </para><para>
        /// The rules regarding file/allocation size are:
        /// <list>
        /// <item>
        /// Allocation size must always be aligned to the allocation unit boundary. The allocation
        /// unit is the product SectorSize * SectorsPerAllocationUnit. The FSD will always send
        /// properly aligned allocation sizes when setting the allocation size.
        /// </item>
        /// <item>
        /// Allocation size is always greater or equal to the file size.
        /// </item>
        /// <item>
        /// A file size of more than the current allocation size will also extend the allocation
        /// size to the next allocation unit boundary.
        /// </item>
        /// <item>
        /// An allocation size of less than the current file size should also truncate the current
        /// file size.
        /// </item>
        /// </list>
        /// </para>
        /// </remarks>
        /// <param name="FileNode">
        /// The file node of the file to set the file/allocation size for.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file to set the file/allocation size for.
        /// </param>
        /// <param name="NewSize">
        /// New file/allocation size to apply to the file.
        /// </param>
        /// <param name="SetAllocationSize">
        /// If true, then the allocation size is being set. if false, then the file size is being set.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the updated file information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 SetFileSize(
            Object FileNode,
            Object FileDesc,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Determines whether a file or directory can be deleted.
        /// </summary>
        /// <remarks>
        /// <para>
        /// This function tests whether a file or directory can be safely deleted. This function does
        /// not need to perform access checks, but may performs tasks such as check for empty
        /// directories, etc.
        /// </para><para>
        /// This function should <b>NEVER</b> delete the file or directory in question. Deletion should
        /// happen during Cleanup with the CleanupDelete flag set.
        /// </para><para>
        /// This function gets called when Win32 API's such as DeleteFile or RemoveDirectory are used.
        /// It does not get called when a file or directory is opened with FILE_DELETE_ON_CLOSE.
        /// </para><para>
        /// NOTE: If both CanDelete and SetDelete are defined, SetDelete takes precedence. However
        /// most file systems need only implement the CanDelete operation.
        /// </para>
        /// </remarks>
        /// <param name="FileNode">
        /// The file node of the file or directory to test for deletion.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to test for deletion.
        /// </param>
        /// <param name="FileName">
        /// The name of the file or directory to test for deletion.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        /// <seealso cref="Cleanup"/>
        /// <seealso cref="SetDelete"/>
        public virtual Int32 CanDelete(
            Object FileNode,
            Object FileDesc,
            String FileName)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Renames a file or directory.
        /// </summary>
        /// <remarks>
        /// The kernel mode FSD provides certain guarantees prior to posting a rename operation:
        /// <list>
        /// <item>
        /// A file cannot be renamed if a file with the same name exists and has open handles.
        /// </item>
        /// <item>
        /// A directory cannot be renamed if it or any of its subdirectories contains a file that
        /// has open handles.
        /// </item>
        /// </list>
        /// </remarks>
        /// <param name="FileNode">
        /// The file node of the file or directory to be renamed.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to be renamed.
        /// </param>
        /// <param name="FileName">
        /// The current name of the file or directory to rename.
        /// </param>
        /// <param name="NewFileName">
        /// The new name for the file or directory.
        /// </param>
        /// <param name="ReplaceIfExists">
        /// Whether to replace a file that already exists at NewFileName.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Rename(
            Object FileNode,
            Object FileDesc,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Gets file or directory security descriptor.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file or directory to get the security descriptor for.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to get the security descriptor for.
        /// </param>
        /// <param name="SecurityDescriptor">
        /// Receives the file security descriptor.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 GetSecurity(
            Object FileNode,
            Object FileDesc,
            ref Byte[] SecurityDescriptor)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Sets file or directory security descriptor.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file or directory to set the security descriptor for.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to set the security descriptor for.
        /// </param>
        /// <param name="Sections">
        /// Describes what parts of the file or directory security descriptor should be modified.
        /// </param>
        /// <param name="SecurityDescriptor">
        /// Describes the modifications to apply to the file or directory security descriptor.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        /// <seealso cref="ModifySecurityDescriptorEx"/>
        public virtual Int32 SetSecurity(
            Object FileNode,
            Object FileDesc,
            AccessControlSections Sections,
            Byte[] SecurityDescriptor)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Reads a directory.
        /// </summary>
        /// <seealso cref="ReadDirectoryEntry"/>
        public virtual Int32 ReadDirectory(
            Object FileNode,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            return SeekableReadDirectory(FileNode, FileDesc, Pattern, Marker, Buffer, Length,
                out BytesTransferred);
        }
        /// <summary>
        /// Reads a directory entry.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the directory to be read.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the directory to be read.
        /// </param>
        /// <param name="Pattern">
        /// The pattern to match against files in this directory. Can be null. The file system
        /// can choose to ignore this parameter as the FSD will always perform its own pattern
        /// matching on the returned results.
        /// </param>
        /// <param name="Marker">
        /// A file name that marks where in the directory to start reading. Files with names
        /// that are greater than (not equal to) this marker (in the directory order determined
        /// by the file system) should be returned. Can be null.
        /// </param>
        /// <param name="Context">
        /// Can be used by the file system to track the ReadDirectory operation.
        /// </param>
        /// <param name="FileName">
        /// Receives the file name for the directory entry.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the file information for the directory entry.
        /// </param>
        /// <returns>True if there are additional directory entries to return. False otherwise.</returns>
        /// <seealso cref="ReadDirectory"/>
        public virtual Boolean ReadDirectoryEntry(
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
        /// <summary>
        /// Resolves reparse points.
        /// </summary>
        public virtual Int32 ResolveReparsePoints(
            String FileName,
            UInt32 ReparsePointIndex,
            Boolean ResolveLastPathComponent,
            out IoStatusBlock IoStatus,
            IntPtr Buffer,
            IntPtr PSize)
        {
            GCHandle Handle = GCHandle.Alloc(this, GCHandleType.Normal);
            try
            {
                return Api.FspFileSystemResolveReparsePoints(
                    IntPtr.Zero,
                    GetReparsePointByName,
                    (IntPtr)Handle,
                    FileName,
                    ReparsePointIndex,
                    ResolveLastPathComponent,
                    out IoStatus,
                    Buffer,
                    PSize);
            }
            finally
            {
                Handle.Free();
            }
        }
        /// <summary>
        /// Gets a reparse point given a file name.
        /// </summary>
        /// <param name="FileName">
        /// The name of the file or directory to get the reparse point for.
        /// </param>
        /// <param name="IsDirectory">
        /// Determines whether the passed file name is assumed to be a directory.
        /// </param>
        /// <param name="ReparseData">
        /// Receives the reparse data for the file or directory.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 GetReparsePointByName(
            String FileName,
            Boolean IsDirectory,
            ref Byte[] ReparseData)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Gets a reparse point.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the reparse point.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the reparse point.
        /// </param>
        /// <param name="FileName">
        /// The file name of the reparse point.
        /// </param>
        /// <param name="ReparseData">
        /// Receives the reparse data for the reparse point.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 GetReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            ref Byte[] ReparseData)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Sets a reparse point.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the reparse point.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the reparse point.
        /// </param>
        /// <param name="FileName">
        /// The file name of the reparse point.
        /// </param>
        /// <param name="ReparseData">
        /// The new reparse data for the reparse point.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 SetReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            Byte[] ReparseData)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Deletes a reparse point.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the reparse point.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the reparse point.
        /// </param>
        /// <param name="FileName">
        /// The file name of the reparse point.
        /// </param>
        /// <param name="ReparseData">
        /// The reparse data for the reparse point.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 DeleteReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            Byte[] ReparseData)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Gets named streams information.
        /// </summary>
        public virtual Int32 GetStreamInfo(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            Object Context = null;
            String StreamName;
            StreamInfo StreamInfo = default(StreamInfo);
            BytesTransferred = default(UInt32);
            while (GetStreamEntry(FileNode, FileDesc, ref Context,
                out StreamName, out StreamInfo.StreamSize, out StreamInfo.StreamAllocationSize))
            {
                StreamInfo.SetStreamNameBuf(StreamName);
                if (!Api.FspFileSystemAddStreamInfo(ref StreamInfo, Buffer, Length,
                    out BytesTransferred))
                    return STATUS_SUCCESS;
            }
            Api.FspFileSystemEndStreamInfo(Buffer, Length, out BytesTransferred);
            return STATUS_SUCCESS;
        }
        /// <summary>
        /// Gets named streams information entry.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the file or directory to get stream information for.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to get stream information for.
        /// </param>
        /// <param name="Context">
        /// Can be used by the file system to track the GetStreamInfo operation.
        /// </param>
        /// <param name="StreamName">
        /// Receives the stream name for the stream entry.
        /// </param>
        /// <param name="StreamSize">
        /// Receives the stream size for the stream entry.
        /// </param>
        /// <param name="StreamAllocationSize">
        /// Receives the stream allocation size for the stream entry.
        /// </param>
        /// <returns>True if there are additional stream entries to return. False otherwise.</returns>
        public virtual Boolean GetStreamEntry(
            Object FileNode,
            Object FileDesc,
            ref Object Context,
            out String StreamName,
            out UInt64 StreamSize,
            out UInt64 StreamAllocationSize)
        {
            StreamName = default(String);
            StreamSize = default(UInt64);
            StreamAllocationSize = default(UInt64);
            return false;
        }
        /// <summary>
        /// Gets directory information for a single file or directory within a parent directory.
        /// </summary>
        /// <param name="FileNode">
        /// The file node of the parent directory.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the parent directory.
        /// </param>
        /// <param name="FileName">
        /// The name of the file or directory to get information for. This name is relative
        /// to the parent directory and is a single path component.
        /// </param>
        /// <param name="NormalizedName">
        /// Receives the normalized name from the directory entry.
        /// </param>
        /// <param name="FileInfo">
        /// Receives the file information.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 GetDirInfoByName(
            Object FileNode,
            Object FileDesc,
            String FileName,
            out String NormalizedName,
            out FileInfo FileInfo)
        {
            NormalizedName = default(String);
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Processes a control code.
        /// </summary>
        /// <remarks>
        /// This function is called when a program uses the DeviceIoControl API.
        /// </remarks>
        /// <param name="FileNode">
        /// The file node of the file or directory to be controled.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to be controled.
        /// </param>
        /// <param name="ControlCode">
        /// The control code for the operation. This code must have a DeviceType with bit
        /// 0x8000 set and must have a TransferType of METHOD_BUFFERED.
        /// </param>
        /// <param name="InputBuffer">
        /// Pointer to a buffer that contains the input data.
        /// </param>
        /// <param name="InputBufferLength">
        /// Input data length.
        /// </param>
        /// <param name="OutputBuffer">
        ///  Pointer to a buffer that will receive the output data.
        /// </param>
        /// <param name="OutputBufferLength">
        /// Output data length.
        /// </param>
        /// <param name="BytesTransferred">
        /// Receives the actual number of bytes transferred.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        public virtual Int32 Control(
            Object FileNode,
            Object FileDesc,
            UInt32 ControlCode,
            IntPtr InputBuffer, UInt32 InputBufferLength,
            IntPtr OutputBuffer, UInt32 OutputBufferLength,
            out UInt32 BytesTransferred)
        {
            BytesTransferred = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        /// <summary>
        /// Sets the file delete flag.
        /// </summary>
        /// <remarks>
        /// <para>
        /// This function sets a flag to indicates whether the FSD file should delete a file
        /// when it is closed. This function does not need to perform access checks, but may
        /// performs tasks such as check for empty directories, etc.
        /// </para><para>
        /// This function should <b>NEVER</b> delete the file or directory in question. Deletion should
        /// happen during Cleanup with the CleanupDelete flag set.
        /// </para><para>
        /// This function gets called when Win32 API's such as DeleteFile or RemoveDirectory are used.
        /// It does not get called when a file or directory is opened with FILE_DELETE_ON_CLOSE.
        /// </para><para>
        /// NOTE: If both CanDelete and SetDelete are defined, SetDelete takes precedence. However
        /// most file systems need only implement the CanDelete operation.
        /// </para>
        /// </remarks>
        /// <param name="FileNode">
        /// The file node of the file or directory to set the delete flag for.
        /// </param>
        /// <param name="FileDesc">
        /// The file descriptor of the file or directory to set the delete flag for.
        /// </param>
        /// <param name="FileName">
        /// The name of the file or directory to set the delete flag for.
        /// </param>
        /// <param name="DeleteFile">
        /// If set to TRUE the FSD indicates that the file will be deleted on Cleanup; otherwise
        /// it will not be deleted. It is legal to receive multiple SetDelete calls for the same
        /// file with different DeleteFile parameters.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        /// <seealso cref="Cleanup"/>
        /// <seealso cref="CanDelete"/>
        public virtual Int32 SetDelete(
            Object FileNode,
            Object FileDesc,
            String FileName,
            Boolean DeleteFile)
        {
            if (DeleteFile)
                return CanDelete(FileNode, FileDesc, FileName);
            else
                return STATUS_SUCCESS;
        }
        public virtual Int32 CreateEx(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            Byte[] SecurityDescriptor,
            UInt64 AllocationSize,
            IntPtr ExtraBuffer,
            UInt32 ExtraLength,
            Boolean ExtraBufferIsReparsePoint,
            out Object FileNode,
            out Object FileDesc,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            return Create(
                FileName,
                CreateOptions,
                GrantedAccess,
                FileAttributes,
                SecurityDescriptor,
                AllocationSize,
                out FileNode,
                out FileDesc,
                out FileInfo,
                out NormalizedName);
        }
        public virtual Int32 OverwriteEx(
            Object FileNode,
            Object FileDesc,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            IntPtr Ea,
            UInt32 EaLength,
            out FileInfo FileInfo)
        {
            return Overwrite(
                FileNode,
                FileDesc,
                FileAttributes,
                ReplaceFileAttributes,
                AllocationSize,
                out FileInfo);
        }
        public virtual Int32 GetEa(
            Object FileNode,
            Object FileDesc,
            IntPtr Ea,
            UInt32 EaLength,
            out UInt32 BytesTransferred)
        {
            Object Context = null;
            String EaName;
            Byte[] EaValue;
            Boolean NeedEa;
            FullEaInformation EaInfo = new FullEaInformation();
            BytesTransferred = default(UInt32);
            while (GetEaEntry(FileNode, FileDesc, ref Context, out EaName, out EaValue, out NeedEa))
            {
                EaInfo.Set(EaName, EaValue, NeedEa);
                if (!Api.FspFileSystemAddEa(ref EaInfo, Ea, EaLength, out BytesTransferred))
                    return STATUS_SUCCESS;
            }
            Api.FspFileSystemEndEa(Ea, EaLength, out BytesTransferred);
            return STATUS_SUCCESS;
        }
        public virtual Boolean GetEaEntry(
            Object FileNode,
            Object FileDesc,
            ref Object Context,
            out String EaName,
            out Byte[] EaValue,
            out Boolean NeedEa)
        {
            EaName = default(String);
            EaValue = default(Byte[]);
            NeedEa = default(Boolean);
            return false;
        }
        public virtual Int32 SetEa(
            Object FileNode,
            Object FileDesc,
            IntPtr Ea,
            UInt32 EaLength,
            out FileInfo FileInfo)
        {
            Int32 Result;
            Result = SetEaEntries(
                FileNode,
                FileDesc,
                Ea,
                EaLength);
            if (0 > Result)
            {
                FileInfo = default(FileInfo);
                return Result;
            }
            return GetFileInfo(FileNode, FileDesc, out FileInfo);
        }
        public virtual Int32 SetEaEntry(
            Object FileNode,
            Object FileDesc,
            ref Object Context,
            String EaName,
            Byte[] EaValue,
            Boolean NeedEa)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }

        /* helpers */
        /// <summary>
        /// Converts a Win32 error code to a Windows kernel status code.
        /// </summary>
        public static Int32 NtStatusFromWin32(UInt32 Error)
        {
            return Api.FspNtStatusFromWin32(Error);
        }
        /// <summary>
        /// Converts a Windows kernel status code to a Win32 error code.
        /// </summary>
        public static UInt32 Win32FromNtStatus(Int32 Status)
        {
            return Api.FspWin32FromNtStatus(Status);
        }
        /// <summary>
        /// Gets the originating process ID.
        /// </summary>
        /// <remarks>
        /// Valid only during Create, Open and Rename requests when the target exists.
        /// </remarks>
        public static int GetOperationProcessId()
        {
            return (int)Api.FspFileSystemOperationProcessId();
        }
        /// <summary>
        /// Modifies a security descriptor. [OBSOLETE]
        /// </summary>
        /// <remarks>
        /// This is a helper for implementing the SetSecurity operation.
        /// </remarks>
        /// <param name="SecurityDescriptor">
        /// The original security descriptor.
        /// </param>
        /// <param name="Sections">
        /// Describes what parts of the file or directory security descriptor should be modified.
        /// </param>
        /// <param name="ModificationDescriptor">
        /// Describes the modifications to apply to the file or directory security descriptor.
        /// </param>
        /// <returns>The modified security descriptor.</returns>
        /// <seealso cref="SetSecurity"/>
        [Obsolete("use ModifySecurityDescriptorEx")]
        public static byte[] ModifySecurityDescriptor(
            Byte[] SecurityDescriptor,
            AccessControlSections Sections,
            Byte[] ModificationDescriptor)
        {
            UInt32 SecurityInformation = 0;
            if (0 != (Sections & AccessControlSections.Owner))
                SecurityInformation |= 1/*OWNER_SECURITY_INFORMATION*/;
            if (0 != (Sections & AccessControlSections.Group))
                SecurityInformation |= 2/*GROUP_SECURITY_INFORMATION*/;
            if (0 != (Sections & AccessControlSections.Access))
                SecurityInformation |= 4/*DACL_SECURITY_INFORMATION*/;
            if (0 != (Sections & AccessControlSections.Audit))
                SecurityInformation |= 8/*SACL_SECURITY_INFORMATION*/;
            return Api.ModifySecurityDescriptor(
                SecurityDescriptor,
                SecurityInformation,
                ModificationDescriptor);
        }
        /// <summary>
        /// Modifies a security descriptor.
        /// </summary>
        /// <remarks>
        /// This is a helper for implementing the SetSecurity operation.
        /// </remarks>
        /// <param name="SecurityDescriptor">
        /// The original security descriptor.
        /// </param>
        /// <param name="Sections">
        /// Describes what parts of the file or directory security descriptor should be modified.
        /// </param>
        /// <param name="ModificationDescriptor">
        /// Describes the modifications to apply to the file or directory security descriptor.
        /// </param>
        /// <param name="ModifiedDescriptor">
        /// The modified security descriptor. This parameter is modified only on success.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        /// <seealso cref="SetSecurity"/>
        public static Int32 ModifySecurityDescriptorEx(
            Byte[] SecurityDescriptor,
            AccessControlSections Sections,
            Byte[] ModificationDescriptor,
            ref Byte[] ModifiedDescriptor)
        {
            UInt32 SecurityInformation = 0;
            if (0 != (Sections & AccessControlSections.Owner))
                SecurityInformation |= 1/*OWNER_SECURITY_INFORMATION*/;
            if (0 != (Sections & AccessControlSections.Group))
                SecurityInformation |= 2/*GROUP_SECURITY_INFORMATION*/;
            if (0 != (Sections & AccessControlSections.Access))
                SecurityInformation |= 4/*DACL_SECURITY_INFORMATION*/;
            if (0 != (Sections & AccessControlSections.Audit))
                SecurityInformation |= 8/*SACL_SECURITY_INFORMATION*/;
            return Api.ModifySecurityDescriptorEx(
                SecurityDescriptor,
                SecurityInformation,
                ModificationDescriptor,
                ref ModifiedDescriptor);
        }
        public Int32 SeekableReadDirectory(
            Object FileNode,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            Object Context = null;
            String FileName;
            DirInfo DirInfo = default(DirInfo);
            BytesTransferred = default(UInt32);
            while (ReadDirectoryEntry(FileNode, FileDesc, Pattern, Marker,
                ref Context, out FileName, out DirInfo.FileInfo))
            {
                DirInfo.SetFileNameBuf(FileName);
                if (!Api.FspFileSystemAddDirInfo(ref DirInfo, Buffer, Length,
                    out BytesTransferred))
                    return STATUS_SUCCESS;
            }
            Api.FspFileSystemEndDirInfo(Buffer, Length, out BytesTransferred);
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
            out UInt32 BytesTransferred)
        {
            Object Context = null;
            String FileName;
            DirInfo DirInfo = default(DirInfo);
            Int32 DirBufferResult = STATUS_SUCCESS;
            BytesTransferred = default(UInt32);
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
                BytesTransferred = default(UInt32);
                return DirBufferResult;
            }
            Api.FspFileSystemReadDirectoryBuffer(ref DirectoryBuffer.DirBuffer,
                Marker, Buffer, Length, out BytesTransferred);
            return STATUS_SUCCESS;
        }
        /// <summary>
        /// Finds a reparse point in file name.
        /// </summary>
        /// <remarks>
        /// This is a helper for implementing the GetSecurityByName operation in file systems
        /// that support reparse points.
        /// </remarks>
        /// <param name="FileName">
        /// The name of the file or directory.
        /// </param>
        /// <param name="ReparsePointIndex">
        /// Receives the index of the first reparse point within FileName.
        /// </param>
        /// <returns>True if a reparse point was found, false otherwise.</returns>
        /// <seealso cref="GetSecurityByName"/>
        public Boolean FindReparsePoint(
            String FileName,
            out UInt32 ReparsePointIndex)
        {
            GCHandle Handle = GCHandle.Alloc(this, GCHandleType.Normal);
            try
            {
                return Api.FspFileSystemFindReparsePoint(
                    IntPtr.Zero,
                    GetReparsePointByName,
                    (IntPtr)Handle,
                    FileName,
                    out ReparsePointIndex);
            }
            finally
            {
                Handle.Free();
            }
        }
        /// <summary>
        /// Makes a byte array that contains a reparse point.
        /// </summary>
        /// <returns>The reparse point byte array.</returns>
        public static Byte[] MakeReparsePoint(
            IntPtr Buffer,
            UInt32 Size)
        {
            return Api.MakeReparsePoint(Buffer, (UIntPtr)Size);
        }
        /// <summary>
        /// Gets the reparse tag from reparse data.
        /// </summary>
        /// <param name="ReparseData">
        /// The reparse data to extract the reparse tag from.
        /// </param>
        /// <returns>The reparse tag.</returns>
        public static UInt32 GetReparseTag(
            Byte[] ReparseData)
        {
            return BitConverter.ToUInt32(ReparseData, 0);
        }
        /// <summary>
        /// Tests whether reparse data can be replaced.
        /// </summary>
        /// <remarks>
        /// This is a helper for implementing the SetReparsePoint/DeleteReparsePoint operation
        /// in file systems that support reparse points.
        /// </remarks>
        /// <param name="CurrentReparseData">
        /// The current reparse data.
        /// </param>
        /// <param name="ReplaceReparseData">
        /// The replacement reparse data.
        /// </param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        /// <seealso cref="SetReparsePoint"/>
        /// <seealso cref="DeleteReparsePoint"/>
        public static Int32 CanReplaceReparsePoint(
            Byte[] CurrentReparseData,
            Byte[] ReplaceReparseData)
        {
            return Api.FspFileSystemCanReplaceReparsePoint(CurrentReparseData, ReplaceReparseData);
        }
        private static Int32 GetReparsePointByName(
            IntPtr FileSystem,
            IntPtr Context,
            String FileName,
            Boolean IsDirectory,
            IntPtr Buffer,
            IntPtr PSize)
        {
            FileSystemBase self = (FileSystemBase)GCHandle.FromIntPtr(Context).Target;
            try
            {
                Byte[] ReparseData;
                Int32 Result;
                ReparseData = null;
                Result = self.GetReparsePointByName(
                    FileName,
                    IsDirectory,
                    ref ReparseData);
                if (0 <= Result)
                    Result = Api.CopyReparsePoint(ReparseData, Buffer, PSize);
                return Result;

            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        public Int32 SetEaEntries(
            Object FileNode,
            Object FileDesc,
            IntPtr Ea,
            UInt32 EaLength)
        {
            return Api.FspFileSystemEnumerateEa(
                FileNode,
                FileDesc,
                this.SetEaEntry,
                Ea,
                EaLength);
        }
        public static UInt32 GetEaEntrySize(
            String EaName,
            Byte[] EaValue,
            Boolean NeedEa)
        {
            return FullEaInformation.PackedSize(EaName, EaValue, NeedEa);
        }
    }

}
