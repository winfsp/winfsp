/*
 * dotnet/Interop.cs
 *
 * Copyright 2015-2021 Bill Zissimopoulos
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
using System.Diagnostics;
using System.IO;
using System.Reflection;
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
        internal const UInt32 PassQueryDirectoryFileName = 0x00002000;
        internal const UInt32 FlushAndPurgeOnCleanup = 0x00004000;
        internal const UInt32 DeviceControl = 0x00008000;
        internal const UInt32 UmFileContextIsUserContext2 = 0x00010000;
        internal const UInt32 UmFileContextIsFullContext = 0x00020000;
        internal const UInt32 AllowOpenInKernelMode = 0x01000000;
        internal const UInt32 CasePreservedExtendedAttributes = 0x02000000;
        internal const UInt32 WslFeatures = 0x04000000;
        internal const UInt32 RejectIrpPriorToTransact0 = 0x10000000;
        internal const UInt32 SupportsPosixUnlinkRename = 0x20000000;
        internal const int PrefixSize = 192;
        internal const int FileSystemNameSize = 16;

        internal const UInt32 VolumeInfoTimeoutValid = 0x00000001;
        internal const UInt32 DirInfoTimeoutValid = 0x00000002;
        internal const UInt32 SecurityTimeoutValid = 0x00000004;
        internal const UInt32 StreamInfoTimeoutValid = 0x00000008;
        internal const UInt32 EaTimeoutValid = 0x00000010;

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
        internal UInt32 AdditionalFlags;
        internal UInt32 VolumeInfoTimeout;
        internal UInt32 DirInfoTimeout;
        internal UInt32 SecurityTimeout;
        internal UInt32 StreamInfoTimeout;
        internal UInt32 EaTimeout;
        internal UInt32 FsextControlCode;
        internal unsafe fixed UInt32 Reserved32[1];
        internal unsafe fixed UInt64 Reserved64[2];

        internal unsafe String GetPrefix()
        {
            fixed (UInt16 *P = Prefix)
                return Marshal.PtrToStringUni((IntPtr)P);
        }
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
        internal unsafe String GetFileSystemName()
        {
            fixed (UInt16 *P = FileSystemName)
                return Marshal.PtrToStringUni((IntPtr)P);
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

    /// <summary>
    /// Contains volume information about a file system.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct VolumeInfo
    {
        internal const int VolumeLabelSize = 32;

        /// <summary>
        /// Total size of volume in bytes.
        /// </summary>
        public UInt64 TotalSize;
        /// <summary>
        /// Free size of volume in bytes.
        /// </summary>
        public UInt64 FreeSize;
        internal UInt16 VolumeLabelLength;
        internal unsafe fixed UInt16 VolumeLabel[VolumeLabelSize];

        /// <summary>
        /// Sets the volume label.
        /// </summary>
        public unsafe void SetVolumeLabel(String Value)
        {
            fixed (UInt16 *P = VolumeLabel)
            {
                int Size = null != Value ? Value.Length : 0;
                if (Size > VolumeLabelSize)
                    Size = VolumeLabelSize;
                for (int I = 0; Size > I; I++)
                    P[I] = Value[I];
                VolumeLabelLength = (UInt16)(Size * 2);
            }
        }
    }

    /// <summary>
    /// Contains metadata information about a file or directory.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct FileInfo
    {
        /// <summary>
        /// The file or directory attributes.
        /// </summary>
        public UInt32 FileAttributes;
        /// <summary>
        /// The reparse tag of the file or directory.
        /// This value is 0 if the file or directory is not a reparse point.
        /// </summary>
        public UInt32 ReparseTag;
        /// <summary>
        /// The allocation size of the file.
        /// </summary>
        public UInt64 AllocationSize;
        /// <summary>
        /// The file size of the file (end of file).
        /// </summary>
        public UInt64 FileSize;
        /// <summary>
        /// The time that the file or directory was created.
        /// </summary>
        public UInt64 CreationTime;
        /// <summary>
        /// The time that the file or directory was last accessed.
        /// </summary>
        public UInt64 LastAccessTime;
        /// <summary>
        /// The time that the file or direcotry was last modified.
        /// </summary>
        public UInt64 LastWriteTime;
        /// <summary>
        /// The time that the file or directory metadata was last modified.
        /// </summary>
        public UInt64 ChangeTime;
        /// <summary>
        /// A unique identifier that is associated with the file or directory.
        /// Not all file systems support this value.
        /// </summary>
        public UInt64 IndexNumber;
        /// <summary>
        /// The number of hard links.
        /// Not currently implemented. Set to 0.
        /// </summary>
        public UInt32 HardLinks;

        /// <summary>
        /// The extended attribute size of the file.
        /// </summary>
        public UInt32 EaSize
        {
            get { return GetEaSize(); }
            set { SetEaSize(value); }
        }

        internal static int EaSizeOffset =
            (int)Marshal.OffsetOf(typeof(FileInfo), "HardLinks") + 4;
        internal unsafe UInt32 GetEaSize()
        {
            fixed (FileInfo *P = &this)
                return *(UInt32 *)((Int64)(IntPtr)P + EaSizeOffset);
        }
        internal unsafe void SetEaSize(UInt32 value)
        {
            fixed (FileInfo *P = &this)
                *(UInt32 *)((Int64)(IntPtr)P + EaSizeOffset) = value;
        }
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
            NormalizedNameSize = (UInt16)(Size * 2);
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct DirInfo
    {
        internal const int FileNameBufSize = 255;
        internal static int FileNameBufOffset =
            (int)Marshal.OffsetOf(typeof(DirInfo), "FileNameBuf");

        internal UInt16 Size;
        internal FileInfo FileInfo;
        internal unsafe fixed Byte Padding[24];
        //internal unsafe fixed UInt16 FileNameBuf[];
        internal unsafe fixed UInt16 FileNameBuf[FileNameBufSize];

        internal unsafe void SetFileNameBuf(String Value)
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
        internal const int StreamNameBufSize = 255;
        internal static int StreamNameBufOffset =
            (int)Marshal.OffsetOf(typeof(StreamInfo), "StreamNameBuf");

        internal UInt16 Size;
        internal UInt64 StreamSize;
        internal UInt64 StreamAllocationSize;
        //internal unsafe fixed UInt16 StreamNameBuf[];
        internal unsafe fixed UInt16 StreamNameBuf[StreamNameBufSize];

        internal unsafe void SetStreamNameBuf(String Value)
        {
            fixed (UInt16 *P = StreamNameBuf)
            {
                int Size = null != Value ? Value.Length : 0;
                if (Size > StreamNameBufSize)
                    Size = StreamNameBufSize;
                for (int I = 0; Size > I; I++)
                    P[I] = Value[I];
                this.Size = (UInt16)(StreamNameBufOffset + Size * 2);
            }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct NotifyInfoInternal
    {
        internal const int FileNameBufSize = 1024 * 2/*FSP_FSCTL_TRANSACT_PATH_SIZEMAX*/;
        internal static int FileNameBufOffset =
            (int)Marshal.OffsetOf(typeof(NotifyInfoInternal), "FileNameBuf");

        internal UInt16 Size;
        internal UInt32 Filter;
        internal UInt32 Action;
        //internal unsafe fixed UInt16 FileNameBuf[];
        internal unsafe fixed UInt16 FileNameBuf[FileNameBufSize];

        internal unsafe void SetFileNameBuf(String Value)
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

    /// <summary>
    /// Enumeration of all the possible values for NotifyInfo.Action
    /// </summary>
    public enum NotifyAction : UInt32
    {
        Added = 1,
        Removed = 2,
        Modified = 3,
        RenamedOldName = 4,
        RenamedNewName = 5,
        AddedStream = 6,
        RemovedStream = 7,
        ModifiedStream = 8,
        RemovedByDelete = 9,
        IdNotTunnelled = 10,
        TunnelledIdCollision = 11,
    }


    /// <summary>
    /// Enumeration of all the possible values for NotifyInfo.Filter
    /// </summary>
    [Flags]
    public enum NotifyFilter : UInt32
    {
        None              = 0x00000,
        ChangeFileName    = 0x00001,
        ChangeDirName     = 0x00002,
        ChangeName        = ChangeFileName | ChangeDirName,
        ChangeAttributes  = 0x00004,
        ChangeSize        = 0x00008,
        ChangeLastWrite   = 0x00010,
        ChangeLastAccess  = 0x00020,
        ChangeCreation    = 0x00040,
        ChangeEa          = 0x00080,
        ChangeSecurity    = 0x00100,
        ChangeStreamName  = 0x00200,
        ChangeStreamSize  = 0x00400,
        ChangeStreamWrite = 0x00800,
    }

    /// <summary>
    /// Contains file change notification information.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct NotifyInfo
    {
        public String FileName;
        public NotifyAction Action;
        public NotifyFilter Filter;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FullEaInformation
    {
        internal const int EaNameSize = 15 * 1024;
            /* Set this to a value smaller than 16384 with sufficient space for additional data.
             * This should really be:
             *     FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX - FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName)
             */

        internal UInt32 NextEntryOffset;
        internal Byte Flags;
        internal Byte EaNameLength;
        internal UInt16 EaValueLength;
        internal unsafe fixed Byte EaName[EaNameSize];

        internal unsafe void Set(String Name, Byte[] Value, Boolean NeedEa)
        {
            int NameLength = 254 < Name.Length ? 254 : Name.Length;
            int ValueLength = EaNameSize - Name.Length - 1 < Value.Length ?
                EaNameSize - Name.Length - 1 : Value.Length;

            NextEntryOffset = 0;
            Flags = NeedEa ? (Byte)0x80/*FILE_NEED_EA*/ : (Byte)0;
            EaNameLength = (Byte)NameLength;
            EaValueLength = (UInt16)ValueLength;

            fixed (Byte *P = EaName)
            {
                int I = 0, J = 0;
                for (; NameLength > I; I++)
                    P[I] = (Byte)Name[I];
                P[I++] = 0;
                for (; ValueLength > J; J++)
                    P[I + J] = Value[J];
            }
        }
        internal static UInt32 PackedSize(String Name, Byte[] Value, Boolean NeedEa)
        {
            int NameLength = 254 < Name.Length ? 254 : Name.Length;
            int ValueLength = EaNameSize - Name.Length - 1 < Value.Length ?
                EaNameSize - Name.Length - 1 : Value.Length;

            /* magic computations are courtesy of NTFS */
            return (UInt32)(5 + NameLength + ValueLength);
        }
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
    internal struct IoStatus
    {
        internal UInt32 Information;
        internal UInt32 Status;
    }

    internal enum FspFsctlTransact
    {
        ReadKind = 5,
        WriteKind = 6,
        QueryDirectoryKind = 14
    }

    [StructLayout(LayoutKind.Explicit)]
    internal struct FspFsctlTransactReq
    {
        [FieldOffset(0)]
        internal UInt16 Version;
        [FieldOffset(2)]
        internal UInt16 Size;
        [FieldOffset(4)]
        internal UInt32 Kind;
        [FieldOffset(8)]
        internal UInt64 Hint;

        [FieldOffset(0)]
        internal unsafe fixed Byte Padding[88];
    }

    [StructLayout(LayoutKind.Explicit)]
    internal struct FspFsctlTransactRsp
    {
        [FieldOffset(0)]
        internal UInt16 Version;
        [FieldOffset(2)]
        internal UInt16 Size;
        [FieldOffset(4)]
        internal UInt32 Kind;
        [FieldOffset(8)]
        internal UInt64 Hint;

        [FieldOffset(16)]
        internal IoStatus IoStatus;

        [FieldOffset(24)]
        internal FileInfo WriteFileInfo;

        [FieldOffset(0)]
        internal unsafe fixed Byte Padding[128];
    }

    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct FspFileSystemOperationContext
    {
        internal FspFsctlTransactReq *Request;
        internal FspFsctlTransactRsp *Response;
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
                [MarshalAs(UnmanagedType.U1)] Boolean ReplaceFileAttributes,
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
                [MarshalAs(UnmanagedType.U1)] Boolean WriteToEndOfFile,
                [MarshalAs(UnmanagedType.U1)] Boolean ConstrainedIo,
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
                [MarshalAs(UnmanagedType.U1)] Boolean SetAllocationSize,
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
                [MarshalAs(UnmanagedType.U1)] Boolean ReplaceIfExists);
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
                [MarshalAs(UnmanagedType.U1)] Boolean ResolveLastPathComponent,
                out IoStatusBlock PIoStatus,
                IntPtr Buffer,
                IntPtr PSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetReparsePoint(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                IntPtr Buffer,
                IntPtr PSize);
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
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetDirInfoByName(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                out DirInfo DirInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Control(
                IntPtr FileSystem,
                ref FullContext FullContext,
                UInt32 ControlCode,
                IntPtr InputBuffer, UInt32 InputBufferLength,
                IntPtr OutputBuffer, UInt32 OutputBufferLength,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 SetDelete(
                IntPtr FileSystem,
                ref FullContext FullContext,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                [MarshalAs(UnmanagedType.U1)] Boolean DeleteFile);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 CreateEx(
                IntPtr FileSystem,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                UInt32 CreateOptions,
                UInt32 GrantedAccess,
                UInt32 FileAttributes,
                IntPtr SecurityDescriptor,
                UInt64 AllocationSize,
                IntPtr ExtraBuffer,
                UInt32 ExtraLength,
                [MarshalAs(UnmanagedType.U1)] Boolean ExtraBufferIsReparsePoint,
                ref FullContext FullContext,
                ref OpenFileInfo OpenFileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 OverwriteEx(
                IntPtr FileSystem,
                ref FullContext FullContext,
                UInt32 FileAttributes,
                [MarshalAs(UnmanagedType.U1)] Boolean ReplaceFileAttributes,
                UInt64 AllocationSize,
                IntPtr Ea,
                UInt32 EaLength,
                out FileInfo FileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetEa(
                IntPtr FileSystem,
                ref FullContext FullContext,
                IntPtr Ea,
                UInt32 EaLength,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 SetEa(
                IntPtr FileSystem,
                ref FullContext FullContext,
                IntPtr Ea,
                UInt32 EaLength,
                out FileInfo FileInfo);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 Obsolete0();
        }

        internal static int Size = IntPtr.Size * 64;

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
        internal Proto.GetDirInfoByName GetDirInfoByName;
        internal Proto.Control Control;
        internal Proto.SetDelete SetDelete;
        internal Proto.CreateEx CreateEx;
        internal Proto.OverwriteEx OverwriteEx;
        internal Proto.GetEa GetEa;
        internal Proto.SetEa SetEa;
        internal Proto.Obsolete0 Obsolete0;
        /* NTSTATUS (*Reserved[33])(); */
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
            internal delegate void FspFileSystemSendResponse(
                IntPtr FileSystem,
                ref FspFsctlTransactRsp Response);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemNotifyBegin(
                IntPtr FileSystem,
                UInt32 Timeout);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemNotifyEnd(
                IntPtr FileSystem);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemNotify(
                IntPtr FileSystem,
                IntPtr NotifyInfo,
                UIntPtr Size);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal unsafe delegate FspFileSystemOperationContext *FspFileSystemGetOperationContext();
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
            internal delegate UInt32 FspFileSystemOperationProcessIdF();
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean FspFileSystemAddDirInfo(
                IntPtr DirInfo,
                IntPtr Buffer,
                UInt32 Length,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean FspFileSystemFindReparsePoint(
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
                [MarshalAs(UnmanagedType.U1)] Boolean ResolveLastPathComponent,
                out IoStatusBlock PIoStatus,
                IntPtr Buffer,
                IntPtr PSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspFileSystemCanReplaceReparsePoint(
                IntPtr CurrentReparseData,
                UIntPtr CurrentReparseDataSize,
                IntPtr ReplaceReparseData,
                UIntPtr ReplaceReparseDataSize);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean FspFileSystemAddStreamInfo(
                IntPtr StreamInfo,
                IntPtr Buffer,
                UInt32 Length,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean FspFileSystemAddEa(
                IntPtr SingleEa,
                IntPtr Ea,
                UInt32 EaLength,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean FspFileSystemAddNotifyInfo(
                IntPtr NotifyInfo,
                IntPtr Buffer,
                UInt32 Length,
                out UInt32 PBytesTransferred);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
            internal delegate Boolean FspFileSystemAcquireDirectoryBuffer(
                ref IntPtr PDirBuffer,
                [MarshalAs(UnmanagedType.U1)] Boolean Reset,
                out Int32 PResult);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            [return: MarshalAs(UnmanagedType.U1)]
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
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 FspSetSecurityDescriptor(
                IntPtr InputDescriptor,
                UInt32 SecurityInformation,
                IntPtr ModificationDescriptor,
                out IntPtr PSecurityDescriptor);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspDeleteSecurityDescriptor(
                IntPtr SecurityDescriptor,
                IntPtr CreateFunc);

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
            internal delegate void FspDebugLog(
                [MarshalAs(UnmanagedType.LPStr)] String Format,
                [MarshalAs(UnmanagedType.LPStr)] String Message);
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate void FspDebugLogSetHandle(
                IntPtr Handle);

            /* callbacks */
            [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
            internal delegate Int32 GetReparsePointByName(
                IntPtr FileSystem,
                IntPtr Context,
                [MarshalAs(UnmanagedType.LPWStr)] String FileName,
                [MarshalAs(UnmanagedType.U1)] Boolean IsDirectory,
                IntPtr Buffer,
                IntPtr PSize);
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
        internal static Proto.FspFileSystemSendResponse FspFileSystemSendResponse;
        internal static Proto.FspFileSystemNotifyBegin FspFileSystemNotifyBegin;
        internal static Proto.FspFileSystemNotifyEnd FspFileSystemNotifyEnd;
        internal static Proto.FspFileSystemNotify _FspFileSystemNotify;
        internal static Proto.FspFileSystemGetOperationContext FspFileSystemGetOperationContext;
        internal static Proto.FspFileSystemMountPointF FspFileSystemMountPoint;
        internal static Proto.FspFileSystemSetOperationGuardStrategyF FspFileSystemSetOperationGuardStrategy;
        internal static Proto.FspFileSystemSetDebugLogF FspFileSystemSetDebugLog;
        internal static Proto.FspFileSystemOperationProcessIdF FspFileSystemOperationProcessId;
        internal static Proto.FspFileSystemAddDirInfo _FspFileSystemAddDirInfo;
        internal static Proto.FspFileSystemFindReparsePoint FspFileSystemFindReparsePoint;
        internal static Proto.FspFileSystemResolveReparsePoints FspFileSystemResolveReparsePoints;
        internal static Proto.FspFileSystemCanReplaceReparsePoint _FspFileSystemCanReplaceReparsePoint;
        internal static Proto.FspFileSystemAddStreamInfo _FspFileSystemAddStreamInfo;
        internal static Proto.FspFileSystemAddEa _FspFileSystemAddEa;
        internal static Proto.FspFileSystemAddNotifyInfo _FspFileSystemAddNotifyInfo;
        internal static Proto.FspFileSystemAcquireDirectoryBuffer FspFileSystemAcquireDirectoryBuffer;
        internal static Proto.FspFileSystemFillDirectoryBuffer FspFileSystemFillDirectoryBuffer;
        internal static Proto.FspFileSystemReleaseDirectoryBuffer FspFileSystemReleaseDirectoryBuffer;
        internal static Proto.FspFileSystemReadDirectoryBuffer FspFileSystemReadDirectoryBuffer;
        internal static Proto.FspFileSystemDeleteDirectoryBuffer FspFileSystemDeleteDirectoryBuffer;
        internal static Proto.FspSetSecurityDescriptor FspSetSecurityDescriptor;
        internal static IntPtr _FspSetSecurityDescriptorPtr;
        internal static Proto.FspDeleteSecurityDescriptor FspDeleteSecurityDescriptor;
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
        internal static Proto.FspDebugLog FspDebugLog;
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
        internal static unsafe UInt64 FspFileSystemGetOperationRequestHint()
        {
            return FspFileSystemGetOperationContext()->Request->Hint;
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
        internal static unsafe Boolean FspFileSystemEndDirInfo(
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            return _FspFileSystemAddDirInfo(IntPtr.Zero, Buffer, Length, out PBytesTransferred);
        }
        internal static unsafe Boolean FspFileSystemAddStreamInfo(
            ref StreamInfo StreamInfo,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            fixed (StreamInfo *P = &StreamInfo)
                return _FspFileSystemAddStreamInfo((IntPtr)P, Buffer, Length, out PBytesTransferred);
        }
        internal static unsafe Boolean FspFileSystemEndStreamInfo(
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            return _FspFileSystemAddStreamInfo(IntPtr.Zero, Buffer, Length, out PBytesTransferred);
        }
        internal static unsafe Boolean FspFileSystemAddNotifyInfo(
            ref NotifyInfoInternal NotifyInfo,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            fixed (NotifyInfoInternal *P = &NotifyInfo)
                return _FspFileSystemAddNotifyInfo((IntPtr)P, Buffer, Length, out PBytesTransferred);
        }

        internal delegate Int32 EnumerateEa(
            Object FileNode,
            Object FileDesc,
            ref Object Context,
            String EaName,
            Byte[] EaValue,
            Boolean NeedEa);
        internal static unsafe Int32 FspFileSystemEnumerateEa(
            Object FileNode,
            Object FileDesc,
            EnumerateEa EnumerateEa,
            IntPtr Ea,
            UInt32 EaLength)
        {
            Object Context = null;
            FullEaInformation *P = (FullEaInformation *)Ea;
            FullEaInformation *EndP = (FullEaInformation *)(Ea.ToInt64() + EaLength);
            Int32 Result;
            Result = 0/*STATUS_SUCCESS*/;
            for (; EndP > P;
                P = 0 != P->NextEntryOffset ?
                    (FullEaInformation *)(((IntPtr)P).ToInt64() + P->NextEntryOffset) :
                    EndP)
            {
                String EaName = Marshal.PtrToStringAnsi((IntPtr)P->EaName, P->EaNameLength);
                Byte[] EaValue = null;
                if (0 != P->EaValueLength)
                {
                    EaValue = new Byte[P->EaValueLength];
                    Marshal.Copy((IntPtr)(((IntPtr)P->EaName).ToInt64() + P->EaNameLength + 1),
                        EaValue, 0, P->EaValueLength);
                }
                Boolean NeedEa = 0 != (0x80/*FILE_NEED_EA*/ & P->Flags);
                Result = EnumerateEa(FileNode, FileDesc, ref Context, EaName, EaValue, NeedEa);
                if (0 > Result)
                    break;
            }
            return Result;
        }
        internal static unsafe Boolean FspFileSystemAddEa(
            ref FullEaInformation EaInfo,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            fixed (FullEaInformation *P = &EaInfo)
                return _FspFileSystemAddEa((IntPtr)P, Buffer, Length, out PBytesTransferred);
        }
        internal static unsafe Boolean FspFileSystemEndEa(
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            return _FspFileSystemAddEa(IntPtr.Zero, Buffer, Length, out PBytesTransferred);
        }

        internal static unsafe Int32 FspFileSystemNotify(
            IntPtr FileSystem,
            NotifyInfo[] NotifyInfoArray)
        {
            int Length = 0;
            for (int I = 0; NotifyInfoArray.Length > I; I++)
            {
                Length += NotifyInfoInternal.FileNameBufOffset +
                    NotifyInfoArray[I].FileName.Length * 2;
                Length = (Length + 7) & ~7; // align to next qword boundary
            }

            Byte[] Buffer = new Byte[Length];
            UInt32 BytesTransferred = default(UInt32);
            fixed (Byte *P = Buffer)
            {
                for (int I = 0; NotifyInfoArray.Length > I; I++)
                {
                    NotifyInfoInternal Internal = default(NotifyInfoInternal);
                    Internal.Action = (UInt32)NotifyInfoArray[I].Action;
                    Internal.Filter = (UInt32)NotifyInfoArray[I].Filter;
                    Internal.SetFileNameBuf(NotifyInfoArray[I].FileName);
                    FspFileSystemAddNotifyInfo(
                        ref Internal, (IntPtr)P, (UInt32)Length, out BytesTransferred);
                }
                return _FspFileSystemNotify(FileSystem, (IntPtr)P, (UIntPtr)BytesTransferred);
            }
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
            Debug.Assert(IntPtr.Zero == *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)));
            GCHandle Handle = GCHandle.Alloc(Obj, GCHandleType.Weak);
            *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)) = (IntPtr)Handle;
        }
        internal unsafe static void DisposeUserContext(
            IntPtr NativePtr)
        {
            IntPtr UserContext = *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr));
            Debug.Assert(IntPtr.Zero != UserContext);
            if (IntPtr.Zero != UserContext)
            {
                GCHandle.FromIntPtr(UserContext).Free();
                *(IntPtr *)((Byte *)NativePtr + sizeof(IntPtr)) = IntPtr.Zero;
            }
        }

        private class FullContextHolder
        {
            public Object FileNode;
            public Object FileDesc;
        }
        internal static void GetFullContext(ref FullContext FullContext,
            out Object FileNode, out Object FileDesc)
        {
            FullContextHolder Holder = 0 != FullContext.UserContext2 ?
                (FullContextHolder)GCHandle.FromIntPtr((IntPtr)FullContext.UserContext2).Target :
                null;
            if (null != Holder)
            {
                FileNode = Holder.FileNode;
                FileDesc = Holder.FileDesc;
            }
            else
            {
                FileNode = null;
                FileDesc = null;
            }
        }
        internal static void SetFullContext(ref FullContext FullContext,
            Object FileNode, Object FileDesc)
        {
            Debug.Assert(0 == FullContext.UserContext && 0 == FullContext.UserContext2);
            FullContextHolder Holder = new FullContextHolder();
            Holder.FileNode = FileNode;
            Holder.FileDesc = FileDesc;
            GCHandle Handle = GCHandle.Alloc(Holder, GCHandleType.Normal);
            FullContext.UserContext2 = (UInt64)(IntPtr)Handle;
        }
        internal static void DisposeFullContext(ref FullContext FullContext)
        {
            Debug.Assert(0 == FullContext.UserContext && 0 != FullContext.UserContext2);
            if (0 != FullContext.UserContext2)
            {
                GCHandle.FromIntPtr((IntPtr)FullContext.UserContext2).Free();
                FullContext.UserContext2 = 0;
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
        internal unsafe static byte[] ModifySecurityDescriptor(
            Byte[] SecurityDescriptorBytes,
            UInt32 SecurityInformation,
            Byte[] ModificationDescriptorBytes)
        {
            fixed (Byte *S = SecurityDescriptorBytes)
                fixed (Byte *M = ModificationDescriptorBytes)
                {
                    IntPtr SecurityDescriptor;
                    Int32 Result = FspSetSecurityDescriptor(
                        (IntPtr)S, SecurityInformation, (IntPtr)M, out SecurityDescriptor);
                    if (0 > Result)
                        return null;
                    SecurityDescriptorBytes = MakeSecurityDescriptor(SecurityDescriptor);
                    FspDeleteSecurityDescriptor(SecurityDescriptor, _FspSetSecurityDescriptorPtr);
                    return SecurityDescriptorBytes;
                }
        }
        internal unsafe static Int32 ModifySecurityDescriptorEx(
            Byte[] SecurityDescriptorBytes,
            UInt32 SecurityInformation,
            Byte[] ModificationDescriptorBytes,
            ref Byte[] ModifiedDescriptorBytes)
        {
            fixed (Byte *S = SecurityDescriptorBytes)
                fixed (Byte *M = ModificationDescriptorBytes)
                {
                    IntPtr SecurityDescriptor;
                    Int32 Result = FspSetSecurityDescriptor(
                        (IntPtr)S, SecurityInformation, (IntPtr)M, out SecurityDescriptor);
                    if (0 > Result)
                        return Result;
                    SecurityDescriptorBytes = MakeSecurityDescriptor(SecurityDescriptor);
                    FspDeleteSecurityDescriptor(SecurityDescriptor, _FspSetSecurityDescriptorPtr);
                    ModifiedDescriptorBytes = SecurityDescriptorBytes;
                    return 0/*STATUS_SUCCESS*/;
                }
        }

        internal unsafe static Int32 CopyReparsePoint(
            Byte[] ReparseData,
            IntPtr Buffer,
            IntPtr PSize)
        {
            if (IntPtr.Zero != Buffer)
            {
                if (null != ReparseData)
                {
                    if (ReparseData.Length > (int)*(UIntPtr *)PSize)
                        return unchecked((Int32)0xc0000023)/*STATUS_BUFFER_TOO_SMALL*/;
                    *(UIntPtr *)PSize = (UIntPtr)ReparseData.Length;
                    Marshal.Copy(ReparseData, 0, Buffer, ReparseData.Length);
                }
                else
                    *(UIntPtr *)PSize = UIntPtr.Zero;
            }
            return 0/*STATUS_SUCCESS*/;
        }
        internal static Byte[] MakeReparsePoint(
            IntPtr Buffer,
            UIntPtr Size)
        {
            if (IntPtr.Zero != Buffer)
            {
                Byte[] ReparseData = new Byte[(int)Size];
                Marshal.Copy(Buffer, ReparseData, 0, ReparseData.Length);
                return ReparseData;
            }
            else
                return null;
        }
        internal unsafe static Int32 FspFileSystemCanReplaceReparsePoint(
            Byte[] CurrentReparseData,
            Byte[] ReplaceReparseData)
        {
            fixed (Byte *C = CurrentReparseData)
                fixed (Byte *R = ReplaceReparseData)
                    return _FspFileSystemCanReplaceReparsePoint(
                        (IntPtr)C, (UIntPtr)CurrentReparseData.Length,
                        (IntPtr)R, (UIntPtr)ReplaceReparseData.Length);
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

        internal static Version GetVersion()
        {
            UInt32 Version = 0;
            FspVersion(out Version);
            return new System.Version((Int32)Version >> 16, (Int32)Version & 0xFFFF);
        }

        /* initialization */
        internal static String ProductName = "WinFsp";
        internal static String ProductFileName = "winfsp";
        private static IntPtr LoadDll()
        {
            String DllPath = null;
            String DllName = 8 == IntPtr.Size ?
                ProductFileName + "-x64.dll" :
                ProductFileName + "-x86.dll";
            String KeyName = 8 == IntPtr.Size ?
                "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\" + ProductName :
                "HKEY_LOCAL_MACHINE\\Software\\" + ProductName;
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
        private static IntPtr GetEntryPointPtr(IntPtr Module, String Name)
        {
            IntPtr Proc = GetProcAddress(Module, Name);
            if (IntPtr.Zero == Proc)
                throw new EntryPointNotFoundException("cannot get entry point " + Name);
            return Proc;
        }
        private static T GetEntryPoint<T>(IntPtr Module)
        {
            return (T)(object)Marshal.GetDelegateForFunctionPointer(
                GetEntryPointPtr(Module, typeof(T).Name), typeof(T));
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
            FspFileSystemSendResponse = GetEntryPoint<Proto.FspFileSystemSendResponse>(Module);
            FspFileSystemNotifyBegin = GetEntryPoint<Proto.FspFileSystemNotifyBegin>(Module);
            FspFileSystemNotifyEnd = GetEntryPoint<Proto.FspFileSystemNotifyEnd>(Module);
            _FspFileSystemNotify = GetEntryPoint<Proto.FspFileSystemNotify>(Module);
            FspFileSystemGetOperationContext = GetEntryPoint<Proto.FspFileSystemGetOperationContext>(Module);
            FspFileSystemMountPoint = GetEntryPoint<Proto.FspFileSystemMountPointF>(Module);
            FspFileSystemSetOperationGuardStrategy = GetEntryPoint<Proto.FspFileSystemSetOperationGuardStrategyF>(Module);
            FspFileSystemSetDebugLog = GetEntryPoint<Proto.FspFileSystemSetDebugLogF>(Module);
            FspFileSystemOperationProcessId = GetEntryPoint<Proto.FspFileSystemOperationProcessIdF>(Module);
            _FspFileSystemAddDirInfo = GetEntryPoint<Proto.FspFileSystemAddDirInfo>(Module);
            FspFileSystemFindReparsePoint = GetEntryPoint<Proto.FspFileSystemFindReparsePoint>(Module);
            FspFileSystemResolveReparsePoints = GetEntryPoint<Proto.FspFileSystemResolveReparsePoints>(Module);
            _FspFileSystemCanReplaceReparsePoint = GetEntryPoint<Proto.FspFileSystemCanReplaceReparsePoint>(Module);
            _FspFileSystemAddStreamInfo = GetEntryPoint<Proto.FspFileSystemAddStreamInfo>(Module);
            _FspFileSystemAddEa = GetEntryPoint<Proto.FspFileSystemAddEa>(Module);
            _FspFileSystemAddNotifyInfo = GetEntryPoint<Proto.FspFileSystemAddNotifyInfo>(Module);
            FspFileSystemAcquireDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemAcquireDirectoryBuffer>(Module);
            FspFileSystemFillDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemFillDirectoryBuffer>(Module);
            FspFileSystemReleaseDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemReleaseDirectoryBuffer>(Module);
            FspFileSystemReadDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemReadDirectoryBuffer>(Module);
            FspFileSystemDeleteDirectoryBuffer = GetEntryPoint<Proto.FspFileSystemDeleteDirectoryBuffer>(Module);
            FspSetSecurityDescriptor = GetEntryPoint<Proto.FspSetSecurityDescriptor>(Module);
            _FspSetSecurityDescriptorPtr = GetEntryPointPtr(Module, "FspSetSecurityDescriptor");
            FspDeleteSecurityDescriptor = GetEntryPoint<Proto.FspDeleteSecurityDescriptor>(Module);
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
            FspDebugLog = GetEntryPoint<Proto.FspDebugLog>(Module);
            FspDebugLogSetHandle = GetEntryPoint<Proto.FspDebugLogSetHandle>(Module);
        }
        private static void CheckVersion()
        {
            FileVersionInfo Info;
            UInt32 Version = 0, VersionMajor, VersionMinor;
            Info = FileVersionInfo.GetVersionInfo(Assembly.GetExecutingAssembly().Location);
            FspVersion(out Version); VersionMajor = Version >> 16; VersionMinor = Version & 0xFFFF;
            if (Info.FileMajorPart != VersionMajor || Info.FileMinorPart > VersionMinor)
                throw new TypeLoadException(String.Format(
                    "incorrect dll version (need {0}.{1}, have {2}.{3})",
                    Info.FileMajorPart, Info.FileMinorPart, VersionMajor, VersionMinor));
        }
        static Api()
        {
#if false //DEBUG
            if (Debugger.IsAttached)
                Debugger.Break();
#endif
            object[] attributes = Assembly.GetExecutingAssembly().GetCustomAttributes(
                typeof(AssemblyProductAttribute), false);
            if (null != attributes &&
                0 < attributes.Length &&
                null != attributes[0] as AssemblyProductAttribute)
            {
                ProductName = (attributes[0] as AssemblyProductAttribute).Product;
                ProductFileName = ProductName.ToLowerInvariant();
            }
            LoadProto(LoadDll());
            CheckVersion();
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
