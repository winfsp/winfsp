/**
 * @file Program.cs
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

using System;
using System.Collections;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;

using Fsp;
using VolumeInfo = Fsp.Interop.VolumeInfo;
using FileInfo = Fsp.Interop.FileInfo;

namespace passthrough
{
    class Ptfs : FileSystemBase
    {
        protected const int ALLOCATION_UNIT = 4096;

        protected static void ThrowIoExceptionWithHResult(Int32 HResult)
        {
            throw new IOException(null, HResult);
        }
        protected static void ThrowIoExceptionWithWin32(Int32 Error)
        {
            ThrowIoExceptionWithHResult(unchecked((Int32)(0x80070000 | Error)));
        }
        protected static void ThrowIoExceptionWithNtStatus(Int32 Status)
        {
            ThrowIoExceptionWithWin32((Int32)Win32FromNtStatus(Status));
        }

        protected class FileDesc
        {
            public FileStream Stream;
            public DirectoryInfo DirInfo;
            public DictionaryEntry[] FileSystemInfos;

            public FileDesc(FileStream Stream)
            {
                this.Stream = Stream;
            }
            public FileDesc(DirectoryInfo DirInfo)
            {
                this.DirInfo = DirInfo;
            }
            public static void GetFileInfoFromFileSystemInfo(
                FileSystemInfo Info,
                out FileInfo FileInfo)
            {
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
            }
            public Int32 GetFileInfo(out FileInfo FileInfo)
            {
                if (null != Stream)
                {
                    BY_HANDLE_FILE_INFORMATION Info;
                    if (!GetFileInformationByHandle(Stream.SafeFileHandle.DangerousGetHandle(),
                        out Info))
                        ThrowIoExceptionWithWin32(Marshal.GetLastWin32Error());
                    FileInfo.FileAttributes = Info.dwFileAttributes;
                    FileInfo.ReparseTag = 0;
                    FileInfo.FileSize = (UInt64)Stream.Length;
                    FileInfo.AllocationSize = (FileInfo.FileSize + ALLOCATION_UNIT - 1)
                        / ALLOCATION_UNIT * ALLOCATION_UNIT;
                    FileInfo.CreationTime = Info.ftCreationTime;
                    FileInfo.LastAccessTime = Info.ftLastAccessTime;
                    FileInfo.LastWriteTime = Info.ftLastWriteTime;
                    FileInfo.ChangeTime = FileInfo.LastWriteTime;
                    FileInfo.IndexNumber = 0;
                    FileInfo.HardLinks = 0;
                }
                else
                    GetFileInfoFromFileSystemInfo(DirInfo, out FileInfo);
                return STATUS_SUCCESS;
            }
            public void SetBasicInfo(
                UInt32 FileAttributes,
                UInt64 CreationTime,
                UInt64 LastAccessTime,
                UInt64 LastWriteTime)
            {
                if (0 == FileAttributes)
                    FileAttributes = (UInt32)System.IO.FileAttributes.Normal;
                if (null != Stream)
                {
                    FILE_BASIC_INFO Info = default(FILE_BASIC_INFO);
                    if (unchecked((UInt32)(-1)) != FileAttributes)
                        Info.FileAttributes = FileAttributes;
                    if (0 != CreationTime)
                        Info.CreationTime = CreationTime;
                    if (0 != LastAccessTime)
                        Info.LastAccessTime = LastAccessTime;
                    if (0 != LastWriteTime)
                        Info.LastWriteTime = LastWriteTime;
                    if (!SetFileInformationByHandle(Stream.SafeFileHandle.DangerousGetHandle(),
                        0/*FileBasicInfo*/, ref Info, (UInt32)Marshal.SizeOf(Info)))
                        ThrowIoExceptionWithWin32(Marshal.GetLastWin32Error());
                }
                else
                {
                    if (unchecked((UInt32)(-1)) != FileAttributes)
                        DirInfo.Attributes = (System.IO.FileAttributes)FileAttributes;
                    if (0 != CreationTime)
                        DirInfo.CreationTimeUtc = DateTime.FromFileTimeUtc((Int64)CreationTime);
                    if (0 != LastAccessTime)
                        DirInfo.LastAccessTimeUtc = DateTime.FromFileTimeUtc((Int64)LastAccessTime);
                    if (0 != LastWriteTime)
                        DirInfo.LastWriteTimeUtc = DateTime.FromFileTimeUtc((Int64)LastWriteTime);
                }
            }
            public UInt32 GetFileAttributes()
            {
                FileInfo FileInfo;
                GetFileInfo(out FileInfo);
                return FileInfo.FileAttributes;
            }
            public void SetFileAttributes(UInt32 FileAttributes)
            {
                SetBasicInfo(FileAttributes, 0, 0, 0);
            }
            public Byte[] GetSecurityDescriptor()
            {
                if (null != Stream)
                    return Stream.GetAccessControl().GetSecurityDescriptorBinaryForm();
                else
                    return DirInfo.GetAccessControl().GetSecurityDescriptorBinaryForm();
            }
            public void SetSecurityDescriptor(AccessControlSections Sections, Byte[] SecurityDescriptor)
            {
                Int32 SecurityInformation = 0;
                if (0 != (Sections & AccessControlSections.Owner))
                    SecurityInformation |= 1/*OWNER_SECURITY_INFORMATION*/;
                if (0 != (Sections & AccessControlSections.Group))
                    SecurityInformation |= 2/*GROUP_SECURITY_INFORMATION*/;
                if (0 != (Sections & AccessControlSections.Access))
                    SecurityInformation |= 4/*DACL_SECURITY_INFORMATION*/;
                if (0 != (Sections & AccessControlSections.Audit))
                    SecurityInformation |= 8/*SACL_SECURITY_INFORMATION*/;
                if (null != Stream)
                {
                    if (!SetKernelObjectSecurity(Stream.SafeFileHandle.DangerousGetHandle(),
                        SecurityInformation, SecurityDescriptor))
                        ThrowIoExceptionWithWin32(Marshal.GetLastWin32Error());
                }
                else
                {
                    if (!SetFileSecurityW(DirInfo.FullName,
                        SecurityInformation, SecurityDescriptor))
                        ThrowIoExceptionWithWin32(Marshal.GetLastWin32Error());
                }
            }
            public void SetDisposition(Boolean Safe)
            {
                if (null != Stream)
                {
                    FILE_DISPOSITION_INFO Info;
                    Info.DeleteFile = true;
                    if (!SetFileInformationByHandle(Stream.SafeFileHandle.DangerousGetHandle(),
                        4/*FileDispositionInfo*/, ref Info, (UInt32)Marshal.SizeOf(Info)))
                        if (!Safe)
                            ThrowIoExceptionWithWin32(Marshal.GetLastWin32Error());
                }
                else
                    try
                    {
                        DirInfo.Delete();
                    }
                    catch (Exception ex)
                    {
                        if (!Safe)
                            ThrowIoExceptionWithHResult(ex.HResult);
                    }
            }
            public static void Rename(String FileName, String NewFileName, Boolean ReplaceIfExists)
            {
                if (!MoveFileExW(FileName, NewFileName, ReplaceIfExists ? 1U/*MOVEFILE_REPLACE_EXISTING*/ : 0))
                    ThrowIoExceptionWithWin32(Marshal.GetLastWin32Error());
            }

            /* interop */
            [StructLayout(LayoutKind.Sequential, Pack = 4)]
            private struct BY_HANDLE_FILE_INFORMATION
            {
                public UInt32 dwFileAttributes;
                public UInt64 ftCreationTime;
                public UInt64 ftLastAccessTime;
                public UInt64 ftLastWriteTime;
                public UInt32 dwVolumeSerialNumber;
                public UInt32 nFileSizeHigh;
                public UInt32 nFileSizeLow;
                public UInt32 nNumberOfLinks;
                public UInt32 nFileIndexHigh;
                public UInt32 nFileIndexLow;
            }
            [StructLayout(LayoutKind.Sequential)]
            private struct FILE_BASIC_INFO
            {
                public UInt64 CreationTime;
                public UInt64 LastAccessTime;
                public UInt64 LastWriteTime;
                public UInt64 ChangeTime;
                public UInt32 FileAttributes;
            }
            [StructLayout(LayoutKind.Sequential)]
            private struct FILE_DISPOSITION_INFO
            {
                public Boolean DeleteFile;
            }
            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern Boolean GetFileInformationByHandle(
                IntPtr hFile,
                out BY_HANDLE_FILE_INFORMATION lpFileInformation);
            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern Boolean SetFileInformationByHandle(
                IntPtr hFile,
                Int32 FileInformationClass,
                ref FILE_BASIC_INFO lpFileInformation,
                UInt32 dwBufferSize);
            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern Boolean SetFileInformationByHandle(
                IntPtr hFile,
                Int32 FileInformationClass,
                ref FILE_DISPOSITION_INFO lpFileInformation,
                UInt32 dwBufferSize);
            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern Boolean MoveFileExW(
                [MarshalAs(UnmanagedType.LPWStr)] String lpExistingFileName,
                [MarshalAs(UnmanagedType.LPWStr)] String lpNewFileName,
                UInt32 dwFlags);
            [DllImport("advapi32.dll", SetLastError = true)]
            private static extern Boolean SetFileSecurityW(
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                Int32 SecurityInformation,
                Byte[] SecurityDescriptor);
            [DllImport("advapi32.dll", SetLastError = true)]
            private static extern Boolean SetKernelObjectSecurity(
                IntPtr Handle,
                Int32 SecurityInformation,
                Byte[] SecurityDescriptor);
        }

        private class DirectoryEntryComparer : IComparer
        {
            public int Compare(object x, object y)
            {
                return String.Compare(
                    (String)((DictionaryEntry)x).Key,
                    (String)((DictionaryEntry)y).Key);
            }
        }
        private static DirectoryEntryComparer _DirectoryEntryComparer =
            new DirectoryEntryComparer();

        public Ptfs(String Path0)
        {
            _Path = Path.GetFullPath(Path0);
            if (_Path.EndsWith("\\"))
                _Path = _Path.Substring(0, _Path.Length - 1);
        }
        public String ConcatPath(String FileName)
        {
            return _Path + FileName;
        }
        public override Int32 ExceptionHandler(Exception ex)
        {
            Int32 HResult = ex.HResult; /* needs Framework 4.5 */
            if (0x80070000 == (HResult & 0xFFFF0000))
                return NtStatusFromWin32((UInt32)HResult & 0xFFFF);
            return STATUS_UNEXPECTED_IO_ERROR;
        }
        public override Int32 Init(Object Host0)
        {
            FileSystemHost Host = (FileSystemHost)Host0;
            Host.SectorSize = ALLOCATION_UNIT;
            Host.SectorsPerAllocationUnit = 1;
            Host.MaxComponentLength = 255;
            Host.FileInfoTimeout = 1000;
            Host.CaseSensitiveSearch = false;
            Host.CasePreservedNames = true;
            Host.UnicodeOnDisk = true;
            Host.PersistentAcls = true;
            Host.PostCleanupWhenModifiedOnly = true;
            Host.PassQueryDirectoryPattern = true;
            Host.FlushAndPurgeOnCleanup = true;
            Host.VolumeCreationTime = (UInt64)File.GetCreationTimeUtc(_Path).ToFileTimeUtc();
            Host.VolumeSerialNumber = 0;
            return STATUS_SUCCESS;
        }
        public override Int32 GetVolumeInfo(
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
        public override Int32 GetSecurityByName(
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
        public override Int32 Create(
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
            FileDesc FileDesc = null;
            try
            {
                FileName = ConcatPath(FileName);
                if (0 == (CreateOptions & FILE_DIRECTORY_FILE))
                {
                    FileSecurity Security = null;
                    if (null != SecurityDescriptor)
                    {
                        Security = new FileSecurity();
                        Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor);
                    }
                    FileDesc = new FileDesc(
                        new FileStream(
                            FileName,
                            FileMode.CreateNew,
                            (FileSystemRights)GrantedAccess | FileSystemRights.WriteAttributes,
                            FileShare.Read | FileShare.Write | FileShare.Delete,
                            4096,
                            0,
                            Security));
                    FileDesc.SetFileAttributes(FileAttributes | (UInt32)System.IO.FileAttributes.Archive);
                }
                else
                {
                    if (Directory.Exists(FileName))
                        ThrowIoExceptionWithNtStatus(STATUS_OBJECT_NAME_COLLISION);
                    DirectorySecurity Security = null;
                    if (null != SecurityDescriptor)
                    {
                        Security = new DirectorySecurity();
                        Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor);
                    }
                    FileDesc = new FileDesc(
                        Directory.CreateDirectory(FileName, Security));
                    FileDesc.SetFileAttributes(FileAttributes);
                }
                FileNode = default(Object);
                FileDesc0 = FileDesc;
                NormalizedName = default(String);
                return FileDesc.GetFileInfo(out FileInfo);
            }
            catch
            {
                if (null != FileDesc && null != FileDesc.Stream)
                    FileDesc.Stream.Dispose();
                throw;
            }
        }
        public override Int32 Open(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            out Object FileNode,
            out Object FileDesc0,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileDesc FileDesc = null;
            try
            {
                FileName = ConcatPath(FileName);
                if (!Directory.Exists(FileName))
                {
                    FileDesc = new FileDesc(
                        new FileStream(
                            FileName,
                            FileMode.Open,
                            (FileSystemRights)GrantedAccess,
                            FileShare.Read | FileShare.Write | FileShare.Delete,
                            4096,
                            0));
                }
                else
                {
                    FileDesc = new FileDesc(
                        new DirectoryInfo(FileName));
                }
                FileNode = default(Object);
                FileDesc0 = FileDesc;
                NormalizedName = default(String);
                return FileDesc.GetFileInfo(out FileInfo);
            }
            catch
            {
                if (null != FileDesc && null != FileDesc.Stream)
                    FileDesc.Stream.Dispose();
                throw;
            }
        }
        public override Int32 Overwrite(
            Object FileNode,
            Object FileDesc0,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (ReplaceFileAttributes)
                FileDesc.SetFileAttributes(FileAttributes |
                    (UInt32)System.IO.FileAttributes.Archive);
            else if (0 != FileAttributes)
                FileDesc.SetFileAttributes(FileDesc.GetFileAttributes() | FileAttributes |
                    (UInt32)System.IO.FileAttributes.Archive);
            FileDesc.Stream.SetLength(0);
            return FileDesc.GetFileInfo(out FileInfo);
        }
        public override void Cleanup(
            Object FileNode,
            Object FileDesc0,
            String FileName,
            UInt32 Flags)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (0 != (Flags & CleanupDelete))
            {
                FileDesc.SetDisposition(true);
                if (null != FileDesc.Stream)
                    FileDesc.Stream.Dispose();
            }
        }
        public override void Close(
            Object FileNode,
            Object FileDesc0)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null != FileDesc.Stream)
                FileDesc.Stream.Dispose();
        }
        public override Int32 Read(
            Object FileNode,
            Object FileDesc0,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (Offset >= (UInt64)FileDesc.Stream.Length)
                ThrowIoExceptionWithNtStatus(STATUS_END_OF_FILE);
            Byte[] Bytes = new byte[Length];
            FileDesc.Stream.Seek((Int64)Offset, SeekOrigin.Begin);
            PBytesTransferred = (UInt32)FileDesc.Stream.Read(Bytes, 0, Bytes.Length);
            Marshal.Copy(Bytes, 0, Buffer, Bytes.Length);
            return STATUS_SUCCESS;
        }
        public override Int32 Write(
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
            if (ConstrainedIo)
            {
                if (Offset >= (UInt64)FileDesc.Stream.Length)
                {
                    PBytesTransferred = default(UInt32);
                    FileInfo = default(FileInfo);
                    return STATUS_SUCCESS;
                }
                if (Offset + Length > (UInt64)FileDesc.Stream.Length)
                    Length = (UInt32)((UInt64)FileDesc.Stream.Length - Offset);
            }
            Byte[] Bytes = new byte[Length];
            Marshal.Copy(Buffer, Bytes, 0, Bytes.Length);
            if (!WriteToEndOfFile)
                FileDesc.Stream.Seek((Int64)Offset, SeekOrigin.Begin);
            FileDesc.Stream.Write(Bytes, 0, Bytes.Length);
            PBytesTransferred = (UInt32)Bytes.Length;
            return FileDesc.GetFileInfo(out FileInfo);
        }
        public override Int32 Flush(
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
            return FileDesc.GetFileInfo(out FileInfo);
        }
        public override Int32 GetFileInfo(
            Object FileNode,
            Object FileDesc0,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            return FileDesc.GetFileInfo(out FileInfo);
        }
        public override Int32 SetBasicInfo(
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
            FileDesc.SetBasicInfo(FileAttributes, CreationTime, LastAccessTime, LastWriteTime);
            return FileDesc.GetFileInfo(out FileInfo);
        }
        public override Int32 SetFileSize(
            Object FileNode,
            Object FileDesc0,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (!SetAllocationSize || (UInt64)FileDesc.Stream.Length > NewSize)
            {
                /*
                 * "FileInfo.FileSize > NewSize" explanation:
                 * Ptfs does not support allocation size. However if the new AllocationSize
                 * is less than the current FileSize we must truncate the file.
                 */
                FileDesc.Stream.SetLength((Int64)NewSize);
            }
            return FileDesc.GetFileInfo(out FileInfo);
        }
        public override Int32 CanDelete(
            Object FileNode,
            Object FileDesc0,
            String FileName)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            FileDesc.SetDisposition(false);
            return STATUS_SUCCESS;
        }
        public override Int32 Rename(
            Object FileNode,
            Object FileDesc0,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            FileName = ConcatPath(FileName);
            NewFileName = ConcatPath(NewFileName);
            FileDesc.Rename(FileName, NewFileName, ReplaceIfExists);
            return STATUS_SUCCESS;
        }
        public override Int32 GetSecurity(
            Object FileNode,
            Object FileDesc0,
            ref Byte[] SecurityDescriptor)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            SecurityDescriptor = FileDesc.GetSecurityDescriptor();
            return STATUS_SUCCESS;
        }
        public override Int32 SetSecurity(
            Object FileNode,
            Object FileDesc0,
            AccessControlSections Sections,
            Byte[] SecurityDescriptor)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            FileDesc.SetSecurityDescriptor(Sections, SecurityDescriptor);
            return STATUS_SUCCESS;
        }
        public override Boolean ReadDirectoryEntry(
            Object FileNode,
            Object FileDesc0,
            String Pattern,
            String Marker,
            ref Object Context,
            out String FileName,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null == FileDesc.FileSystemInfos)
            {
                if (null != Pattern)
                    Pattern = Pattern.Replace('<', '*').Replace('>', '?').Replace('"', '.');
                else
                    Pattern = "*";
                IEnumerable Enum = FileDesc.DirInfo.EnumerateFileSystemInfos(Pattern);
                SortedList List = new SortedList();
                if (null != FileDesc.DirInfo && null != FileDesc.DirInfo.Parent)
                {
                    List.Add(".", FileDesc.DirInfo);
                    List.Add("..", FileDesc.DirInfo.Parent);
                }
                foreach (FileSystemInfo Info in Enum)
                    List.Add(Info.Name, Info);
                FileDesc.FileSystemInfos = new DictionaryEntry[List.Count];
                List.CopyTo(FileDesc.FileSystemInfos, 0);
            }
            int Index;
            if (null == Context)
            {
                Index = 0;
                if (null != Marker)
                {
                    Index = Array.BinarySearch(FileDesc.FileSystemInfos,
                        new DictionaryEntry(Marker, null),
                        _DirectoryEntryComparer);
                    if (0 <= Index)
                        Index++;
                    else
                        Index = ~Index;
                }
            }
            else
                Index = (int)Context;
            if (FileDesc.FileSystemInfos.Length > Index)
            {
                Context = Index + 1;
                FileName = (String)FileDesc.FileSystemInfos[Index].Key;
                FileDesc.GetFileInfoFromFileSystemInfo(
                    (FileSystemInfo)FileDesc.FileSystemInfos[Index].Value,
                    out FileInfo);
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
            try
            {
                String DebugLogFile = null;
                UInt32 DebugFlags = 0;
                String VolumePrefix = null;
                String PassThrough = null;
                String MountPoint = null;
                IntPtr DebugLogHandle = (IntPtr)(-1);
                FileSystemHost Host = null;
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

                if (null != DebugLogFile)
                    if (0 > FileSystemHost.SetDebugLogFile(DebugLogFile))
                        throw new CommandLineUsageException("cannot open debug log file");

                Host = new FileSystemHost(Ptfs = new Ptfs(PassThrough));
                Host.Prefix = VolumePrefix;
                if (0 > Host.Mount(MountPoint, null, true, DebugFlags))
                    throw new IOException("cannot mount file system");
                MountPoint = Host.MountPoint();
                _Host = Host;

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
                Log(EVENTLOG_ERROR_TYPE, String.Format("{0}", ex.Message));
                throw;
            }
        }
        protected override void OnStop()
        {
            _Host.Unmount();
            _Host = null;
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

        private FileSystemHost _Host;
    }

    class Program
    {
        static void Main(string[] args)
        {
            Environment.ExitCode = new PtfsService().Run();
        }
    }
}
