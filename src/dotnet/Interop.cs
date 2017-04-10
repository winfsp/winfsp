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
using System.IO;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.AccessControl;

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
                int Size = null != Value ? Value.Length : 0;
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
                int Size = null != Value ? Value.Length : 0;
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

        public unsafe void SetVolumeLabel(String Value)
        {
            fixed (UInt16 *P = VolumeLabel)
            {
                int Size = null != Value ? Value.Length : 0;
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
    internal struct OpenFileInfo
    {
        internal FileInfo FileInfo;
        internal IntPtr NormalizedName;
        internal UInt16 NormalizedNameSize;

        internal unsafe void SetNormalizedName(String Value)
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
        internal const int FileNameBufSize = 255;
        internal static int FileNameBufOffset = (int)Marshal.OffsetOf(typeof(DirInfo), "FileNameBuf");

        internal UInt16 Size;
        internal FileInfo FileInfo;
        internal unsafe fixed Byte Padding[24];
        //internal unsafe fixed UInt16 FileNameBuf[];
        internal unsafe fixed UInt16 FileNameBuf[FileNameBufSize];

        public unsafe void SetFileNameBuf(String Value)
        {
            fixed (UInt16 *P = FileNameBuf)
            {
                int Size = null != Value ? Value.Length : 0;
                if (Size > FileNameBufSize)
                    Size = FileNameBufSize;
                for (int I = 0; Size > I; I++)
                    P[I] = Value[I];
                this.Size = (UInt16)(FileNameBufOffset + Size * 2);
            }
        }
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
                ref OpenFileInfo OpenFileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Open(
                IntPtr FileSystem,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                UInt32 CreateOptions,
                UInt32 GrantedAccess,
                ref FullContext FullContext,
                ref OpenFileInfo OpenFileInfo);
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
        internal Proto.GetStreamInfo GetStreamInfo;
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
                IntPtr Interface,
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
            internal delegate IntPtr FspFileSystemMountPointF(
                IntPtr FileSystem);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspFileSystemSetOperationGuardStrategyF(
                IntPtr FileSystem,
                Int32 GuardStrategy);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspFileSystemSetDebugLogF(
                IntPtr FileSystem,
                UInt32 DebugLog);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Boolean FspFileSystemAddDirInfo(
                IntPtr DirInfo,
                IntPtr Buffer,
                UInt32 Length,
                out UInt32 PBytesTransferred);
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
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemCanReplaceReparsePoint(
                IntPtr CurrentReparseData,
                UIntPtr CurrentReparseDataSize,
                IntPtr ReplaceReparseData,
                UIntPtr ReplaceReparseDataSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Boolean FspFileSystemAddStreamInfo(
                IntPtr StreamInfo,
                IntPtr Buffer,
                UInt32 Length,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Boolean FspFileSystemAcquireDirectoryBuffer(
                ref IntPtr PDirBuffer,
                Boolean Reset,
                out Int32 PResult);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Boolean FspFileSystemFillDirectoryBuffer(
                ref IntPtr PDirBuffer,
                ref DirInfo DirInfo,
                out Int32 PResult);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspFileSystemReleaseDirectoryBuffer(
                ref IntPtr PDirBuffer);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspFileSystemReadDirectoryBuffer(
                ref IntPtr PDirBuffer,
                [MarshalAs(UnmanagedType.LPWStr)] String Marker,
                IntPtr Buffer,
                UInt32 Length,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspFileSystemDeleteDirectoryBuffer(
                ref IntPtr PDirBuffer);

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
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspDebugLogSetHandle(
                IntPtr Handle);

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
        internal static Proto.FspFileSystemMountPointF FspFileSystemMountPoint;
        internal static Proto.FspFileSystemSetOperationGuardStrategyF FspFileSystemSetOperationGuardStrategy;
        internal static Proto.FspFileSystemSetDebugLogF FspFileSystemSetDebugLog;
        internal static Proto.FspFileSystemAddDirInfo _FspFileSystemAddDirInfo;
        internal static Proto.FspFileSystemFindReparsePoint FspFileSystemFindReparsePoint;
        internal static Proto.FspFileSystemResolveReparsePoints FspFileSystemResolveReparsePoints;
        internal static Proto.FspFileSystemCanReplaceReparsePoint FspFileSystemCanReplaceReparsePoint;
        internal static Proto.FspFileSystemAddStreamInfo FspFileSystemAddStreamInfo;
        internal static Proto.FspFileSystemAcquireDirectoryBuffer FspFileSystemAcquireDirectoryBuffer;
        internal static Proto.FspFileSystemFillDirectoryBuffer FspFileSystemFillDirectoryBuffer;
        internal static Proto.FspFileSystemReleaseDirectoryBuffer FspFileSystemReleaseDirectoryBuffer;
        internal static Proto.FspFileSystemReadDirectoryBuffer FspFileSystemReadDirectoryBuffer;
        internal static Proto.FspFileSystemDeleteDirectoryBuffer FspFileSystemDeleteDirectoryBuffer;
        internal static Proto.FspServiceCreate FspServiceCreate;
        internal static Proto.FspServiceDelete FspServiceDelete;
        internal static Proto.FspServiceAllowConsoleMode FspServiceAllowConsoleMode;
        internal static Proto.FspServiceRequestTime FspServiceRequestTime;
        internal static Proto.FspServiceSetExitCode FspServiceSetExitCode;
        internal static Proto.FspServiceGetExitCode FspServiceGetExitCode;
        internal static Proto.FspServiceLoop FspServiceLoop;
        internal static Proto.FspServiceStop FspServiceStop;
        internal static Proto.FspServiceLog FspServiceLog;
        internal static Proto.FspVersion FspVersion;
        internal static Proto.FspNtStatusFromWin32 FspNtStatusFromWin32;
        internal static Proto.FspWin32FromNtStatus FspWin32FromNtStatus;
        internal static Proto.FspDebugLogSetHandle FspDebugLogSetHandle;

        internal static unsafe Int32 FspFileSystemSetMountPointEx(
            IntPtr FileSystem,
            String MountPoint,
            Byte[] SecurityDescriptor)
        {
            if (null != SecurityDescriptor)
            {
                fixed (Byte *P = SecurityDescriptor)
                    return _FspFileSystemSetMountPointEx(FileSystem, MountPoint, (IntPtr)P);
            }
            else
                return _FspFileSystemSetMountPointEx(FileSystem, MountPoint, IntPtr.Zero);
        }
        internal static unsafe Boolean FspFileSystemAddDirInfo(
            ref DirInfo DirInfo,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            fixed (DirInfo *P = &DirInfo)
                return _FspFileSystemAddDirInfo((IntPtr)P, Buffer, Length, out PBytesTransferred);
        }

        internal unsafe static Object GetUserContext(
            IntPtr NativePtr)
        {
            IntPtr UserContext = *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr));
            return IntPtr.Zero != UserContext ? GCHandle.FromIntPtr(UserContext).Target : null;
        }
        internal unsafe static void SetUserContext(
            IntPtr NativePtr,
            Object Obj)
        {
            if (null != Obj)
            {
                Debug.Assert(IntPtr.Zero == *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)));
                GCHandle Handle = GCHandle.Alloc(Obj, GCHandleType.Weak);
                *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)) = (IntPtr)Handle;
            }
            else
            {
                IntPtr UserContext = *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr));
                if (IntPtr.Zero != UserContext)
                {
                    GCHandle.FromIntPtr(UserContext).Free();
                    *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)) = IntPtr.Zero;
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
            Byte[] SecurityDescriptorBytes,
            IntPtr SecurityDescriptor,
            IntPtr PSecurityDescriptorSize)
        {
            if (IntPtr.Zero != PSecurityDescriptorSize)
            {
                if (null != SecurityDescriptorBytes)
                {
                    if (SecurityDescriptorBytes.Length > (int)*(IntPtr *)PSecurityDescriptorSize)
                    {
                        *(IntPtr *)PSecurityDescriptorSize = (IntPtr)SecurityDescriptorBytes.Length;
                        return unchecked((Int32)0x80000005)/*STATUS_BUFFER_OVERFLOW*/;
                    }
                    *(IntPtr *)PSecurityDescriptorSize = (IntPtr)SecurityDescriptorBytes.Length;
                    if (IntPtr.Zero != SecurityDescriptor)
                        Marshal.Copy(SecurityDescriptorBytes, 0,
                            SecurityDescriptor, SecurityDescriptorBytes.Length);
                }
                else
                    *(IntPtr *)PSecurityDescriptorSize = IntPtr.Zero;
            }
            return 0/*STATUS_SUCCESS*/;
        }
        internal static Byte[] MakeSecurityDescriptor(
            IntPtr SecurityDescriptor)
        {
            if (IntPtr.Zero != SecurityDescriptor)
            {
                Byte[] SecurityDescriptorBytes = new Byte[GetSecurityDescriptorLength(SecurityDescriptor)];
                Marshal.Copy(SecurityDescriptor,
                    SecurityDescriptorBytes, 0, SecurityDescriptorBytes.Length);
                return SecurityDescriptorBytes;
            }
            else
                return null;
        }

        internal static Int32 SetDebugLogFile(String FileName)
        {
            IntPtr Handle;
            if ("-" == FileName)
                Handle = GetStdHandle(unchecked((UInt32)(-12))/*STD_ERROR_HANDLE*/);
            else
                Handle = CreateFileW(
                    FileName,
                    (UInt32)FileSystemRights.AppendData,
                    (UInt32)(FileShare.Read | FileShare.Write),
                    IntPtr.Zero,
                    (UInt32)FileMode.OpenOrCreate,
                    (UInt32)FileAttributes.Normal,
                    IntPtr.Zero);
            if ((IntPtr)(-1) == Handle)
                return FspNtStatusFromWin32((UInt32)Marshal.GetLastWin32Error());
            Api.FspDebugLogSetHandle(Handle);
            return 0/*STATUS_SUCCESS*/;
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
                {
                    DllPath = Path.Combine(DllPath, Path.Combine("bin", DllName));
                    Module = LoadLibraryW(DllPath);
                }
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
            FspFileSystemMountPoint = GetEntryPoint<Proto.FspFileSystemMountPointF>(Module);
            FspFileSystemSetOperationGuardStrategy = GetEntryPoint<Proto.FspFileSystemSetOperationGuardStrategyF>(Module);
            FspFileSystemSetDebugLog = GetEntryPoint<Proto.FspFileSystemSetDebugLogF>(Module);
            _FspFileSystemAddDirInfo = GetEntryPoint<Proto.FspFileSystemAddDirInfo>(Module);
            FspFileSystemFindReparsePoint = GetEntryPoint<Proto.FspFileSystemFindReparsePoint>(Module);
            FspFileSystemResolveReparsePoints = GetEntryPoint<Proto.FspFileSystemResolveReparsePoints>(Module);
            FspFileSystemCanReplaceReparsePoint = GetEntryPoint<Proto.FspFileSystemCanReplaceReparsePoint>(Module);
            FspFileSystemAddStreamInfo = GetEntryPoint<Proto.FspFileSystemAddStreamInfo>(Module);
            FspFileSystemAcquireDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemAcquireDirectoryBuffer>(Module);
            FspFileSystemFillDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemFillDirectoryBuffer>(Module);
            FspFileSystemReleaseDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemReleaseDirectoryBuffer>(Module);
            FspFileSystemReadDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemReadDirectoryBuffer>(Module);
            FspFileSystemDeleteDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemDeleteDirectoryBuffer>(Module);
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
            FspDebugLogSetHandle = GetEntryPoint<Proto.FspDebugLogSetHandle>(Module);
        }
        static Api()
        {
#if DEBUG
            if (Debugger.IsAttached)
                Debugger.Break();
#endif
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
        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr GetStdHandle(UInt32 nStdHandle);
        [DllImport("kernel32.dll", CallingConvention = CallingConvention.StdCall, SetLastError = true)]
        private static extern IntPtr CreateFileW(
            [MarshalAs(UnmanagedType.LPWStr)] String lpFileName,
            UInt32 dwDesiredAccess,
            UInt32 dwShareMode,
            IntPtr lpSecurityAttributes,
            UInt32 dwCreationDisposition,
            UInt32 dwFlagsAndAttributes,
            IntPtr hTemplateFile);
    }

}
