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
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;

using Fsp;
using VolumeInfo = Fsp.Interop.VolumeInfo;
using FileInfo = Fsp.Interop.FileInfo;

namespace passthrough
{
    class Ptfs : FileSystem
    {
        private const int ALLOCATION_UNIT = 4096;

        protected class FileDesc
        {
            public FileSystemInfo Info;
            public FileStream Stream;

            public FileDesc(FileSystemInfo Info, FileStream Stream)
            {
                this.Info = Info;
                this.Stream = Stream;
            }
            public UInt32 FileAttributes
            {
                get
                {
                    return (UInt32)Info.Attributes;
                }
                set
                {
                    Info.Attributes = 0 == value ?
                        System.IO.FileAttributes.Normal : (FileAttributes)value;
                }
            }
        }

        public Ptfs() : base()
        {
            SetSectorSize(ALLOCATION_UNIT);
            SetSectorsPerAllocationUnit(1);
            SetFileInfoTimeout(1000);
            SetCaseSensitiveSearch(false);
            SetCasePreservedNames(true);
            SetUnicodeOnDisk(true);
            SetPersistentAcls(true);
            SetPostCleanupWhenModifiedOnly(true);
            SetPassQueryDirectoryPattern(true);
        }
        void SetPath(String value)
        {
            _Path = Path.GetFullPath(value);
            SetVolumeCreationTime((UInt64)File.GetCreationTimeUtc(_Path).ToFileTimeUtc());
            SetVolumeSerialNumber(0);
        }

        protected override Int32 ExceptionHandler(Exception ex)
        {
            Int32 HResult = ex.HResult; /* needs Framework 4.5 */
            if (0x80070000 == (HResult & 0xFFFF0000))
                return NtStatusFromWin32((UInt32)HResult & 0xFFFF);
            return STATUS_UNEXPECTED_IO_ERROR;
        }
        protected String ConcatPath(String FileName)
        {
            return Path.Combine(_Path, FileName);
        }
        protected Int32 GetFileInfoInternal(FileDesc FileDesc, Boolean Refresh,
            out FileInfo FileInfo)
        {
            if (Refresh)
                FileDesc.Info.Refresh();
            FileInfo.FileAttributes = FileDesc.FileAttributes;
            FileInfo.ReparseTag = 0;
            FileInfo.FileSize = FileDesc.Info is System.IO.FileInfo ?
                (UInt64)((System.IO.FileInfo)FileDesc.Info).Length : 0;
            FileInfo.AllocationSize = (FileInfo.FileSize + ALLOCATION_UNIT - 1)
                / ALLOCATION_UNIT * ALLOCATION_UNIT;
            FileInfo.CreationTime = (UInt64)FileDesc.Info.CreationTimeUtc.ToFileTimeUtc();
            FileInfo.LastAccessTime = (UInt64)FileDesc.Info.LastAccessTimeUtc.ToFileTimeUtc();
            FileInfo.LastWriteTime = (UInt64)FileDesc.Info.LastWriteTimeUtc.ToFileTimeUtc();
            FileInfo.ChangeTime = FileInfo.LastWriteTime;
            FileInfo.IndexNumber = 0;
            FileInfo.HardLinks = 0;
            return STATUS_SUCCESS;
        }
        protected override Int32 GetVolumeInfo(
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
        protected override Int32 GetSecurityByName(
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
        protected override Int32 Create(
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
            FileDesc FileDesc;
            FileName = ConcatPath(FileName);
            if (0 != (CreateOptions & FILE_DIRECTORY_FILE))
            {
                DirectorySecurity Security = null;
                if (null != SecurityDescriptor)
                {
                    Security = new DirectorySecurity();
                    Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor);
                }
                // ???: FILE_DELETE_ON_CLOSE
                FileDesc = new FileDesc(
                    Directory.CreateDirectory(FileName, Security),
                    null);
            }
            else
            {
                FileSecurity Security = null;
                if (null != SecurityDescriptor)
                {
                    Security = new FileSecurity();
                    Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor);
                }
                FileOptions Options = 0 != (CreateOptions & FILE_DELETE_ON_CLOSE) ?
                    FileOptions.DeleteOnClose : 0;
                FileDesc = new FileDesc(
                    new System.IO.FileInfo(FileName),
                    new FileStream(
                        FileName,
                        FileMode.CreateNew,
                        (FileSystemRights)GrantedAccess,
                        FileShare.Read | FileShare.Write | FileShare.Delete,
                        4096,
                        Options,
                        Security));
            }
            FileDesc.FileAttributes = FileAttributes;
            FileNode = default(Object);
            FileDesc0 = FileDesc;
            NormalizedName = default(String);
            return GetFileInfoInternal(FileDesc, false, out FileInfo);
        }
        protected override Int32 Open(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            out Object FileNode,
            out Object FileDesc0,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileDesc FileDesc;
            FileName = ConcatPath(FileName);
            if (Directory.Exists(FileName))
            {
                // ???: FILE_DELETE_ON_CLOSE
                FileDesc = new FileDesc(
                    new System.IO.DirectoryInfo(FileName),
                    null);
            }
            else
            {
                FileOptions Options = 0 != (CreateOptions & FILE_DELETE_ON_CLOSE) ?
                    FileOptions.DeleteOnClose : 0;
                FileDesc = new FileDesc(
                    new System.IO.FileInfo(FileName),
                    new FileStream(
                        FileName,
                        FileMode.Open,
                        (FileSystemRights)GrantedAccess,
                        FileShare.Read | FileShare.Write | FileShare.Delete,
                        4096,
                        Options));
            }
            FileNode = default(Object);
            FileDesc0 = FileDesc;
            NormalizedName = default(String);
            return GetFileInfoInternal(FileDesc, false, out FileInfo);
        }
        protected override Int32 Overwrite(
            Object FileNode,
            Object FileDesc0,
            UInt32 FileAttributes,
            Boolean ReplaceFileAttributes,
            UInt64 AllocationSize,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (ReplaceFileAttributes)
                FileDesc.FileAttributes = FileAttributes;
            else if (0 != FileAttributes)
                FileDesc.FileAttributes |= FileAttributes;
            FileDesc.Stream.SetLength(0);
            return GetFileInfoInternal(FileDesc, true, out FileInfo);
        }
        protected override void Cleanup(
            Object FileNode,
            Object FileDesc0,
            String FileName,
            UInt32 Flags)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (0 == (Flags & CleanupDelete))
                FileDesc.Stream.Dispose();
        }
        protected override void Close(
            Object FileNode,
            Object FileDesc0)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            FileDesc.Stream.Dispose();
        }
        protected override Int32 Read(
            Object FileNode,
            Object FileDesc0,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            Byte[] Bytes = new byte[Length];
            FileDesc.Stream.Seek((Int64)Offset, SeekOrigin.Begin);
            PBytesTransferred = (UInt32)FileDesc.Stream.Read(Bytes, 0, Bytes.Length);
            Marshal.Copy(Bytes, 0, Buffer, Bytes.Length);
            return STATUS_SUCCESS;
        }
        protected override Int32 Write(
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
            UInt64 FileSize;
            if (ConstrainedIo)
            {
                FileDesc.Info.Refresh();
                FileSize = (UInt64)((System.IO.FileInfo)FileDesc.Info).Length;
                if (Offset >= FileSize)
                {
                    PBytesTransferred = default(UInt32);
                    FileInfo = default(FileInfo);
                    return STATUS_SUCCESS;
                }
                if (Offset + Length > FileSize)
                    Length = (UInt32)(FileSize - Offset);
            }
            Byte[] Bytes = new byte[Length];
            Marshal.Copy(Buffer, Bytes, 0, Bytes.Length);
            FileDesc.Stream.Seek((Int64)Offset, SeekOrigin.Begin);
            FileDesc.Stream.Write(Bytes, 0, Bytes.Length);
            PBytesTransferred = (UInt32)Bytes.Length;
            return GetFileInfoInternal(FileDesc, true, out FileInfo);
        }
        protected override Int32 Flush(
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
            return GetFileInfoInternal(FileDesc, true, out FileInfo);
        }
        protected override Int32 GetFileInfo(
            Object FileNode,
            Object FileDesc0,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            return GetFileInfoInternal(FileDesc, true, out FileInfo);
        }
        protected override Int32 SetBasicInfo(
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
            if (unchecked((UInt32)(-1)) != FileAttributes)
                FileDesc.FileAttributes = FileAttributes;
            if (0 != CreationTime)
                FileDesc.Info.CreationTimeUtc = DateTime.FromFileTimeUtc((Int64)CreationTime);
            if (0 != LastAccessTime)
                FileDesc.Info.LastAccessTimeUtc = DateTime.FromFileTimeUtc((Int64)LastAccessTime);
            if (0 != LastWriteTime)
                FileDesc.Info.LastWriteTimeUtc = DateTime.FromFileTimeUtc((Int64)LastWriteTime);
            return GetFileInfoInternal(FileDesc, true, out FileInfo);
        }
        protected override Int32 SetFileSize(
            Object FileNode,
            Object FileDesc0,
            UInt64 NewSize,
            Boolean SetAllocationSize,
            out FileInfo FileInfo)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            GetFileInfoInternal(FileDesc, true, out FileInfo);
            if (!SetAllocationSize || FileInfo.FileSize > NewSize)
            {
                /*
                 * "FileInfo.FileSize > NewSize" explanation:
                 * Ptfs does not support allocation size. However if the new AllocationSize
                 * is less than the current FileSize we must truncate the file.
                 */
                FileDesc.Stream.SetLength((Int64)NewSize);
                FileInfo.FileSize = NewSize;
                FileInfo.AllocationSize = (FileInfo.FileSize + ALLOCATION_UNIT - 1)
                    / ALLOCATION_UNIT * ALLOCATION_UNIT;
            }
            return STATUS_SUCCESS;
        }
        protected override Int32 CanDelete(
            Object FileNode,
            Object FileDesc0,
            String FileName)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            FileName = ConcatPath(FileName);
            /*
             * If a file has an open handle the Delete call below
             * will only mark it for disposition.
             */
            if (null == FileDesc.Stream)
                Directory.Delete(FileName);
            else
                File.Delete(FileName);
            return STATUS_SUCCESS;
        }
        protected override Int32 Rename(
            Object FileNode,
            Object FileDesc0,
            String FileName,
            String NewFileName,
            Boolean ReplaceIfExists)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            FileName = ConcatPath(FileName);
            NewFileName = ConcatPath(NewFileName);
            if (null == FileDesc.Stream)
            {
                if (ReplaceIfExists)
                    return STATUS_ACCESS_DENIED;
                Directory.Move(FileName, NewFileName);
            }
            else
            {
                if (ReplaceIfExists)
                    File.Delete(NewFileName);
                File.Move(FileName, NewFileName);
            }
            return STATUS_SUCCESS;
        }
        protected override Int32 GetSecurity(
            Object FileNode,
            Object FileDesc0,
            ref Byte[] SecurityDescriptor)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null == FileDesc.Stream)
                SecurityDescriptor = ((System.IO.DirectoryInfo)FileDesc.Info).GetAccessControl().
                    GetSecurityDescriptorBinaryForm();
            else
                SecurityDescriptor = ((System.IO.FileInfo)FileDesc.Info).GetAccessControl().
                    GetSecurityDescriptorBinaryForm();
            return STATUS_SUCCESS;
        }
        protected override Int32 SetSecurity(
            Object FileNode,
            Object FileDesc0,
            AccessControlSections Sections,
            Byte[] SecurityDescriptor)
        {
            FileDesc FileDesc = (FileDesc)FileDesc0;
            if (null == FileDesc.Stream)
            {
                DirectorySecurity Security = ((System.IO.DirectoryInfo)FileDesc.Info).GetAccessControl();
                Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor, Sections);
                ((System.IO.DirectoryInfo)FileDesc.Info).SetAccessControl(Security);
            }
            else
            {
                FileSecurity Security = ((System.IO.FileInfo)FileDesc.Info).GetAccessControl();
                Security.SetSecurityDescriptorBinaryForm(SecurityDescriptor, Sections);
                ((System.IO.FileInfo)FileDesc.Info).SetAccessControl(Security);
            }
            return STATUS_SUCCESS;
        }
        protected override Int32 ReadDirectory(
            Object FileNode,
            Object FileDesc,
            String Pattern,
            String Marker,
            IntPtr Buffer,
            UInt32 Length,
            out UInt32 PBytesTransferred)
        {
            PBytesTransferred = default(UInt32);
            return STATUS_INVALID_DEVICE_REQUEST;
        }

        private String _Path;
    }

    class PtfsService : Service
    {
        public PtfsService() : base("PtfsService")
        {
        }

        protected override void OnStart(String[] Args)
        {
#if false
    wchar_t **argp, **arge;
    String DebugLogFile = null;
    UInt32 DebugFlags = 0;
    String VolumePrefix = null;
    String PassThrough = null;
    String MountPoint = null;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
    WCHAR PassThroughBuf[MAX_PATH];
    PTFS *Ptfs = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'd':
            argtol(DebugFlags);
            break;
        case L'D':
            argtos(DebugLogFile);
            break;
        case L'm':
            argtos(MountPoint);
            break;
        case L'p':
            argtos(PassThrough);
            break;
        case L'u':
            argtos(VolumePrefix);
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (0 == PassThrough && 0 != VolumePrefix)
    {
        PWSTR P;

        P = wcschr(VolumePrefix, L'\\');
        if (0 != P && L'\\' != P[1])
        {
            P = wcschr(P + 1, L'\\');
            if (0 != P &&
                (
                (L'A' <= P[1] && P[1] <= L'Z') ||
                (L'a' <= P[1] && P[1] <= L'z')
                ) &&
                L'$' == P[2])
            {
                StringCbPrintf(PassThroughBuf, sizeof PassThroughBuf, L"%c:%s", P[1], P + 3);
                PassThrough = PassThroughBuf;
            }
        }
    }

    if (0 == PassThrough || 0 == MountPoint)
        goto usage;

    EnableBackupRestorePrivileges();

    if (0 != DebugLogFile)
    {
        if (0 == wcscmp(L"-", DebugLogFile))
            DebugLogHandle = GetStdHandle(STD_ERROR_HANDLE);
        else
            DebugLogHandle = CreateFileW(
                DebugLogFile,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == DebugLogHandle)
        {
            fail(L"cannot open debug log file");
            goto usage;
        }

        FspDebugLogSetHandle(DebugLogHandle);
    }

    Ptfs = new PTFS;

    Ptfs->SetPrefix(VolumePrefix);

    Result = Ptfs->SetPath(PassThrough);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create file system");
        goto exit;
    }

    Result = Ptfs->Mount(MountPoint, 0, FALSE, DebugFlags);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot mount file system");
        goto exit;
    }

    MountPoint = Ptfs->MountPoint();

    info(L"%s%s%s -p %s -m %s",
        L"" PROGNAME,
        0 != VolumePrefix && L'\0' != VolumePrefix[0] ? L" -u " : L"",
            0 != VolumePrefix && L'\0' != VolumePrefix[0] ? VolumePrefix : L"",
        PassThrough,
        MountPoint);

    _Ptfs = Ptfs;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != Ptfs)
        delete Ptfs;

    return Result;

usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -d DebugFlags       [-1: enable all debug logs]\n"
        "    -D DebugLogFile     [file path; use - for stderr]\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -p Directory        [directory to expose as pass through file system]\n"
        "    -m MountPoint       [X:|*|directory]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;
#endif
        }
        protected override void OnStop()
        {
            _Ptfs.Unmount();
            _Ptfs = null;
        }

        private Ptfs _Ptfs;
    }

    class Program
    {
        static void Main(string[] args)
        {
            Environment.ExitCode = new PtfsService().Run();
        }
    }
}

#if false

using namespace Fsp;

struct PTFS_FILE_DESC
{
    PTFS_FILE_DESC() : Handle(INVALID_HANDLE_VALUE), DirBuffer()
    {
    }
    ~PTFS_FILE_DESC()
    {
        CloseHandle(Handle);
        PTFS::DeleteDirectoryBuffer(&DirBuffer);
    }
    HANDLE Handle;
    PVOID DirBuffer;
};

NTSTATUS PTFS::ReadDirectory(
    const FILE_CONTEXT *FileContext, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG BufferLength, PULONG PBytesTransferred)
{
    PTFS_FILE_DESC *FileDesc = (PTFS_FILE_DESC *)FileContext->FileDesc;
    HANDLE Handle = FileDesc->Handle;
    WCHAR FullPath[FULLPATH_SIZE];
    ULONG Length, PatternLength;
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
    union
    {
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_DIR_INFO D;
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO *DirInfo = &DirInfoBuf.D;
    NTSTATUS DirBufferResult;

    DirBufferResult = STATUS_SUCCESS;
    if (AcquireDirectoryBuffer(&FileDesc->DirBuffer, 0 == Marker, &DirBufferResult))
    {
        if (0 == Pattern)
            Pattern = L"*";
        PatternLength = (ULONG)wcslen(Pattern);

        Length = GetFinalPathNameByHandleW(Handle, FullPath, FULLPATH_SIZE - 1, 0);
        if (0 == Length)
            DirBufferResult = NtStatusFromWin32(GetLastError());
        else if (Length + 1 + PatternLength >= FULLPATH_SIZE)
            DirBufferResult = STATUS_OBJECT_NAME_INVALID;
        if (!NT_SUCCESS(DirBufferResult))
        {
            ReleaseDirectoryBuffer(&FileDesc->DirBuffer);
            return DirBufferResult;
        }

        if (L'\\' != FullPath[Length - 1])
            FullPath[Length++] = L'\\';
        memcpy(FullPath + Length, Pattern, PatternLength * sizeof(WCHAR));
        FullPath[Length + PatternLength] = L'\0';

        FindHandle = FindFirstFileW(FullPath, &FindData);
        if (INVALID_HANDLE_VALUE != FindHandle)
        {
            do
            {
                memset(DirInfo, 0, sizeof *DirInfo);
                Length = (ULONG)wcslen(FindData.cFileName);
                DirInfo->Size = (UINT16)(FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + Length * sizeof(WCHAR));
                DirInfo->FileInfo.FileAttributes = FindData.dwFileAttributes;
                DirInfo->FileInfo.ReparseTag = 0;
                DirInfo->FileInfo.FileSize =
                    ((UINT64)FindData.nFileSizeHigh << 32) | (UINT64)FindData.nFileSizeLow;
                DirInfo->FileInfo.AllocationSize = (DirInfo->FileInfo.FileSize + ALLOCATION_UNIT - 1)
                    / ALLOCATION_UNIT * ALLOCATION_UNIT;
                DirInfo->FileInfo.CreationTime = ((PLARGE_INTEGER)&FindData.ftCreationTime)->QuadPart;
                DirInfo->FileInfo.LastAccessTime = ((PLARGE_INTEGER)&FindData.ftLastAccessTime)->QuadPart;
                DirInfo->FileInfo.LastWriteTime = ((PLARGE_INTEGER)&FindData.ftLastWriteTime)->QuadPart;
                DirInfo->FileInfo.ChangeTime = DirInfo->FileInfo.LastWriteTime;
                DirInfo->FileInfo.IndexNumber = 0;
                DirInfo->FileInfo.HardLinks = 0;
                memcpy(DirInfo->FileNameBuf, FindData.cFileName, Length * sizeof(WCHAR));

                if (!FillDirectoryBuffer(&FileDesc->DirBuffer, DirInfo, &DirBufferResult))
                    break;
            } while (FindNextFileW(FindHandle, &FindData));

            FindClose(FindHandle);
        }

        ReleaseDirectoryBuffer(&FileDesc->DirBuffer);
    }

    if (!NT_SUCCESS(DirBufferResult))
        return DirBufferResult;

    ReadDirectoryBuffer(&FileDesc->DirBuffer,
        Marker, Buffer, BufferLength, PBytesTransferred);

    return STATUS_SUCCESS;
}

static NTSTATUS EnableBackupRestorePrivileges(VOID)
{
    union
    {
        TOKEN_PRIVILEGES P;
        UINT8 B[sizeof(TOKEN_PRIVILEGES) + sizeof(LUID_AND_ATTRIBUTES)];
    } Privileges;
    HANDLE Token;

    Privileges.P.PrivilegeCount = 2;
    Privileges.P.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    Privileges.P.Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(0, SE_BACKUP_NAME, &Privileges.P.Privileges[0].Luid) ||
        !LookupPrivilegeValueW(0, SE_RESTORE_NAME, &Privileges.P.Privileges[1].Luid))
        return NtStatusFromWin32(GetLastError());

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &Token))
        return NtStatusFromWin32(GetLastError());

    if (!AdjustTokenPrivileges(Token, FALSE, &Privileges.P, 0, 0, 0))
    {
        CloseHandle(Token);

        return NtStatusFromWin32(GetLastError());
    }

    CloseHandle(Token);

    return STATUS_SUCCESS;
}

static ULONG wcstol_deflt(wchar_t *w, ULONG deflt)
{
    wchar_t *endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

NTSTATUS PTFS_SERVICE::OnStart(ULONG argc, PWSTR *argv)
{

    wchar_t **argp, **arge;
    PWSTR DebugLogFile = 0;
    ULONG DebugFlags = 0;
    PWSTR VolumePrefix = 0;
    PWSTR PassThrough = 0;
    PWSTR MountPoint = 0;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
    WCHAR PassThroughBuf[MAX_PATH];
    PTFS *Ptfs = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'd':
            argtol(DebugFlags);
            break;
        case L'D':
            argtos(DebugLogFile);
            break;
        case L'm':
            argtos(MountPoint);
            break;
        case L'p':
            argtos(PassThrough);
            break;
        case L'u':
            argtos(VolumePrefix);
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (0 == PassThrough && 0 != VolumePrefix)
    {
        PWSTR P;

        P = wcschr(VolumePrefix, L'\\');
        if (0 != P && L'\\' != P[1])
        {
            P = wcschr(P + 1, L'\\');
            if (0 != P &&
                (
                (L'A' <= P[1] && P[1] <= L'Z') ||
                (L'a' <= P[1] && P[1] <= L'z')
                ) &&
                L'$' == P[2])
            {
                StringCbPrintf(PassThroughBuf, sizeof PassThroughBuf, L"%c:%s", P[1], P + 3);
                PassThrough = PassThroughBuf;
            }
        }
    }

    if (0 == PassThrough || 0 == MountPoint)
        goto usage;

    EnableBackupRestorePrivileges();

    if (0 != DebugLogFile)
    {
        if (0 == wcscmp(L"-", DebugLogFile))
            DebugLogHandle = GetStdHandle(STD_ERROR_HANDLE);
        else
            DebugLogHandle = CreateFileW(
                DebugLogFile,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == DebugLogHandle)
        {
            fail(L"cannot open debug log file");
            goto usage;
        }

        FspDebugLogSetHandle(DebugLogHandle);
    }

    Ptfs = new PTFS;

    Ptfs->SetPrefix(VolumePrefix);

    Result = Ptfs->SetPath(PassThrough);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create file system");
        goto exit;
    }

    Result = Ptfs->Mount(MountPoint, 0, FALSE, DebugFlags);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot mount file system");
        goto exit;
    }

    MountPoint = Ptfs->MountPoint();

    info(L"%s%s%s -p %s -m %s",
        L"" PROGNAME,
        0 != VolumePrefix && L'\0' != VolumePrefix[0] ? L" -u " : L"",
            0 != VolumePrefix && L'\0' != VolumePrefix[0] ? VolumePrefix : L"",
        PassThrough,
        MountPoint);

    _Ptfs = Ptfs;
    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != Ptfs)
        delete Ptfs;

    return Result;

usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -d DebugFlags       [-1: enable all debug logs]\n"
        "    -D DebugLogFile     [file path; use - for stderr]\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n"
        "    -p Directory        [directory to expose as pass through file system]\n"
        "    -m MountPoint       [X:|*|directory]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;

#undef argtos
#undef argtol
}
#endif
