/**
 * @file Program.cs
 *
 * @copyright 2015-2025 Bill Zissimopoulos
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
        public static string GetDirectoryName(string path)
        {
            int index = path.LastIndexOf('\\');
            if (0 > index)
                return path;
            else if (0 == index)
                return "\\";
            else
                return path.Substring(0, index);
        }

        public static string GetFileName(string path)
        {
            int index = path.LastIndexOf('\\');
            if (0 > index)
                return path;
            else
                return path.Substring(index + 1);
        }
    }

    struct EaValueData
    {
        public byte[] eaValue;
        public bool needEa;
    }

    class FileNode
    {
        public FileNode(string fileName)
        {
            this.fileName = fileName;
            fileInfo.CreationTime =
            fileInfo.LastAccessTime =
            fileInfo.LastWriteTime =
            fileInfo.ChangeTime = (ulong)DateTime.Now.ToFileTimeUtc();
            fileInfo.IndexNumber = indexNumber++;
        }
        public FileInfo GetFileInfo()
        {
            if (null == mainFileNode)
                return this.fileInfo;
            else
            {
                FileInfo fileInfo = mainFileNode.fileInfo;
                fileInfo.FileAttributes &= ~(uint)FileAttributes.Directory;
                /* named streams cannot be directories */
                fileInfo.AllocationSize = this.fileInfo.AllocationSize;
                fileInfo.FileSize = this.fileInfo.FileSize;
                return fileInfo;
            }
        }
        public SortedDictionary<string, EaValueData> GetEaMap(bool Force)
        {
            //FileNode fileNode = null == mainFileNode ? this : mainFileNode;
            if (null == eaMap && Force)
                eaMap = new SortedDictionary<string, EaValueData>(StringComparer.OrdinalIgnoreCase);
            return eaMap;
        }

        private static ulong indexNumber = 1;
        public string fileName;
        public FileInfo fileInfo;
        public byte[] fileSecurity;
        public byte[] fileData;
        public byte[] reparseData;
        private SortedDictionary<string, EaValueData> eaMap;
        public FileNode mainFileNode;
        public int openCount;
    }

    class FileNodeMap
    {
        public FileNodeMap(bool caseInsensitive)
        {
            this.caseInsensitive = caseInsensitive;
            StringComparer comparer = caseInsensitive ?
                StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
            _set = new SortedSet<string>(comparer);
            _map = new Dictionary<string, FileNode>(comparer);
        }
        public uint Count()
        {
            return (uint)_map.Count;
        }
        public FileNode Get(string fileName)
        {
            FileNode fileNode;
            return _map.TryGetValue(fileName, out fileNode) ? fileNode : null;
        }
        public FileNode GetMain(string fileName)
        {
            int index = fileName.IndexOf(':');
            if (0 > index)
                return null;
            FileNode fileNode;
            return _map.TryGetValue(fileName.Substring(0, index), out fileNode) ? fileNode : null;
        }
        public FileNode GetParent(string fileName, ref int result)
        {
            FileNode fileNode;
            _map.TryGetValue(Path.GetDirectoryName(fileName), out fileNode);
            if (null == fileNode)
            {
                result = FileSystemBase.STATUS_OBJECT_PATH_NOT_FOUND;
                return null;
            }
            if (0 == (fileNode.fileInfo.FileAttributes & (uint)FileAttributes.Directory))
            {
                result = FileSystemBase.STATUS_NOT_A_DIRECTORY;
                return null;
            }
            return fileNode;
        }
        public void TouchParent(FileNode fileNode)
        {
            if ("\\" == fileNode.fileName)
                return;
            int result = FileSystemBase.STATUS_SUCCESS;
            FileNode parent = GetParent(fileNode.fileName, ref result);
            if (null == parent)
                return;
            parent.fileInfo.LastAccessTime =
            parent.fileInfo.LastWriteTime =
            parent.fileInfo.ChangeTime = (ulong)DateTime.Now.ToFileTimeUtc();
        }
        public void Insert(FileNode fileNode)
        {
            _set.Add(fileNode.fileName);
            _map.Add(fileNode.fileName, fileNode);
            TouchParent(fileNode);
        }
        public void Remove(FileNode fileNode)
        {
            if (_set.Remove(fileNode.fileName))
            {
                _map.Remove(fileNode.fileName);
                TouchParent(fileNode);
            }
        }
        public bool HasChild(FileNode fileNode)
        {
            foreach (string name in GetChildrenFileNames(fileNode, null))
                return true;
            return false;
        }
        public IEnumerable<string> GetChildrenFileNames(FileNode fileNode, string marker)
        {
            string minName = "\\";
            string maxName = "]";
            if ("\\" != fileNode.fileName)
            {
                minName = fileNode.fileName + "\\";
                maxName = fileNode.fileName + "]";
            }
            if (null != marker)
                minName += marker;
            foreach (string name in _set.GetViewBetween(minName, maxName))
                if (name != minName &&
                    name.Length > maxName.Length && -1 == name.IndexOfAny(_delimiters, maxName.Length))
                    yield return name;
        }
        public IEnumerable<string> GetStreamFileNames(FileNode fileNode)
        {
            string minName = fileNode.fileName + ":";
            string maxName = fileNode.fileName + ";";
            foreach (string name in _set.GetViewBetween(minName, maxName))
                if (name.Length > minName.Length)
                    yield return name;
        }
        public IEnumerable<string> GetDescendantFileNames(FileNode fileNode)
        {
            yield return fileNode.fileName;
            string minName = fileNode.fileName + ":";
            string maxName = fileNode.fileName + ";";
            foreach (string name in _set.GetViewBetween(minName, maxName))
                if (name.Length > minName.Length)
                    yield return name;
            minName = "\\";
            maxName = "]";
            if ("\\" != fileNode.fileName)
            {
                minName = fileNode.fileName + "\\";
                maxName = fileNode.fileName + "]";
            }
            foreach (string name in _set.GetViewBetween(minName, maxName))
                if (name.Length > minName.Length)
                    yield return name;
        }

        private static readonly char[] _delimiters = new char[] { '\\', ':' };
        public bool caseInsensitive;
        private SortedSet<string> _set;
        private Dictionary<string, FileNode> _map;
    }

    class Memfs : FileSystemBase
    {
        private FileSystemHost _host;
        public const ushort MEMFS_SECTOR_SIZE = 512;
        public const ushort MEMFS_SECTORS_PER_ALLOCATION_UNIT = 1;

        public Memfs(
            bool caseInsensitive, uint maxFileNodes, uint maxFileSize, string rootSddl,
            ulong slowioMaxDelay, ulong slowioPercentDelay, ulong slowioRarefyDelay)
        {
            this._fileNodeMap = new FileNodeMap(caseInsensitive);
            this._maxFileNodes = maxFileNodes;
            this._maxFileSize = maxFileSize;
            this._slowioMaxDelay = slowioMaxDelay;
            this._slowioPercentDelay = slowioPercentDelay;
            this._slowioRarefyDelay = slowioRarefyDelay;

            /*
             * Create root directory.
             */

            FileNode rootNode = new FileNode("\\");
            rootNode.fileInfo.FileAttributes = (uint)FileAttributes.Directory;
            if (null == rootSddl)
                rootSddl = "O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)";
            RawSecurityDescriptor rootSecurityDescriptor = new RawSecurityDescriptor(rootSddl);
            rootNode.fileSecurity = new byte[rootSecurityDescriptor.BinaryLength];
            rootSecurityDescriptor.GetBinaryForm(rootNode.fileSecurity, 0);

            _fileNodeMap.Insert(rootNode);
        }

        public override int Init(object host0)
        {
            _host = (FileSystemHost)host0;
            _host.SectorSize = Memfs.MEMFS_SECTOR_SIZE;
            _host.SectorsPerAllocationUnit = Memfs.MEMFS_SECTORS_PER_ALLOCATION_UNIT;
            _host.VolumeCreationTime = (ulong)DateTime.Now.ToFileTimeUtc();
            _host.VolumeSerialNumber = (uint)(_host.VolumeCreationTime / (10000 * 1000));
            _host.CaseSensitiveSearch = !_fileNodeMap.caseInsensitive;
            _host.CasePreservedNames = true;
            _host.UnicodeOnDisk = true;
            _host.PersistentAcls = true;
            _host.ReparsePoints = true;
            _host.ReparsePointsAccessCheck = false;
            _host.NamedStreams = true;
            _host.PostCleanupWhenModifiedOnly = true;
            _host.PostDispositionWhenNecessaryOnly = true;
            _host.PassQueryDirectoryFileName = true;
            _host.ExtendedAttributes = true;
            _host.WslFeatures = true;
            _host.RejectIrpPriorToTransact0 = true;
            _host.SupportsPosixUnlinkRename = true;
            return STATUS_SUCCESS;
        }

#if MEMFS_SLOWIO
        public override int Mounted(object host)
        {
            _slowioTasksRunning = 0;
            return STATUS_SUCCESS;
        }

        public override void Unmounted(object host)
        {
            while (_slowioTasksRunning != 0)
                Thread.Sleep(1000);
        }
#endif

        public override int GetVolumeInfo(
            out VolumeInfo volumeInfo)
        {
            volumeInfo = default;
            volumeInfo.TotalSize = _maxFileNodes * (ulong)_maxFileSize;
            volumeInfo.FreeSize = (_maxFileNodes - _fileNodeMap.Count()) * (ulong)_maxFileSize;
            volumeInfo.SetVolumeLabel(_volumeLabel);
            return STATUS_SUCCESS;
        }

        public override int SetVolumeLabel(
            string volumeLabel,
            out VolumeInfo volumeInfo)
        {
            this._volumeLabel = volumeLabel;
            return GetVolumeInfo(out volumeInfo);
        }

        public override int GetSecurityByName(
            string fileName,
            out uint fileAttributes/* or ReparsePointIndex */,
            ref byte[] securityDescriptor)
        {
            FileNode fileNode = _fileNodeMap.Get(fileName);
            if (null == fileNode)
            {
                int result = STATUS_OBJECT_NAME_NOT_FOUND;
                if (FindReparsePoint(fileName, out fileAttributes))
                    result = STATUS_REPARSE;
                else
                    _fileNodeMap.GetParent(fileName, ref result);
                return result;
            }

            uint fileAttributesMask = ~(uint)0;
            if (null != fileNode.mainFileNode)
            {
                fileAttributesMask = ~(uint)System.IO.FileAttributes.Directory;
                fileNode = fileNode.mainFileNode;
            }
            fileAttributes = fileNode.fileInfo.FileAttributes & fileAttributesMask;
            if (null != securityDescriptor)
                securityDescriptor = fileNode.fileSecurity;

            return STATUS_SUCCESS;
        }

        public override int CreateEx(
            string fileName,
            uint createOptions,
            uint grantedAccess,
            uint fileAttributes,
            byte[] securityDescriptor,
            ulong allocationSize,
            IntPtr extraBuffer,
            uint extraLength,
            bool extraBufferIsReparsePoint,
            out object fileNode0,
            out object fileDesc,
            out FileInfo fileInfo,
            out string normalizedName)
        {
            fileNode0 = default;
            fileDesc = default;
            fileInfo = default;
            normalizedName = default;

            FileNode fileNode;
            FileNode parentNode;
            int result = STATUS_SUCCESS;

            fileNode = _fileNodeMap.Get(fileName);
            if (null != fileNode)
                return STATUS_OBJECT_NAME_COLLISION;
            parentNode = _fileNodeMap.GetParent(fileName, ref result);
            if (null == parentNode)
                return result;

            if (0 != (createOptions & FILE_DIRECTORY_FILE))
                allocationSize = 0;
            if (_fileNodeMap.Count() >= _maxFileNodes)
                return STATUS_CANNOT_MAKE;
            if (allocationSize > _maxFileSize)
                return STATUS_DISK_FULL;

            if ("\\" != parentNode.fileName)
                /* normalize name */
                fileName = parentNode.fileName + "\\" + Path.GetFileName(fileName);
            fileNode = new FileNode(fileName);
            fileNode.mainFileNode = _fileNodeMap.GetMain(fileName);
            fileNode.fileInfo.FileAttributes = 0 != (fileAttributes & (uint)System.IO.FileAttributes.Directory) ?
                fileAttributes : fileAttributes | (uint)System.IO.FileAttributes.Archive;
            fileNode.fileSecurity = securityDescriptor;
            if (IntPtr.Zero != extraBuffer)
            {
                if (!extraBufferIsReparsePoint)
                {
                    result = SetEaEntries(fileNode, null, extraBuffer, extraLength);
                    if (0 > result)
                        return result;
                }
                else
                {
                    byte[] reparseData = MakeReparsePoint(extraBuffer, extraLength);
                    fileNode.fileInfo.FileAttributes |= (uint)System.IO.FileAttributes.ReparsePoint;
                    fileNode.fileInfo.ReparseTag = GetReparseTag(reparseData);
                    fileNode.reparseData = reparseData;
                }
            }
            if (0 != allocationSize)
            {
                result = SetFileSizeInternal(fileNode, allocationSize, true);
                if (0 > result)
                    return result;
            }
            _fileNodeMap.Insert(fileNode);

            Interlocked.Increment(ref fileNode.openCount);
            fileNode0 = fileNode;
            fileInfo = fileNode.GetFileInfo();
            normalizedName = fileNode.fileName;

            return STATUS_SUCCESS;
        }

        public override int Open(
            string fileName,
            uint createOptions,
            uint grantedAccess,
            out object fileNode0,
            out object fileDesc,
            out FileInfo fileInfo,
            out string normalizedName)
        {
            fileNode0 = default;
            fileDesc = default;
            fileInfo = default;
            normalizedName = default;

            FileNode fileNode;
            int result;

            fileNode = _fileNodeMap.Get(fileName);
            if (null == fileNode)
            {
                result = STATUS_OBJECT_NAME_NOT_FOUND;
                _fileNodeMap.GetParent(fileName, ref result);
                return result;
            }

            if (0 != (createOptions & FILE_NO_EA_KNOWLEDGE) &&
                null == fileNode.mainFileNode)
            {
                SortedDictionary<string, EaValueData> eaMap = fileNode.GetEaMap(false);
                if (null != eaMap)
                {
                    foreach (KeyValuePair<string, EaValueData> pair in eaMap)
                        if (pair.Value.needEa)
                            return STATUS_ACCESS_DENIED;
                }
            }

            Interlocked.Increment(ref fileNode.openCount);
            fileNode0 = fileNode;
            fileInfo = fileNode.GetFileInfo();
            normalizedName = fileNode.fileName;

            return STATUS_SUCCESS;
        }

        public override int OverwriteEx(
            object fileNode0,
            object fileDesc,
            uint fileAttributes,
            bool replaceFileAttributes,
            ulong allocationSize,
            IntPtr ea,
            uint eaLength,
            out FileInfo fileInfo)
        {
            fileInfo = default;

            FileNode fileNode = (FileNode)fileNode0;
            int result;

            List<string> streamFileNames = new List<string>(_fileNodeMap.GetStreamFileNames(fileNode));
            foreach (string streamFileName in streamFileNames)
            {
                FileNode streamNode = _fileNodeMap.Get(streamFileName);
                if (null == streamNode)
                    continue; /* should not happen */
                if (0 == streamNode.openCount)
                    _fileNodeMap.Remove(streamNode);
            }

            SortedDictionary<string, EaValueData> eaMap = fileNode.GetEaMap(false);
            if (null != eaMap)
            {
                eaMap.Clear();
                fileNode.fileInfo.EaSize = 0;
            }
            if (IntPtr.Zero != ea)
            {
                result = SetEaEntries(fileNode, null, ea, eaLength);
                if (0 > result)
                    return result;
            }

            result = SetFileSizeInternal(fileNode, allocationSize, true);
            if (0 > result)
                return result;
            if (replaceFileAttributes)
                fileNode.fileInfo.FileAttributes = fileAttributes | (uint)System.IO.FileAttributes.Archive;
            else
                fileNode.fileInfo.FileAttributes |= fileAttributes | (uint)System.IO.FileAttributes.Archive;
            fileNode.fileInfo.FileSize = 0;
            fileNode.fileInfo.LastAccessTime =
            fileNode.fileInfo.LastWriteTime =
            fileNode.fileInfo.ChangeTime = (ulong)DateTime.Now.ToFileTimeUtc();

            fileInfo = fileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override void Cleanup(
            object fileNode0,
            object fileDesc,
            string fileName,
            uint flags)
        {
            FileNode fileNode = (FileNode)fileNode0;
            FileNode mainFileNode = null != fileNode.mainFileNode ?
                fileNode.mainFileNode : fileNode;

            if (0 != (flags & CleanupSetArchiveBit))
            {
                if (0 == (mainFileNode.fileInfo.FileAttributes & (uint)FileAttributes.Directory))
                    mainFileNode.fileInfo.FileAttributes |= (uint)FileAttributes.Archive;
            }

            if (0 != (flags & (CleanupSetLastAccessTime | CleanupSetLastWriteTime | CleanupSetChangeTime)))
            {
                ulong systemTime = (ulong)DateTime.Now.ToFileTimeUtc();

                if (0 != (flags & CleanupSetLastAccessTime))
                    mainFileNode.fileInfo.LastAccessTime = systemTime;
                if (0 != (flags & CleanupSetLastWriteTime))
                    mainFileNode.fileInfo.LastWriteTime = systemTime;
                if (0 != (flags & CleanupSetChangeTime))
                    mainFileNode.fileInfo.ChangeTime = systemTime;
            }

            if (0 != (flags & CleanupSetAllocationSize))
            {
                ulong allocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
                ulong allocationSize = (fileNode.fileInfo.FileSize + allocationUnit - 1) /
                    allocationUnit * allocationUnit;
                SetFileSizeInternal(fileNode, allocationSize, true);
            }

            if (0 != (flags & CleanupDelete) && !_fileNodeMap.HasChild(fileNode))
            {
                List<string> streamFileNames = new List<string>(_fileNodeMap.GetStreamFileNames(fileNode));
                foreach (string streamFileName in streamFileNames)
                {
                    FileNode streamNode = _fileNodeMap.Get(streamFileName);
                    if (null == streamNode)
                        continue; /* should not happen */
                    _fileNodeMap.Remove(streamNode);
                }
                _fileNodeMap.Remove(fileNode);
            }
        }

        public override void Close(
            object fileNode0,
            object fileDesc)
        {
            FileNode fileNode = (FileNode)fileNode0;
            Interlocked.Decrement(ref fileNode.openCount);
        }

#if MEMFS_SLOWIO
        private ulong Hash(ulong x)
        {
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ul;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebul;
            x = x ^ (x >> 31);
            return x;
        }

        private static int s_spin = 0;

        private ulong PseudoRandom(ulong to)
        {
            /* John Oberschelp's PRNG */
            Interlocked.Increment(ref s_spin);
            return Hash((ulong)s_spin) % to;
        }

        private bool SlowioReturnPending()
        {
            if (0 == _slowioMaxDelay)
            {
                return false;
            }
            return PseudoRandom(100) < _slowioPercentDelay;
        }

        private void SlowioSnooze()
        {
            double millis = PseudoRandom(_slowioMaxDelay + 1) >> (int)PseudoRandom(_slowioRarefyDelay + 1);
            Thread.Sleep(TimeSpan.FromMilliseconds(millis));
        }

        private void SlowioReadTask(
            object fileNode0,
            IntPtr buffer,
            ulong offset,
            ulong endOffset,
            ulong requestHint)
        {
            SlowioSnooze();

            uint bytesTransferred = (uint)(endOffset - offset);
            FileNode fileNode = (FileNode)fileNode0;
            Marshal.Copy(fileNode.fileData, (int)offset, buffer, (int)bytesTransferred);

            _host.SendReadResponse(requestHint, STATUS_SUCCESS, bytesTransferred);
            Interlocked.Decrement(ref _slowioTasksRunning);
        }

        private void SlowioWriteTask(
            object fileNode0,
            IntPtr buffer,
            ulong offset,
            ulong endOffset,
            ulong requestHint)
        {
            SlowioSnooze();

            uint BytesTransferred = (uint)(endOffset - offset);
            FileNode fileNode = (FileNode)fileNode0;
            FileInfo fileInfo = fileNode.GetFileInfo();
            Marshal.Copy(buffer, fileNode.fileData, (int)offset, (int)BytesTransferred);

            _host.SendWriteResponse(requestHint, STATUS_SUCCESS, BytesTransferred, ref fileInfo);
            Interlocked.Decrement(ref _slowioTasksRunning);
        }

        private void SlowioReadDirectoryTask(
            object fileNode0,
            object fileDesc,
            string pattern,
            string marker,
            IntPtr buffer,
            uint length,
            ulong requestHint)
        {
            SlowioSnooze();

            uint bytesTransferred;
            var status = SeekableReadDirectory(fileNode0, fileDesc, pattern, marker, buffer, length, out bytesTransferred);

            _host.SendReadDirectoryResponse(requestHint, status, bytesTransferred);
            Interlocked.Decrement(ref _slowioTasksRunning);
        }
#endif

        public override int Read(
            object fileNode0,
            object fileDesc,
            IntPtr buffer,
            ulong offset,
            uint length,
            out uint bytesTransferred)
        {
            FileNode fileNode = (FileNode)fileNode0;
            ulong endOffset;

            if (offset >= fileNode.fileInfo.FileSize)
            {
                bytesTransferred = default;
                return STATUS_END_OF_FILE;
            }

            endOffset = offset + length;
            if (endOffset > fileNode.fileInfo.FileSize)
                endOffset = fileNode.fileInfo.FileSize;

#if MEMFS_SLOWIO
            if (SlowioReturnPending())
            {
                var hint = _host.GetOperationRequestHint();
                try
                {
                    Interlocked.Increment(ref _slowioTasksRunning);
                    Task.Run(() => SlowioReadTask(fileNode0, buffer, offset, endOffset, hint)).ConfigureAwait(false);

                    bytesTransferred = 0;
                    return STATUS_PENDING;
                }
                catch (Exception)
                {
                    Interlocked.Decrement(ref _slowioTasksRunning);
                }
            }
#endif

            bytesTransferred = (uint)(endOffset - offset);
            Marshal.Copy(fileNode.fileData, (int)offset, buffer, (int)bytesTransferred);

            return STATUS_SUCCESS;
        }

        public override int Write(
            object fileNode0,
            object fileDesc,
            IntPtr buffer,
            ulong offset,
            uint length,
            bool writeToEndOfFile,
            bool constrainedIo,
            out uint bytesTransferred,
            out FileInfo fileInfo)
        {
            FileNode fileNode = (FileNode)fileNode0;
            ulong endOffset;

            if (constrainedIo)
            {
                if (offset >= fileNode.fileInfo.FileSize)
                {
                    bytesTransferred = default;
                    fileInfo = default;
                    return STATUS_SUCCESS;
                }
                endOffset = offset + length;
                if (endOffset > fileNode.fileInfo.FileSize)
                    endOffset = fileNode.fileInfo.FileSize;
            }
            else
            {
                if (writeToEndOfFile)
                    offset = fileNode.fileInfo.FileSize;
                endOffset = offset + length;
                if (endOffset > fileNode.fileInfo.FileSize)
                {
                    int Result = SetFileSizeInternal(fileNode, endOffset, false);
                    if (0 > Result)
                    {
                        bytesTransferred = default;
                        fileInfo = default;
                        return Result;
                    }
                }
            }

#if MEMFS_SLOWIO
            if (SlowioReturnPending())
            {
                var hint = _host.GetOperationRequestHint();
                try
                {
                    Interlocked.Increment(ref _slowioTasksRunning);
                    Task.Run(() => SlowioWriteTask(fileNode0, buffer, offset, endOffset, hint)).ConfigureAwait(false);

                    bytesTransferred = 0;
                    fileInfo = default;
                    return STATUS_PENDING;
                }
                catch (Exception)
                {
                    Interlocked.Decrement(ref _slowioTasksRunning);
                }
            }
#endif

            bytesTransferred = (uint)(endOffset - offset);
            Marshal.Copy(buffer, fileNode.fileData, (int)offset, (int)bytesTransferred);

            fileInfo = fileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override int Flush(
            object fileNode0,
            object fileDesc,
            out FileInfo fileInfo)
        {
            FileNode fileNode = (FileNode)fileNode0;

            /*  nothing to flush, since we do not cache anything */
            fileInfo = null != fileNode ? fileNode.GetFileInfo() : default;

            return STATUS_SUCCESS;
        }

        public override int GetFileInfo(
            object fileNode0,
            object fileDesc,
            out FileInfo fileInfo)
        {
            FileNode fileNode = (FileNode)fileNode0;

            fileInfo = fileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override int SetBasicInfo(
            object fileNode0,
            object fileDesc,
            uint fileAttributes,
            ulong creationTime,
            ulong lastAccessTime,
            ulong lastWriteTime,
            ulong changeTime,
            out FileInfo fileInfo)
        {
            FileNode fileNode = (FileNode)fileNode0;

            if (null != fileNode.mainFileNode)
                fileNode = fileNode.mainFileNode;

            if (unchecked((uint)(-1)) != fileAttributes)
                fileNode.fileInfo.FileAttributes = fileAttributes;
            if (0 != creationTime)
                fileNode.fileInfo.CreationTime = creationTime;
            if (0 != lastAccessTime)
                fileNode.fileInfo.LastAccessTime = lastAccessTime;
            if (0 != lastWriteTime)
                fileNode.fileInfo.LastWriteTime = lastWriteTime;
            if (0 != changeTime)
                fileNode.fileInfo.ChangeTime = changeTime;

            fileInfo = fileNode.GetFileInfo();

            return STATUS_SUCCESS;
        }

        public override int SetFileSize(
            object fileNode0,
            object fileDesc,
            ulong newSize,
            bool setAllocationSize,
            out FileInfo fileInfo)
        {
            FileNode fileNode = (FileNode)fileNode0;
            int result;

            result = SetFileSizeInternal(fileNode, newSize, setAllocationSize);
            fileInfo = 0 <= result ? fileNode.GetFileInfo() : default;

            return STATUS_SUCCESS;
        }

        private int SetFileSizeInternal(
            FileNode fileNode,
            ulong newSize,
            bool setAllocationSize)
        {
            if (setAllocationSize)
            {
                if (fileNode.fileInfo.AllocationSize != newSize)
                {
                    if (newSize > _maxFileSize)
                        return STATUS_DISK_FULL;

                    byte[] fileData = null;
                    if (0 != newSize)
                        try
                        {
                            fileData = new byte[newSize];
                        }
                        catch
                        {
                            return STATUS_INSUFFICIENT_RESOURCES;
                        }
                    int CopyLength = (int)Math.Min(fileNode.fileInfo.AllocationSize, newSize);
                    if (0 != CopyLength)
                        Array.Copy(fileNode.fileData, fileData, CopyLength);

                    fileNode.fileData = fileData;
                    fileNode.fileInfo.AllocationSize = newSize;
                    if (fileNode.fileInfo.FileSize > newSize)
                        fileNode.fileInfo.FileSize = newSize;
                }
            }
            else
            {
                if (fileNode.fileInfo.FileSize != newSize)
                {
                    if (fileNode.fileInfo.AllocationSize < newSize)
                    {
                        ulong allocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
                        ulong allocationSize = (newSize + allocationUnit - 1) / allocationUnit * allocationUnit;
                        int Result = SetFileSizeInternal(fileNode, allocationSize, true);
                        if (0 > Result)
                            return Result;
                    }

                    if (fileNode.fileInfo.FileSize < newSize)
                    {
                        int copyLength = (int)(newSize - fileNode.fileInfo.FileSize);
                        if (0 != copyLength)
                            Array.Clear(fileNode.fileData, (int)fileNode.fileInfo.FileSize, copyLength);
                    }
                    fileNode.fileInfo.FileSize = newSize;
                }
            }

            return STATUS_SUCCESS;
        }

        public override int CanDelete(
            object fileNode0,
            object fileDesc,
            string fileName)
        {
            FileNode FileNode = (FileNode)fileNode0;

            if (_fileNodeMap.HasChild(FileNode))
                return STATUS_DIRECTORY_NOT_EMPTY;

            return STATUS_SUCCESS;
        }

        public override int Rename(
            object fileNode0,
            object fileDesc,
            string fileName,
            string newFileName,
            bool replaceIfExists)
        {
            FileNode fileNode = (FileNode)fileNode0;
            FileNode newFileNode;

            newFileNode = _fileNodeMap.Get(newFileName);
            if (null != newFileNode && fileNode != newFileNode)
            {
                if (!replaceIfExists)
                    return STATUS_OBJECT_NAME_COLLISION;
                if (0 != (newFileNode.fileInfo.FileAttributes & (uint)FileAttributes.Directory))
                    return STATUS_ACCESS_DENIED;
            }

            if (null != newFileNode && fileNode != newFileNode)
                _fileNodeMap.Remove(newFileNode);

            List<string> descendantFileNames = new List<string>(_fileNodeMap.GetDescendantFileNames(fileNode));
            foreach (string descendantFileName in descendantFileNames)
            {
                FileNode descendantFileNode = _fileNodeMap.Get(descendantFileName);
                if (null == descendantFileNode)
                    continue; /* should not happen */
                _fileNodeMap.Remove(descendantFileNode);
                descendantFileNode.fileName =
                    newFileName + descendantFileNode.fileName.Substring(fileName.Length);
                _fileNodeMap.Insert(descendantFileNode);
            }

            return STATUS_SUCCESS;
        }

        public override int GetSecurity(
            object fileNode0,
            object fileDesc,
            ref byte[] securityDescriptor)
        {
            FileNode fileNode = (FileNode)fileNode0;

            if (null != fileNode.mainFileNode)
                fileNode = fileNode.mainFileNode;

            securityDescriptor = fileNode.fileSecurity;

            return STATUS_SUCCESS;
        }

        public override int SetSecurity(
            object fileNode0,
            object fileDesc,
            AccessControlSections sections,
            byte[] securityDescriptor)
        {
            FileNode fileNode = (FileNode)fileNode0;

            if (null != fileNode.mainFileNode)
                fileNode = fileNode.mainFileNode;

            return ModifySecurityDescriptorEx(fileNode.fileSecurity, sections, securityDescriptor,
                ref fileNode.fileSecurity);
        }

        public override bool ReadDirectoryEntry(
            object fileNode0,
            object fileDesc,
            string pattern,
            string marker,
            ref object context,
            out string fileName,
            out FileInfo fileInfo)
        {
            FileNode fileNode = (FileNode)fileNode0;
            IEnumerator<string> enumerator = (IEnumerator<string>)context;

            if (null == enumerator)
            {
                List<string> childrenFileNames = new List<string>();
                if ("\\" != fileNode.fileName)
                {
                    /* if this is not the root directory add the dot entries */
                    if (null == marker)
                        childrenFileNames.Add(".");
                    if (null == marker || "." == marker)
                        childrenFileNames.Add("..");
                }
                childrenFileNames.AddRange(_fileNodeMap.GetChildrenFileNames(fileNode,
                    "." != marker && ".." != marker ? marker : null));
                context = enumerator = childrenFileNames.GetEnumerator();
            }

            while (enumerator.MoveNext())
            {
                string fullFileName = enumerator.Current;
                if ("." == fullFileName)
                {
                    fileName = ".";
                    fileInfo = fileNode.GetFileInfo();
                    return true;
                }
                else if (".." == fullFileName)
                {
                    int result = STATUS_SUCCESS;
                    FileNode ParentNode = _fileNodeMap.GetParent(fileNode.fileName, ref result);
                    if (null != ParentNode)
                    {
                        fileName = "..";
                        fileInfo = ParentNode.GetFileInfo();
                        return true;
                    }
                }
                else
                {
                    FileNode childFileNode = _fileNodeMap.Get(fullFileName);
                    if (null != childFileNode)
                    {
                        fileName = Path.GetFileName(fullFileName);
                        fileInfo = childFileNode.GetFileInfo();
                        return true;
                    }
                }
            }

            fileName = default;
            fileInfo = default;
            return false;
        }

#if MEMFS_SLOWIO
        public override int ReadDirectory(
            object fileNode0,
            object fileDesc,
            string pattern,
            string marker,
            IntPtr buffer,
            uint length,
            out uint bytesTransferred)
        {
            if (SlowioReturnPending())
            {
                var hint = _host.GetOperationRequestHint();
                try
                {
                    Interlocked.Increment(ref _slowioTasksRunning);
                    Task.Run(() => SlowioReadDirectoryTask(fileNode0, fileDesc, pattern, marker, buffer, length, hint));
                    bytesTransferred = 0;

                    return STATUS_PENDING;
                }
                catch (Exception)
                {
                    Interlocked.Decrement(ref _slowioTasksRunning);
                }
            }

            return SeekableReadDirectory(fileNode0, fileDesc, pattern, marker, buffer, length, out bytesTransferred);
        }
#endif

        public override int GetDirInfoByName(
            object parentNode0,
            object fileDesc,
            string fileName,
            out string normalizedName,
            out FileInfo fileInfo)
        {
            FileNode parentNode = (FileNode)parentNode0;
            FileNode fileNode;

            fileName =
                parentNode.fileName +
                ("\\" == parentNode.fileName ? "" : "\\") +
                Path.GetFileName(fileName);

            fileNode = _fileNodeMap.Get(fileName);
            if (null == fileNode)
            {
                normalizedName = default;
                fileInfo = default;
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            normalizedName = Path.GetFileName(fileNode.fileName);
            fileInfo = fileNode.fileInfo;

            return STATUS_SUCCESS;
        }

        public override int GetReparsePointByName(
            string fileName,
            bool isDirectory,
            ref byte[] reparseData)
        {
            FileNode fileNode;

            fileNode = _fileNodeMap.Get(fileName);
            if (null == fileNode)
                return STATUS_OBJECT_NAME_NOT_FOUND;

            if (0 == (fileNode.fileInfo.FileAttributes & (uint)FileAttributes.ReparsePoint))
                return STATUS_NOT_A_REPARSE_POINT;

            reparseData = fileNode.reparseData;

            return STATUS_SUCCESS;
        }

        public override int GetReparsePoint(
            object fileNode0,
            object fileDesc,
            string fileName,
            ref byte[] reparseData)
        {
            FileNode fileNode = (FileNode)fileNode0;

            if (null != fileNode.mainFileNode)
                fileNode = fileNode.mainFileNode;

            if (0 == (fileNode.fileInfo.FileAttributes & (uint)FileAttributes.ReparsePoint))
                return STATUS_NOT_A_REPARSE_POINT;

            reparseData = fileNode.reparseData;

            return STATUS_SUCCESS;
        }

        public override int SetReparsePoint(
            object fileNode0,
            object fileDesc,
            string fileName,
            byte[] reparseData)
        {
            FileNode fileNode = (FileNode)fileNode0;

            if (null != fileNode.mainFileNode)
                fileNode = fileNode.mainFileNode;

            if (_fileNodeMap.HasChild(fileNode))
                return STATUS_DIRECTORY_NOT_EMPTY;

            if (null != fileNode.reparseData)
            {
                int Result = CanReplaceReparsePoint(fileNode.reparseData, reparseData);
                if (0 > Result)
                    return Result;
            }

            fileNode.fileInfo.FileAttributes |= (uint)FileAttributes.ReparsePoint;
            fileNode.fileInfo.ReparseTag = GetReparseTag(reparseData);
            fileNode.reparseData = reparseData;

            return STATUS_SUCCESS;
        }

        public override int DeleteReparsePoint(
            object fileNode0,
            object fileDesc,
            string fileName,
            byte[] reparseData)
        {
            FileNode fileNode = (FileNode)fileNode0;

            if (null != fileNode.mainFileNode)
                fileNode = fileNode.mainFileNode;

            if (null != fileNode.reparseData)
            {
                int Result = CanReplaceReparsePoint(fileNode.reparseData, reparseData);
                if (0 > Result)
                    return Result;
            }
            else
                return STATUS_NOT_A_REPARSE_POINT;

            fileNode.fileInfo.FileAttributes &= ~(uint)FileAttributes.ReparsePoint;
            fileNode.fileInfo.ReparseTag = 0;
            fileNode.reparseData = null;

            return STATUS_SUCCESS;
        }

        public override bool GetStreamEntry(
            object fileNode0,
            object fileDesc,
            ref object context,
            out string streamName,
            out ulong streamSize,
            out ulong streamAllocationSize)
        {
            FileNode fileNode = (FileNode)fileNode0;
            IEnumerator<string> enumerator = (IEnumerator<string>)context;

            if (null == enumerator)
            {
                if (null != fileNode.mainFileNode)
                    fileNode = fileNode.mainFileNode;

                List<string> streamFileNames = new List<string>();
                if (0 == (fileNode.fileInfo.FileAttributes & (uint)FileAttributes.Directory))
                    streamFileNames.Add(fileNode.fileName);
                streamFileNames.AddRange(_fileNodeMap.GetStreamFileNames(fileNode));
                context = enumerator = streamFileNames.GetEnumerator();
            }

            while (enumerator.MoveNext())
            {
                string fullFileName = enumerator.Current;
                FileNode streamFileNode = _fileNodeMap.Get(fullFileName);
                if (null != streamFileNode)
                {
                    int index = fullFileName.IndexOf(':');
                    if (0 > index)
                        streamName = "";
                    else
                        streamName = fullFileName.Substring(index + 1);
                    streamSize = streamFileNode.fileInfo.FileSize;
                    streamAllocationSize = streamFileNode.fileInfo.AllocationSize;
                    return true;
                }
            }

            streamName = default;
            streamSize = default;
            streamAllocationSize = default;
            return false;
        }
        public override bool GetEaEntry(
            object fileNode0,
            object fileDesc,
            ref object context,
            out string eaName,
            out byte[] eaValue,
            out bool needEa)
        {
            FileNode fileNode = (FileNode)fileNode0;
            IEnumerator<KeyValuePair<string, EaValueData>> enumerator =
                (IEnumerator<KeyValuePair<string, EaValueData>>)context;

            if (null == enumerator)
            {
                SortedDictionary<string, EaValueData> eaMap = fileNode.GetEaMap(false);
                if (null == eaMap)
                {
                    eaName = default;
                    eaValue = default;
                    needEa = default;
                    return false;
                }

                context = enumerator = eaMap.GetEnumerator();
            }

            while (enumerator.MoveNext())
            {
                KeyValuePair<string, EaValueData> pair = enumerator.Current;
                eaName = pair.Key;
                eaValue = pair.Value.eaValue;
                needEa = pair.Value.needEa;
                return true;
            }

            eaName = default;
            eaValue = default;
            needEa = default;
            return false;
        }
        public override int SetEaEntry(
            object fileNode0,
            object fileDesc,
            ref object context,
            string eaName,
            byte[] eaValue,
            bool needEa)
        {
            FileNode fileNode = (FileNode)fileNode0;
            SortedDictionary<string, EaValueData> eaMap = fileNode.GetEaMap(true);
            EaValueData data;
            uint eaSizePlus = 0, eaSizeMinus = 0;
            if (eaMap.TryGetValue(eaName, out data))
            {
                eaSizeMinus = GetEaEntrySize(eaName, data.eaValue, data.needEa);
                eaMap.Remove(eaName);
            }
            if (null != eaValue)
            {
                data.eaValue = eaValue;
                data.needEa = needEa;
                eaMap[eaName] = data;
                eaSizePlus = GetEaEntrySize(eaName, eaValue, needEa);
            }
            fileNode.fileInfo.EaSize = fileNode.fileInfo.EaSize + eaSizePlus - eaSizeMinus;
            return STATUS_SUCCESS;
        }

        private FileNodeMap _fileNodeMap;
        private uint _maxFileNodes;
        private uint _maxFileSize;
        private ulong _slowioMaxDelay;
        private ulong _slowioPercentDelay;
        private ulong _slowioRarefyDelay;
        private volatile int _slowioTasksRunning;
        private string _volumeLabel;
    }

    public class MemfsService : Service
    {
        private class CommandLineUsageException : Exception
        {
            public CommandLineUsageException(string message = null) : base(message) { }

            public bool HasMessage => Message != null;
        }

        private const string PROGNAME = "memfs-dotnet";

        public MemfsService() : base("MemfsService")
        {
        }
        protected override void OnStart(string[] args)
        {
            try
            {
                bool caseInsensitive = false;
                string debugLogFile = null;
                uint debugFlags = 0;
                uint fileInfoTimeout = unchecked((uint)-1);
                uint maxFileNodes = 1024;
                uint maxFileSize = 16 * 1024 * 1024;
                uint slowioMaxDelay = 0;
                uint slowioPercentDelay = 0;
                uint slowioRarefyDelay = 0;
                string fileSystemName = null;
                string volumePrefix = null;
                string mountPoint = null;
                string rootSddl = null;
                FileSystemHost host = null;
                Memfs memfs = null;
                int i;

                for (i = 1; args.Length > i; i++)
                {
                    string arg = args[i];
                    if ('-' != arg[0])
                        break;
                    switch (arg[1])
                    {
                        case '?':
                            throw new CommandLineUsageException();
                        case 'D':
                            argtos(args, ref i, ref debugLogFile);
                            break;
                        case 'd':
                            argtol(args, ref i, ref debugFlags);
                            break;
                        case 'F':
                            argtos(args, ref i, ref fileSystemName);
                            break;
                        case 'i':
                            caseInsensitive = true;
                            break;
                        case 'm':
                            argtos(args, ref i, ref mountPoint);
                            break;
                        case 'M':
                            argtol(args, ref i, ref slowioMaxDelay);
                            break;
                        case 'n':
                            argtol(args, ref i, ref maxFileNodes);
                            break;
                        case 'P':
                            argtol(args, ref i, ref slowioPercentDelay);
                            break;
                        case 'R':
                            argtol(args, ref i, ref slowioRarefyDelay);
                            break;
                        case 'S':
                            argtos(args, ref i, ref rootSddl);
                            break;
                        case 's':
                            argtol(args, ref i, ref maxFileSize);
                            break;
                        case 't':
                            argtol(args, ref i, ref fileInfoTimeout);
                            break;
                        case 'u':
                            argtos(args, ref i, ref volumePrefix);
                            break;
                        default:
                            throw new CommandLineUsageException();
                    }
                }

                if (args.Length > i)
                    throw new CommandLineUsageException();

                if ((null == volumePrefix || 0 == volumePrefix.Length) && null == mountPoint)
                    throw new CommandLineUsageException();

                if (null != debugLogFile)
                    if (0 > FileSystemHost.SetDebugLogFile(debugLogFile))
                        throw new CommandLineUsageException("cannot open debug log file");

                host = new FileSystemHost(memfs = new Memfs(
                    caseInsensitive, maxFileNodes, maxFileSize, rootSddl,
                    slowioMaxDelay, slowioPercentDelay, slowioRarefyDelay));
                host.FileInfoTimeout = fileInfoTimeout;
                host.Prefix = volumePrefix;
                host.FileSystemName = null != fileSystemName ? fileSystemName : "-MEMFS";
                if (0 > host.Mount(mountPoint, null, false, debugFlags))
                    throw new IOException("cannot mount file system");
                mountPoint = host.MountPoint();
                _host = host;

                Log(EVENTLOG_INFORMATION_TYPE, string.Format("{0} -t {1} -n {2} -s {3}{4}{5}{6}{7}{8}{9}",
                    PROGNAME, (int)fileInfoTimeout, maxFileNodes, maxFileSize,
                    null != rootSddl ? " -S " : "", null != rootSddl ? rootSddl : "",
                    null != volumePrefix && 0 < volumePrefix.Length ? " -u " : "",
                        null != volumePrefix && 0 < volumePrefix.Length ? volumePrefix : "",
                    null != mountPoint ? " -m " : "", null != mountPoint ? mountPoint : ""));
            }
            catch (CommandLineUsageException ex)
            {
                Log(EVENTLOG_ERROR_TYPE, string.Format(
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
                Log(EVENTLOG_ERROR_TYPE, string.Format("{0}", ex.Message));
                throw;
            }
        }
        protected override void OnStop()
        {
            _host.Unmount();
            _host = null;
        }

        private static void argtos(string[] args, ref int i, ref string v)
        {
            if (args.Length > ++i)
                v = args[i];
            else
                throw new CommandLineUsageException();
        }
        private static void argtol(string[] args, ref int i, ref uint v)
        {
            int r;
            if (args.Length > ++i)
                v = int.TryParse(args[i], out r) ? (uint)r : v;
            else
                throw new CommandLineUsageException();
        }

        private FileSystemHost _host;
    }

    class Program
    {
        static void Main(string[] args)
        {
            Environment.ExitCode = new MemfsService().Run();
        }
    }
}
