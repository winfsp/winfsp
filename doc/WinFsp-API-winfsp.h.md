# winfsp/winfsp.h

WinFsp User Mode API.

In order to use the WinFsp API the user mode file system must include <winfsp/winfsp.h>
and link with the winfsp\_x64.dll (or winfsp\_x86.dll) library.

## FILE SYSTEM

A user mode file system is a program that uses the WinFsp API to expose a file system to
Windows. The user mode file system must implement the operations in FSP\_FILE\_SYSTEM\_INTERFACE,
create a file system object using FspFileSystemCreate and start its dispatcher using
FspFileSystemStartDispatcher. At that point it will start receiving file system requests on the
FSP\_FILE\_SYSTEM\_INTERFACE operations.

### Classes

<details>
<summary>
<b>FSP_FILE_SYSTEM_INTERFACE</b> - File system interface.
</summary>
<blockquote>
<br/>

**Discussion**

The operations in this interface must be implemented by the user mode
file system. Not all operations need be implemented. For example,
a user mode file system that does not wish to support reparse points,
need not implement the reparse point operations.

Most of the operations accept a FileContext parameter. This parameter
has different meanings depending on the value of the FSP\_FSCTL\_VOLUME\_PARAMS
flags UmFileContextIsUserContext2 and UmFileContextIsFullContext.

There are three cases to consider:

- When both of these flags are unset (default), the FileContext parameter
represents the file node. The file node is a void pointer (or an integer
that can fit in a pointer) that is used to uniquely identify an open file.
Opening the same file name should always yield the same file node value
for as long as the file with that name remains open anywhere in the system.


- When the UmFileContextIsUserContext2 is set, the FileContext parameter
represents the file descriptor. The file descriptor is a void pointer (or
an integer that can fit in a pointer) that is used to identify an open
instance of a file. Opening the same file name may yield a different file
descriptor.


- When the UmFileContextIsFullContext is set, the FileContext parameter
is a pointer to a FSP\_FSCTL\_TRANSACT\_FULL\_CONTEXT. This allows a user mode
file system to access the low-level UserContext and UserContext2 values.
The UserContext is used to store the file node and the UserContext2 is
used to store the file descriptor for an open file.

#### Member Functions

<details>
<summary>
<b>CanDelete</b> - Determine whether a file or directory can be deleted.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *CanDelete)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PWSTR FileName);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to test for deletion.
- _FileName_ \- The name of the file or directory to test for deletion.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function tests whether a file or directory can be safely deleted. This function does
not need to perform access checks, but may performs tasks such as check for empty
directories, etc.

This function should **NEVER** delete the file or directory in question. Deletion should
happen during Cleanup with the FspCleanupDelete flag set.

This function gets called when Win32 API's such as DeleteFile or RemoveDirectory are used.
It does not get called when a file or directory is opened with FILE\_DELETE\_ON\_CLOSE.

NOTE: If both CanDelete and SetDelete are defined, SetDelete takes precedence. However
most file systems need only implement the CanDelete operation.

**See Also**

- Cleanup
- SetDelete


</blockquote>
</details>

<details>
<summary>
<b>Cleanup</b> - Cleanup a file.
</summary>
<blockquote>
<br/>

```c
VOID ( *Cleanup)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PWSTR FileName,
    ULONG Flags);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to cleanup.
- _FileName_ \- The name of the file or directory to cleanup. Sent only when a Delete is requested.
- _Flags_ \- These flags determine whether the file was modified and whether to delete the file.

**Discussion**

When CreateFile is used to open or create a file the kernel creates a kernel mode file
object (type FILE\_OBJECT) and a handle for it, which it returns to user-mode. The handle may
be duplicated (using DuplicateHandle), but all duplicate handles always refer to the same
file object. When all handles for a particular file object get closed (using CloseHandle)
the system sends a Cleanup request to the file system.

There will be a Cleanup operation for every Create or Open operation posted to the user mode
file system. However the Cleanup operation is **not** the final close operation on a file.
The file system must be ready to receive additional operations until close time. This is true
even when the file is being deleted!

The Flags parameter contains information about the cleanup operation:

- FspCleanupDelete -
An important function of the Cleanup operation is to complete a delete operation. Deleting
a file or directory in Windows is a three-stage process where the file is first opened, then
tested to see if the delete can proceed and if the answer is positive the file is then
deleted during Cleanup.

If the file system supports POSIX unlink (FSP\_FSCTL\_VOLUME\_PARAMS ::
SupportsPosixUnlinkRename), then a Cleanup / FspCleanupDelete operation may arrive while
there are other open file handles for this particular file node. If the file system does not
support POSIX unlink, then a Cleanup / FspCleanupDelete operation will always be the last
outstanding cleanup for this particular file node.


- FspCleanupSetAllocationSize -
The NTFS and FAT file systems reset a file's allocation size when they receive the last
outstanding cleanup for a particular file node. User mode file systems that implement
allocation size and wish to duplicate the NTFS and FAT behavior can use this flag.


- FspCleanupSetArchiveBit -
File systems that support the archive bit should set the file node's archive bit when this
flag is set.


- FspCleanupSetLastAccessTime, FspCleanupSetLastWriteTime, FspCleanupSetChangeTime - File
systems should set the corresponding file time when each one of these flags is set. Note that
updating the last access time is expensive and a file system may choose to not implement it.



There is no way to report failure of this operation. This is a Windows limitation.

As an optimization a file system may specify the FSP\_FSCTL\_VOLUME\_PARAMS ::
PostCleanupWhenModifiedOnly flag. In this case the FSD will only post Cleanup requests when
the file was modified/deleted.

**See Also**

- Close
- CanDelete
- SetDelete


</blockquote>
</details>

<details>
<summary>
<b>Close</b> - Close a file.
</summary>
<blockquote>
<br/>

```c
VOID ( *Close)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to be closed.


</blockquote>
</details>

<details>
<summary>
<b>Control</b> - Process control code.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Control)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    UINT32 ControlCode, 
    PVOID InputBuffer,
    ULONG InputBufferLength, 
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG PBytesTransferred);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to be controled.
- _ControlCode_ \- The control code for the operation. This code must have a DeviceType with bit
0x8000 set and must have a TransferType of METHOD\_BUFFERED.
- _InputBuffer_ \- Pointer to a buffer that contains the input data.
- _InputBufferLength_ \- Input data length.
- _OutputBuffer_ \- Pointer to a buffer that will receive the output data.
- _OutputBufferLength_ \- Output data length.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes transferred.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function is called when a program uses the DeviceIoControl API.


</blockquote>
</details>

<details>
<summary>
<b>Create</b> - Create new file or directory.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Create)(
    FSP_FILE_SYSTEM *FileSystem, 
    PWSTR FileName,
    UINT32 CreateOptions,
    UINT32 GrantedAccess, 
    UINT32 FileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    UINT64 AllocationSize, 
    PVOID *PFileContext,
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileName_ \- The name of the file or directory to be created.
- _CreateOptions_ \- Create options for this request. This parameter has the same meaning as the
CreateOptions parameter of the NtCreateFile API. User mode file systems should typically
only be concerned with the flag FILE\_DIRECTORY\_FILE, which is an instruction to create a
directory rather than a file. Some file systems may also want to pay attention to the
FILE\_NO\_INTERMEDIATE\_BUFFERING and FILE\_WRITE\_THROUGH flags, although these are
typically handled by the FSD component.
- _GrantedAccess_ \- Determines the specific access rights that have been granted for this request. Upon
receiving this call all access checks have been performed and the user mode file system
need not perform any additional checks. However this parameter may be useful to a user
mode file system; for example the WinFsp-FUSE layer uses this parameter to determine
which flags to use in its POSIX open() call.
- _FileAttributes_ \- File attributes to apply to the newly created file or directory.
- _SecurityDescriptor_ \- Security descriptor to apply to the newly created file or directory. This security
descriptor will always be in self-relative format. Its length can be retrieved using the
Windows GetSecurityDescriptorLength API. Will be NULL for named streams.
- _AllocationSize_ \- Allocation size for the newly created file.
- _PFileContext_ \- [out]
Pointer that will receive the file context on successful return from this call.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>CreateEx</b> - Create new file or directory.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *CreateEx)(
    FSP_FILE_SYSTEM *FileSystem, 
    PWSTR FileName,
    UINT32 CreateOptions,
    UINT32 GrantedAccess, 
    UINT32 FileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    UINT64 AllocationSize, 
    PVOID ExtraBuffer,
    ULONG ExtraLength,
    BOOLEAN ExtraBufferIsReparsePoint, 
    PVOID *PFileContext,
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileName_ \- The name of the file or directory to be created.
- _CreateOptions_ \- Create options for this request. This parameter has the same meaning as the
CreateOptions parameter of the NtCreateFile API. User mode file systems should typically
only be concerned with the flag FILE\_DIRECTORY\_FILE, which is an instruction to create a
directory rather than a file. Some file systems may also want to pay attention to the
FILE\_NO\_INTERMEDIATE\_BUFFERING and FILE\_WRITE\_THROUGH flags, although these are
typically handled by the FSD component.
- _GrantedAccess_ \- Determines the specific access rights that have been granted for this request. Upon
receiving this call all access checks have been performed and the user mode file system
need not perform any additional checks. However this parameter may be useful to a user
mode file system; for example the WinFsp-FUSE layer uses this parameter to determine
which flags to use in its POSIX open() call.
- _FileAttributes_ \- File attributes to apply to the newly created file or directory.
- _SecurityDescriptor_ \- Security descriptor to apply to the newly created file or directory. This security
descriptor will always be in self-relative format. Its length can be retrieved using the
Windows GetSecurityDescriptorLength API. Will be NULL for named streams.
- _AllocationSize_ \- Allocation size for the newly created file.
- _ExtraBuffer_ \- Extended attributes or reparse point buffer.
- _ExtraLength_ \- Extended attributes or reparse point buffer length.
- _ExtraBufferIsReparsePoint_ \- FALSE: extra buffer is extended attributes; TRUE: extra buffer is reparse point.
- _PFileContext_ \- [out]
Pointer that will receive the file context on successful return from this call.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function works like Create, except that it also accepts an extra buffer that
may contain extended attributes or a reparse point.

NOTE: If both Create and CreateEx are defined, CreateEx takes precedence.


</blockquote>
</details>

<details>
<summary>
<b>DeleteReparsePoint</b> - Delete reparse point.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *DeleteReparsePoint)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    PWSTR FileName,
    PVOID Buffer,
    SIZE_T Size);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the reparse point.
- _FileName_ \- The file name of the reparse point.
- _Buffer_ \- Pointer to a buffer that contains the data for this operation.
- _Size_ \- Size of data to write.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>Flush</b> - Flush a file or volume.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Flush)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to be flushed. When NULL the whole volume is being flushed.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc. Used when
flushing file (not volume).

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

Note that the FSD will also flush all file/volume caches prior to invoking this operation.


</blockquote>
</details>

<details>
<summary>
<b>GetDirInfoByName</b> - Get directory information for a single file or directory within a parent directory.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetDirInfoByName)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PWSTR FileName, 
    FSP_FSCTL_DIR_INFO *DirInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the parent directory.
- _FileName_ \- The name of the file or directory to get information for. This name is relative
to the parent directory and is a single path component.
- _DirInfo_ \- [out]
Pointer to a structure that will receive the directory information on successful
return from this call. This information includes the file name, but also file
attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>GetEa</b> - Get extended attributes.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetEa)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    PFILE_FULL_EA_INFORMATION Ea,
    ULONG EaLength,
    PULONG PBytesTransferred);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to get extended attributes for.
- _Ea_ \- Extended attributes buffer.
- _EaLength_ \- Extended attributes buffer length.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes transferred.

**Return Value**

STATUS\_SUCCESS or error code.

**See Also**

- SetEa
- FspFileSystemAddEa


</blockquote>
</details>

<details>
<summary>
<b>GetFileInfo</b> - Get file or directory information.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetFileInfo)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to get information for.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>GetReparsePoint</b> - Get reparse point.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetReparsePoint)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    PWSTR FileName,
    PVOID Buffer,
    PSIZE_T PSize);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the reparse point.
- _FileName_ \- The file name of the reparse point.
- _Buffer_ \- Pointer to a buffer that will receive the results of this operation. If
the function returns a symbolic link path, it should not be NULL terminated.
- _PSize_ \- [in,out]
Pointer to the buffer size. On input it contains the size of the buffer.
On output it will contain the actual size of data copied.

**Return Value**

STATUS\_SUCCESS or error code.

**See Also**

- SetReparsePoint


</blockquote>
</details>

<details>
<summary>
<b>GetSecurity</b> - Get file or directory security descriptor.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetSecurity)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    SIZE_T *PSecurityDescriptorSize);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to get the security descriptor for.
- _SecurityDescriptor_ \- Pointer to a buffer that will receive the file security descriptor on successful return
from this call. May be NULL.
- _PSecurityDescriptorSize_ \- [in,out]
Pointer to the security descriptor buffer size. On input it contains the size of the
security descriptor buffer. On output it will contain the actual size of the security
descriptor copied into the security descriptor buffer. Cannot be NULL.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>GetSecurityByName</b> - Get file or directory attributes and security descriptor given a file name.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetSecurityByName)(
    FSP_FILE_SYSTEM *FileSystem, 
    PWSTR FileName,
    PUINT32 PFileAttributes/* or ReparsePointIndex */, 
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    SIZE_T *PSecurityDescriptorSize);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileName_ \- The name of the file or directory to get the attributes and security descriptor for.
- _PFileAttributes_ \- Pointer to a memory location that will receive the file attributes on successful return
from this call. May be NULL.

If this call returns STATUS\_REPARSE, the file system MAY place here the index of the
first reparse point within FileName. The file system MAY also leave this at its default
value of 0.
- _SecurityDescriptor_ \- Pointer to a buffer that will receive the file security descriptor on successful return
from this call. May be NULL.
- _PSecurityDescriptorSize_ \- [in,out]
Pointer to the security descriptor buffer size. On input it contains the size of the
security descriptor buffer. On output it will contain the actual size of the security
descriptor copied into the security descriptor buffer. May be NULL.

**Return Value**

STATUS\_SUCCESS, STATUS\_REPARSE or error code.

STATUS\_REPARSE should be returned by file systems that support reparse points when
they encounter a FileName that contains reparse points anywhere but the final path
component.


</blockquote>
</details>

<details>
<summary>
<b>GetStreamInfo</b> - Get named streams information.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetStreamInfo)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PVOID Buffer,
    ULONG Length, 
    PULONG PBytesTransferred);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to get stream information for.
- _Buffer_ \- Pointer to a buffer that will receive the stream information.
- _Length_ \- Length of buffer.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes stored.

**Return Value**

STATUS\_SUCCESS or error code.

**See Also**

- FspFileSystemAddStreamInfo


</blockquote>
</details>

<details>
<summary>
<b>GetVolumeInfo</b> - Get volume information.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *GetVolumeInfo)(
    FSP_FILE_SYSTEM *FileSystem, 
    FSP_FSCTL_VOLUME_INFO *VolumeInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _VolumeInfo_ \- [out]
Pointer to a structure that will receive the volume information on successful return
from this call.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>Open</b> - Open a file or directory.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Open)(
    FSP_FILE_SYSTEM *FileSystem, 
    PWSTR FileName,
    UINT32 CreateOptions,
    UINT32 GrantedAccess, 
    PVOID *PFileContext,
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileName_ \- The name of the file or directory to be opened.
- _CreateOptions_ \- Create options for this request. This parameter has the same meaning as the
CreateOptions parameter of the NtCreateFile API. User mode file systems typically
do not need to do anything special with respect to this parameter. Some file systems may
also want to pay attention to the FILE\_NO\_INTERMEDIATE\_BUFFERING and FILE\_WRITE\_THROUGH
flags, although these are typically handled by the FSD component.
- _GrantedAccess_ \- Determines the specific access rights that have been granted for this request. Upon
receiving this call all access checks have been performed and the user mode file system
need not perform any additional checks. However this parameter may be useful to a user
mode file system; for example the WinFsp-FUSE layer uses this parameter to determine
which flags to use in its POSIX open() call.
- _PFileContext_ \- [out]
Pointer that will receive the file context on successful return from this call.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>Overwrite</b> - Overwrite a file.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Overwrite)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    UINT32 FileAttributes,
    BOOLEAN ReplaceFileAttributes,
    UINT64 AllocationSize, 
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to overwrite.
- _FileAttributes_ \- File attributes to apply to the overwritten file.
- _ReplaceFileAttributes_ \- When TRUE the existing file attributes should be replaced with the new ones.
When FALSE the existing file attributes should be merged (or'ed) with the new ones.
- _AllocationSize_ \- Allocation size for the overwritten file.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>OverwriteEx</b> - Overwrite a file.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *OverwriteEx)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    UINT32 FileAttributes,
    BOOLEAN ReplaceFileAttributes,
    UINT64 AllocationSize, 
    PFILE_FULL_EA_INFORMATION Ea,
    ULONG EaLength, 
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to overwrite.
- _FileAttributes_ \- File attributes to apply to the overwritten file.
- _ReplaceFileAttributes_ \- When TRUE the existing file attributes should be replaced with the new ones.
When FALSE the existing file attributes should be merged (or'ed) with the new ones.
- _AllocationSize_ \- Allocation size for the overwritten file.
- _Ea_ \- Extended attributes buffer.
- _EaLength_ \- Extended attributes buffer length.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function works like Overwrite, except that it also accepts EA (extended attributes).

NOTE: If both Overwrite and OverwriteEx are defined, OverwriteEx takes precedence.


</blockquote>
</details>

<details>
<summary>
<b>Read</b> - Read a file.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Read)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length, 
    PULONG PBytesTransferred);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to be read.
- _Buffer_ \- Pointer to a buffer that will receive the results of the read operation.
- _Offset_ \- Offset within the file to read from.
- _Length_ \- Length of data to read.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes read.

**Return Value**

STATUS\_SUCCESS or error code. STATUS\_PENDING is supported allowing for asynchronous
operation.


</blockquote>
</details>

<details>
<summary>
<b>ReadDirectory</b> - Read a directory.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *ReadDirectory)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PWSTR Pattern,
    PWSTR Marker, 
    PVOID Buffer,
    ULONG Length,
    PULONG PBytesTransferred);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the directory to be read.
- _Pattern_ \- The pattern to match against files in this directory. Can be NULL. The file system
can choose to ignore this parameter as the FSD will always perform its own pattern
matching on the returned results.
- _Marker_ \- A file name that marks where in the directory to start reading. Files with names
that are greater than (not equal to) this marker (in the directory order determined
by the file system) should be returned. Can be NULL.
- _Buffer_ \- Pointer to a buffer that will receive the results of the read operation.
- _Length_ \- Length of data to read.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes read.

**Return Value**

STATUS\_SUCCESS or error code. STATUS\_PENDING is supported allowing for asynchronous
operation.

**See Also**

- FspFileSystemAddDirInfo


</blockquote>
</details>

<details>
<summary>
<b>Rename</b> - Renames a file or directory.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Rename)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    PWSTR FileName,
    PWSTR NewFileName,
    BOOLEAN ReplaceIfExists);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to be renamed.
- _FileName_ \- The current name of the file or directory to rename.
- _NewFileName_ \- The new name for the file or directory.
- _ReplaceIfExists_ \- Whether to replace a file that already exists at NewFileName.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

The kernel mode FSD provides certain guarantees prior to posting a rename operation:

- A file cannot be renamed if a file with the same name exists and has open handles.


- A directory cannot be renamed if it or any of its subdirectories contains a file that
has open handles.


</blockquote>
</details>

<details>
<summary>
<b>ResolveReparsePoints</b> - Resolve reparse points.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *ResolveReparsePoints)(
    FSP_FILE_SYSTEM *FileSystem, 
    PWSTR FileName,
    UINT32 ReparsePointIndex,
    BOOLEAN ResolveLastPathComponent, 
    PIO_STATUS_BLOCK PIoStatus,
    PVOID Buffer,
    PSIZE_T PSize);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileName_ \- The name of the file or directory to have its reparse points resolved.
- _ReparsePointIndex_ \- The index of the first reparse point within FileName.
- _ResolveLastPathComponent_ \- If FALSE, the last path component of FileName should not be resolved, even
if it is a reparse point that can be resolved. If TRUE, all path components
should be resolved if possible.
- _PIoStatus_ \- Pointer to storage that will receive the status to return to the FSD. When
this function succeeds it must set PIoStatus->Status to STATUS\_REPARSE and
PIoStatus->Information to either IO\_REPARSE or the reparse tag.
- _Buffer_ \- Pointer to a buffer that will receive the resolved file name (IO\_REPARSE) or
reparse data (reparse tag). If the function returns a file name, it should
not be NULL terminated.
- _PSize_ \- [in,out]
Pointer to the buffer size. On input it contains the size of the buffer.
On output it will contain the actual size of data copied.

**Return Value**

STATUS\_REPARSE or error code.

**Discussion**

Reparse points are a general mechanism for attaching special behavior to files.
A file or directory can contain a reparse point. A reparse point is data that has
special meaning to the file system, Windows or user applications. For example, NTFS
and Windows use reparse points to implement symbolic links. As another example,
a particular file system may use reparse points to emulate UNIX FIFO's.

This function is expected to resolve as many reparse points as possible. If a reparse
point is encountered that is not understood by the file system further reparse point
resolution should stop; the reparse point data should be returned to the FSD with status
STATUS\_REPARSE/reparse-tag. If a reparse point (symbolic link) is encountered that is
understood by the file system but points outside it, the reparse point should be
resolved, but further reparse point resolution should stop; the resolved file name
should be returned to the FSD with status STATUS\_REPARSE/IO\_REPARSE.


</blockquote>
</details>

<details>
<summary>
<b>SetBasicInfo</b> - Set file or directory basic information.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *SetBasicInfo)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    UINT32 FileAttributes, 
    UINT64 CreationTime,
    UINT64 LastAccessTime,
    UINT64 LastWriteTime,
    UINT64 ChangeTime, 
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to set information for.
- _FileAttributes_ \- File attributes to apply to the file or directory. If the value INVALID\_FILE\_ATTRIBUTES
is sent, the file attributes should not be changed.
- _CreationTime_ \- Creation time to apply to the file or directory. If the value 0 is sent, the creation
time should not be changed.
- _LastAccessTime_ \- Last access time to apply to the file or directory. If the value 0 is sent, the last
access time should not be changed.
- _LastWriteTime_ \- Last write time to apply to the file or directory. If the value 0 is sent, the last
write time should not be changed.
- _ChangeTime_ \- Change time to apply to the file or directory. If the value 0 is sent, the change time
should not be changed.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>SetDelete</b> - Set the file delete flag.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *SetDelete)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PWSTR FileName,
    BOOLEAN DeleteFile);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to set the delete flag for.
- _FileName_ \- The name of the file or directory to set the delete flag for.
- _DeleteFile_ \- If set to TRUE the FSD indicates that the file will be deleted on Cleanup; otherwise
it will not be deleted. It is legal to receive multiple SetDelete calls for the same
file with different DeleteFile parameters.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function sets a flag to indicates whether the FSD file should delete a file
when it is closed. This function does not need to perform access checks, but may
performs tasks such as check for empty directories, etc.

This function should **NEVER** delete the file or directory in question. Deletion should
happen during Cleanup with the FspCleanupDelete flag set.

This function gets called when Win32 API's such as DeleteFile or RemoveDirectory are used.
It does not get called when a file or directory is opened with FILE\_DELETE\_ON\_CLOSE.

NOTE: If both CanDelete and SetDelete are defined, SetDelete takes precedence. However
most file systems need only implement the CanDelete operation.

**See Also**

- Cleanup
- CanDelete


</blockquote>
</details>

<details>
<summary>
<b>SetEa</b> - Set extended attributes.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *SetEa)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    PFILE_FULL_EA_INFORMATION Ea,
    ULONG EaLength, 
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to set extended attributes for.
- _Ea_ \- Extended attributes buffer.
- _EaLength_ \- Extended attributes buffer length.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.

**See Also**

- GetEa


</blockquote>
</details>

<details>
<summary>
<b>SetFileSize</b> - Set file/allocation size.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *SetFileSize)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    UINT64 NewSize,
    BOOLEAN SetAllocationSize, 
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to set the file/allocation size for.
- _NewSize_ \- New file/allocation size to apply to the file.
- _SetAllocationSize_ \- If TRUE, then the allocation size is being set. if FALSE, then the file size is being set.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function is used to change a file's sizes. Windows file systems maintain two kinds
of sizes: the file size is where the End Of File (EOF) is, and the allocation size is the
actual size that a file takes up on the "disk".

The rules regarding file/allocation size are:

- Allocation size must always be aligned to the allocation unit boundary. The allocation
unit is the product `(UINT64)SectorSize \* (UINT64)SectorsPerAllocationUnit` from
the FSP\_FSCTL\_VOLUME\_PARAMS structure. The FSD will always send properly aligned allocation
sizes when setting the allocation size.


- Allocation size is always greater or equal to the file size.


- A file size of more than the current allocation size will also extend the allocation
size to the next allocation unit boundary.


- An allocation size of less than the current file size should also truncate the current
file size.


</blockquote>
</details>

<details>
<summary>
<b>SetReparsePoint</b> - Set reparse point.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *SetReparsePoint)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    PWSTR FileName,
    PVOID Buffer,
    SIZE_T Size);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the reparse point.
- _FileName_ \- The file name of the reparse point.
- _Buffer_ \- Pointer to a buffer that contains the data for this operation. If this buffer
contains a symbolic link path, it should not be assumed to be NULL terminated.
- _Size_ \- Size of data to write.

**Return Value**

STATUS\_SUCCESS or error code.

**See Also**

- GetReparsePoint


</blockquote>
</details>

<details>
<summary>
<b>SetSecurity</b> - Set file or directory security descriptor.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *SetSecurity)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext, 
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR ModificationDescriptor);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file or directory to set the security descriptor for.
- _SecurityInformation_ \- Describes what parts of the file or directory security descriptor should
be modified.
- _ModificationDescriptor_ \- Describes the modifications to apply to the file or directory security descriptor.

**Return Value**

STATUS\_SUCCESS or error code.

**See Also**

- FspSetSecurityDescriptor
- FspDeleteSecurityDescriptor


</blockquote>
</details>

<details>
<summary>
<b>SetVolumeLabel</b> - Set volume label.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *SetVolumeLabel)(
    FSP_FILE_SYSTEM *FileSystem, 
    PWSTR VolumeLabel, 
    FSP_FSCTL_VOLUME_INFO *VolumeInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _VolumeLabel_ \- The new label for the volume.
- _VolumeInfo_ \- [out]
Pointer to a structure that will receive the volume information on successful return
from this call.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>Write</b> - Write a file.
</summary>
<blockquote>
<br/>

```c
NTSTATUS ( *Write)(
    FSP_FILE_SYSTEM *FileSystem, 
    PVOID FileContext,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length, 
    BOOLEAN WriteToEndOfFile,
    BOOLEAN ConstrainedIo, 
    PULONG PBytesTransferred,
    FSP_FSCTL_FILE_INFO *FileInfo);  
```

**Parameters**

- _FileSystem_ \- The file system on which this request is posted.
- _FileContext_ \- The file context of the file to be written.
- _Buffer_ \- Pointer to a buffer that contains the data to write.
- _Offset_ \- Offset within the file to write to.
- _Length_ \- Length of data to write.
- _WriteToEndOfFile_ \- When TRUE the file system must write to the current end of file. In this case the Offset
parameter will contain the value -1.
- _ConstrainedIo_ \- When TRUE the file system must not extend the file (i.e. change the file size).
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes written.
- _FileInfo_ \- [out]
Pointer to a structure that will receive the file information on successful return
from this call. This information includes file attributes, file times, etc.

**Return Value**

STATUS\_SUCCESS or error code. STATUS\_PENDING is supported allowing for asynchronous
operation.


</blockquote>
</details>


</blockquote>
</details>

### Functions

<details>
<summary>
<b>FspDeleteSecurityDescriptor</b> - Delete security descriptor.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspDeleteSecurityDescriptor(
    PSECURITY_DESCRIPTOR SecurityDescriptor, 
    NTSTATUS (*CreateFunc)());  
```

**Parameters**

- _SecurityDescriptor_ \- The security descriptor to be deleted.
- _CreateFunc_ \- Function used to create the security descriptor. This parameter should be
set to FspSetSecurityDescriptor for the public API.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This is a helper for implementing the SetSecurity operation.

**See Also**

- SetSecurity
- FspSetSecurityDescriptor


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemAddDirInfo</b> - Add directory information to a buffer.
</summary>
<blockquote>
<br/>

```c
FSP_API BOOLEAN FspFileSystemAddDirInfo(
    FSP_FSCTL_DIR_INFO *DirInfo, 
    PVOID Buffer,
    ULONG Length,
    PULONG PBytesTransferred);  
```

**Parameters**

- _DirInfo_ \- The directory information to add. A value of NULL acts as an EOF marker for a ReadDirectory
operation.
- _Buffer_ \- Pointer to a buffer that will receive the results of the read operation. This should contain
the same value passed to the ReadDirectory Buffer parameter.
- _Length_ \- Length of data to read. This should contain the same value passed to the ReadDirectory
Length parameter.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes read. This should
contain the same value passed to the ReadDirectory PBytesTransferred parameter.
FspFileSystemAddDirInfo uses the value pointed by this parameter to track how much of the
buffer has been used so far.

**Return Value**

TRUE if the directory information was added, FALSE if there was not enough space to add it.

**Discussion**

This is a helper for implementing the ReadDirectory operation.

**See Also**

- ReadDirectory


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemAddEa</b> - Add extended attribute to a buffer.
</summary>
<blockquote>
<br/>

```c
FSP_API BOOLEAN FspFileSystemAddEa(
    PFILE_FULL_EA_INFORMATION SingleEa, 
    PFILE_FULL_EA_INFORMATION Ea,
    ULONG EaLength,
    PULONG PBytesTransferred);  
```

**Parameters**

- _SingleEa_ \- The extended attribute to add. A value of NULL acts as an EOF marker for a GetEa
operation.
- _Ea_ \- Pointer to a buffer that will receive the extended attribute. This should contain
the same value passed to the GetEa Ea parameter.
- _EaLength_ \- Length of buffer. This should contain the same value passed to the GetEa
EaLength parameter.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes stored. This should
contain the same value passed to the GetEa PBytesTransferred parameter.

**Return Value**

TRUE if the extended attribute was added, FALSE if there was not enough space to add it.

**Discussion**

This is a helper for implementing the GetEa operation.

**See Also**

- GetEa


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemAddNotifyInfo</b> - Add notify information to a buffer.
</summary>
<blockquote>
<br/>

```c
FSP_API BOOLEAN FspFileSystemAddNotifyInfo(
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo, 
    PVOID Buffer,
    ULONG Length,
    PULONG PBytesTransferred);  
```

**Parameters**

- _NotifyInfo_ \- The notify information to add.
- _Buffer_ \- Pointer to a buffer that will receive the notify information.
- _Length_ \- Length of buffer.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes stored. This should
be initialized to 0 prior to the first call to FspFileSystemAddNotifyInfo for a particular
buffer.

**Return Value**

TRUE if the notify information was added, FALSE if there was not enough space to add it.

**Discussion**

This is a helper for filling a buffer to use with FspFileSystemNotify.

**See Also**

- FspFileSystemNotify


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemAddStreamInfo</b> - Add named stream information to a buffer.
</summary>
<blockquote>
<br/>

```c
FSP_API BOOLEAN FspFileSystemAddStreamInfo(
    FSP_FSCTL_STREAM_INFO *StreamInfo, 
    PVOID Buffer,
    ULONG Length,
    PULONG PBytesTransferred);  
```

**Parameters**

- _StreamInfo_ \- The stream information to add. A value of NULL acts as an EOF marker for a GetStreamInfo
operation.
- _Buffer_ \- Pointer to a buffer that will receive the stream information. This should contain
the same value passed to the GetStreamInfo Buffer parameter.
- _Length_ \- Length of buffer. This should contain the same value passed to the GetStreamInfo
Length parameter.
- _PBytesTransferred_ \- [out]
Pointer to a memory location that will receive the actual number of bytes stored. This should
contain the same value passed to the GetStreamInfo PBytesTransferred parameter.

**Return Value**

TRUE if the stream information was added, FALSE if there was not enough space to add it.

**Discussion**

This is a helper for implementing the GetStreamInfo operation.

**See Also**

- GetStreamInfo


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemCanReplaceReparsePoint</b> - Test whether reparse data can be replaced.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemCanReplaceReparsePoint( 
    PVOID CurrentReparseData,
    SIZE_T CurrentReparseDataSize, 
    PVOID ReplaceReparseData,
    SIZE_T ReplaceReparseDataSize);  
```

**Parameters**

- _CurrentReparseData_ \- Pointer to the current reparse data.
- _CurrentReparseDataSize_ \- Pointer to the current reparse data size.
- _ReplaceReparseData_ \- Pointer to the replacement reparse data.
- _ReplaceReparseDataSize_ \- Pointer to the replacement reparse data size.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This is a helper for implementing the SetReparsePoint/DeleteReparsePoint operation
in file systems that support reparse points.

**See Also**

- SetReparsePoint
- DeleteReparsePoint


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemCreate</b> - Create a file system object.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemCreate(
    PWSTR DevicePath, 
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams, 
    const FSP_FILE_SYSTEM_INTERFACE *Interface, 
    FSP_FILE_SYSTEM **PFileSystem);  
```

**Parameters**

- _DevicePath_ \- The name of the control device for this file system. This must be either
FSP\_FSCTL\_DISK\_DEVICE\_NAME or FSP\_FSCTL\_NET\_DEVICE\_NAME.
- _VolumeParams_ \- Volume parameters for the newly created file system.
- _Interface_ \- A pointer to the actual operations that actually implement this user mode file system.
- _PFileSystem_ \- [out]
Pointer that will receive the file system object created on successful return from this
call.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemDelete</b> - Delete a file system object.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspFileSystemDelete(
    FSP_FILE_SYSTEM *FileSystem);  
```

**Parameters**

- _FileSystem_ \- The file system object.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemEnumerateEa</b> - Enumerate extended attributes in a buffer.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemEnumerateEa(
    FSP_FILE_SYSTEM *FileSystem, 
    NTSTATUS (*EnumerateEa)( 
        FSP_FILE_SYSTEM *FileSystem,
        PVOID Context, 
        PFILE_FULL_EA_INFORMATION SingleEa), 
    PVOID Context, 
    PFILE_FULL_EA_INFORMATION Ea,
    ULONG EaLength);  
```

**Parameters**

- _FileSystem_ \- The file system object.
- _EnumerateEa_ \- Pointer to function that receives a single extended attribute. The function
should return STATUS\_SUCCESS or an error code if unsuccessful.
- _Context_ \- User context to supply to EnumEa.
- _Ea_ \- Extended attributes buffer.
- _EaLength_ \- Extended attributes buffer length.

**Return Value**

STATUS\_SUCCESS or error code from EnumerateEa.

**Discussion**

This is a helper for implementing the CreateEx and SetEa operations in file systems
that support extended attributes.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemFindReparsePoint</b> - Find reparse point in file name.
</summary>
<blockquote>
<br/>

```c
FSP_API BOOLEAN FspFileSystemFindReparsePoint(
    FSP_FILE_SYSTEM *FileSystem, 
    NTSTATUS (*GetReparsePointByName)( 
        FSP_FILE_SYSTEM *FileSystem,
        PVOID Context, 
        PWSTR FileName,
        BOOLEAN IsDirectory,
        PVOID Buffer,
        PSIZE_T PSize), 
    PVOID Context, 
    PWSTR FileName,
    PUINT32 PReparsePointIndex);  
```

**Parameters**

- _FileSystem_ \- The file system object.
- _GetReparsePointByName_ \- Pointer to function that can retrieve reparse point information by name. The
FspFileSystemFindReparsePoint will call this function with the Buffer and PSize
arguments set to NULL. The function should return STATUS\_SUCCESS if the passed
FileName is a reparse point or STATUS\_NOT\_A\_REPARSE\_POINT (or other error code)
otherwise.
- _Context_ \- User context to supply to GetReparsePointByName.
- _FileName_ \- The name of the file or directory.
- _PReparsePointIndex_ \- Pointer to a memory location that will receive the index of the first reparse point
within FileName. A value is only placed in this memory location if the function returns
TRUE. May be NULL.

**Return Value**

TRUE if a reparse point was found, FALSE otherwise.

**Discussion**

Given a file name this function returns an index to the first path component that is a reparse
point. The function will call the supplied GetReparsePointByName function for every path
component until it finds a reparse point or the whole path is processed.

This is a helper for implementing the GetSecurityByName operation in file systems
that support reparse points.

**See Also**

- GetSecurityByName


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemGetEaPackedSize</b> - Get extended attribute "packed" size. This computation matches what NTFS reports.
</summary>
<blockquote>
<br/>

```c
static inline UINT32 FspFileSystemGetEaPackedSize(
    PFILE_FULL_EA_INFORMATION SingleEa) 
```

**Parameters**

- _SingleEa_ \- The extended attribute to get the size for.

**Return Value**

The packed size of the extended attribute.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemGetOpenFileInfo</b> - Get open information buffer.
</summary>
<blockquote>
<br/>

```c
static inline FSP_FSCTL_OPEN_FILE_INFO *FspFileSystemGetOpenFileInfo(
    FSP_FSCTL_FILE_INFO *FileInfo) 
```

**Parameters**

- _FileInfo_ \- The FileInfo parameter as passed to Create or Open operation.

**Return Value**

A pointer to the open information buffer for this Create or Open operation.

**Discussion**

This is a helper for implementing the Create and Open operations. It cannot be used with
any other operations.

The FileInfo parameter to Create and Open is typed as pointer to FSP\_FSCTL\_FILE\_INFO. The
true type of this parameter is pointer to FSP\_FSCTL\_OPEN\_FILE\_INFO. This simple function
converts from one type to the other.

The FSP\_FSCTL\_OPEN\_FILE\_INFO type contains a FSP\_FSCTL\_FILE\_INFO as well as the fields
NormalizedName and NormalizedNameSize. These fields can be used for file name normalization.
File name normalization is used to ensure that the FSD and the OS know the correct case
of a newly opened file name.

For case-sensitive file systems this functionality should be ignored. The FSD will always
assume that the normalized file name is the same as the file name used to open the file.

For case-insensitive file systems this functionality may be ignored. In this case the FSD
will assume that the normalized file name is the upper case version of the file name used
to open the file. The file system will work correctly and the only way an application will
be able to tell that the file system does not preserve case in normalized file names is by
issuing a GetFinalPathNameByHandle API call (or NtQueryInformationFile with
FileNameInformation/FileNormalizedNameInformation).

For case-insensitive file systems this functionality may also be used. In this case the
user mode file system may use the NormalizedName and NormalizedNameSize parameters to
report to the FSD the normalized file name. It should be noted that the normalized file
name may only differ in case from the file name used to open the file. The NormalizedName
field will point to a buffer that can receive the normalized file name. The
NormalizedNameSize field will contain the size of the normalized file name buffer. On
completion of the Create or Open operation it should contain the actual size of the
normalized file name copied into the normalized file name buffer. The normalized file name
should not contain a terminating zero.

**See Also**

- Create
- Open


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemGetOperationContext</b> - Get the current operation context.
</summary>
<blockquote>
<br/>

```c
FSP_API FSP_FILE_SYSTEM_OPERATION_CONTEXT *FspFileSystemGetOperationContext(
    VOID);  
```

**Return Value**

The current operation context.

**Discussion**

This function may be used only when servicing one of the FSP\_FILE\_SYSTEM\_INTERFACE operations.
The current operation context is stored in thread local storage. It allows access to the
Request and Response associated with this operation.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemNotify</b> - Notify Windows that the file system has file changes.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemNotify(
    FSP_FILE_SYSTEM *FileSystem, 
    FSP_FSCTL_NOTIFY_INFO *NotifyInfo,
    SIZE_T Size);  
```

**Parameters**

- _FileSystem_ \- The file system object.
- _NotifyInfo_ \- Buffer containing information about file changes.
- _Size_ \- Size of buffer.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

A file system that wishes to notify Windows about file changes must
first issue an FspFileSystemBegin call, followed by 0 or more
FspFileSystemNotify calls, followed by an FspFileSystemNotifyEnd call.

Note that FspFileSystemNotify requires file names to be normalized. A
normalized file name is one that contains the correct case of all characters
in the file name.

For case-sensitive file systems all file names are normalized by definition.
For case-insensitive file systems that implement file name normalization,
a normalized file name is the one that the file system specifies in the
response to Create or Open (see also FspFileSystemGetOpenFileInfo). For
case-insensitive file systems that do not implement file name normalization
a normalized file name is the upper case version of the file name used
to open the file.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemNotifyBegin</b> - Begin notifying Windows that the file system has file changes.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemNotifyBegin(
    FSP_FILE_SYSTEM *FileSystem,
    ULONG Timeout);  
```

**Parameters**

- _FileSystem_ \- The file system object.

**Return Value**

STATUS\_SUCCESS or error code. The error code STATUS\_CANT\_WAIT means that
a file rename operation is currently in progress and the operation must be
retried at a later time.

**Discussion**

A file system that wishes to notify Windows about file changes must
first issue an FspFileSystemBegin call, followed by 0 or more
FspFileSystemNotify calls, followed by an FspFileSystemNotifyEnd call.

This operation blocks concurrent file rename operations. File rename
operations may interfere with file notification, because a file being
notified may also be concurrently renamed. After all file change
notifications have been issued, you must make sure to call
FspFileSystemNotifyEnd to allow file rename operations to proceed.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemNotifyEnd</b> - End notifying Windows that the file system has file changes.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemNotifyEnd(
    FSP_FILE_SYSTEM *FileSystem);  
```

**Parameters**

- _FileSystem_ \- The file system object.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

A file system that wishes to notify Windows about file changes must
first issue an FspFileSystemBegin call, followed by 0 or more
FspFileSystemNotify calls, followed by an FspFileSystemNotifyEnd call.

This operation allows any blocked file rename operations to proceed.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemOperationProcessId</b> - Gets the originating process ID.
</summary>
<blockquote>
<br/>

```c
static inline UINT32 FspFileSystemOperationProcessId(
    VOID) 
```

**Discussion**

Valid only during Create, Open and Rename requests when the target exists.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemPreflight</b> - Check whether creating a file system object is possible.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemPreflight(
    PWSTR DevicePath, 
    PWSTR MountPoint);  
```

**Parameters**

- _DevicePath_ \- The name of the control device for this file system. This must be either
FSP\_FSCTL\_DISK\_DEVICE\_NAME or FSP\_FSCTL\_NET\_DEVICE\_NAME.
- _MountPoint_ \- The mount point for the new file system. A value of NULL means that the file system should
use the next available drive letter counting downwards from Z: as its mount point.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemRemoveMountPoint</b> - Remove the mount point for a file system.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspFileSystemRemoveMountPoint(
    FSP_FILE_SYSTEM *FileSystem);  
```

**Parameters**

- _FileSystem_ \- The file system object.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemResolveReparsePoints</b> - Resolve reparse points.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemResolveReparsePoints(
    FSP_FILE_SYSTEM *FileSystem, 
    NTSTATUS (*GetReparsePointByName)( 
        FSP_FILE_SYSTEM *FileSystem,
        PVOID Context, 
        PWSTR FileName,
        BOOLEAN IsDirectory,
        PVOID Buffer,
        PSIZE_T PSize), 
    PVOID Context, 
    PWSTR FileName,
    UINT32 ReparsePointIndex,
    BOOLEAN ResolveLastPathComponent, 
    PIO_STATUS_BLOCK PIoStatus,
    PVOID Buffer,
    PSIZE_T PSize);  
```

**Parameters**

- _FileSystem_ \- The file system object.
- _GetReparsePointByName_ \- Pointer to function that can retrieve reparse point information by name. The function
should return STATUS\_SUCCESS if the passed FileName is a reparse point or
STATUS\_NOT\_A\_REPARSE\_POINT (or other error code) otherwise.
- _Context_ \- User context to supply to GetReparsePointByName.
- _FileName_ \- The name of the file or directory to have its reparse points resolved.
- _ReparsePointIndex_ \- The index of the first reparse point within FileName.
- _ResolveLastPathComponent_ \- If FALSE, the last path component of FileName should not be resolved, even
if it is a reparse point that can be resolved. If TRUE, all path components
should be resolved if possible.
- _PIoStatus_ \- Pointer to storage that will receive the status to return to the FSD. When
this function succeeds it must set PIoStatus->Status to STATUS\_REPARSE and
PIoStatus->Information to either IO\_REPARSE or the reparse tag.
- _Buffer_ \- Pointer to a buffer that will receive the resolved file name (IO\_REPARSE) or
reparse data (reparse tag). If the function returns a file name, it should
not be NULL terminated.
- _PSize_ \- [in,out]
Pointer to the buffer size. On input it contains the size of the buffer.
On output it will contain the actual size of data copied.

**Return Value**

STATUS\_REPARSE or error code.

**Discussion**

Given a file name (and an index where to start resolving) this function will attempt to
resolve as many reparse points as possible. The function will call the supplied
GetReparsePointByName function for every path component until it resolves the reparse points
or the whole path is processed.

This is a helper for implementing the ResolveReparsePoints operation in file systems
that support reparse points.

**See Also**

- ResolveReparsePoints


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemSendResponse</b> - Send a response to the FSD.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspFileSystemSendResponse(
    FSP_FILE_SYSTEM *FileSystem, 
    FSP_FSCTL_TRANSACT_RSP *Response);  
```

**Parameters**

- _FileSystem_ \- The file system object.
- _Response_ \- The response buffer.

**Discussion**

This call is not required when the user mode file system performs synchronous processing of
requests. It is possible however for the following FSP\_FILE\_SYSTEM\_INTERFACE operations to be
processed asynchronously:

- Read


- Write


- ReadDirectory



These operations are allowed to return STATUS\_PENDING to postpone sending a response to the FSD.
At a later time the file system can use FspFileSystemSendResponse to send the response.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemSetMountPoint</b> - Set the mount point for a file system.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemSetMountPoint(
    FSP_FILE_SYSTEM *FileSystem,
    PWSTR MountPoint);  
```

**Parameters**

- _FileSystem_ \- The file system object.
- _MountPoint_ \- The mount point for the new file system. A value of NULL means that the file system should
use the next available drive letter counting downwards from Z: as its mount point.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function supports drive letters (X:) or directories as mount points:

- Drive letters: Refer to the documentation of the DefineDosDevice Windows API
to better understand how they are created.


- Directories: They can be used as mount points for disk based file systems. They cannot
be used for network file systems. This is a limitation that Windows imposes on junctions.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemSetOperationGuardStrategy</b> - Set file system locking strategy.
</summary>
<blockquote>
<br/>

```c
static inline VOID FspFileSystemSetOperationGuardStrategy(
    FSP_FILE_SYSTEM *FileSystem, 
    FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY GuardStrategy) 
```

**Parameters**

- _FileSystem_ \- The file system object.
- _GuardStrategy_ \- The locking (guard) strategy.

**See Also**

- FSP\_FILE\_SYSTEM\_OPERATION\_GUARD\_STRATEGY


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemStartDispatcher</b> - Start the file system dispatcher.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspFileSystemStartDispatcher(
    FSP_FILE_SYSTEM *FileSystem,
    ULONG ThreadCount);  
```

**Parameters**

- _FileSystem_ \- The file system object.
- _ThreadCount_ \- The number of threads for the file system dispatcher. A value of 0 will create a default
number of threads and should be chosen in most cases.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

The file system dispatcher is used to dispatch operations posted by the FSD to the user mode
file system. Once this call starts executing the user mode file system will start receiving
file system requests from the kernel.


</blockquote>
</details>

<details>
<summary>
<b>FspFileSystemStopDispatcher</b> - Stop the file system dispatcher.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspFileSystemStopDispatcher(
    FSP_FILE_SYSTEM *FileSystem);  
```

**Parameters**

- _FileSystem_ \- The file system object.


</blockquote>
</details>

<details>
<summary>
<b>FspSetSecurityDescriptor</b> - Modify security descriptor.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspSetSecurityDescriptor( 
    PSECURITY_DESCRIPTOR InputDescriptor, 
    SECURITY_INFORMATION SecurityInformation, 
    PSECURITY_DESCRIPTOR ModificationDescriptor, 
    PSECURITY_DESCRIPTOR *PSecurityDescriptor);  
```

**Parameters**

- _InputDescriptor_ \- The input security descriptor to be modified.
- _SecurityInformation_ \- Describes what parts of the InputDescriptor should be modified. This should contain
the same value passed to the SetSecurity SecurityInformation parameter.
- _ModificationDescriptor_ \- Describes the modifications to apply to the InputDescriptor. This should contain
the same value passed to the SetSecurity ModificationDescriptor parameter.
- _PSecurityDescriptor_ \- [out]
Pointer to a memory location that will receive the resulting security descriptor.
This security descriptor can be later freed using FspDeleteSecurityDescriptor.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This is a helper for implementing the SetSecurity operation.

**See Also**

- SetSecurity
- FspDeleteSecurityDescriptor


</blockquote>
</details>

### Typedefs

<details>
<summary>
<b>FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY</b> - User mode file system locking strategy.
</summary>
<blockquote>
<br/>

```c
typedef enum { 
    FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_FINE = 0, 
    FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE, 
} FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY;  
```

**Discussion**

Two concurrency models are provided:

1. A fine-grained concurrency model where file system NAMESPACE accesses
are guarded using an exclusive-shared (read-write) lock. File I/O is not
guarded and concurrent reads/writes/etc. are possible. [Note that the FSD
will still apply an exclusive-shared lock PER INDIVIDUAL FILE, but it will
not limit I/O operations for different files.]
The fine-grained concurrency model applies the exclusive-shared lock as
follows:

- EXCL: SetVolumeLabel, Flush(Volume),
Create, Cleanup(Delete), SetInformation(Rename)


- SHRD: GetVolumeInfo, Open, SetInformation(Disposition), ReadDirectory


- NONE: all other operations



2. A coarse-grained concurrency model where all file system accesses are
guarded by a mutually exclusive lock.

**See Also**

- FspFileSystemSetOperationGuardStrategy


</blockquote>
</details>

## SERVICE FRAMEWORK

User mode file systems typically are run as Windows services. WinFsp provides an API to make
the creation of Windows services easier. This API is provided for convenience and is not
necessary to expose a user mode file system to Windows.

### Functions

<details>
<summary>
<b>FspServiceAcceptControl</b> - Configure the control codes that a service accepts.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspServiceAcceptControl(
    FSP_SERVICE *Service,
    ULONG Control);  
```

**Parameters**

- _Service_ \- The service object.
- _Control_ \- The control codes to accept. Note that the SERVICE\_ACCEPT\_PAUSE\_CONTINUE code is silently
ignored.

**Discussion**

This API should be used prior to Start operations.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceAllowConsoleMode</b> - Allow a service to run in console mode.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspServiceAllowConsoleMode(
    FSP_SERVICE *Service);  
```

**Parameters**

- _Service_ \- The service object.

**Discussion**

A service that is run in console mode runs with a console attached and outside the control of
the Service Control Manager. This is useful for debugging and testing a service during
development.

User mode file systems that wish to use the WinFsp Launcher functionality must also use this
call. The WinFsp Launcher is a Windows service that can be configured to launch and manage
multiple instances of a user mode file system.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceContextCheck</b> - Check if the supplied token is from the service context.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspServiceContextCheck(
    HANDLE Token,
    PBOOLEAN PIsLocalSystem);  
```

**Parameters**

- _Token_ \- Token to check. Pass NULL to check the current process token.
- _PIsLocalSystem_ \- Pointer to a boolean that will receive a TRUE value if the token belongs to LocalSystem
and FALSE otherwise. May be NULL.

**Return Value**

STATUS\_SUCCESS if the token is from the service context. STATUS\_ACCESS\_DENIED if it is not.
Other error codes are possible.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceCreate</b> - Create a service object.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspServiceCreate(
    PWSTR ServiceName, 
    FSP_SERVICE_START *OnStart, 
    FSP_SERVICE_STOP *OnStop, 
    FSP_SERVICE_CONTROL *OnControl, 
    FSP_SERVICE **PService);  
```

**Parameters**

- _ServiceName_ \- The name of the service.
- _OnStart_ \- Function to call when the service starts.
- _OnStop_ \- Function to call when the service stops.
- _OnControl_ \- Function to call when the service receives a service control code.
- _PService_ \- [out]
Pointer that will receive the service object created on successful return from this
call.

**Return Value**

STATUS\_SUCCESS or error code.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceDelete</b> - Delete a service object.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspServiceDelete(
    FSP_SERVICE *Service);  
```

**Parameters**

- _Service_ \- The service object.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceGetExitCode</b> - Get the service process exit code.
</summary>
<blockquote>
<br/>

```c
FSP_API ULONG FspServiceGetExitCode(
    FSP_SERVICE *Service);  
```

**Parameters**

- _Service_ \- The service object.

**Return Value**

Service process exit code.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceIsInteractive</b> - Determine if the current process is running in user interactive mode.
</summary>
<blockquote>
<br/>

```c
FSP_API BOOLEAN FspServiceIsInteractive(
    VOID);  
```

**Return Value**

TRUE if the process is running in running user interactive mode.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceLog</b> - Log a service message.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspServiceLog(
    ULONG Type,
    PWSTR Format,
    ...);  
```

**Parameters**

- _Type_ \- One of EVENTLOG\_INFORMATION\_TYPE, EVENTLOG\_WARNING\_TYPE, EVENTLOG\_ERROR\_TYPE.
- _Format_ \- Format specification. This function uses the Windows wsprintf API for formatting. Refer to
that API's documentation for details on the format specification.

**Discussion**

This function can be used to log an arbitrary message to the Windows Event Log or to the current
console if running in user interactive mode.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceLoop</b> - Run a service main loop.
</summary>
<blockquote>
<br/>

```c
FSP_API NTSTATUS FspServiceLoop(
    FSP_SERVICE *Service);  
```

**Parameters**

- _Service_ \- The service object.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

This function starts and runs a service. It executes the Windows StartServiceCtrlDispatcher API
to connect the service process to the Service Control Manager. If the Service Control Manager is
not available (and console mode is allowed) it will enter console mode.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceRequestTime</b> - Request additional time from the Service Control Manager.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspServiceRequestTime(
    FSP_SERVICE *Service,
    ULONG Time);  
```

**Parameters**

- _Service_ \- The service object.
- _Time_ \- Additional time (in milliseconds).

**Discussion**

This API should be used during Start and Stop operations only.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceRunEx</b> - Run a service.
</summary>
<blockquote>
<br/>

```c
FSP_API ULONG FspServiceRunEx(
    PWSTR ServiceName, 
    FSP_SERVICE_START *OnStart, 
    FSP_SERVICE_STOP *OnStop, 
    FSP_SERVICE_CONTROL *OnControl, 
    PVOID UserContext);  
```

**Parameters**

- _ServiceName_ \- The name of the service.
- _OnStart_ \- Function to call when the service starts.
- _OnStop_ \- Function to call when the service stops.
- _OnControl_ \- Function to call when the service receives a service control code.

**Return Value**

Service process exit code.

**Discussion**

This function wraps calls to FspServiceCreate, FspServiceLoop and FspServiceDelete to create,
run and delete a service. It is intended to be used from a service's main/wmain function.

This function runs a service with console mode allowed.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceSetExitCode</b> - Set the service process exit code.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspServiceSetExitCode(
    FSP_SERVICE *Service,
    ULONG ExitCode);  
```

**Parameters**

- _Service_ \- The service object.
- _ExitCode_ \- Service process exit code.


</blockquote>
</details>

<details>
<summary>
<b>FspServiceStop</b> - Stops a running service.
</summary>
<blockquote>
<br/>

```c
FSP_API VOID FspServiceStop(
    FSP_SERVICE *Service);  
```

**Parameters**

- _Service_ \- The service object.

**Return Value**

STATUS\_SUCCESS or error code.

**Discussion**

Stopping a service usually happens when the Service Control Manager instructs the service to
stop. In some situations (e.g. fatal errors) the service may wish to stop itself. It can do so
in a clean manner by calling this function.


</blockquote>
</details>


<br/>
<p align="center">
<sub>
Copyright © 2015-2021 Bill Zissimopoulos
<br/>
Generated with <a href="https://github.com/billziss-gh/prettydoc">prettydoc</a>
</sub>
</p>
