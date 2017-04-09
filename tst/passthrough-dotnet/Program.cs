/**
 * @file Program.cs
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
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;

using Fsp;
using VolumeInfo = Fsp.Interop.VolumeInfo;
using FileInfo = Fsp.Interop.FileInfo;

namespace passthrough
{
    class Ptfs : FileSystem
    {
        protected class FileDesc
        {
            public FileSystemInfo Info;
            public FileStream Stream;
            public DirectoryBuffer DirBuffer;

            public FileDesc(FileSystemInfo Info, FileStream Stream)
            {
                this.Info = Info;
                this.Stream = Stream;
            }
            public UInt32 FileAttributes
            {
                get
                {
                    return (UInt32)Info.Attributes;
                }
                set
                {
                    Info.Attributes = 0 == value ?
                        System.IO.FileAttributes.Normal : (FileAttributes)value;
                }
            }
        }

        private const int ALLOCATION_UNIT = 4096;

        public Ptfs() : base()
        {
            SetSectorSize(ALLOCATION_UNIT);
            SetSectorsPerAllocationUnit(1);
            SetMaxComponentLength(255);
            SetFileInfoTimeout(1000);
            SetCaseSensitiveSearch(false);
            SetCasePreservedNames(true);
            SetUnicodeOnDisk(true);
            SetPersistentAcls(true);
            SetPostCleanupWhenModifiedOnly(true);
            SetPassQueryDirectoryPattern(true);
        }
        public void SetPath(String value)
        {
            _Path = Path.GetFullPath(value);
            if (_Path.EndsWith("\\"))
                _Path = _Path.Substring(0, _Path.Length - 1);
            SetVolumeCreationTime((UInt64)File.GetCreationTimeUtc(_Path).ToFileTimeUtc());
            SetVolumeSerialNumber(0);
        }

        protected override Int32 ExceptionHandler(Exception ex)
        {
            Int32 HResult = ex.HResult; /* needs Framework 4.5 */
            if (0x80070000 == (HResult & 0xFFFF0000))
                return NtStatusFromWin32((UInt32)HResult & 0xFFFF);
            return STATUS_UNEXPECTED_IO_ERROR;
        }
        protected String ConcatPath(String FileName)
        {
            return _Path + FileName;
        }
        protected Int32 GetFileInfoInternal(FileSystemInfo Info, Boolean Refresh,
            out FileInfo FileInfo)
        {
            if (Refresh)
                Info.Refresh();
            FileInfo.FileAttributes = (UInt32)Info.Attributes;
            FileInfo.ReparseTag = 0;
            FileInfo.FileSize = Info is System.IO.FileInfo ?
                (UInt64)((System.IO.FileInfo)Info).Length : 0;
            FileInfo.AllocationSize = (FileInfo.FileSize + ALLOCATION_UNIT - 1)
                / ALLOCATION_UNIT * ALLOCATION_UNIT;
            FileInfo.CreationTime = (UInt64)Info.CreationTimeUtc.ToFileTimeUtc();
            FileInfo.LastAccessTime = (UInt64)Info.LastAccessTimeUtc.ToFileTimeUtc();
            FileInfo.LastWriteTime = (UInt64)Info.LastWriteTimeUtc.ToFileTimeUtc();
            FileInfo.ChangeTime = FileInfo.LastWriteTime;
            FileInfo.IndexNumber = 0;
            FileInfo.HardLinks = 0;
            return STATUS_SUCCESS;
        }
        protected override Int32 GetVolumeInfo(
            out VolumeInfo VolumeInfo)
        {
            VolumeInfo = default(VolumeInfo);
            try
            {
                DriveInfo Info = new DriveInfo(_Path);
                VolumeInfo.TotalSize = (UInt64)Info.TotalSize;
                VolumeInfo.FreeSize = (UInt64)Info.TotalFreeSpace;
            }
            catch (ArgumentException)
            {
                /*
                 * DriveInfo only supports drives and does not support UNC paths.
                 * It would be better to use GetDiskFreeSpaceEx here.
                 */
            }
            return STATUS_SUCCESS;
        }
        protected override Int32 GetSecurityByName(
            String FileName,
            out UInt32 FileAttributes/* or ReparsePointIndex */,
            ref Byte[] SecurityDescriptor)
        {
            FileName = ConcatPath(FileName);
            System.IO.FileInfo Info = new System.IO.FileInfo(FileName);
            FileAttributes = (UInt32)Info.Attributes;
            if (null != SecurityDescriptor)
                SecurityDescriptor = Info.GetAccessControl().GetSecurityDescriptorBinaryForm();
            return STATUS_SUCCESS;
        }
        protected override Int32 Create(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            Byte[] SecurityDescriptor,
            UInt64 AllocationSize,
            out Object FileNode,
            out Object FileDesc0,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileDesc FileDesc;
            FileName = ConcatPath(FileName);
            if (0 != (CreateOptions & FILE_DIRECTORY_FILE))
            {
                DirectorySecurity Security = null;
                if (null != SecurityDescriptor)
                {
                    Security = new DirectorySecurity();
                    Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor);
                }
                // ???: FILE_DELETE_ON_CLOSE
                FileDesc = new FileDesc(
                    Directory.CreateDirectory(FileName, Security),
                    null);
            }
            else
            {
                FileSecurity Security = null;
                if (null != SecurityDescriptor)
                {
                    Security = new FileSecurity();
                    Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor);
                }
                FileOptions Options = 0 != (CreateOptions & FILE_DELETE_ON_CLOSE) ?
                    FileOptions.DeleteOnClose : 0;
                FileDesc = new FileDesc(
                    new System.IO.FileInfo(FileName),
                    new FileStream(
                        FileName,
                        FileMode.CreateNew,
                        (FileSystemRights)GrantedAccess,
                        FileShare.Read | FileShare.Write | FileShare.Delete,
                        4096,
                        Options,
                        Security));
            }
            FileDesc.FileAttributes = FileAttributes;
            FileNode = default(Object);
            FileDesc0 = FileDesc;
            NormalizedName = default(String);
            return GetFileInfoInternal(FileDesc.Info, false, out FileInfo);
        }
        protected override Int32 Open(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            out Object FileNode,
            out Object FileDesc0,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileDesc FileDesc;
            FileName = ConcatPath(FileName);
            if (Directory.Exists(FileName))
            {
                // ???: FILE_DELETE_ON_CLOSE
                FileDesc = new FileDesc(
                    new System.IO.DirectoryInfo(FileName),
                    null);
            }
            else
            {
                FileOptions Options = 0 != (CreateOptions & FILE_DELETE_ON_CLOSE) ?
                    FileOptions.DeleteOnClose : 0;
                FileDesc = new FileDesc(
                    new System.IO.FileInfo(FileName),
                    new FileStream(
                        FileName,
                        FileMode.Open,
                        (FileSystemRights)GrantedAccess,
                        FileShare.Read | FileShare.Write | FileShare.Delete,
                        4096,
                        Options));
            }
            FileNode = default(Object);
            FileDesc0 = FileDesc;
            NormalizedName = default(String);
            return GetFileInfoInternal(FileDesc.Info, false, out FileInfo);
        }
        protected override Int32 Overwrite(
            Object FileNode,
            Object FileDesc0,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (ReplaceFileAttributes)
                FileDesc.FileAttributes = FileAttributes;
            else if (0 != FileAttributes)
                FileDesc.FileAttributes |= FileAttributes;
            FileDesc.Stream.SetLength(0);
            return GetFileInfoInternal(FileDesc.Info, true, out FileInfo);
        }
        protected override void Cleanup(
            Object FileNode,
            Object FileDesc0,
            String FileName,
            UInt32 Flags)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (0 == (Flags & CleanupDelete))
                FileDesc.Stream.Dispose();
        }
        protected override void Close(
            Object FileNode,
            Object FileDesc0)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null != FileDesc.Stream)
                FileDesc.Stream.Dispose();
            if (null != FileDesc.DirBuffer)
                FileDesc.DirBuffer.Dispose();
        }
        protected override Int32 Read(
            Object FileNode,
            Object FileDesc0,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            Byte[] Bytes = new byte[Length];
            FileDesc.Stream.Seek((Int64)Offset, SeekOrigin.Begin);
            PBytesTransferred = (UInt32)FileDesc.Stream.Read(Bytes, 0, Bytes.Length);
            Marshal.Copy(Bytes, 0, Buffer, Bytes.Length);
            return STATUS_SUCCESS;
        }
        protected override Int32 Write(
            Object FileNode,
            Object FileDesc0,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 PBytesTransferred,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            UInt64 FileSize;
            if (ConstrainedIo)
            {
                FileDesc.Info.Refresh();
                FileSize = (UInt64)((System.IO.FileInfo)FileDesc.Info).Length;
                if (Offset >= FileSize)
                {
                    PBytesTransferred = default(UInt32);
                    FileInfo = default(FileInfo);
                    return STATUS_SUCCESS;
                }
                if (Offset + Length > FileSize)
                    Length = (UInt32)(FileSize - Offset);
            }
            Byte[] Bytes = new byte[Length];
            Marshal.Copy(Buffer, Bytes, 0, Bytes.Length);
            FileDesc.Stream.Seek((Int64)Offset, SeekOrigin.Begin);
            FileDesc.Stream.Write(Bytes, 0, Bytes.Length);
            PBytesTransferred = (UInt32)Bytes.Length;
            return GetFileInfoInternal(FileDesc.Info, true, out FileInfo);
        }
        protected override Int32 Flush(
            Object FileNode,
            Object FileDesc0,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null == FileDesc)
            {
                /* we do not flush the whole volume, so just return SUCCESS */
                FileInfo = default(FileInfo);
                return STATUS_SUCCESS;
            }
            FileDesc.Stream.Flush(true);
            return GetFileInfoInternal(FileDesc.Info, true, out FileInfo);
        }
        protected override Int32 GetFileInfo(
            Object FileNode,
            Object FileDesc0,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            return GetFileInfoInternal(FileDesc.Info, true, out FileInfo);
        }
        protected override Int32 SetBasicInfo(
            Object FileNode,
            Object FileDesc0,
            UInt32 FileAttributes,
            UInt64 CreationTime,
            UInt64 LastAccessTime,
            UInt64 LastWriteTime,
            UInt64 ChangeTime,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (unchecked((UInt32)(-1)) != FileAttributes)
                FileDesc.FileAttributes = FileAttributes;
            if (0 != CreationTime)
                FileDesc.Info.CreationTimeUtc = DateTime.FromFileTimeUtc((Int64)CreationTime);
            if (0 != LastAccessTime)
                FileDesc.Info.LastAccessTimeUtc = DateTime.FromFileTimeUtc((Int64)LastAccessTime);
            if (0 != LastWriteTime)
                FileDesc.Info.LastWriteTimeUtc = DateTime.FromFileTimeUtc((Int64)LastWriteTime);
            return GetFileInfoInternal(FileDesc.Info, true, out FileInfo);
        }
        protected override Int32 SetFileSize(
            Object FileNode,
            Object FileDesc0,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            GetFileInfoInternal(FileDesc.Info, true, out FileInfo);
            if (!SetAllocationSize || FileInfo.FileSize > NewSize)
            {
                /*
                 * "FileInfo.FileSize > NewSize" explanation:
                 * Ptfs does not support allocation size. However if the new AllocationSize
                 * is less than the current FileSize we must truncate the file.
                 */
                FileDesc.Stream.SetLength((Int64)NewSize);
                FileInfo.FileSize = NewSize;
                FileInfo.AllocationSize = (FileInfo.FileSize + ALLOCATION_UNIT - 1)
                    / ALLOCATION_UNIT * ALLOCATION_UNIT;
            }
            return STATUS_SUCCESS;
        }
        protected override Int32 CanDelete(
            Object FileNode,
            Object FileDesc0,
            String FileName)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            FileName = ConcatPath(FileName);
            /*
             * If a file has an open handle the Delete call below
             * will only mark it for disposition.
             */
            if (null == FileDesc.Stream)
                Directory.Delete(FileName);
            else
                File.Delete(FileName);
            return STATUS_SUCCESS;
        }
        protected override Int32 Rename(
            Object FileNode,
            Object FileDesc0,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            FileName = ConcatPath(FileName);
            NewFileName = ConcatPath(NewFileName);
            if (null == FileDesc.Stream)
            {
                if (ReplaceIfExists)
                    return STATUS_ACCESS_DENIED;
                Directory.Move(FileName, NewFileName);
            }
            else
            {
                if (ReplaceIfExists)
                    File.Delete(NewFileName);
                File.Move(FileName, NewFileName);
            }
            return STATUS_SUCCESS;
        }
        protected override Int32 GetSecurity(
            Object FileNode,
            Object FileDesc0,
            ref Byte[] SecurityDescriptor)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null == FileDesc.Stream)
                SecurityDescriptor = ((System.IO.DirectoryInfo)FileDesc.Info).GetAccessControl().
                    GetSecurityDescriptorBinaryForm();
            else
                SecurityDescriptor = ((System.IO.FileInfo)FileDesc.Info).GetAccessControl().
                    GetSecurityDescriptorBinaryForm();
            return STATUS_SUCCESS;
        }
        protected override Int32 SetSecurity(
            Object FileNode,
            Object FileDesc0,
            AccessControlSections Sections,
            Byte[] SecurityDescriptor)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null == FileDesc.Stream)
            {
                DirectorySecurity Security = ((System.IO.DirectoryInfo)FileDesc.Info).GetAccessControl();
                Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor, Sections);
                ((System.IO.DirectoryInfo)FileDesc.Info).SetAccessControl(Security);
            }
            else
            {
                FileSecurity Security = ((System.IO.FileInfo)FileDesc.Info).GetAccessControl();
                Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor, Sections);
                ((System.IO.FileInfo)FileDesc.Info).SetAccessControl(Security);
            }
            return STATUS_SUCCESS;
        }
        protected override Int32 ReadDirectory(
            Object FileNode,
            Object FileDesc0,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null == FileDesc.DirBuffer)
                FileDesc.DirBuffer = new DirectoryBuffer();
            return BufferedReadDirectory(FileDesc.DirBuffer,
                FileNode, FileDesc, Pattern, Marker, Buffer, Length, out PBytesTransferred);
        }
        protected override Boolean ReadDirectoryEntry(
            Object FileNode,
            Object FileDesc0,
            String Pattern,
            String Marker,
            ref Object Context,
            out String FileName,
            out FileInfo FileInfo)
        {
            if (null == Context)
            {
                FileDesc FileDesc = (FileDesc)FileDesc0;
                if (null == Pattern)
                    Pattern = "*";
                Context = (FileDesc.Info as DirectoryInfo).EnumerateFileSystemInfos(Pattern).
                    GetEnumerator();
            }
            IEnumerator<FileSystemInfo> InfoEnumerator = (IEnumerator<FileSystemInfo>)Context;
            if (InfoEnumerator.MoveNext())
            {
                FileName = InfoEnumerator.Current.Name;
                GetFileInfoInternal(InfoEnumerator.Current, false, out FileInfo);
                return true;
            }
            else
            {
                FileName = default(String);
                FileInfo = default(FileInfo);
                return false;
            }
        }

        private String _Path;
    }

    class PtfsService : Service
    {
        private class CommandLineUsageException : Exception
        {
            public CommandLineUsageException(String Message = null) : base(Message)
            {
                HasMessage = null != Message;
            }

            public bool HasMessage;
        }

        private const String PROGNAME = "passthrough-dotnet";

        public PtfsService() : base("PtfsService")
        {
        }

        protected override void OnStart(String[] Args)
        {
            String FailMessage = null;
            try
            {
                String DebugLogFile = null;
                UInt32 DebugFlags = 0;
                String VolumePrefix = null;
                String PassThrough = null;
                String MountPoint = null;
                IntPtr DebugLogHandle = (IntPtr)(-1);
                Ptfs Ptfs = null;
                int I;

                for (I = 1; Args.Length > I; I++)
                {
                    String Arg = Args[I];
                    if ('-' != Arg[0])
                        break;
                    switch (Arg[1])
                    {
                    case '?':
                        throw new CommandLineUsageException();
                    case 'd':
                        argtol(Args, ref I, ref DebugFlags);
                        break;
                    case 'D':
                        argtos(Args, ref I, ref DebugLogFile);
                        break;
                    case 'm':
                        argtos(Args, ref I, ref MountPoint);
                        break;
                    case 'p':
                        argtos(Args, ref I, ref PassThrough);
                        break;
                    case 'u':
                        argtos(Args, ref I, ref VolumePrefix);
                        break;
                    default:
                        throw new CommandLineUsageException();
                    }
                }

                if (Args.Length > I)
                    throw new CommandLineUsageException();

                if (null == PassThrough && null != VolumePrefix)
                {
                    I = VolumePrefix.IndexOf('\\');
                    if (-1 != I && VolumePrefix.Length > I && '\\' != VolumePrefix[I + 1])
                    {
                        I = VolumePrefix.IndexOf('\\', I + 1);
                        if (-1 != I &&
                            VolumePrefix.Length > I + 1 &&
                            (
                            ('A' <= VolumePrefix[I + 1] && VolumePrefix[I + 1] <= 'Z') ||
                            ('a' <= VolumePrefix[I + 1] && VolumePrefix[I + 1] <= 'z')
                            ) &&
                            '$' == VolumePrefix[I + 2])
                        {
                            PassThrough = String.Format("{0}:{1}", VolumePrefix[I + 1], VolumePrefix.Substring(I + 3));
                        }
                    }
                }

                if (null == PassThrough || null == MountPoint)
                    throw new CommandLineUsageException();

                //EnableBackupRestorePrivileges();

                if (null != DebugLogFile)
                    if (0 > FileSystem.SetDebugLogFile(DebugLogFile))
                        throw new CommandLineUsageException("cannot open debug log file");

                FailMessage = "cannot create file system";
                Ptfs = new Ptfs();
                Ptfs.SetPrefix(VolumePrefix);
                Ptfs.SetPath(PassThrough);

                FailMessage = "cannot mount file system";
                Ptfs.Mount(MountPoint, null, true, DebugFlags);
                MountPoint = Ptfs.MountPoint();
                _Ptfs = Ptfs;

                Log(EVENTLOG_INFORMATION_TYPE, String.Format("{0}{1}{2} -p {3} -m {4}",
                    PROGNAME,
                    null != VolumePrefix && 0 < VolumePrefix.Length ? " -u " : "",
                        null != VolumePrefix && 0 < VolumePrefix.Length ? VolumePrefix : "",
                    PassThrough,
                    MountPoint));
            }
            catch (CommandLineUsageException ex)
            {
                Log(EVENTLOG_ERROR_TYPE, String.Format(
                    "{0}" +
                    "usage: {1} OPTIONS\n" +
                    "\n" +
                    "options:\n" +
                    "    -d DebugFlags       [-1: enable all debug logs]\n" +
                    "    -D DebugLogFile     [file path; use - for stderr]\n" +
                    "    -u \\Server\\Share    [UNC prefix (single backslash)]\n" +
                    "    -p Directory        [directory to expose as pass through file system]\n" +
                    "    -m MountPoint       [X:|*|directory]\n",
                    ex.HasMessage ? ex.Message + "\n" : "",
                    PROGNAME));
                throw;
            }
            catch (Exception ex)
            {
                Log(EVENTLOG_ERROR_TYPE, String.Format("{0}{1}",
                    null != FailMessage ? FailMessage + "\n" : "",
                    ex.Message));
                throw;
            }
        }
        protected override void OnStop()
        {
            _Ptfs.Unmount();
            _Ptfs = null;
        }

        private static void argtos(String[] Args, ref int I, ref String V)
        {
            if (Args.Length > ++I)
                V = Args[I];
            else
                throw new CommandLineUsageException();
        }
        private static void argtol(String[] Args, ref int I, ref UInt32 V)
        {
            Int32 R;
            if (Args.Length > ++I)
                V = Int32.TryParse(Args[I], out R) ? (UInt32)R : V;
            else
                throw new CommandLineUsageException();
        }

#if false
        /*
         * Turns out there is no managed way to adjust privileges.
         * So we have to write our own using P/Invoke. Joy!
         */
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
                return NtStatusFromWin32(GetLastError());

            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token))
                return NtStatusFromWin32(GetLastError());

            if (!AdjustTokenPrivileges(Token, FALSE, &Privileges.P, 0, 0, 0))
            {
                CloseHandle(Token);

                return NtStatusFromWin32(GetLastError());
            }

            CloseHandle(Token);

            return STATUS_SUCCESS;
        }
#endif

        private Ptfs _Ptfs;
    }

    class Program
    {
        static void Main(string[] args)
        {
            Environment.ExitCode = new PtfsService().Run();
        }
    }
}
