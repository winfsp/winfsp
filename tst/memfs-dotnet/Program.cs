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
using System.Security.AccessControl;

using Fsp;
using VolumeInfo = Fsp.Interop.VolumeInfo;
using FileInfo = Fsp.Interop.FileInfo;

namespace memfs
{
    class FileNode
    {
        public FileNode(String FileName)
        {
            this.FileName = FileName;
            FileInfo.CreationTime =
            FileInfo.LastAccessTime =
            FileInfo.LastWriteTime =
            FileInfo.ChangeTime = (UInt64)DateTime.Now.ToFileTimeUtc();
            FileInfo.IndexNumber = IndexNumber++;
        }
        public FileInfo GetFileInfo()
        {
            if (null == MainFileNode)
                return this.FileInfo;
            else
            {
                FileInfo FileInfo = MainFileNode.FileInfo;
                FileInfo.FileAttributes &= ~(UInt32)FileAttributes.Directory;
                    /* named streams cannot be directories */
                FileInfo.AllocationSize = this.FileInfo.AllocationSize;
                FileInfo.FileSize = this.FileInfo.FileSize;
                return FileInfo;
            }
        }

        private static UInt64 IndexNumber = 1;
        public String FileName;
        public FileInfo FileInfo;
        public Byte[] FileSecurity;
        public Byte[] FileData;
        public Byte[] ReparseData;
        public FileNode MainFileNode;
        public int OpenCount;
    }

    class FileNodeMap
    {
        public FileNodeMap(Boolean CaseInsensitive)
        {
            this.CaseInsensitive = CaseInsensitive;
            StringComparer Comparer = CaseInsensitive ?
                StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
            Set = new SortedSet<String>(Comparer);
            Map = new Dictionary<String, FileNode>(Comparer);
        }
        public UInt32 Count()
        {
            return (UInt32)Map.Count;
        }
        public FileNode Get(String FileName)
        {
            return Map[FileName];
        }
        public FileNode GetMain(String FileName)
        {
            int Index = FileName.IndexOf(':');
            if (0 > Index)
                return null;
            return Map[FileName.Substring(0, Index)];
        }
        public FileNode GetParent(String FileName, out Int32 Result)
        {
            FileNode FileNode = Map[Path.GetDirectoryName(FileName)];
            if (null == FileNode)
            {
                Result = FileSystemBase.STATUS_OBJECT_PATH_NOT_FOUND;
                return null;
            }
            if (0 == (FileNode.FileInfo.FileAttributes & (UInt32)FileAttributes.Directory))
            {
                Result = FileSystemBase.STATUS_NOT_A_DIRECTORY;
                return null;
            }
            Result = FileSystemBase.STATUS_SUCCESS;
            return FileNode;
        }
        public void TouchParent(FileNode FileNode)
        {
            if ("\\" == FileNode.FileName)
                return;
            Int32 Result;
            FileNode Parent = GetParent(FileNode.FileName, out Result);
            if (null == Parent)
                return;
            Parent.FileInfo.LastAccessTime =
            Parent.FileInfo.LastWriteTime =
            Parent.FileInfo.ChangeTime = (UInt64)DateTime.Now.ToFileTimeUtc();
        }
        public void Insert(FileNode FileNode)
        {
            Set.Add(FileNode.FileName);
            Map.Add(FileNode.FileName, FileNode);
            TouchParent(FileNode);
        }
        public void Remove(FileNode FileNode)
        {
            if (Set.Remove(FileNode.FileName))
            {
                Map.Remove(FileNode.FileName);
                TouchParent(FileNode);
            }
        }
        public Boolean HasChild(FileNode FileNode)
        {
            String MinName = FileNode.FileName + "\\";
            String MaxName = FileNode.FileName + "]";
            SortedSet<String> View = Set.GetViewBetween(MinName, MaxName);
            foreach (String Name in View)
                if (Name.StartsWith(MinName, CaseInsensitive ?
                    StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
                    return true;
            return false;
        }
        public IEnumerable<String> GetChildrenFileNames(FileNode FileNode)
        {
            String MinName = FileNode.FileName + "\\";
            String MaxName = FileNode.FileName + "]";
            SortedSet<String> View = Set.GetViewBetween(MinName, MaxName);
            foreach (String Name in View)
                if (Name.StartsWith(MinName, CaseInsensitive ?
                    StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
                    yield return Name;
        }
        public IEnumerable<String> GetStreamFileNames(FileNode FileNode)
        {
            String MinName = FileNode.FileName + ":";
            String MaxName = FileNode.FileName + ";";
            SortedSet<String> View = Set.GetViewBetween(MinName, MaxName);
            foreach (String Name in View)
                if (Name.StartsWith(MinName, CaseInsensitive ?
                    StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
                    yield return Name;
        }
        public IEnumerable<String> GetDescendantFileNames(FileNode FileNode)
        {
            String MinName = FileNode.FileName;
            String MaxName = FileNode.FileName + "]";
            SortedSet<String> View = Set.GetViewBetween(MinName, MaxName);
            foreach (String Name in View)
                if (Name.StartsWith(MinName, CaseInsensitive ?
                    StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal) &&
                    Name.Length == MinName.Length || (1 == MinName.Length && '\\' == MinName[0]) ||
                    '\\' == Name[MinName.Length] || ':' == Name[MinName.Length])
                    yield return Name;
        }

        public Boolean CaseInsensitive;
        private SortedSet<String> Set;
        private Dictionary<String, FileNode> Map;
    }

    class Memfs : FileSystemBase
    {
        public const UInt16 MEMFS_SECTOR_SIZE = 512;
        public const UInt16 MEMFS_SECTORS_PER_ALLOCATION_UNIT = 1;

        public Memfs(
            Boolean CaseInsensitive, UInt32 MaxFileNodes, UInt32 MaxFileSize, String RootSddl)
        {
            this.FileNodeMap = new FileNodeMap(CaseInsensitive);
            this.OpenNodeSet = new HashSet<FileNode>();
            this.MaxFileNodes = MaxFileNodes;
            this.MaxFileSize = MaxFileSize;

            /*
             * Create root directory.
             */

            FileNode RootNode = new FileNode("\\");
            RootNode.FileInfo.FileAttributes = (UInt32)FileAttributes.Directory;
            if (null == RootSddl)
                RootSddl = "O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)";
            RawSecurityDescriptor RootSecurityDescriptor = new RawSecurityDescriptor(RootSddl);
            RootNode.FileSecurity = new Byte[RootSecurityDescriptor.BinaryLength];
            RootSecurityDescriptor.GetBinaryForm(RootNode.FileSecurity, 0);

            FileNodeMap.Insert(RootNode);
        }

        public override Int32 Init(Object Host0)
        {
            FileSystemHost Host = (FileSystemHost)Host0;
            Host.SectorSize = Memfs.MEMFS_SECTOR_SIZE;
            Host.SectorsPerAllocationUnit = Memfs.MEMFS_SECTORS_PER_ALLOCATION_UNIT;
            Host.VolumeCreationTime = (UInt64)DateTime.Now.ToFileTimeUtc();
            Host.VolumeSerialNumber = (UInt32)(Host.VolumeCreationTime / (10000 * 1000));
            Host.CaseSensitiveSearch = !FileNodeMap.CaseInsensitive;
            Host.CasePreservedNames = true;
            Host.UnicodeOnDisk = true;
            Host.PersistentAcls = true;
            Host.ReparsePoints = true;
            Host.ReparsePointsAccessCheck = false;
            Host.NamedStreams = true;
            Host.PostCleanupWhenModifiedOnly = true;
            return STATUS_SUCCESS;
        }

        public override Int32 GetVolumeInfo(
            out VolumeInfo VolumeInfo)
        {
            VolumeInfo = default(VolumeInfo);
            VolumeInfo.TotalSize = MaxFileNodes * (UInt64)MaxFileSize;
            VolumeInfo.FreeSize = (MaxFileNodes - FileNodeMap.Count()) * (UInt64)MaxFileSize;
            VolumeInfo.SetVolumeLabel(VolumeLabel);
            return STATUS_SUCCESS;
        }

        public override Int32 SetVolumeLabel(
            String VolumeLabel,
            out VolumeInfo VolumeInfo)
        {
            this.VolumeLabel = VolumeLabel;
            return GetVolumeInfo(out VolumeInfo);
        }

        public override Int32 GetSecurityByName(
            String FileName,
            out UInt32 FileAttributes/* or ReparsePointIndex */,
            ref Byte[] SecurityDescriptor)
        {
            FileNode FileNode = FileNodeMap.Get(FileName);
            if (null == FileNode)
            {
                Int32 Result = STATUS_OBJECT_NAME_NOT_FOUND;
                if (FindReparsePoint(FileName, out FileAttributes))
                    Result = STATUS_REPARSE;
                else
                    FileNodeMap.GetParent(FileName, out Result);
                return Result;
            }

            UInt32 FileAttributesMask = ~(UInt32)0;
            if (null != FileNode.MainFileNode)
            {
                FileAttributesMask = ~(UInt32)System.IO.FileAttributes.Directory;
                FileNode = FileNode.MainFileNode;
            }
            FileAttributes = FileNode.FileInfo.FileAttributes & FileAttributesMask;
            if (null != SecurityDescriptor)
                SecurityDescriptor = FileNode.FileSecurity;

            return STATUS_SUCCESS;
        }

        public override Int32 Create(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            Byte[] SecurityDescriptor,
            UInt64 AllocationSize,
            out Object FileNode0,
            out Object FileDesc,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileNode0 = default(Object);
            FileDesc = default(Object);
            FileInfo = default(FileInfo);
            NormalizedName = default(String);

            FileNode FileNode;
            FileNode ParentNode;
            Int32 Result;

            FileNode = FileNodeMap.Get(FileName);
            if (null != FileNode)
                return STATUS_OBJECT_NAME_COLLISION;
            ParentNode = FileNodeMap.GetParent(FileName, out Result);
            if (null == ParentNode)
                return Result;

            if (0 != (CreateOptions & FILE_DIRECTORY_FILE))
                AllocationSize = 0;
            if (FileNodeMap.Count() >= MaxFileNodes)
                return STATUS_CANNOT_MAKE;
            if (AllocationSize > MaxFileSize)
                return STATUS_DISK_FULL;

            FileName = Path.Combine(ParentNode.FileName, Path.GetFileName(FileName));
                /* normalize name */
            FileNode = new FileNode(FileName);
            FileNode.MainFileNode = FileNodeMap.GetMain(FileName);
            FileNode.FileInfo.FileAttributes = 0 != (FileAttributes & (UInt32)System.IO.FileAttributes.Directory) ?
                FileAttributes : FileAttributes | (UInt32)System.IO.FileAttributes.Archive;
            FileNode.FileSecurity = SecurityDescriptor;
            FileNode.FileInfo.AllocationSize = AllocationSize;
            if (0 != AllocationSize)
                FileNode.FileData = new byte[AllocationSize];
            FileNodeMap.Insert(FileNode);

            InsertOpenNode(FileNode);
            FileNode0 = FileNode;
            FileInfo = FileNode.GetFileInfo();
            NormalizedName = FileNode.FileName;

            return STATUS_SUCCESS;
        }

        public override Int32 Open(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            out Object FileNode0,
            out Object FileDesc,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileNode0 = default(Object);
            FileDesc = default(Object);
            FileInfo = default(FileInfo);
            NormalizedName = default(String);

            FileNode FileNode;
            Int32 Result;

            FileNode = FileNodeMap.Get(FileName);
            if (null == FileNode)
            {
                Result = STATUS_OBJECT_NAME_NOT_FOUND;
                FileNodeMap.GetParent(FileName, out Result);
                return Result;
            }

            InsertOpenNode(FileNode);
            FileNode0 = FileNode;
            FileInfo = FileNode.GetFileInfo();
            NormalizedName = FileNode.FileName;

            return STATUS_SUCCESS;
        }

        public override Int32 Overwrite(
            Object FileNode0,
            Object FileDesc,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);

            FileNode FileNode = (FileNode)FileNode0;
            Int32 Result;

            List<String> StreamFileNames = new List<String>(FileNodeMap.GetStreamFileNames(FileNode));
            lock (OpenNodeSet)
            {
                foreach (String StreamFileName in StreamFileNames)
                {
                    FileNode StreamNode = FileNodeMap.Get(StreamFileName);
                    if (null == StreamNode)
                        continue; /* should not happen */
                    if (!OpenNodeSet.Contains(StreamNode))
                        FileNodeMap.Remove(StreamNode);
                }
            }

            Result = SetFileSizeInternal(FileNode, AllocationSize, true);
            if (0 > Result)
                return Result;
            if (ReplaceFileAttributes)
                FileNode.FileInfo.FileAttributes = FileAttributes | (UInt32)System.IO.FileAttributes.Archive;
            else
                FileNode.FileInfo.FileAttributes |= FileAttributes | (UInt32)System.IO.FileAttributes.Archive;
            FileNode.FileInfo.FileSize = 0;
            FileNode.FileInfo.LastAccessTime =
            FileNode.FileInfo.LastWriteTime =
            FileNode.FileInfo.ChangeTime = (UInt64)DateTime.Now.ToFileTimeUtc();

            FileInfo = FileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override void Cleanup(
            Object FileNode0,
            Object FileDesc,
            String FileName,
            UInt32 Flags)
        {
            FileNode FileNode = (FileNode)FileNode0;
            FileNode MainFileNode = null != FileNode.MainFileNode ?
                FileNode.MainFileNode : FileNode;

            if (0 != (Flags & CleanupSetArchiveBit))
            {
                if (0 == (MainFileNode.FileInfo.FileAttributes & (UInt32)FileAttributes.Directory))
                    MainFileNode.FileInfo.FileAttributes |= (UInt32)FileAttributes.Archive;
            }

            if (0 != (Flags & (CleanupSetLastAccessTime | CleanupSetLastWriteTime | CleanupSetChangeTime)))
            {
                UInt64 SystemTime = (UInt64)DateTime.Now.ToFileTimeUtc();

                if (0 != (Flags & CleanupSetLastAccessTime))
                    MainFileNode.FileInfo.LastAccessTime = SystemTime;
                if (0 != (Flags & CleanupSetLastWriteTime))
                    MainFileNode.FileInfo.LastWriteTime = SystemTime;
                if (0 != (Flags & CleanupSetChangeTime))
                    MainFileNode.FileInfo.ChangeTime = SystemTime;
            }

            if (0 != (Flags & CleanupSetAllocationSize))
            {
                UInt64 AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
                UInt64 AllocationSize = (FileNode.FileInfo.FileSize + AllocationUnit - 1) /
                    AllocationUnit * AllocationUnit;
                SetFileSizeInternal(FileNode, AllocationSize, true);
            }

            if (0 != (Flags & CleanupDelete) && !FileNodeMap.HasChild(FileNode))
            {
                List<String> StreamFileNames = new List<String>(FileNodeMap.GetStreamFileNames(FileNode));
                foreach (String StreamFileName in StreamFileNames)
                {
                    FileNode StreamNode = FileNodeMap.Get(StreamFileName);
                    if (null == StreamNode)
                        continue; /* should not happen */
                    FileNodeMap.Remove(StreamNode);
                }
                FileNodeMap.Remove(FileNode);
            }
        }

        public override void Close(
            Object FileNode0,
            Object FileDesc)
        {
            FileNode FileNode = (FileNode)FileNode0;
            RemoveOpenNode(FileNode);
        }

        public override Int32 Read(
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
        public override Int32 Write(
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
        public override Int32 Flush(
            Object FileNode,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 GetFileInfo(
            Object FileNode,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 SetBasicInfo(
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
        public override Int32 SetFileSize(
            Object FileNode,
            Object FileDesc,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        private Int32 SetFileSizeInternal(
            FileNode FileNode,
            UInt64 NewSize,
            Boolean SetAllocationSize)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 CanDelete(
            Object FileNode,
            Object FileDesc,
            String FileName)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 Rename(
            Object FileNode,
            Object FileDesc,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 GetSecurity(
            Object FileNode,
            Object FileDesc,
            ref Byte[] SecurityDescriptor)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 SetSecurity(
            Object FileNode,
            Object FileDesc,
            AccessControlSections Sections,
            Byte[] SecurityDescriptor)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Boolean ReadDirectoryEntry(
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
        public override Int32 GetReparsePointByName(
            String FileName,
            Boolean IsDirectory,
            IntPtr Buffer,
            ref UIntPtr Size)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 GetReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            IntPtr Buffer,
            out UIntPtr Size)
        {
            Size = default(UIntPtr);
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 SetReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 DeleteReparsePoint(
            Object FileNode,
            Object FileDesc,
            String FileName,
            IntPtr Buffer,
            UIntPtr Size)
        {
            return STATUS_INVALID_DEVICE_REQUEST;
        }
        public override Int32 GetStreamInfo(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            BytesTransferred = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }

        private void InsertOpenNode(FileNode FileNode)
        {
            lock (OpenNodeSet)
            {
                if (1 == ++FileNode.OpenCount)
                    OpenNodeSet.Add(FileNode);
            }
        }
        private void RemoveOpenNode(FileNode FileNode)
        {
            lock (OpenNodeSet)
            {
                if (0 == --FileNode.OpenCount)
                    OpenNodeSet.Remove(FileNode);
            }
        }

        private FileNodeMap FileNodeMap;
        private HashSet<FileNode> OpenNodeSet;
        private UInt32 MaxFileNodes;
        private UInt32 MaxFileSize;
        private String VolumeLabel;
    }

    class MemfsService : Service
    {
        private class CommandLineUsageException : Exception
        {
            public CommandLineUsageException(String Message = null) : base(Message)
            {
                HasMessage = null != Message;
            }

            public bool HasMessage;
        }

        private const String PROGNAME = "memfs-dotnet";

        public MemfsService() : base("MemfsService")
        {
        }
        protected override void OnStart(String[] Args)
        {
            try
            {
                Boolean CaseInsensitive = false;
                String DebugLogFile = null;
                UInt32 DebugFlags = 0;
                UInt32 FileInfoTimeout = unchecked((UInt32)(-1));
                UInt32 MaxFileNodes = 1024;
                UInt32 MaxFileSize = 16 * 1024 * 1024;
                String FileSystemName = null;
                String VolumePrefix = null;
                String MountPoint = null;
                String RootSddl = null;
                FileSystemHost Host = null;
                Memfs Memfs = null;
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
                    case 'D':
                        argtos(Args, ref I, ref DebugLogFile);
                        break;
                    case 'd':
                        argtol(Args, ref I, ref DebugFlags);
                        break;
                    case 'F':
                        argtos(Args, ref I, ref FileSystemName);
                        break;
                    case 'i':
                        CaseInsensitive = true;
                        break;
                    case 'm':
                        argtos(Args, ref I, ref MountPoint);
                        break;
                    case 'n':
                        argtol(Args, ref I, ref MaxFileNodes);
                        break;
                    case 'S':
                        argtos(Args, ref I, ref RootSddl);
                        break;
                    case 's':
                        argtol(Args, ref I, ref MaxFileSize);
                        break;
                    case 't':
                        argtol(Args, ref I, ref FileInfoTimeout);
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

                if ((null == VolumePrefix || 0 == VolumePrefix.Length) && null == MountPoint)
                    throw new CommandLineUsageException();

                if (null != DebugLogFile)
                    if (0 > FileSystemHost.SetDebugLogFile(DebugLogFile))
                        throw new CommandLineUsageException("cannot open debug log file");

                Host = new FileSystemHost(Memfs = new Memfs(
                    CaseInsensitive, MaxFileNodes, MaxFileSize, RootSddl));
                Host.FileInfoTimeout = FileInfoTimeout;
                Host.Prefix = VolumePrefix;
                Host.FileSystemName = null != FileSystemName ? FileSystemName : "-MEMFS";
                if (0 > Host.Mount(MountPoint, null, false, DebugFlags))
                    throw new IOException("cannot mount file system");
                MountPoint = Host.MountPoint();
                _Host = Host;

                Log(EVENTLOG_INFORMATION_TYPE, String.Format("{0} -t {1} -n {2} -s {3} {4}{5}{6}{7}{8}{9}",
                    PROGNAME, FileInfoTimeout, MaxFileNodes, MaxFileSize,
                    null != RootSddl ? " -S " : "", null != RootSddl ? RootSddl : "",
                    null != VolumePrefix && 0 < VolumePrefix.Length ? " -u " : "",
                        null != VolumePrefix && 0 < VolumePrefix.Length ? VolumePrefix : "",
                    null != MountPoint ? " -m " : "", null != MountPoint ? MountPoint : ""));
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
                    "    -i                  [case insensitive file system]\n" +
                    "    -t FileInfoTimeout  [millis]\n" +
                    "    -n MaxFileNodes\n" +
                    "    -s MaxFileSize      [bytes]\n" +
                    "    -F FileSystemName\n" +
                    "    -S RootSddl         [file rights: FA, etc; NO generic rights: GA, etc.]\n" +
                    "    -u \\Server\\Share    [UNC prefix (single backslash)]\n" +
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
            Environment.ExitCode = new MemfsService().Run();
        }
    }
}

#if false
using System.Collections;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;

namespace memfs
{
    class Memfs : FileSystemBase
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

        public Memfs(String Path0)
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
                }
                FileDesc.SetFileAttributes(FileAttributes);
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
                FileDesc.SetFileAttributes(FileAttributes);
            else if (0 != FileAttributes)
                FileDesc.SetFileAttributes(FileDesc.GetFileAttributes() | FileAttributes);
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
                IEnumerable Enum = FileDesc.DirInfo.EnumerateFileSystemInfos(
                    null != Pattern ? Pattern : "*");
                SortedList List = new SortedList();
                List.Add(".", FileDesc.DirInfo);
                List.Add("..", FileDesc.DirInfo.Parent);
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
#endif
