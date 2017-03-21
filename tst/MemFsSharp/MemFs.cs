using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using WinFspNet;
namespace MemFsSharp
{
    public class FileNameComparator : IEqualityComparer<string>
    {
        /// <summary>
        /// Has a good distribution.
        /// </summary>
        const int _multiplier = 89;

        /// <summary>
        /// Whether the two strings are equal
        /// </summary>
        public bool Equals(string x, string y)
        {
            return String.Equals(x, y, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Return the hash code for this string.
        /// </summary>
        public int GetHashCode(string obj)
        {
            // Stores the result.
            int result = 0;

            // Don't compute hash code on null object.
            if (obj == null)
            {
                return 0;
            }

            // Get length.
            int length = obj.Length;

            // Return default code for zero-length strings [valid, nothing to hash with].
            if (length > 0)
            {
                // Compute hash for strings with length greater than 1
                char let1 = obj[0];          // First char of string we use
                char let2 = obj[length - 1]; // Final char

                // Compute hash code from two characters
                int part1 = let1 + length;
                result = (_multiplier * part1) + let2 + length;
            }
            return result;
        }
    }
    partial class RamFS : WinFspFileSystem.WinFspMinimalOperation
    {
      
        WinFspConfig Config{ get; set; }
        WinFsp Fsp { get; set; }
        RamDir rootDirectory; 
        VolInfo FsVolumeInfo;
        public RamFS() {
            _fileObjects = new ConcurrentDictionary<string, FileObject>(StringComparer.OrdinalIgnoreCase);
            InitConfig();
            InitFsp();
            
        }
        public void Start() {
            Fsp.StartFs("*");
        }
      
        private void InitConfig() {
            Config = new WinFspConfig();            
            Config.FileSystemName = "RamFs";
            Config.UnicodeOnDisk = true;
            Config.VolumeCreationTime = (uint)DateTime.Now.Ticks;           
            Config.MaxComponentLength = 256;
            Config.FileInfoTimeOut = 25;
                       
        }

        private void InitFsp() {
            Fsp = new WinFsp(Config,this ,null,null);
            FsVolumeInfo = new VolInfo();
            FsVolumeInfo.FreeSize = (ulong)1024 * 1024 *1024 *10;
            FsVolumeInfo.TotalSize= (ulong)1024 * 1024 * 1024 * 10;
            FsVolumeInfo.VolumeLable = "InfyCloud";
            rootDirectory = new RamDir("\\");
            _fileObjects.TryAdd("\\", rootDirectory);
            _fileObjects.TryAdd("", rootDirectory);
        }

        public void Cleanup(WinFspFileSystem FileSystem, FileOpenContext Context, string FileName, uint Flag)
        {
            if (((uint)CleanUpFlags.FspCleanupDelete & Flag) != 0)
            {
                string parentFile = Path.GetDirectoryName(FileName);                
                var dir = GetFileObject(parentFile);
                if (dir != null && dir is RamDir)
                {
                    RemoveFileObject(FileName, dir);
                    Trace.WriteLine($"Delete FileName {FileName} {GetFileObject(FileName)}");
                    Trace.WriteLine($"List Parent Dir- "+string.Join(",",(dir as RamDir).Childeren.Keys.ToArray()));

                }
            }
            
        }

        public void Close(WinFspFileSystem FileSystem, FileOpenContext Context)
        {

        }

        public uint GetVolumeInfo(WinFspFileSystem FileSystem, ref VolInfo VolumeInfo)
        {
            VolumeInfo = FsVolumeInfo;
            return 0;
        }

         public uint SetVolumeLabelA(WinFspFileSystem FileSystem, string VolumeLabel, ref VolInfo VolumeInfo)
        {
            return 0;
        }

         public uint GetSecurityByName(WinFspFileSystem FileSystem, string FileName, ref uint PFileAttributes, SecuirtyDescriptor SecurityDescriptor)
        {
            return NtStatus.STATUS_REQUEST_NOT_ACCEPTED;
        }

         public uint Create(WinFspFileSystem FileSystem, string FileName, uint CreateOptions, uint GrantedAccess, uint FileAttributes, SecuirtyDescriptor SecurityDescriptor, ulong AllocationSize, FileOpenContext Context)
        {
            var parentName = Path.GetDirectoryName(FileName);
            var parent = GetFileObject(parentName);

            if (parent == null)
                return NtStatus.STATUS_OBJECT_PATH_NOT_FOUND;


            var isFolder = (CreateOption.FILE_DIRECTORY_FILE & CreateOptions) > 0;

            FileObject file;
            uint status = CreateFileObject(FileName, isFolder, parent, out file);
          //  Trace.WriteLine($"Create FileName {FileName} {GetFileObject(FileName)} status {String.Format("{0:X}", status)}");
           // Trace.WriteLine($"List Parent Dir- " + string.Join(",", (parent as RamDir).Childeren.Keys.ToArray()));
            if (NtStatus.STATUS_SUCCESS !=status) {
                return status;
            }

            if (file == null)
            {
                return (uint)NtStatus.STATUS_OBJECT_NAME_NOT_FOUND;
            }
            if (!isFolder)
            {
                var ramFile = file as RamFile;
                Context.Node.Info.AllocationSize = AllocationSize;
                ramFile.FileData.SetLength((long)AllocationSize);
                file.Info.FileAttributes = FileAttirutes.FILE_ATTRIBUTE_NORMAL;
            }
            else {
                file.Info.FileAttributes = FileAttirutes.FILE_ATTRIBUTE_DIRECTORY;
            }
            Context.Node.Info = file.Info;
            
            Context.Node.UserContext = file;
            return 0;
        }

         public uint Open(WinFspFileSystem FileSystem, string FileName, uint CreateOptions, uint GrantedAccess, FileOpenContext Context)
        {
            FileName = FileName.TrimEnd('\\');
            var file= GetFileObject(FileName);

            if (file == null)
                return NtStatus.STATUS_OBJECT_NAME_NOT_FOUND;
            Context.Node.Info = file.Info;
            Context.Node.UserContext = file;
            return 0;
        }

         public uint Overwrite(WinFspFileSystem FileSystem, FileOpenContext Context, uint FileAttributes, bool ReplaceFileAttributes)
        {
            if (Context.Node.UserContext == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            var file = Context.Node.UserContext as FileObject;

            file.Info.FileSize = 0;            
            file.Info.LastAccessTime= file.Info.LastWriteTime = WinFsp.GetFileTime();
            return NtStatus.STATUS_SUCCESS;

        }

        public uint Read(WinFspFileSystem FileSystem, FileOpenContext Context, ReadFileBuffer buffer)
        {
        //    Console.WriteLine("READ: BufferLength {0} Offset {1} ", buffer.Length, buffer.Offset);
            if (Context.Node.UserContext == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            
            



            var file = Context.Node.UserContext as RamFile;
            if (file == null)
                return NtStatus.STATUS_INVALID_PARAMETER;

            if ( (ulong)buffer.Offset>= file.Info.FileSize ) 
                return NtStatus.STATUS_END_OF_FILE;

            ulong EndOfOffset = buffer.Offset + buffer.Length;

            if (EndOfOffset > file.Info.FileSize)
                EndOfOffset = file.Info.FileSize;

            int length = (int)( EndOfOffset- buffer.Offset);

            byte[] data = new byte[length];                        
                        
            file.FileData.Seek((long)buffer.Offset, SeekOrigin.Begin);
            file.FileData.Read(data, 0, length);            
            buffer.FillBuffer(ref data,0,(uint)length);
            return NtStatus.STATUS_SUCCESS;
        }

         public uint Write(WinFspFileSystem FileSystem, FileOpenContext Context, WriteFileBuffer buffer)
        {
         //   Console.WriteLine("WRITE: BufferLength {0} Offset {1} IsConstrained {2}", buffer.Length, buffer.Offset, buffer.ConstrainedIo);
            if (Context.Node.UserContext == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
           
            var file = Context.Node.UserContext as RamFile;
           
            if(file==null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            ulong EndOffset = 0;
            if (buffer.ConstrainedIo)
            {
                if (buffer.Offset >= file.Info.FileSize)
                    return NtStatus.STATUS_SUCCESS;
                EndOffset = buffer.Length + buffer.Offset;
                if (EndOffset > file.Info.FileSize)
                    EndOffset = file.Info.FileSize;
            }
            else {
                if (buffer.WriteToEndOfFile)
                    buffer.Offset = file.Info.FileSize;
                EndOffset = buffer.Offset + buffer.Length;
                if (EndOffset > file.Info.FileSize)
                    SetFileSize(FileSystem, Context, EndOffset, false);
            }
            var data = buffer.GetData();
            int effectiveLength = (int)EndOffset - (int)buffer.Offset;
            file.FileData.Seek((long)buffer.Offset, SeekOrigin.Begin);
            file.FileData.Write(data, 0, effectiveLength);
            FsVolumeInfo.FreeSize -= (ulong)effectiveLength;
            buffer.SetTransferedBytes((uint)effectiveLength);

            return NtStatus.STATUS_SUCCESS;
        }

         public uint Flush(WinFspFileSystem FileSystem, FileOpenContext Context)
        {
            return NtStatus.STATUS_SUCCESS;
        }

         public uint GetFileInfo(WinFspFileSystem FileSystem, FileOpenContext Context)
        {            
            return NtStatus.STATUS_SUCCESS;
        }

         public uint SetBasicInfo(WinFspFileSystem FileSystem, FileOpenContext Context, uint FileAttributes, ulong CreationTime, ulong LastAccessTime, ulong LastWriteTime)
        {
            if (Context.Node.UserContext == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            if(FileAttributes!=FileAttirutes.INVALID_ATTRIBUTE)
            Context.Node.Info.FileAttributes = FileAttributes;
            if (CreationTime != 0)
                Context.Node.Info.CreationTime = CreationTime;
            if(LastAccessTime!=0)
            Context.Node.Info.LastAccessTime = LastAccessTime;
            if(LastWriteTime!=0)
            Context.Node.Info.LastWriteTime = LastWriteTime;
            return NtStatus.STATUS_SUCCESS;

        }

        public uint SetFileSize(WinFspFileSystem FileSystem, FileOpenContext Context, ulong NewSize, bool SetAllocationSize)
        {
            if (Context.Node.UserContext == null)
                return NtStatus.STATUS_INVALID_PARAMETER;

            var file = Context.Node.UserContext as RamFile;

            if (file == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            //     Console.WriteLine("Size: Length {0}", NewSize);
            if (SetAllocationSize)
            {
                if (file.Info.AllocationSize != NewSize) 
                    file.Info.AllocationSize = NewSize;
                file.Info.FileSize = NewSize;
                file.FileData.SetLength((long)NewSize);
            }
            else
            {
                if (file.Info.FileSize != NewSize)
                {
                    if (file.Info.AllocationSize < NewSize) {
                        uint AllocationUnit = (uint)Config.SectorSize * (uint)Config.SectorPerAllocationUnit;
                        uint AllocationSize = (uint)(NewSize + AllocationUnit - 1) / AllocationUnit * AllocationUnit;
                        SetFileSize(FileSystem, Context, AllocationSize, true);
                    }
                    file.Info.FileSize = NewSize;
                }        
            }
            return NtStatus.STATUS_SUCCESS;
        }

         public uint CanDelete(WinFspFileSystem FileSystem, FileOpenContext Context, string FileName)
        {           
            FileName = FileName.TrimEnd('\\');
            var dir = GetFileObject(FileName);
            if (dir != null && dir is RamDir)
            {
                var dirL = dir as RamDir;
                if (dirL.Childeren.Count > 0)
                    return NtStatus.STATUS_DIRECTORY_NOT_EMPTY;
            }
            return NtStatus.STATUS_SUCCESS;
        }

         public uint Rename(WinFspFileSystem FileSystem, FileOpenContext Context, string FileName, string NewFileName, bool ReplaceIfExists)
        {
            if (Context == null || Context.Node.UserContext == null || Context.Node.Info == null)
                return NtStatus.STATUS_INVALID_PARAMETER;



            return MoveObject(FileName, NewFileName, ReplaceIfExists);
        }

         public uint SetSecurity(WinFspFileSystem FileSystem, FileOpenContext Context, uint SecurityInformation, SecuirtyDescriptor ModificationDescriptor)
        {
            return NtStatus.STATUS_SUCCESS;
        }

        public uint ReadDirectory(WinFspFileSystem FileSystem, FileOpenContext Context, string Pattern, ReadDirectoryBuffer dirBuffer)
        {
            if (Context == null || Context.Node.UserContext == null || Context.Node.Info == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            var dir = Context.Node.UserContext as RamDir;

            if (dir == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            bool gotMarker = false;

            foreach (var item in dir.Childeren.Values)
            {


                if (!gotMarker)
                    gotMarker = string.IsNullOrEmpty(dirBuffer.Context.Marker) || item.FileName == dirBuffer.Context.Marker;
                if(gotMarker)
                {
                    if (!string.IsNullOrEmpty(Pattern) && !Regex.IsMatch(item.FileName, Pattern, RegexOptions.IgnoreCase))
                        continue;
                    if (!dirBuffer.AddItem(item.FileName, item.Info))
                        break;
                }
            }
            dirBuffer.SetEof();
            return NtStatus.STATUS_SUCCESS;
        }

         public uint GetSecurity(WinFspFileSystem FileSystem, FileOpenContext Context, ref SecuirtyDescriptor descriptor)
        {
            return NtStatus.STATUS_SUCCESS;
        }
    }
}
