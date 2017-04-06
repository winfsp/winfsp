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
                VolumeLabelLength = (UInt16)Size;
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct FileInfo
    {
        public UInt32 FileAttributes;
        public UInt32 ReparseTag;
        public UInt64 AllocationSize;
        public UInt64 FileSize;
        public UInt64 CreationTime;
        public UInt64 LastAccessTime;
        public UInt64 LastWriteTime;
        public UInt64 ChangeTime;
        public UInt64 IndexNumber;
        public UInt32 HardLinks;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct OpenFileInfo
    {
        public FileInfo FileInfo;
        public IntPtr NormalizedName;
        public UInt16 NormalizedNameSize;

        public unsafe void SetNormalizedName(String Value)
        {
            UInt16 *P = (UInt16 *)NormalizedName;
            int Size = Value.Length;
            if (Size > NormalizedNameSize)
                Size = NormalizedNameSize;
            for (int I = 0; Size > I; I++)
                P[I] = Value[I];
            NormalizedNameSize = (UInt16)Size;
        }
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
    internal struct StreamInfo
    {
        internal UInt16 Size;
        internal UInt64 StreamSize;
        internal UInt64 StreamAllocationSize;
        //internal unsafe fixed UInt16 StreamNameBuf[];
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FullContext
    {
        internal UInt64 UserContext;
        internal UInt64 UserContext2;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct IoStatusBlock
    {
        public IntPtr Status;
        public IntPtr Information;
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
                ref FullContext FullContext,
                out OpenFileInfo OpenFileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Open(
                IntPtr FileSystem,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                UInt32 CreateOptions,
                UInt32 GrantedAccess,
                ref FullContext FullContext,
                out OpenFileInfo OpenFileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Overwrite(
                IntPtr FileSystem,
                ref FullContext FullContext,
                UInt32 FileAttributes,
                Boolean ReplaceFileAttributes,
                UInt64 AllocationSize,
                out FileInfo FileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void Cleanup(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                UInt32 Flags);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void Close(
                IntPtr FileSystem,
                ref FullContext FullContext);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Read(
                IntPtr FileSystem,
                ref FullContext FullContext,
                IntPtr Buffer,
                UInt64 Offset,
                UInt32 Length,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Write(
                IntPtr FileSystem,
                ref FullContext FullContext,
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
                ref FullContext FullContext,
                out FileInfo FileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetFileInfo(
                IntPtr FileSystem,
                ref FullContext FullContext,
                out FileInfo FileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 SetBasicInfo(
                IntPtr FileSystem,
                ref FullContext FullContext,
                UInt32 FileAttributes,
                UInt64 CreationTime,
                UInt64 LastAccessTime,
                UInt64 LastWriteTime,
                UInt64 ChangeTime,
                out FileInfo FileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 SetFileSize(
                IntPtr FileSystem,
                ref FullContext FullContext,
                UInt64 NewSize,
                Boolean SetAllocationSize,
                out FileInfo FileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 CanDelete(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Rename(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                [MarshalAs(UnmanagedType.LPWStr)] String NewFileName,
                Boolean ReplaceIfExists);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetSecurity(
                IntPtr FileSystem,
                ref FullContext FullContext,
                IntPtr SecurityDescriptor,
                IntPtr PSecurityDescriptorSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 SetSecurity(
                IntPtr FileSystem,
                ref FullContext FullContext,
                UInt32 SecurityInformation,
                IntPtr ModificationDescriptor);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 ReadDirectory(
                IntPtr FileSystem,
                ref FullContext FullContext,
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
                ref UIntPtr PSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetReparsePoint(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                IntPtr Buffer,
                out UIntPtr PSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 SetReparsePoint(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                IntPtr Buffer,
                UIntPtr Size);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 DeleteReparsePoint(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                IntPtr Buffer,
                UIntPtr Size);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetStreamInfo(
                IntPtr FileSystem,
                ref FullContext FullContext,
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
        internal struct Proto
        {
            /* FileSystem */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemPreflight(
                [MarshalAs(UnmanagedType.LPWStr)] String DevicePath,
                [MarshalAs(UnmanagedType.LPWStr)] String MountPoint);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemCreate(
                [MarshalAs(UnmanagedType.LPWStr)] String DevicePath,
                ref VolumeParams VolumeParams,
                ref FileSystemInterface Interface,
                out IntPtr PFileSystem);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspFileSystemDelete(
                IntPtr FileSystem);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemSetMountPoint(
                IntPtr FileSystem,
                [MarshalAs(UnmanagedType.LPWStr)] String MountPoint);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemSetMountPointEx(
                IntPtr FileSystem,
                [MarshalAs(UnmanagedType.LPWStr)] String MountPoint,
                IntPtr SecurityDescriptor);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemRemoveMountPoint(
                IntPtr FileSystem);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemStartDispatcher(
                IntPtr FileSystem,
                UInt32 ThreadCount);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemStopDispatcher(
                IntPtr FileSystem);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemFindReparsePoint(
                IntPtr FileSystem,
                GetReparsePointByName GetReparsePointByName,
                IntPtr Context,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                out UInt32 PReparsePointIndex);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemResolveReparsePoints(
                IntPtr FileSystem,
                GetReparsePointByName GetReparsePointByName,
                IntPtr Context,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                UInt32 ReparsePointIndex,
                Boolean ResolveLastPathComponent,
                out IoStatusBlock PIoStatus,
                IntPtr Buffer,
                ref UIntPtr PSize);

            /* Service */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspServiceCreate(
                [MarshalAs(UnmanagedType.LPWStr)] String ServiceName,
                ServiceStart OnStart,
                ServiceStop OnStop,
                ServiceControl OnControl,
                out IntPtr PService);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspServiceDelete(
                IntPtr Service);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspServiceAllowConsoleMode(
                IntPtr Service);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspServiceRequestTime(
                IntPtr Service,
                UInt32 Time);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspServiceSetExitCode(
                IntPtr Service,
                UInt32 ExitCode);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate UInt32 FspServiceGetExitCode(
                IntPtr Service);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspServiceLoop(
                IntPtr Service);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspServiceStop(
                IntPtr Service);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspServiceLog(
                UInt32 Type,
                [MarshalAs(UnmanagedType.LPWStr)] String Format,
                [MarshalAs(UnmanagedType.LPWStr)] String Message);

            /* utility */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspVersion(
                out UInt32 PVersion);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspNtStatusFromWin32(
                UInt32 Error);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate UInt32 FspWin32FromNtStatus(
                Int32 Status);

            /* callbacks */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetReparsePointByName(
                IntPtr FileSystem,
                IntPtr Context,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                Boolean IsDirectory,
                IntPtr Buffer,
                ref UIntPtr PSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 ServiceStart(
                IntPtr Service,
                UInt32 Argc,
                [MarshalAs(UnmanagedType.LPArray,
                    ArraySubType = UnmanagedType.LPWStr, SizeParamIndex = 1)]
                String[] Argv);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 ServiceStop(
                IntPtr Service);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 ServiceControl(
                IntPtr Service,
                UInt32 Control,
                UInt32 EventType,
                IntPtr EventData);
        }

        internal static Proto.FspFileSystemPreflight FspFileSystemPreflight;
        internal static Proto.FspFileSystemCreate FspFileSystemCreate;
        internal static Proto.FspFileSystemDelete FspFileSystemDelete;
        internal static Proto.FspFileSystemSetMountPoint FspFileSystemSetMountPoint;
        internal static Proto.FspFileSystemSetMountPointEx _FspFileSystemSetMountPointEx;
        internal static Proto.FspFileSystemRemoveMountPoint FspFileSystemRemoveMountPoint;
        internal static Proto.FspFileSystemStartDispatcher FspFileSystemStartDispatcher;
        internal static Proto.FspFileSystemStopDispatcher FspFileSystemStopDispatcher;
        internal static Proto.FspFileSystemFindReparsePoint FspFileSystemFindReparsePoint;
        internal static Proto.FspFileSystemResolveReparsePoints FspFileSystemResolveReparsePoints;
        internal static Proto.FspVersion FspVersion;
        internal static Proto.FspServiceCreate FspServiceCreate;
        internal static Proto.FspServiceDelete FspServiceDelete;
        internal static Proto.FspServiceAllowConsoleMode FspServiceAllowConsoleMode;
        internal static Proto.FspServiceRequestTime FspServiceRequestTime;
        internal static Proto.FspServiceSetExitCode FspServiceSetExitCode;
        internal static Proto.FspServiceGetExitCode FspServiceGetExitCode;
        internal static Proto.FspServiceLoop FspServiceLoop;
        internal static Proto.FspServiceStop FspServiceStop;
        internal static Proto.FspServiceLog FspServiceLog;
        internal static Proto.FspNtStatusFromWin32 FspNtStatusFromWin32;
        internal static Proto.FspWin32FromNtStatus FspWin32FromNtStatus;

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
                    return _FspFileSystemSetMountPointEx(FileSystem, MountPoint, (IntPtr)P);
            }
            else
                return _FspFileSystemSetMountPointEx(FileSystem, MountPoint, IntPtr.Zero);
        }

        internal unsafe static Object GetUserContext(
            IntPtr NativePtr)
        {
            IntPtr UserContext = *(IntPtr *)((byte *)NativePtr + sizeof(IntPtr));
            return IntPtr.Zero != UserContext ? GCHandle.FromIntPtr(UserContext).Target : null;
        }
        internal unsafe static void SetUserContext(
            IntPtr NativePtr,
            Object Obj)
        {
            if (null != Obj)
            {
                Debug.Assert(IntPtr.Zero == *(IntPtr *)((byte *)NativePtr + sizeof(IntPtr)));
                GCHandle Handle = GCHandle.Alloc(Obj, GCHandleType.Weak);
                *(IntPtr *)((byte *)NativePtr + sizeof(IntPtr)) = (IntPtr)Handle;
            }
            else
            {
                IntPtr UserContext = *(IntPtr *)((byte *)NativePtr + sizeof(IntPtr));
                if (IntPtr.Zero != UserContext)
                {
                    GCHandle.FromIntPtr(UserContext).Free();
                    *(IntPtr *)((byte *)NativePtr + sizeof(IntPtr)) = IntPtr.Zero;
                }
            }
        }

        internal static void GetFullContext(ref FullContext FullContext,
            out Object FileNode, out Object FileDesc)
        {
            FileNode = 0 != FullContext.UserContext ?
                GCHandle.FromIntPtr((IntPtr)FullContext.UserContext).Target : null;
            FileDesc = 0 != FullContext.UserContext2 ?
                GCHandle.FromIntPtr((IntPtr)FullContext.UserContext2).Target : null;
        }
        internal static void SetFullContext(ref FullContext FullContext,
            Object FileNode, Object FileDesc)
        {
            if (null != FileNode)
            {
                Debug.Assert(0 == FullContext.UserContext);
                GCHandle Handle = GCHandle.Alloc(FileNode, GCHandleType.Normal);
                FullContext.UserContext = (UInt64)(IntPtr)Handle;
            }
            else
            {
                if (0 != FullContext.UserContext)
                {
                    GCHandle.FromIntPtr((IntPtr)FullContext.UserContext).Free();
                    FullContext.UserContext = 0;
                }
            }
            if (null != FileDesc)
            {
                Debug.Assert(0 == FullContext.UserContext2);
                GCHandle Handle = GCHandle.Alloc(FileDesc, GCHandleType.Normal);
                FullContext.UserContext2 = (UInt64)(IntPtr)Handle;
            }
            else
            {
                if (0 != FullContext.UserContext2)
                {
                    GCHandle.FromIntPtr((IntPtr)FullContext.UserContext2).Free();
                    FullContext.UserContext2 = 0;
                }
            }
        }

        internal unsafe static Int32 CopySecurityDescriptor(
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
                    if (GenericSecurityDescriptor.BinaryLength > (int)*(IntPtr *)PSecurityDescriptorSize)
                    {
                        *(IntPtr *)PSecurityDescriptorSize = (IntPtr)GenericSecurityDescriptor.BinaryLength;
                        return unchecked((Int32)0x80000005)/*STATUS_BUFFER_OVERFLOW*/;
                    }
                    *(IntPtr *)PSecurityDescriptorSize = (IntPtr)GenericSecurityDescriptor.BinaryLength;
                    if (IntPtr.Zero != SecurityDescriptor)
                    {
                        byte[] Bytes = new byte[GenericSecurityDescriptor.BinaryLength];
                        GenericSecurityDescriptor.GetBinaryForm(Bytes, 0);
                        Marshal.Copy(Bytes, 0, SecurityDescriptor, Bytes.Length);
                    }
                }
                else
                    *(IntPtr *)PSecurityDescriptorSize = IntPtr.Zero;
            }
            return 0/*STATUS_SUCCESS*/;
        }
        internal static Object MakeSecurityDescriptor(
            IntPtr SecurityDescriptor)
        {
            if (IntPtr.Zero != SecurityDescriptor)
            {
                byte[] Bytes = new byte[GetSecurityDescriptorLength(SecurityDescriptor)];
                Marshal.Copy(SecurityDescriptor, Bytes, 0, Bytes.Length);
                return new RawSecurityDescriptor(Bytes, 0);
            }
            else
                return null;
        }

        /* initialization */
        private static IntPtr LoadDll()
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
                if (null != DllPath)
                    Module = LoadLibraryW(DllPath + DllName);
                if (IntPtr.Zero == Module)
                    throw new DllNotFoundException("cannot load " + DllName);
            }
            return Module;
        }
        private static T GetEntryPoint<T>(IntPtr Module)
        {
            try
            {
                return (T)(object)Marshal.GetDelegateForFunctionPointer(
                    GetProcAddress(Module, typeof(T).Name), typeof(T));
            }
            catch (ArgumentNullException)
            {
                throw new EntryPointNotFoundException("cannot get entry point " + typeof(T).Name);
            }
        }
        private static void LoadProto(IntPtr Module)
        {
            FspFileSystemPreflight = GetEntryPoint<Proto.FspFileSystemPreflight>(Module);
            FspFileSystemCreate = GetEntryPoint<Proto.FspFileSystemCreate>(Module);
            FspFileSystemDelete = GetEntryPoint<Proto.FspFileSystemDelete>(Module);
            FspFileSystemSetMountPoint = GetEntryPoint<Proto.FspFileSystemSetMountPoint>(Module);
            _FspFileSystemSetMountPointEx = GetEntryPoint<Proto.FspFileSystemSetMountPointEx>(Module);
            FspFileSystemRemoveMountPoint = GetEntryPoint<Proto.FspFileSystemRemoveMountPoint>(Module);
            FspFileSystemStartDispatcher = GetEntryPoint<Proto.FspFileSystemStartDispatcher>(Module);
            FspFileSystemStopDispatcher = GetEntryPoint<Proto.FspFileSystemStopDispatcher>(Module);
            FspFileSystemFindReparsePoint = GetEntryPoint<Proto.FspFileSystemFindReparsePoint>(Module);
            FspFileSystemResolveReparsePoints = GetEntryPoint<Proto.FspFileSystemResolveReparsePoints>(Module);
            FspServiceCreate = GetEntryPoint<Proto.FspServiceCreate>(Module);
            FspServiceDelete = GetEntryPoint<Proto.FspServiceDelete>(Module);
            FspServiceAllowConsoleMode = GetEntryPoint<Proto.FspServiceAllowConsoleMode>(Module);
            FspServiceRequestTime = GetEntryPoint<Proto.FspServiceRequestTime>(Module);
            FspServiceSetExitCode = GetEntryPoint<Proto.FspServiceSetExitCode>(Module);
            FspServiceGetExitCode = GetEntryPoint<Proto.FspServiceGetExitCode>(Module);
            FspServiceLoop = GetEntryPoint<Proto.FspServiceLoop>(Module);
            FspServiceStop = GetEntryPoint<Proto.FspServiceStop>(Module);
            FspServiceLog = GetEntryPoint<Proto.FspServiceLog>(Module);
            FspVersion = GetEntryPoint<Proto.FspVersion>(Module);
            FspNtStatusFromWin32 = GetEntryPoint<Proto.FspNtStatusFromWin32>(Module);
            FspWin32FromNtStatus = GetEntryPoint<Proto.FspWin32FromNtStatus>(Module);
        }
        static Api()
        {
            LoadProto(LoadDll());
        }

        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr LoadLibraryW(
            [MarshalAs(UnmanagedType.LPWStr)] String DllName);
        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr GetProcAddress(
            IntPtr hModule,
            [MarshalAs(UnmanagedType.LPStr)] String lpProcName);
        [DllImport("advapi32.dll", CallingConvention = CallingConvention.StdCall)]
        private static extern UInt32 GetSecurityDescriptorLength(IntPtr SecurityDescriptor);
    }

}
