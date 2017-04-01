/**
 * @file dotnet/Interop.cs
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
using System.Security;

namespace Fsp.Interop
{

    [StructLayout(LayoutKind.Sequential)]
    internal struct VolumeParams
    {
        internal UInt16 Version;
        internal UInt16 SectorSize;
        internal UInt16 SectorsPerAllocationUnit;
        internal UInt16 MaxComponentLength;
        internal UInt64 VolumeCreationTime;
        internal UInt32 VolumeSerialNumber;
        internal UInt32 TransactTimeout;
        internal UInt32 IrpTimeout;
        internal UInt32 IrpCapacity;
        internal UInt32 FileInfoTimeout;
        internal UInt32 Flags;
        internal unsafe fixed UInt16 Prefix[192];
        internal unsafe fixed UInt16 FileSystemName[16];

        internal const UInt32 CaseSensitiveSearch = 0x00000001;
        internal const UInt32 CasePreservedNames = 0x00000002;
        internal const UInt32 UnicodeOnDisk = 0x00000004;
        internal const UInt32 PersistentAcls = 0x00000008;
        internal const UInt32 ReparsePoints = 0x00000010;
        internal const UInt32 ReparsePointsAccessCheck = 0x00000020;
        internal const UInt32 NamedStreams = 0x00000040;
        internal const UInt32 HardLinks = 0x00000080;
        internal const UInt32 ExtendedAttributes = 0x00000100;
        internal const UInt32 ReadOnlyVolume = 0x00000200;
        internal const UInt32 PostCleanupWhenModifiedOnly = 0x00000400;
        internal const UInt32 PassQueryDirectoryPattern = 0x00000800;
        internal const UInt32 AlwaysUseDoubleBuffering = 0x00001000;
        internal const UInt32 UmFileContextIsUserContext2 = 0x00010000;
        internal const UInt32 UmFileContextIsFullContext = 0x00020000;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct VolumeInfo
    {
        internal UInt64 TotalSize;
        internal UInt64 FreeSize;
        internal UInt16 VolumeLabelLength;
        internal unsafe fixed UInt16 VolumeLabel[32];
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FileInfo
    {
        internal UInt32 FileAttributes;
        internal UInt32 ReparseTag;
        internal UInt64 AllocationSize;
        internal UInt64 FileSize;
        internal UInt64 CreationTime;
        internal UInt64 LastAccessTime;
        internal UInt64 LastWriteTime;
        internal UInt64 ChangeTime;
        internal UInt64 IndexNumber;
        internal UInt32 HardLinks;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct OpenFileInfo
    {
        internal FileInfo FileInfo;
        [MarshalAs(UnmanagedType.LPWStr)]
        internal String NormalizedName;
        internal UInt16 NormalizedNameSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct DirInfo
    {
        internal UInt16 Size;
        internal FileInfo FileInfo;
        internal unsafe fixed Byte Padding[24];
        //internal unsafe fixed UInt16 FileNameBuf[];
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct IoStatusBlock
    {
        internal IntPtr Status;
        internal IntPtr Information;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FileSystemInterface
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 GetVolumeInfo(
            IntPtr FileSystem,
            out VolumeInfo VolumeInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 SetVolumeLabel(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String VolumeLabel,
            out VolumeInfo VolumeInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 GetSecurityByName(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            out UInt32 PFileAttributes/* or ReparsePointIndex */,
            IntPtr SecurityDescriptor,
            out UIntPtr PSecurityDescriptorSize);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 Create(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            UInt32 FileAttributes,
            IntPtr SecurityDescriptor,
            UInt64 AllocationSize,
            IntPtr PFileContext,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 Open(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            IntPtr PFileContext,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 Overwrite(
            IntPtr FileSystem,
            IntPtr FileContext,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate void Cleanup(
            IntPtr FileSystem,
            IntPtr FileContext,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            UInt32 Flags);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate void Close(
            IntPtr FileSystem,
            IntPtr FileContext);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 Read(
            IntPtr FileSystem,
            IntPtr FileContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 Write(
            IntPtr FileSystem,
            IntPtr FileContext,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            Boolean WriteToEndOfFile,
            Boolean ConstrainedIo,
            out UInt32 PBytesTransferred,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 Flush(
            IntPtr FileSystem,
            IntPtr FileContext,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 GetFileInfo(
            IntPtr FileSystem,
            IntPtr FileContext,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 SetBasicInfo(
            IntPtr FileSystem,
            IntPtr FileContext,
            UInt32 FileAttributes,
            UInt64 CreationTime,
            UInt64 LastAccessTime,
            UInt64 LastWriteTime,
            UInt64 ChangeTime,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 SetFileSize(
            IntPtr FileSystem,
            IntPtr FileContext,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 CanDelete(
            IntPtr FileSystem,
            IntPtr FileContext,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 Rename(
            IntPtr FileSystem,
            IntPtr FileContext,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            [MarshalAs(UnmanagedType.LPWStr)] String NewFileName,
            Boolean ReplaceIfExists);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 GetSecurity(
            IntPtr FileSystem,
            IntPtr FileContext,
            IntPtr SecurityDescriptor,
            out UIntPtr PSecurityDescriptorSize);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 SetSecurity(
            IntPtr FileSystem,
            IntPtr FileContext,
            UInt32 SecurityInformation,
            IntPtr ModificationDescriptor);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 ReadDirectory(
            IntPtr FileSystem,
            IntPtr FileContext,
            [MarshalAs(UnmanagedType.LPWStr)] String Pattern,
            [MarshalAs(UnmanagedType.LPWStr)] String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 ResolveReparsePoints(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            UInt32 ReparsePointIndex,
            Boolean ResolveLastPathComponent,
            out IoStatusBlock PIoStatus,
            IntPtr Buffer,
            out UIntPtr PSize);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 GetReparsePoint(
            IntPtr FileSystem,
            IntPtr FileContext,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            IntPtr Buffer,
            out UIntPtr PSize);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 SetReparsePoint(
            IntPtr FileSystem,
            IntPtr FileContext,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            IntPtr Buffer,
            UIntPtr Size);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 DeleteReparsePoint(
            IntPtr FileSystem,
            IntPtr FileContext,
            [MarshalAs(UnmanagedType.LPWStr)] String FileName,
            IntPtr Buffer,
            UIntPtr Size);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        internal delegate Int32 GetStreamInfo(
            IntPtr FileSystem,
            IntPtr FileContext,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred);

        internal unsafe fixed ulong Reserved[40];
            /* cannot use IntPtr with fixed; on 32-bit struct will be too long, but that's ok */
    }

    [SuppressUnmanagedCodeSecurity]
    internal static class Native
    {
        internal const String dllname = "winfsp.dll";

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemPreflight(
            [MarshalAs(UnmanagedType.LPWStr)] String DevicePath,
            [MarshalAs(UnmanagedType.LPWStr)] String MountPoint);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemCreate(
            [MarshalAs(UnmanagedType.LPWStr)] String DevicePath,
            ref VolumeParams VolumeParams,
            ref FileSystemInterface Interface,
            out IntPtr PFileSystem);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void FspFileSystemDelete(IntPtr FileSystem);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemSetMountPoint(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String MountPoint);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemSetMountPointEx(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String MountPoint,
            IntPtr SecurityDescriptor);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemRemoveMountPoint(
            IntPtr FileSystem);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemStartDispatcher(
            IntPtr FileSystem,
            UInt32 ThreadCount);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemStopDispatcher(
            IntPtr FileSystem);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspVersion(
            out UInt32 PVersion);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspNtStatusFromWin32(
            UInt32 Error);

        [DllImport(dllname, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 FspWin32FromNtStatus(
            Int32 Status);
    }

}
