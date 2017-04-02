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
using Fsp.Interop;

namespace Fsp
{

    public class FileSystem : IDisposable
    {
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
            Dispose(true);
            GC.SuppressFinalize(true);
        }

        protected void Dispose(bool disposing)
        {
            if (IntPtr.Zero != _FileSystem)
            {
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
            _VolumeParams.Flags = CaseSensitiveSearch ? VolumeParams.CaseSensitiveSearch : 0;
        }
        public void SetCasePreservedNames(Boolean CasePreservedNames)
        {
            _VolumeParams.Flags = CasePreservedNames ? VolumeParams.CasePreservedNames : 0;
        }
        public void SetUnicodeOnDisk(Boolean UnicodeOnDisk)
        {
            _VolumeParams.Flags = UnicodeOnDisk ? VolumeParams.UnicodeOnDisk : 0;
        }
        public void SetPersistentAcls(Boolean PersistentAcls)
        {
            _VolumeParams.Flags = PersistentAcls ? VolumeParams.PersistentAcls : 0;
        }
        public void SetReparsePoints(Boolean ReparsePoints)
        {
            _VolumeParams.Flags = ReparsePoints ? VolumeParams.ReparsePoints : 0;
        }
        public void SetReparsePointsAccessCheck(Boolean ReparsePointsAccessCheck)
        {
            _VolumeParams.Flags = ReparsePointsAccessCheck ? VolumeParams.ReparsePointsAccessCheck : 0;
        }
        public void SetNamedStreams(Boolean NamedStreams)
        {
            _VolumeParams.Flags = NamedStreams ? VolumeParams.NamedStreams : 0;
        }
        public void SetPostCleanupWhenModifiedOnly(Boolean PostCleanupWhenModifiedOnly)
        {
            _VolumeParams.Flags = PostCleanupWhenModifiedOnly ? VolumeParams.PostCleanupWhenModifiedOnly : 0;
        }
        public void SetPassQueryDirectoryPattern(Boolean PassQueryDirectoryPattern)
        {
            _VolumeParams.Flags = PassQueryDirectoryPattern ? VolumeParams.PassQueryDirectoryPattern : 0;
        }
        public void SetPrefix(String Prefix)
        {
            _VolumeParams.Prefix = Prefix;
        }
        public void SetFileSystemName(String FileSystemName)
        {
            _VolumeParams.FileSystemName = FileSystemName;
        }

        private VolumeParams _VolumeParams;
        private IntPtr _FileSystem;
    }

}
