using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using WinFspNet;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;

namespace MemFsSharp
{
    partial class RamFS
    {
        private readonly ConcurrentDictionary<string, FileObject> _fileObjects;

        internal class FileObject {
            public string FileName { get; set; }
            public WinFspNet.FileInfo Info { get; protected set; }
            public FileObject() {
                Info = new WinFspNet.FileInfo();
            }

        }
        internal class RamFile:FileObject
        {
      
            public MemoryStream FileData;
            public RamFile(string FName)
            {
                Info.FileAttributes= (uint)FileAttributes.Normal;
                Info.ChangeTime = WinFsp.GetFileTime();
                Info.CreationTime = WinFsp.GetFileTime();
                Info.LastAccessTime = WinFsp.GetFileTime();
                Info.LastWriteTime = WinFsp.GetFileTime();                
                FileData = new MemoryStream();
                FileName = FName;                
            }
        }
        internal class RamDir : FileObject
        {
            public ConcurrentDictionary<string, FileObject> Childeren { get; private set; }
            private long IndexCount=0;
            public RamDir(string FName)
            {                
                Info.FileAttributes = MemFsSharp.FileAttirutes.FILE_ATTRIBUTE_DIRECTORY;
                Childeren = new ConcurrentDictionary<string, FileObject>(StringComparer.OrdinalIgnoreCase);
                FileName = FName;                
                Info.ChangeTime = WinFsp.GetFileTime();
                Info.CreationTime = WinFsp.GetFileTime();
                Info.LastAccessTime = WinFsp.GetFileTime();
                Info.LastWriteTime = WinFsp.GetFileTime();
            }
            public long GetNextIndex() {
                return Interlocked.Increment(ref IndexCount);
            }
        }
        internal FileObject GetFileObject(string path)
        {
            path = path.TrimEnd('\\');
            FileObject retFileObject;
            _fileObjects.TryGetValue(path, out retFileObject);
            return retFileObject;
        }
        uint CreateFileObject(string path, bool isFolder, FileObject parent,out FileObject ret)
        {            
            path = path.TrimEnd('\\');
            var newName = Path.GetFileName(path);
            var dir = parent as RamDir;
            FileObject newFileObject;
            ret = null;
            if (dir != null)
            {

                if (isFolder)
                   ret = newFileObject = new RamDir(newName);
                else
                  ret = newFileObject = new RamFile(newName);                

                if (!dir.Childeren.TryAdd(newName, newFileObject))
                    return NtStatus.STATUS_OBJECT_NAME_COLLISION;
                else
                    newFileObject.Info.IndexNumber = (ulong)dir.GetNextIndex();                    
                
            }
            else
                return NtStatus.STATUS_INVALID_PARAMETER;

            if (!_fileObjects.TryAdd(path, newFileObject))
            {
                return NtStatus.STATUS_OBJECT_NAME_COLLISION;
            }
            ret = newFileObject;
            return NtStatus.STATUS_SUCCESS;
        }
        internal FileObject RemoveFileObject(string fileName, FileObject parent)
        {
            fileName = fileName.TrimEnd('\\');
            var targetFileName = Path.GetFileName(fileName);
            var dir = parent as RamDir;
            FileObject removeFileObject;
            if (dir != null)
            {
                if (!_fileObjects.TryRemove(fileName, out removeFileObject))
                    throw new KeyNotFoundException("FileObject not exist in Db.");
                if (!dir.Childeren.TryRemove(targetFileName, out removeFileObject))
                    throw new KeyNotFoundException("FileObject not exist in Dir.");
            }
            else
                throw new InvalidOperationException("Parent Object can be create in directory only");

            return removeFileObject;
        }
        internal uint MoveObject(string oldPath, string newPath, bool replace)
        {
            oldPath = oldPath.TrimEnd('\\');
            newPath = newPath.TrimEnd('\\');

            var srcPrnt = GetFileObject(Path.GetDirectoryName(oldPath).TrimEnd('\\'));
            var dstPrnt = GetFileObject(Path.GetDirectoryName(newPath).TrimEnd('\\'));
            var src = GetFileObject(oldPath);

            var newName = Path.GetFileName(newPath);
            var oldname = Path.GetFileName(oldPath);
            var dst = GetFileObject(newPath);
            var dstDir = dstPrnt as RamDir;
            var srcDir = srcPrnt as RamDir;
            if (srcPrnt == null || srcDir == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            if (dstPrnt == null || dstDir == null)
                return NtStatus.STATUS_INVALID_PARAMETER;
            if (dst != null)
            {
                if (!replace)
                {
                    return NtStatus.STATUS_OBJECT_NAME_COLLISION;
                }
                else
                {
                    RemoveFileObject(newPath, dstPrnt);
                }
            }          

            src.FileName = newName;
            if (!srcDir.Childeren.TryRemove(oldname, out src))
                throw new KeyNotFoundException("FileObject not found in sohurce dir");
            if (!_fileObjects.TryAdd(newPath, src))
                throw new IOException("FileObject with same name already exist in Db.");
            if (!dstDir.Childeren.TryAdd(newName, src))
                throw new IOException("FileObject with same name already exist in directory.");

            return NtStatus.STATUS_SUCCESS;
        }
    }
}

