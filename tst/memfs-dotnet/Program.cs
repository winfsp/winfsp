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

#define MEMFS_SLOWIO

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Threading;
#if MEMFS_SLOWIO
using System.Threading.Tasks;
#endif

using Fsp;
using VolumeInfo = Fsp.Interop.VolumeInfo;
using FileInfo = Fsp.Interop.FileInfo;

namespace memfs
{
    class Path
    {
        public static String GetDirectoryName(String Path)
        {
            int Index = Path.LastIndexOf('\\');
            if (0 > Index)
                return Path;
            else if (0 == Index)
                return "\\";
            else
                return Path.Substring(0, Index);
        }

        public static String GetFileName(String Path)
        {
            int Index = Path.LastIndexOf('\\');
            if (0 > Index)
                return Path;
            else
                return Path.Substring(Index + 1);
        }
    }

    struct EaValueData
    {
        public Byte[] EaValue;
        public Boolean NeedEa;
    }

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
        public SortedDictionary<String, EaValueData> GetEaMap(Boolean Force)
        {
            FileNode FileNode = null == MainFileNode ? this : MainFileNode;
            if (null == EaMap && Force)
                EaMap = new SortedDictionary<String, EaValueData>(StringComparer.OrdinalIgnoreCase);
            return EaMap;
        }

        private static UInt64 IndexNumber = 1;
        public String FileName;
        public FileInfo FileInfo;
        public Byte[] FileSecurity;
        public Byte[] FileData;
        public Byte[] ReparseData;
        private SortedDictionary<String, EaValueData> EaMap;
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
            FileNode FileNode;
            return Map.TryGetValue(FileName, out FileNode) ? FileNode : null;
        }
        public FileNode GetMain(String FileName)
        {
            int Index = FileName.IndexOf(':');
            if (0 > Index)
                return null;
            FileNode FileNode;
            return Map.TryGetValue(FileName.Substring(0, Index), out FileNode) ? FileNode : null;
        }
        public FileNode GetParent(String FileName, ref Int32 Result)
        {
            FileNode FileNode;
            Map.TryGetValue(Path.GetDirectoryName(FileName), out FileNode);
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
            return FileNode;
        }
        public void TouchParent(FileNode FileNode)
        {
            if ("\\" == FileNode.FileName)
                return;
            Int32 Result = FileSystemBase.STATUS_SUCCESS;
            FileNode Parent = GetParent(FileNode.FileName, ref Result);
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
            foreach (String Name in GetChildrenFileNames(FileNode, null))
                return true;
            return false;
        }
        public IEnumerable<String> GetChildrenFileNames(FileNode FileNode, String Marker)
        {
            String MinName = "\\";
            String MaxName = "]";
            if ("\\" != FileNode.FileName)
            {
                MinName = FileNode.FileName + "\\";
                MaxName = FileNode.FileName + "]";
            }
            if (null != Marker)
                MinName += Marker;
            foreach (String Name in Set.GetViewBetween(MinName, MaxName))
                if (Name != MinName &&
                    Name.Length > MaxName.Length && -1 == Name.IndexOfAny(Delimiters, MaxName.Length))
                    yield return Name;
        }
        public IEnumerable<String> GetStreamFileNames(FileNode FileNode)
        {
            String MinName = FileNode.FileName + ":";
            String MaxName = FileNode.FileName + ";";
            foreach (String Name in Set.GetViewBetween(MinName, MaxName))
                if (Name.Length > MinName.Length)
                    yield return Name;
        }
        public IEnumerable<String> GetDescendantFileNames(FileNode FileNode)
        {
            yield return FileNode.FileName;
            String MinName = FileNode.FileName + ":";
            String MaxName = FileNode.FileName + ";";
            foreach (String Name in Set.GetViewBetween(MinName, MaxName))
                if (Name.Length > MinName.Length)
                    yield return Name;
            MinName = "\\";
            MaxName = "]";
            if ("\\" != FileNode.FileName)
            {
                MinName = FileNode.FileName + "\\";
                MaxName = FileNode.FileName + "]";
            }
            foreach (String Name in Set.GetViewBetween(MinName, MaxName))
                if (Name.Length > MinName.Length)
                    yield return Name;
        }

        private static readonly Char[] Delimiters = new Char[] { '\\', ':' };
        public Boolean CaseInsensitive;
        private SortedSet<String> Set;
        private Dictionary<String, FileNode> Map;
    }

    class Memfs : FileSystemBase
    {
        private FileSystemHost Host;
        public const UInt16 MEMFS_SECTOR_SIZE = 512;
        public const UInt16 MEMFS_SECTORS_PER_ALLOCATION_UNIT = 1;

        public Memfs(
            Boolean CaseInsensitive, UInt32 MaxFileNodes, UInt32 MaxFileSize, String RootSddl,
            UInt64 SlowioMaxDelay, UInt64 SlowioPercentDelay, UInt64 SlowioRarefyDelay)
        {
            this.FileNodeMap = new FileNodeMap(CaseInsensitive);
            this.MaxFileNodes = MaxFileNodes;
            this.MaxFileSize = MaxFileSize;
            this.SlowioMaxDelay = SlowioMaxDelay;
            this.SlowioPercentDelay = SlowioPercentDelay;
            this.SlowioRarefyDelay = SlowioRarefyDelay;

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
            Host = (FileSystemHost)Host0;
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
            Host.PassQueryDirectoryFileName = true;
            Host.ExtendedAttributes = true;
            Host.WslFeatures = true;
            Host.RejectIrpPriorToTransact0 = true;
            Host.SupportsPosixUnlinkRename = true;
            return STATUS_SUCCESS;
        }

#if MEMFS_SLOWIO
        public override int Mounted(object Host)
        {
            SlowioTasksRunning = 0;
            return STATUS_SUCCESS;
        }

        public override void Unmounted(object Host)
        {
            while (SlowioTasksRunning != 0)
                Thread.Sleep(1000);
        }
#endif

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
                    FileNodeMap.GetParent(FileName, ref Result);
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

        public override Int32 CreateEx(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            Byte[] SecurityDescriptor,
            UInt64 AllocationSize,
            IntPtr ExtraBuffer,
            UInt32 ExtraLength,
            Boolean ExtraBufferIsReparsePoint,
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
            Int32 Result = STATUS_SUCCESS;

            FileNode = FileNodeMap.Get(FileName);
            if (null != FileNode)
                return STATUS_OBJECT_NAME_COLLISION;
            ParentNode = FileNodeMap.GetParent(FileName, ref Result);
            if (null == ParentNode)
                return Result;

            if (0 != (CreateOptions & FILE_DIRECTORY_FILE))
                AllocationSize = 0;
            if (FileNodeMap.Count() >= MaxFileNodes)
                return STATUS_CANNOT_MAKE;
            if (AllocationSize > MaxFileSize)
                return STATUS_DISK_FULL;

            if ("\\" != ParentNode.FileName)
                /* normalize name */
                FileName = ParentNode.FileName + "\\" + Path.GetFileName(FileName);
            FileNode = new FileNode(FileName);
            FileNode.MainFileNode = FileNodeMap.GetMain(FileName);
            FileNode.FileInfo.FileAttributes = 0 != (FileAttributes & (UInt32)System.IO.FileAttributes.Directory) ?
                FileAttributes : FileAttributes | (UInt32)System.IO.FileAttributes.Archive;
            FileNode.FileSecurity = SecurityDescriptor;
            if (IntPtr.Zero != ExtraBuffer)
            {
                if (!ExtraBufferIsReparsePoint)
                {
                    Result = SetEaEntries(FileNode, null, ExtraBuffer, ExtraLength);
                    if (0 > Result)
                        return Result;
                }
                else
                {
                    Byte[] ReparseData = MakeReparsePoint(ExtraBuffer, ExtraLength);
                    FileNode.FileInfo.FileAttributes |= (UInt32)System.IO.FileAttributes.ReparsePoint;
                    FileNode.FileInfo.ReparseTag = GetReparseTag(ReparseData);
                    FileNode.ReparseData = ReparseData;
                }
            }
            if (0 != AllocationSize)
            {
                Result = SetFileSizeInternal(FileNode, AllocationSize, true);
                if (0 > Result)
                    return Result;
            }
            FileNodeMap.Insert(FileNode);

            Interlocked.Increment(ref FileNode.OpenCount);
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
                FileNodeMap.GetParent(FileName, ref Result);
                return Result;
            }

            if (0 != (CreateOptions & FILE_NO_EA_KNOWLEDGE) &&
                null == FileNode.MainFileNode)
            {
                SortedDictionary<String, EaValueData> EaMap = FileNode.GetEaMap(false);
                if (null != EaMap)
                {
                    foreach (KeyValuePair<String, EaValueData> Pair in EaMap)
                        if (Pair.Value.NeedEa)
                            return STATUS_ACCESS_DENIED;
                }
            }

            Interlocked.Increment(ref FileNode.OpenCount);
            FileNode0 = FileNode;
            FileInfo = FileNode.GetFileInfo();
            NormalizedName = FileNode.FileName;

            return STATUS_SUCCESS;
        }

        public override Int32 OverwriteEx(
            Object FileNode0,
            Object FileDesc,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            IntPtr Ea,
            UInt32 EaLength,
            out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);

            FileNode FileNode = (FileNode)FileNode0;
            Int32 Result;

            List<String> StreamFileNames = new List<String>(FileNodeMap.GetStreamFileNames(FileNode));
            foreach (String StreamFileName in StreamFileNames)
            {
                FileNode StreamNode = FileNodeMap.Get(StreamFileName);
                if (null == StreamNode)
                    continue; /* should not happen */
                if (0 == StreamNode.OpenCount)
                    FileNodeMap.Remove(StreamNode);
            }

            SortedDictionary<String, EaValueData> EaMap = FileNode.GetEaMap(false);
            if (null != EaMap)
            {
                EaMap.Clear();
                FileNode.FileInfo.EaSize = 0;
            }
            if (IntPtr.Zero != Ea)
            {
                Result = SetEaEntries(FileNode, null, Ea, EaLength);
                if (0 > Result)
                    return Result;
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
            Interlocked.Decrement(ref FileNode.OpenCount);
        }

#if MEMFS_SLOWIO
        private UInt64 Hash(UInt64 X)
        {
            X = (X ^ (X >> 30)) * 0xbf58476d1ce4e5b9ul;
            X = (X ^ (X >> 27)) * 0x94d049bb133111ebul;
            X = X ^ (X >> 31);
            return X;
        }

        private static int Spin = 0;

        private UInt64 PseudoRandom(UInt64 To)
        {
            /* John Oberschelp's PRNG */
            Interlocked.Increment(ref Spin);
            return Hash((UInt64)Spin) % To;
        }

        private bool SlowioReturnPending()
        {
            if (0 == SlowioMaxDelay)
            {
                return false;
            }
            return PseudoRandom(100) < SlowioPercentDelay;
        }

        private void SlowioSnooze()
        {
            double Millis = PseudoRandom(SlowioMaxDelay + 1) >> (int) PseudoRandom(SlowioRarefyDelay + 1);
            Thread.Sleep(TimeSpan.FromMilliseconds(Millis));
        }

        private void SlowioReadTask(
            Object FileNode0,
            IntPtr Buffer,
            UInt64 Offset,
            UInt64 EndOffset,
            UInt64 RequestHint)
        {
            SlowioSnooze();

            UInt32 BytesTransferred = (UInt32)(EndOffset - Offset);
            FileNode FileNode = (FileNode)FileNode0;
            Marshal.Copy(FileNode.FileData, (int)Offset, Buffer, (int)BytesTransferred);

            Host.SendReadResponse(RequestHint, STATUS_SUCCESS, BytesTransferred);
            Interlocked.Decrement(ref SlowioTasksRunning);
        }

        private void SlowioWriteTask(
            Object FileNode0,
            IntPtr Buffer,
            UInt64 Offset,
            UInt64 EndOffset,
            UInt64 RequestHint)
        {
            SlowioSnooze();

            UInt32 BytesTransferred = (UInt32)(EndOffset - Offset);
            FileNode FileNode = (FileNode)FileNode0;
            FileInfo FileInfo = FileNode.GetFileInfo();
            Marshal.Copy(Buffer, FileNode.FileData, (int)Offset, (int)BytesTransferred);

            Host.SendWriteResponse(RequestHint, STATUS_SUCCESS, BytesTransferred, ref FileInfo);
            Interlocked.Decrement(ref SlowioTasksRunning);
        }

        private void SlowioReadDirectoryTask(
            Object FileNode0,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            UInt64 RequestHint)
        {
            SlowioSnooze();

            UInt32 BytesTransferred;
            var Status = SeekableReadDirectory(FileNode0, FileDesc, Pattern, Marker, Buffer, Length, out BytesTransferred);

            Host.SendReadDirectoryResponse(RequestHint, Status, BytesTransferred);
            Interlocked.Decrement(ref SlowioTasksRunning);
        }
#endif

        public override Int32 Read(
            Object FileNode0,
            Object FileDesc,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            FileNode FileNode = (FileNode)FileNode0;
            UInt64 EndOffset;

            if (Offset >= FileNode.FileInfo.FileSize)
            {
                BytesTransferred = default(UInt32);
                return STATUS_END_OF_FILE;
            }

            EndOffset = Offset + Length;
            if (EndOffset > FileNode.FileInfo.FileSize)
                EndOffset = FileNode.FileInfo.FileSize;

#if MEMFS_SLOWIO
            if (SlowioReturnPending())
            {
                var Hint = Host.GetOperationRequestHint();
                try
                {
                    Interlocked.Increment(ref SlowioTasksRunning);
                    Task.Run(() => SlowioReadTask(FileNode0, Buffer, Offset, EndOffset, Hint)).ConfigureAwait(false);

                    BytesTransferred = 0;
                    return STATUS_PENDING;
                }
                catch (Exception)
                {
                    Interlocked.Decrement(ref SlowioTasksRunning);
                }
            }
#endif

            BytesTransferred = (UInt32)(EndOffset - Offset);
            Marshal.Copy(FileNode.FileData, (int)Offset, Buffer, (int)BytesTransferred);

            return STATUS_SUCCESS;
        }

        public override Int32 Write(
            Object FileNode0,
            Object FileDesc,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 BytesTransferred,
            out FileInfo FileInfo)
        {
            FileNode FileNode = (FileNode)FileNode0;
            UInt64 EndOffset;

            if (ConstrainedIo)
            {
                if (Offset >= FileNode.FileInfo.FileSize)
                {
                    BytesTransferred = default(UInt32);
                    FileInfo = default(FileInfo);
                    return STATUS_SUCCESS;
                }
                EndOffset = Offset + Length;
                if (EndOffset > FileNode.FileInfo.FileSize)
                    EndOffset = FileNode.FileInfo.FileSize;
            }
            else
            {
                if (WriteToEndOfFile)
                    Offset = FileNode.FileInfo.FileSize;
                EndOffset = Offset + Length;
                if (EndOffset > FileNode.FileInfo.FileSize)
                {
                    Int32 Result = SetFileSizeInternal(FileNode, EndOffset, false);
                    if (0 > Result)
                    {
                        BytesTransferred = default(UInt32);
                        FileInfo = default(FileInfo);
                        return Result;
                    }
                }
            }

#if MEMFS_SLOWIO
            if (SlowioReturnPending())
            {
                var hint = Host.GetOperationRequestHint();
                try
                {
                    Interlocked.Increment(ref SlowioTasksRunning);
                    Task.Run(() => SlowioWriteTask(FileNode0, Buffer, Offset, EndOffset, hint)).ConfigureAwait(false);

                    BytesTransferred = 0;
                    FileInfo = default(FileInfo);
                    return STATUS_PENDING;
                }
                catch (Exception)
                {
                    Interlocked.Decrement(ref SlowioTasksRunning);
                }
            }
#endif

            BytesTransferred = (UInt32)(EndOffset - Offset);
            Marshal.Copy(Buffer, FileNode.FileData, (int)Offset, (int)BytesTransferred);

            FileInfo = FileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override Int32 Flush(
            Object FileNode0,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileNode FileNode = (FileNode)FileNode0;

            /*  nothing to flush, since we do not cache anything */
            FileInfo = null != FileNode ? FileNode.GetFileInfo() : default(FileInfo);

            return STATUS_SUCCESS;
        }

        public override Int32 GetFileInfo(
            Object FileNode0,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            FileNode FileNode = (FileNode)FileNode0;

            FileInfo = FileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override Int32 SetBasicInfo(
            Object FileNode0,
            Object FileDesc,
            UInt32 FileAttributes,
            UInt64 CreationTime,
            UInt64 LastAccessTime,
            UInt64 LastWriteTime,
            UInt64 ChangeTime,
            out FileInfo FileInfo)
        {
            FileNode FileNode = (FileNode)FileNode0;

            if (null != FileNode.MainFileNode)
                FileNode = FileNode.MainFileNode;

            if (unchecked((UInt32)(-1)) != FileAttributes)
                FileNode.FileInfo.FileAttributes = FileAttributes;
            if (0 != CreationTime)
                FileNode.FileInfo.CreationTime = CreationTime;
            if (0 != LastAccessTime)
                FileNode.FileInfo.LastAccessTime = LastAccessTime;
            if (0 != LastWriteTime)
                FileNode.FileInfo.LastWriteTime = LastWriteTime;
            if (0 != ChangeTime)
                FileNode.FileInfo.ChangeTime = ChangeTime;

            FileInfo = FileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override Int32 SetFileSize(
            Object FileNode0,
            Object FileDesc,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileNode FileNode = (FileNode)FileNode0;
            Int32 Result;

            Result = SetFileSizeInternal(FileNode, NewSize, SetAllocationSize);
            FileInfo = 0 <= Result ? FileNode.GetFileInfo() : default(FileInfo);

            return STATUS_SUCCESS;
        }

        private Int32 SetFileSizeInternal(
            FileNode FileNode,
            UInt64 NewSize,
            Boolean SetAllocationSize)
        {
            if (SetAllocationSize)
            {
                if (FileNode.FileInfo.AllocationSize != NewSize)
                {
                    if (NewSize > MaxFileSize)
                        return STATUS_DISK_FULL;

                    byte[] FileData = null;
                    if (0 != NewSize)
                        try
                        {
                            FileData = new byte[NewSize];
                        }
                        catch
                        {
                            return STATUS_INSUFFICIENT_RESOURCES;
                        }
                    int CopyLength = (int)Math.Min(FileNode.FileInfo.AllocationSize, NewSize);
                    if (0 != CopyLength)
                        Array.Copy(FileNode.FileData, FileData, CopyLength);

                    FileNode.FileData = FileData;
                    FileNode.FileInfo.AllocationSize = NewSize;
                    if (FileNode.FileInfo.FileSize > NewSize)
                        FileNode.FileInfo.FileSize = NewSize;
                }
            }
            else
            {
                if (FileNode.FileInfo.FileSize != NewSize)
                {
                    if (FileNode.FileInfo.AllocationSize < NewSize)
                    {
                        UInt64 AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
                        UInt64 AllocationSize = (NewSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
                        Int32 Result = SetFileSizeInternal(FileNode, AllocationSize, true);
                        if (0 > Result)
                            return Result;
                    }

                    if (FileNode.FileInfo.FileSize < NewSize)
                    {
                        int CopyLength = (int)(NewSize - FileNode.FileInfo.FileSize);
                        if (0 != CopyLength)
                            Array.Clear(FileNode.FileData, (int)FileNode.FileInfo.FileSize, CopyLength);
                    }
                    FileNode.FileInfo.FileSize = NewSize;
                }
            }

            return STATUS_SUCCESS;
        }

        public override Int32 CanDelete(
            Object FileNode0,
            Object FileDesc,
            String FileName)
        {
            FileNode FileNode = (FileNode)FileNode0;

            if (FileNodeMap.HasChild(FileNode))
                return STATUS_DIRECTORY_NOT_EMPTY;

            return STATUS_SUCCESS;
        }

        public override Int32 Rename(
            Object FileNode0,
            Object FileDesc,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            FileNode FileNode = (FileNode)FileNode0;
            FileNode NewFileNode;

            NewFileNode = FileNodeMap.Get(NewFileName);
            if (null != NewFileNode && FileNode != NewFileNode)
            {
                if (!ReplaceIfExists)
                    return STATUS_OBJECT_NAME_COLLISION;
                if (0 != (NewFileNode.FileInfo.FileAttributes & (UInt32)FileAttributes.Directory))
                    return STATUS_ACCESS_DENIED;
            }

            if (null != NewFileNode && FileNode != NewFileNode)
                FileNodeMap.Remove(NewFileNode);

            List<String> DescendantFileNames = new List<String>(FileNodeMap.GetDescendantFileNames(FileNode));
            foreach (String DescendantFileName in DescendantFileNames)
            {
                FileNode DescendantFileNode = FileNodeMap.Get(DescendantFileName);
                if (null == DescendantFileNode)
                    continue; /* should not happen */
                FileNodeMap.Remove(DescendantFileNode);
                DescendantFileNode.FileName =
                    NewFileName + DescendantFileNode.FileName.Substring(FileName.Length);
                FileNodeMap.Insert(DescendantFileNode);
            }

            return STATUS_SUCCESS;
        }

        public override Int32 GetSecurity(
            Object FileNode0,
            Object FileDesc,
            ref Byte[] SecurityDescriptor)
        {
            FileNode FileNode = (FileNode)FileNode0;

            if (null != FileNode.MainFileNode)
                FileNode = FileNode.MainFileNode;

            SecurityDescriptor = FileNode.FileSecurity;

            return STATUS_SUCCESS;
        }

        public override Int32 SetSecurity(
            Object FileNode0,
            Object FileDesc,
            AccessControlSections Sections,
            Byte[] SecurityDescriptor)
        {
            FileNode FileNode = (FileNode)FileNode0;

            if (null != FileNode.MainFileNode)
                FileNode = FileNode.MainFileNode;

            return ModifySecurityDescriptorEx(FileNode.FileSecurity, Sections, SecurityDescriptor,
                ref FileNode.FileSecurity);
        }

        public override Boolean ReadDirectoryEntry(
            Object FileNode0,
            Object FileDesc,
            String Pattern,
            String Marker,
            ref Object Context,
            out String FileName,
            out FileInfo FileInfo)
        {
            FileNode FileNode = (FileNode)FileNode0;
            IEnumerator<String> Enumerator = (IEnumerator<String>)Context;

            if (null == Enumerator)
            {
                List<String> ChildrenFileNames = new List<String>();
                if ("\\" != FileNode.FileName)
                {
                    /* if this is not the root directory add the dot entries */
                    if (null == Marker)
                        ChildrenFileNames.Add(".");
                    if (null == Marker || "." == Marker)
                        ChildrenFileNames.Add("..");
                }
                ChildrenFileNames.AddRange(FileNodeMap.GetChildrenFileNames(FileNode,
                    "." != Marker && ".." != Marker ? Marker : null));
                Context = Enumerator = ChildrenFileNames.GetEnumerator();
            }

            while (Enumerator.MoveNext())
            {
                String FullFileName = Enumerator.Current;
                if ("." == FullFileName)
                {
                    FileName = ".";
                    FileInfo = FileNode.GetFileInfo();
                    return true;
                }
                else if (".." == FullFileName)
                {
                    Int32 Result = STATUS_SUCCESS;
                    FileNode ParentNode = FileNodeMap.GetParent(FileNode.FileName, ref Result);
                    if (null != ParentNode)
                    {
                        FileName = "..";
                        FileInfo = ParentNode.GetFileInfo();
                        return true;
                    }
                }
                else
                {
                    FileNode ChildFileNode = FileNodeMap.Get(FullFileName);
                    if (null != ChildFileNode)
                    {
                        FileName = Path.GetFileName(FullFileName);
                        FileInfo = ChildFileNode.GetFileInfo();
                        return true;
                    }
                }
            }

            FileName = default(String);
            FileInfo = default(FileInfo);
            return false;
        }

#if MEMFS_SLOWIO
        public override int ReadDirectory(
            Object FileNode0,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            if (SlowioReturnPending())
            {
                var Hint = Host.GetOperationRequestHint();
                try
                {
                    Interlocked.Increment(ref SlowioTasksRunning);
                    Task.Run(() => SlowioReadDirectoryTask(FileNode0, FileDesc, Pattern, Marker, Buffer, Length, Hint));
                    BytesTransferred = 0;

                    return STATUS_PENDING;
                }
                catch (Exception)
                {
                    Interlocked.Decrement(ref SlowioTasksRunning);
                }
            }

            return SeekableReadDirectory(FileNode0, FileDesc, Pattern, Marker, Buffer, Length, out BytesTransferred);
        }
#endif

        public override int GetDirInfoByName(
            Object ParentNode0,
            Object FileDesc,
            String FileName,
            out String NormalizedName,
            out FileInfo FileInfo)
        {
            FileNode ParentNode = (FileNode)ParentNode0;
            FileNode FileNode;

            FileName =
                ParentNode.FileName +
                ("\\" == ParentNode.FileName ? "" : "\\") +
                Path.GetFileName(FileName);

            FileNode = FileNodeMap.Get(FileName);
            if (null == FileNode)
            {
                NormalizedName = default(String);
                FileInfo = default(FileInfo);
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            NormalizedName = Path.GetFileName(FileNode.FileName);
            FileInfo = FileNode.FileInfo;

            return STATUS_SUCCESS;
        }

        public override Int32 GetReparsePointByName(
            String FileName,
            Boolean IsDirectory,
            ref Byte[] ReparseData)
        {
            FileNode FileNode;

            FileNode = FileNodeMap.Get(FileName);
            if (null == FileNode)
                return STATUS_OBJECT_NAME_NOT_FOUND;

            if (0 == (FileNode.FileInfo.FileAttributes & (UInt32)FileAttributes.ReparsePoint))
                return STATUS_NOT_A_REPARSE_POINT;

            ReparseData = FileNode.ReparseData;

            return STATUS_SUCCESS;
        }

        public override Int32 GetReparsePoint(
            Object FileNode0,
            Object FileDesc,
            String FileName,
            ref Byte[] ReparseData)
        {
            FileNode FileNode = (FileNode)FileNode0;

            if (null != FileNode.MainFileNode)
                FileNode = FileNode.MainFileNode;

            if (0 == (FileNode.FileInfo.FileAttributes & (UInt32)FileAttributes.ReparsePoint))
                return STATUS_NOT_A_REPARSE_POINT;

            ReparseData = FileNode.ReparseData;

            return STATUS_SUCCESS;
        }

        public override Int32 SetReparsePoint(
            Object FileNode0,
            Object FileDesc,
            String FileName,
            Byte[] ReparseData)
        {
            FileNode FileNode = (FileNode)FileNode0;

            if (null != FileNode.MainFileNode)
                FileNode = FileNode.MainFileNode;

            if (FileNodeMap.HasChild(FileNode))
                return STATUS_DIRECTORY_NOT_EMPTY;

            if (null != FileNode.ReparseData)
            {
                Int32 Result = CanReplaceReparsePoint(FileNode.ReparseData, ReparseData);
                if (0 > Result)
                    return Result;
            }

            FileNode.FileInfo.FileAttributes |= (UInt32)FileAttributes.ReparsePoint;
            FileNode.FileInfo.ReparseTag = GetReparseTag(ReparseData);
            FileNode.ReparseData = ReparseData;

            return STATUS_SUCCESS;
        }

        public override Int32 DeleteReparsePoint(
            Object FileNode0,
            Object FileDesc,
            String FileName,
            Byte[] ReparseData)
        {
            FileNode FileNode = (FileNode)FileNode0;

            if (null != FileNode.MainFileNode)
                FileNode = FileNode.MainFileNode;

            if (null != FileNode.ReparseData)
            {
                Int32 Result = CanReplaceReparsePoint(FileNode.ReparseData, ReparseData);
                if (0 > Result)
                    return Result;
            }
            else
                return STATUS_NOT_A_REPARSE_POINT;

            FileNode.FileInfo.FileAttributes &= ~(UInt32)FileAttributes.ReparsePoint;
            FileNode.FileInfo.ReparseTag = 0;
            FileNode.ReparseData = null;

            return STATUS_SUCCESS;
        }

        public override Boolean GetStreamEntry(
            Object FileNode0,
            Object FileDesc,
            ref Object Context,
            out String StreamName,
            out UInt64 StreamSize,
            out UInt64 StreamAllocationSize)
        {
            FileNode FileNode = (FileNode)FileNode0;
            IEnumerator<String> Enumerator = (IEnumerator<String>)Context;

            if (null == Enumerator)
            {
                if (null != FileNode.MainFileNode)
                    FileNode = FileNode.MainFileNode;

                List<String> StreamFileNames = new List<String>();
                if (0 == (FileNode.FileInfo.FileAttributes & (UInt32)FileAttributes.Directory))
                    StreamFileNames.Add(FileNode.FileName);
                StreamFileNames.AddRange(FileNodeMap.GetStreamFileNames(FileNode));
                Context = Enumerator = StreamFileNames.GetEnumerator();
            }

            while (Enumerator.MoveNext())
            {
                String FullFileName = Enumerator.Current;
                FileNode StreamFileNode = FileNodeMap.Get(FullFileName);
                if (null != StreamFileNode)
                {
                    int Index = FullFileName.IndexOf(':');
                    if (0 > Index)
                        StreamName = "";
                    else
                        StreamName = FullFileName.Substring(Index + 1);
                    StreamSize = StreamFileNode.FileInfo.FileSize;
                    StreamAllocationSize = StreamFileNode.FileInfo.AllocationSize;
                    return true;
                }
            }

            StreamName = default(String);
            StreamSize = default(UInt64);
            StreamAllocationSize = default(UInt64);
            return false;
        }
        public override Boolean GetEaEntry(
            Object FileNode0,
            Object FileDesc,
            ref Object Context,
            out String EaName,
            out Byte[] EaValue,
            out Boolean NeedEa)
        {
            FileNode FileNode = (FileNode)FileNode0;
            IEnumerator<KeyValuePair<String, EaValueData>> Enumerator =
                (IEnumerator<KeyValuePair<String, EaValueData>>)Context;

            if (null == Enumerator)
            {
                SortedDictionary<String, EaValueData> EaMap = FileNode.GetEaMap(false);
                if (null == EaMap)
                {
                    EaName = default(String);
                    EaValue = default(Byte[]);
                    NeedEa = default(Boolean);
                    return false;
                }

                Context = Enumerator = EaMap.GetEnumerator();
            }

            while (Enumerator.MoveNext())
            {
                KeyValuePair<String, EaValueData> Pair = Enumerator.Current;
                EaName = Pair.Key;
                EaValue = Pair.Value.EaValue;
                NeedEa = Pair.Value.NeedEa;
                return true;
            }

            EaName = default(String);
            EaValue = default(Byte[]);
            NeedEa = default(Boolean);
            return false;
        }
        public override Int32 SetEaEntry(
            Object FileNode0,
            Object FileDesc,
            ref Object Context,
            String EaName,
            Byte[] EaValue,
            Boolean NeedEa)
        {
            FileNode FileNode = (FileNode)FileNode0;
            SortedDictionary<String, EaValueData> EaMap = FileNode.GetEaMap(true);
            EaValueData Data;
            UInt32 EaSizePlus = 0, EaSizeMinus = 0;
            if (EaMap.TryGetValue(EaName, out Data))
            {
                EaSizeMinus = GetEaEntrySize(EaName, Data.EaValue, Data.NeedEa);
                EaMap.Remove(EaName);
            }
            if (null != EaValue)
            {
                Data.EaValue = EaValue;
                Data.NeedEa = NeedEa;
                EaMap[EaName] = Data;
                EaSizePlus = GetEaEntrySize(EaName, EaValue, NeedEa);
            }
            FileNode.FileInfo.EaSize = FileNode.FileInfo.EaSize + EaSizePlus - EaSizeMinus;
            return STATUS_SUCCESS;
        }

        private FileNodeMap FileNodeMap;
        private UInt32 MaxFileNodes;
        private UInt32 MaxFileSize;
        private UInt64 SlowioMaxDelay;
        private UInt64 SlowioPercentDelay;
        private UInt64 SlowioRarefyDelay;
        private volatile Int32 SlowioTasksRunning;
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
                UInt32 SlowioMaxDelay = 0;
                UInt32 SlowioPercentDelay = 0;
                UInt32 SlowioRarefyDelay = 0;
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
                    case 'M':
                        argtol(Args, ref I, ref SlowioMaxDelay);
                        break;
                    case 'n':
                        argtol(Args, ref I, ref MaxFileNodes);
                        break;
                    case 'P':
                        argtol(Args, ref I, ref SlowioPercentDelay);
                        break;
                    case 'R':
                       argtol(Args, ref I, ref SlowioRarefyDelay);
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
                    CaseInsensitive, MaxFileNodes, MaxFileSize, RootSddl,
                    SlowioMaxDelay, SlowioPercentDelay, SlowioRarefyDelay));
                Host.FileInfoTimeout = FileInfoTimeout;
                Host.Prefix = VolumePrefix;
                Host.FileSystemName = null != FileSystemName ? FileSystemName : "-MEMFS";
                if (0 > Host.Mount(MountPoint, null, false, DebugFlags))
                    throw new IOException("cannot mount file system");
                MountPoint = Host.MountPoint();
                _Host = Host;

                Log(EVENTLOG_INFORMATION_TYPE, String.Format("{0} -t {1} -n {2} -s {3}{4}{5}{6}{7}{8}{9}",
                    PROGNAME, (Int32)FileInfoTimeout, MaxFileNodes, MaxFileSize,
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
                    "    -M MaxDelay         [maximum slow IO delay in millis]\n" +
                    "    -P PercentDelay     [percent of slow IO to make pending]\n" +
                    "    -R RarefyDelay      [adjust the rarity of pending slow IO]\n" +
                    "    -F FileSystemName\n" +
                    "    -S RootSddl         [file rights: FA, etc; NO generic rights: GA, etc.]\n" +
                    "    -u \\Server\\Share    [UNC prefix (single backslash)]\n" +
                    "    -m MountPoint       [X:|* (required if no UNC prefix)]\n",
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
