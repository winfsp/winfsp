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
using System.Diagnostics;
using System.Security.AccessControl;
using System.Runtime.InteropServices;
using System.Security;

namespace Fsp.Interop
{

    [StructLayout(LayoutKind.Sequential)]
    internal struct VolumeParams
    {
        /* const */
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
        internal const int PrefixSize = 192;
        internal const int FileSystemNameSize = 16;

        /* fields */
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
        internal unsafe fixed UInt16 Prefix[PrefixSize];
        internal unsafe fixed UInt16 FileSystemName[FileSystemNameSize];

        /* helpers */
        internal unsafe void SetPrefix(String Value)
        {
            fixed (UInt16 *P = Prefix)
            {
                int Size = Value.Length;
                if (Size > PrefixSize - 1)
                    Size = PrefixSize - 1;
                for (int I = 0; Size > I; I++)
                    P[I] = Value[I];
                P[Size] = 0;
            }
        }
        internal unsafe void SetFileSystemName(String Value)
        {
            fixed (UInt16 *P = FileSystemName)
            {
                int Size = Value.Length;
                if (Size > FileSystemNameSize - 1)
                    Size = FileSystemNameSize - 1;
                for (int I = 0; Size > I; I++)
                    P[I] = Value[I];
                P[Size] = 0;
            }
        }
        internal unsafe Boolean IsPrefixEmpty()
        {
            fixed (UInt16 *P = Prefix)
                return 0 == *P;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct VolumeInfo
    {
        internal const int VolumeLabelSize = 32;

        public UInt64 TotalSize;
        public UInt64 FreeSize;
        internal UInt16 VolumeLabelLength;
        internal unsafe fixed UInt16 VolumeLabel[VolumeLabelSize];

        internal unsafe void SetVolumeLabel(String Value)
        {
            fixed (UInt16 *P = VolumeLabel)
            {
                int Size = Value.Length;
                if (Size > VolumeLabelSize)
                    Size = VolumeLabelSize;
                for (int I = 0; Size > I; I++)
                    P[I] = Value[I];
                VolumeLabelLength = VolumeLabelSize;
            }
        }
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
        internal IntPtr NormalizedName;
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
        internal struct Proto
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
                IntPtr PFileAttributes/* or ReparsePointIndex */,
                IntPtr SecurityDescriptor,
                IntPtr PSecurityDescriptorSize);
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
                IntPtr PSecurityDescriptorSize);
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
        }

        internal Proto.GetVolumeInfo GetVolumeInfo;
        internal Proto.SetVolumeLabel SetVolumeLabel;
        internal Proto.GetSecurityByName GetSecurityByName;
        internal Proto.Create Create;
        internal Proto.Open Open;
        internal Proto.Overwrite Overwrite;
        internal Proto.Cleanup Cleanup;
        internal Proto.Close Close;
        internal Proto.Read Read;
        internal Proto.Write Write;
        internal Proto.Flush Flush;
        internal Proto.GetFileInfo GetFileInfo;
        internal Proto.SetBasicInfo SetBasicInfo;
        internal Proto.SetFileSize SetFileSize;
        internal Proto.CanDelete CanDelete;
        internal Proto.Rename Rename;
        internal Proto.GetSecurity GetSecurity;
        internal Proto.SetSecurity SetSecurity;
        internal Proto.ReadDirectory ReadDirectory;
        internal Proto.ResolveReparsePoints ResolveReparsePoints;
        internal Proto.GetReparsePoint GetReparsePoint;
        internal Proto.SetReparsePoint SetReparsePoint;
        internal Proto.DeleteReparsePoint DeleteReparsePoint;
        internal Proto.DeleteReparsePoint GetStreamInfo;
        internal unsafe fixed long/*IntPtr*/ Reserved[40];
            /* NTSTATUS (*Reserved[40])(); */
    }

    [SuppressUnmanagedCodeSecurity]
    internal static class Api
    {
        /* const */
        internal const String DllName = "winfsp.dll";

        /* API */
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemPreflight(
            [MarshalAs(UnmanagedType.LPWStr)] String DevicePath,
            [MarshalAs(UnmanagedType.LPWStr)] String MountPoint);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemCreate(
            [MarshalAs(UnmanagedType.LPWStr)] String DevicePath,
            ref VolumeParams VolumeParams,
            ref FileSystemInterface Interface,
            out IntPtr PFileSystem);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void FspFileSystemDelete(
            IntPtr FileSystem);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemSetMountPoint(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String MountPoint);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemSetMountPointEx(
            IntPtr FileSystem,
            [MarshalAs(UnmanagedType.LPWStr)] String MountPoint,
            IntPtr SecurityDescriptor);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemRemoveMountPoint(
            IntPtr FileSystem);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemStartDispatcher(
            IntPtr FileSystem,
            UInt32 ThreadCount);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspFileSystemStopDispatcher(
            IntPtr FileSystem);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspVersion(
            out UInt32 PVersion);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Int32 FspNtStatusFromWin32(
            UInt32 Error);
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern UInt32 FspWin32FromNtStatus(
            Int32 Status);
        internal static unsafe Int32 FspFileSystemSetMountPointEx(
            IntPtr FileSystem,
            String MountPoint,
            GenericSecurityDescriptor SecurityDescriptor)
        {
            if (null != SecurityDescriptor)
            {
                byte[] Bytes = new byte[SecurityDescriptor.BinaryLength];
                SecurityDescriptor.GetBinaryForm(Bytes, 0);
                fixed (byte *P = Bytes)
                    return FspFileSystemSetMountPointEx(FileSystem, MountPoint, (IntPtr)P);
            }
            else
                return FspFileSystemSetMountPointEx(FileSystem, MountPoint, IntPtr.Zero);
        }
        internal unsafe static Object FspFileSystemGetUserContext(
            IntPtr FileSystem)
        {
            IntPtr UserContext = Marshal.ReadIntPtr(FileSystem, sizeof(IntPtr));
            return IntPtr.Zero != UserContext ? ((GCHandle)UserContext).Target : null;
        }
        internal unsafe static void FspFileSystemSetUserContext(
            IntPtr FileSystem,
            Object Obj)
        {
            if (null != Obj)
            {
                Debug.Assert(IntPtr.Zero == Marshal.ReadIntPtr(FileSystem, sizeof(IntPtr)));
                GCHandle Handle = GCHandle.Alloc(Obj, GCHandleType.Weak);
                Marshal.WriteIntPtr(FileSystem, sizeof(IntPtr), (IntPtr)Handle);
            }
            else
            {
                IntPtr UserContext = Marshal.ReadIntPtr(FileSystem, sizeof(IntPtr));
                if (IntPtr.Zero != UserContext)
                {
                    ((GCHandle)UserContext).Free();
                    Marshal.WriteIntPtr(FileSystem, sizeof(IntPtr), IntPtr.Zero);
                }
            }
        }
        internal static Int32 CopySecurityDescriptor(
            Object SecurityDescriptorObject,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            if (IntPtr.Zero != PSecurityDescriptorSize)
            {
                GenericSecurityDescriptor GenericSecurityDescriptor =
                    SecurityDescriptorObject as GenericSecurityDescriptor;
                if (null != GenericSecurityDescriptor)
                {
                    if (GenericSecurityDescriptor.BinaryLength > Marshal.ReadInt32(PSecurityDescriptorSize))
                    {
                        Marshal.WriteInt32(PSecurityDescriptorSize, GenericSecurityDescriptor.BinaryLength);
                        return unchecked((Int32)0x80000005)/*STATUS_BUFFER_OVERFLOW*/;
                    }
                    Marshal.WriteInt32(PSecurityDescriptorSize, GenericSecurityDescriptor.BinaryLength);
                    if (IntPtr.Zero != SecurityDescriptor)
                    {
                        byte[] Bytes = new byte[GenericSecurityDescriptor.BinaryLength];
                        GenericSecurityDescriptor.GetBinaryForm(Bytes, 0);
                        Marshal.Copy(Bytes, 0, SecurityDescriptor, GenericSecurityDescriptor.BinaryLength);
                    }
                }
                else
                    Marshal.WriteInt32(PSecurityDescriptorSize, 0);
            }
            return 0/*STATUS_SUCCESS*/;
        }

        /* initialization */
        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr LoadLibraryW(
            [MarshalAs(UnmanagedType.LPWStr)] String DllName);
        private static Boolean Load()
        {
            String DllPath = null;
            String DllName = 8 == IntPtr.Size ? "winfsp-x64.dll" : "winfsp-x86.dll";
            String KeyName = 8 == IntPtr.Size ?
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\WinFsp" :
                "HKEY_LOCAL_MACHINE\\Software\\WinFsp";
            IntPtr Module;
            Module = LoadLibraryW(DllName);
            if (IntPtr.Zero == Module)
            {
                DllPath = Microsoft.Win32.Registry.GetValue(KeyName, "InstallDir", null) as String;
                if (null == DllPath)
                    return false;
                Module = LoadLibraryW(DllPath + DllName);
                if (IntPtr.Zero == Module)
                    return false;
            }
            return true;
        }
        static Api()
        {
            if (!Load())
                return;
        }
    }

}
