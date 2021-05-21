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

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Text;
using System.Threading;

using Fsp;
using VolumeInfo = Fsp.Interop.VolumeInfo;
using FileInfo = Fsp.Interop.FileInfo;
using NotifyInfo = Fsp.Interop.NotifyInfo;
using NotifyAction = Fsp.Interop.NotifyAction;
using NotifyFilter = Fsp.Interop.NotifyFilter;

namespace notifyfs
{
    class Notifyfs : FileSystemBase
    {
        public override Int32 Init(Object Host)
        {
            _Host = (FileSystemHost)Host;
            _Host.SectorSize = ALLOCATION_UNIT;
            _Host.SectorsPerAllocationUnit = 1;
            _Host.FileInfoTimeout = 1000;
            _Host.CaseSensitiveSearch = false;
            _Host.CasePreservedNames = true;
            _Host.UnicodeOnDisk = true;
            _Host.PersistentAcls = false;
            _Host.PostCleanupWhenModifiedOnly = true;
            _Host.VolumeCreationTime = 0;
            _Host.VolumeSerialNumber = 0;

            return STATUS_SUCCESS;
        }
        public override Int32 Mounted(Object Host)
        {
            _Timer = new Timer(this.Tick, null, 0, 1000);

            return STATUS_SUCCESS;
        }
        public override void Unmounted(Object Host)
        {
            WaitHandle Event = new ManualResetEvent(false);
            _Timer.Dispose(Event);
            Event.WaitOne();
        }
        public override Int32 GetVolumeInfo(
            out VolumeInfo VolumeInfo)
        {
            VolumeInfo = default(VolumeInfo);

            return STATUS_SUCCESS;
        }
        public override Int32 GetSecurityByName(
            String FileName,
            out UInt32 FileAttributes/* or ReparsePointIndex */,
            ref Byte[] SecurityDescriptor)
        {
            int Index = FileLookup(FileName);
            if (-1 == Index)
            {
                FileAttributes = default(UInt32);
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            FileAttributes = 0 == Index ? (UInt32)System.IO.FileAttributes.Directory : 0;
            if (null != SecurityDescriptor)
                SecurityDescriptor = DefaultSecurity;

            return STATUS_SUCCESS;
        }
        public override Int32 Open(
            String FileName,
            UInt32 CreateOptions,
            UInt32 GrantedAccess,
            out Object FileNode,
            out Object FileDesc,
            out FileInfo FileInfo,
            out String NormalizedName)
        {
            FileNode = default(Object);
            FileDesc = default(Object);
            FileInfo = default(FileInfo);
            NormalizedName = default(String);

            int Index = FileLookup(FileName);
            if (-1 == Index)
                return STATUS_OBJECT_NAME_NOT_FOUND;

            FileNode = Index;
            FillFileInfo(Index, out FileInfo);

            return STATUS_SUCCESS;
        }
        public override Int32 Read(
            Object FileNode,
            Object FileDesc,
            IntPtr Buffer,
            UInt64 Offset,
            UInt32 Length,
            out UInt32 BytesTransferred)
        {
            int Index = (int)FileNode;
            UInt64 EndOffset;
            Byte[] Contents = FileContents(Index);

            if (Offset >= (UInt64)Contents.Length)
            {
                BytesTransferred = 0;
                return STATUS_END_OF_FILE;
            }

            EndOffset = Offset + Length;
            if (EndOffset > (UInt64)Contents.Length)
                EndOffset = (UInt64)Contents.Length;

            BytesTransferred = (UInt32)(EndOffset - Offset);
            Marshal.Copy(Contents, (int)Offset, Buffer, (int)BytesTransferred);

            return STATUS_SUCCESS;
        }
        public override Int32 GetFileInfo(
            Object FileNode,
            Object FileDesc,
            out FileInfo FileInfo)
        {
            int Index = (int)FileNode;

            FillFileInfo(Index, out FileInfo);

            return STATUS_SUCCESS;
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
            IEnumerator<String> Enumerator = (IEnumerator<String>)Context;

            if (null == Enumerator)
            {
                List<String> ChildrenFileNames = new List<String>();
                for (int Index = 1, Count = FileCount(); Count >= Index; Index++)
                    ChildrenFileNames.Add(String.Format("{0}", Index));
                Context = Enumerator = ChildrenFileNames.GetEnumerator();
            }

            while (Enumerator.MoveNext())
            {
                FileName = Enumerator.Current;
                FillFileInfo(int.Parse(FileName), out FileInfo);
                return true;
            }

            FileName = default(String);
            FileInfo = default(FileInfo);
            return false;
        }

        private static int CountFromTicks(int Ticks)
        {
            /*
             * The formula below produces the periodic sequence:
             *     0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
             *     0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
             *     ...
             */
            int div10 = (Ticks % 20) / 10;
            int mod10 = Ticks % 10;
            int mdv10 = 1 - div10;
            int mmd10 = 10 - mod10;
            return mdv10 * mod10 + div10 * mmd10;
        }
        private int FileCount()
        {
            int Ticks = Thread.VolatileRead(ref _Ticks);
            return CountFromTicks(Ticks);
        }
        private int FileLookup(String FileName)
        {
            FileName = FileName.Substring(1);
            if ("" == FileName)
                return 0;   /* root */
            int Count = FileCount();
            Boolean Valid = int.TryParse(FileName, out int Index);
            if (!Valid || 0 >= Index || Index > Count)
                return -1;  /* not found */
            return Index;   /* regular file named 1, 2, ..., Count */
        }
        private static Byte[] FileContents(int Index)
        {
            if (0 == Index)
                return EmptyByteArray;
            return Encoding.UTF8.GetBytes(String.Format("{0}\n", Index));
        }
        private static void FillFileInfo(int Index, out FileInfo FileInfo)
        {
            FileInfo = default(FileInfo);
            FileInfo.FileAttributes = 0 == Index ? (UInt32)System.IO.FileAttributes.Directory : 0;
            FileInfo.FileSize = (UInt64)FileContents(Index).Length;
            FileInfo.AllocationSize = (FileInfo.FileSize + ALLOCATION_UNIT - 1)
                / ALLOCATION_UNIT * ALLOCATION_UNIT;
            FileInfo.CreationTime =
            FileInfo.LastAccessTime =
            FileInfo.LastWriteTime =
            FileInfo.ChangeTime = (UInt64)DateTime.Now.ToFileTimeUtc();
        }
        private void Tick(Object Context)
        {
            int Ticks = Interlocked.Increment(ref _Ticks);
            int OldCount = CountFromTicks(Ticks - 1);
            int NewCount = CountFromTicks(Ticks);
            NotifyInfo[] NotifyInfo = new NotifyInfo[1];

            if (OldCount < NewCount)
            {
                NotifyInfo[0].FileName = String.Format("\\{0}", NewCount);
                NotifyInfo[0].Action = NotifyAction.Added;
                NotifyInfo[0].Filter = NotifyFilter.ChangeFileName;
                Console.Error.WriteLine("CREATE \\{0}", NewCount);
            }
            else if (OldCount > NewCount)
            {
                NotifyInfo[0].FileName = String.Format("\\{0}", OldCount);
                NotifyInfo[0].Action = NotifyAction.Removed;
                NotifyInfo[0].Filter = NotifyFilter.ChangeFileName;
                Console.Error.WriteLine("REMOVE \\{0}", OldCount);
            }

            if (OldCount != NewCount)
            {
                if (STATUS_SUCCESS == _Host.NotifyBegin(500))
                {
                    _Host.Notify(NotifyInfo);
                    _Host.NotifyEnd();
                }
            }
        }

        static Notifyfs()
        {
            RawSecurityDescriptor RootSecurityDescriptor = new RawSecurityDescriptor(
                "O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)");
            DefaultSecurity = new Byte[RootSecurityDescriptor.BinaryLength];
            RootSecurityDescriptor.GetBinaryForm(DefaultSecurity, 0);
        }

        private const int ALLOCATION_UNIT = 4096;
        private static readonly Byte[] EmptyByteArray = new Byte[0];
        private static readonly Byte[] DefaultSecurity;
        private FileSystemHost _Host;
        private Timer _Timer;
        private int _Ticks;
    }

    class NotifyfsService : Service
    {
        private class CommandLineUsageException : Exception
        {
            public CommandLineUsageException(String Message = null) : base(Message)
            {
                HasMessage = null != Message;
            }

            public bool HasMessage;
        }

        private const String PROGNAME = "notifyfs-dotnet";

        public NotifyfsService() : base("NotifyfsService")
        {
        }

        protected override void OnStart(String[] Args)
        {
            try
            {
                String VolumePrefix = null;
                String MountPoint = null;
                FileSystemHost Host = null;
                Notifyfs Notifyfs = null;
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
                    case 'm':
                        argtos(Args, ref I, ref MountPoint);
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

                if (null == MountPoint)
                    throw new CommandLineUsageException();

                FileSystemHost.SetDebugLogFile("-");

                Host = new FileSystemHost(Notifyfs = new Notifyfs());
                Host.Prefix = VolumePrefix;
                if (0 > Host.Mount(MountPoint))
                    throw new IOException("cannot mount file system");
                MountPoint = Host.MountPoint();
                _Host = Host;

                Log(EVENTLOG_INFORMATION_TYPE, String.Format("{0}{1}{2} -m {3}",
                    PROGNAME,
                    null != VolumePrefix && 0 < VolumePrefix.Length ? " -u " : "",
                        null != VolumePrefix && 0 < VolumePrefix.Length ? VolumePrefix : "",
                    MountPoint));
            }
            catch (CommandLineUsageException ex)
            {
                Log(EVENTLOG_ERROR_TYPE, String.Format(
                    "{0}" +
                    "usage: {1} OPTIONS\n" +
                    "\n" +
                    "options:\n" +
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

        private FileSystemHost _Host;
    }

    class Program
    {
        static void Main(string[] args)
        {
            Environment.ExitCode = new NotifyfsService().Run();
        }
    }
}
