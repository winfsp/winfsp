/**
 * @file winfsp/winfsp.h
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */

#ifndef WINFSP_WINFSP_H_INCLUDED
#define WINFSP_WINFSP_H_INCLUDED

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>

#if defined(WINFSP_DLL_INTERNAL)
#define FSP_API                         __declspec(dllexport)
#else
#define FSP_API                         __declspec(dllimport)
#endif

#include <winfsp/fsctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * File System
 */
typedef struct _FSP_FILE_SYSTEM FSP_FILE_SYSTEM;
typedef VOID FSP_FILE_SYSTEM_OPERATION_GUARD(FSP_FILE_SYSTEM *,
    FSP_FSCTL_TRANSACT_REQ *, FSP_FSCTL_TRANSACT_RSP *);
typedef NTSTATUS FSP_FILE_SYSTEM_OPERATION(FSP_FILE_SYSTEM *,
    FSP_FSCTL_TRANSACT_REQ *, FSP_FSCTL_TRANSACT_RSP *);
/**
 * @class FSP_FILE_SYSTEM
 */
typedef struct _FSP_FILE_SYSTEM_INTERFACE
{
    /**
     * Get volume information.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param VolumeInfo [out]
     *     Pointer to a structure that will receive the volume information on successful return
     *     from this call.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*GetVolumeInfo)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        FSP_FSCTL_VOLUME_INFO *VolumeInfo);
    /**
     * Set volume label.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param VolumeLabel
     *     The new label for the volume.
     * @param VolumeInfo [out]
     *     Pointer to a structure that will receive the volume information on successful return
     *     from this call.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*SetVolumeLabel)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PWSTR VolumeLabel,
        FSP_FSCTL_VOLUME_INFO *VolumeInfo);
    /**
     * Get file or directory attributes and security descriptor given a file name.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param FileName
     *     The name of the file or directory to get the attributes and security descriptor for.
     * @param PFileAttributes
     *     Pointer to a memory location that will receive the file attributes on successful return
     *     from this call. May be NULL.
     * @param SecurityDescriptor
     *     Pointer to a buffer that will receive the file security descriptor on successful return
     *     from this call. May be NULL.
     * @param PSecurityDescriptorSize [in,out]
     *     Pointer to the security descriptor buffer size. On input it contains the size of the
     *     security descriptor buffer. On output it will contain the actual size of the security
     *     descriptor copied into the security descriptor buffer. May be NULL.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*GetSecurityByName)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, PUINT32 PFileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    /**
     * Create new file or directory.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileName
     *     The name of the file or directory to be created.
     * @param CaseSensitive
     *     Whether to treat the FileName as case-sensitive or case-insensitive. Case-sensitive
     *     file systems always treat FileName as case-sensitive regardless of this parameter.
     * @param CreateOptions
     *     Create options for this request. This parameter has the same meaning as the
     *     CreateOptions parameter of the NtCreateFile API. User mode file systems should typically
     *     only be concerned with the flag FILE_DIRECTORY_FILE, which is an instruction to create a
     *     directory rather than a file. Some file systems may also want to pay attention to the
     *     FILE_NO_INTERMEDIATE_BUFFERING and FILE_WRITE_THROUGH flags, although these are
     *     typically handled by the FSD component.
     * @param FileAttributes
     *     File attributes to apply to the newly created file or directory.
     * @param SecurityDescriptor
     *     Security descriptor to apply to the newly created file or directory. This security
     *     descriptor will always be in self-relative format. Its length can be retrieved using the
     *     Windows GetSecurityDescriptorLength API.
     * @param AllocationSize
     *     Allocation size for the newly created file.
     * @param PFileNode [out]
     *     Pointer that will receive the file node on successful return from this call. The file
     *     node is a void pointer (or an integer that can fit in a pointer) that is used to
     *     uniquely identify an open file. Opening the same file name should always return the same
     *     file node value for as long as the file with that name remains open anywhere in the
     *     system. The file system can place any value it needs here.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*Create)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
        UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
        PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Open a file or directory.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileName
     *     The name of the file or directory to be opened.
     * @param CaseSensitive
     *     Whether to treat the FileName as case-sensitive or case-insensitive. Case-sensitive
     *     file systems always treat FileName as case-sensitive regardless of this parameter.
     * @param CreateOptions
     *     Create options for this request. This parameter has the same meaning as the
     *     CreateOptions parameter of the NtCreateFile API. User mode file systems typically
     *     do not need to do anything special with respect to this parameter. Some file systems may
     *     also want to pay attention to the FILE_NO_INTERMEDIATE_BUFFERING and FILE_WRITE_THROUGH
     *     flags, although these are typically handled by the FSD component.
     * @param PFileNode [out]
     *     Pointer that will receive the file node on successful return from this call. The file
     *     node is a void pointer (or an integer that can fit in a pointer) that is used to
     *     uniquely identify an open file. Opening the same file name should always return the same
     *     file node value for as long as the file with that name remains open anywhere in the
     *     system. The file system can place any value it needs here.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*Open)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
        PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Overwrite a file.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file to overwrite.
     * @param FileAttributes
     *     File attributes to apply to the overwritten file.
     * @param ReplaceFileAttributes
     *     When TRUE the existing file attributes should be replaced with the new ones.
     *     When FALSE the existing file attributes should be merged (or'ed) with the new ones.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*Overwrite)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes,
        FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Cleanup a file.
     *
     * When CreateFile is used to open or create a file the kernel creates a kernel mode file
     * object (type FILE_OBJECT) and a handle for it, which it returns to user-mode. The handle may
     * be duplicated (using DuplicateHandle), but all duplicate handles always refer to the same
     * file object. When all handles for a particular file object get closed (using CloseHandle)
     * the system sends a Cleanup request to the file system.
     *
     * There will be a Cleanup operation for every Create or Open operation posted to the user mode
     * file system. However the Cleanup operation is <b>not</b> the final close operation on a file. The
     * file system must be ready to receive additional operations until close time. This is true
     * even when the file is being deleted!
     *
     * An important function of the Cleanup operation is to complete a delete operation. Deleting
     * a file or directory in Windows is a three-stage process where the file is first opened, then
     * tested to see if the delete can proceed and if the answer is positive the file is then
     * deleted during Cleanup.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file or directory to cleanup.
     * @param FileName
     *     The name of the file or directory to cleanup. Sent only when a Delete is requested.
     * @param Delete
     *     Determines whether to delete the file. Note that there is no way to report failure of
     *     this operation. Also note that when this parameter is TRUE this is the last outstanding
     *     cleanup for this particular file node.
     * @see
     *     Close
     *     CanDelete
     */
    VOID (*Cleanup)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PWSTR FileName, BOOLEAN Delete);
    /**
     * Close a file.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file or directory to be closed.
     */
    VOID (*Close)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode);
    /**
     * Read a file.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file to be read.
     * @param Buffer
     *     Pointer to a buffer that will receive the results of the read operation.
     * @param Offset
     *     Offset within the file to read from.
     * @param Length
     *     Length of data to read.
     * @param PBytesTransferred [out]
     *     Pointer to a memory location that will receive the actual number of bytes read.
     * @return
     *     STATUS_SUCCESS on error code. STATUS_PENDING is supported allowing for asynchronous
     *     operation.
     */
    NTSTATUS (*Read)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
        PULONG PBytesTransferred);
    /**
     * Write a file.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file to be written.
     * @param Buffer
     *     Pointer to a buffer that contains the data to write.
     * @param Offset
     *     Offset within the file to write to.
     * @param Length
     *     Length of data to write.
     * @param WriteToEndOfFile
     *     When TRUE the file system must write to the current end of file. In this case the Offset
     *     parameter will contain the value -1.
     * @param ConstrainedIo
     *     When TRUE the file system must not extend the file (i.e. change the file size).
     * @param PBytesTransferred [out]
     *     Pointer to a memory location that will receive the actual number of bytes written.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code. STATUS_PENDING is supported allowing for asynchronous
     *     operation.
     */
    NTSTATUS (*Write)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
        BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Flush a file or volume.
     *
     * Note that the FSD will also flush all file/volume caches prior to invoking this operation.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file to be flushed. When NULL the whole volume is being flushed.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*Flush)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode);
    /**
     * Get file or directory information.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file or directory to get information for.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*GetFileInfo)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Set file or directory basic information.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file or directory to set information for.
     * @param FileAttributes
     *     File attributes to apply to the file or directory. If the value INVALID_FILE_ATTRIBUTES
     *     is sent, the file attributes should not be changed.
     * @param CreationTime
     *     Creation time to apply to the file or directory. If the value 0 is sent, the creation
     *     time should not be changed.
     * @param LastAccessTime
     *     Last access time to apply to the file or directory. If the value 0 is sent, the last
     *     access time should not be changed.
     * @param LastWriteTime
     *     Last write time to apply to the file or directory. If the value 0 is sent, the last
     *     write time should not be changed.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*SetBasicInfo)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT32 FileAttributes,
        UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime,
        FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Set file allocation size.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file to set the allocation size for.
     * @param AllocationSize
     *     Allocation size to apply to the file. Allocation size is always greater than file size.
     *     An allocation size of less than the current file size should also truncate the current
     *     file size.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*SetAllocationSize)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT64 AllocationSize,
        FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Set file size.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file to set the size for.
     * @param FileSize
     *     FileSize size to apply to the file. Allocation size is always greater than file size.
     *     A file size of more than the current allocation size will also extend the allocation
     *     size to the next allocation unit boundary.
     * @param FileInfo [out]
     *     Pointer to a structure that will receive the file information on successful return
     *     from this call. This information includes file attributes, file times, etc.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*SetFileSize)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT64 FileSize,
        FSP_FSCTL_FILE_INFO *FileInfo);
    /**
     * Determine whether a file or directory can be deleted.
     *
     * This function tests whether a file or directory can be safely deleted. This function does
     * need to perform access checks, but may performs tasks such as check for empty directories,
     * etc.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file or directory to test for deletion.
     * @param FileName
     *     The name of the file or directory to test for deletion.
     * @return
     *     STATUS_SUCCESS on error code.
     * @see
     *     Cleanup
     */
    NTSTATUS (*CanDelete)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PWSTR FileName);
    /**
     * Renames a file or directory.
     *
     * The kernel mode FSD provides certain guarantees prior to posting a rename operation:
     * <ul>
     * <li>A file cannot be renamed if it has any open handles, other than the one used to perform
     * the rename.</li>
     * <li>A file cannot be renamed if a file with the same name exists and has open handles.</li>
     * <li>A directory cannot be renamed if it or any of its subdirectories contains a file that
     * has open handles.</li>
     * </ul>
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the file or directory to be renamed.
     * @param FileName
     *     The current name of the file or directory to rename.
     * @param NewFileName
     *     The new name for the file or directory.
     * @param ReplaceIfExists
     *     Whether to replace a file that already exists at NewFileName.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*Rename)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists);
    /**
     * Get file or directory security descriptor.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param FileNode
     *     The file node of the file or directory to get the security descriptor for.
     * @param SecurityDescriptor
     *     Pointer to a buffer that will receive the file security descriptor on successful return
     *     from this call. May be NULL.
     * @param PSecurityDescriptorSize [in,out]
     *     Pointer to the security descriptor buffer size. On input it contains the size of the
     *     security descriptor buffer. On output it will contain the actual size of the security
     *     descriptor copied into the security descriptor buffer. Cannot be NULL.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*GetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    /**
     * Set file or directory security descriptor.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param FileNode
     *     The file node of the file or directory to set the security descriptor for.
     * @param SecurityInformation
     *     Indicates what part of the file or directory security descriptor to change.
     * @param SecurityDescriptor
     *     Security descriptor to apply to the file or directory. This security descriptor will
     *     always be in self-relative format.
     * @return
     *     STATUS_SUCCESS on error code.
     */
    NTSTATUS (*SetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor);
    /**
     * Read a directory.
     *
     * @param FileSystem
     *     The file system on which this request is posted.
     * @param Request
     *     The request posted by the kernel mode FSD.
     * @param FileNode
     *     The file node of the directory to be read.
     * @param Buffer
     *     Pointer to a buffer that will receive the results of the read operation.
     * @param Offset
     *     Offset within the directory to read from. The kernel does not interpret this value
     *     which is used solely by the file system to locate directory entries. However the
     *     special value 0 indicates that the read should start from the first entries. The first
     *     two entries returned by ReadDirectory should always be the "." and ".." entries.
     * @param Length
     *     Length of data to read.
     * @param Pattern
     *     The pattern to match against files in this directory. Can be NULL. The file system
     *     can choose to ignore this parameter as the FSD will always perform its own pattern
     *     matching on the returned results.
     * @param PBytesTransferred [out]
     *     Pointer to a memory location that will receive the actual number of bytes read.
     * @return
     *     STATUS_SUCCESS on error code. STATUS_PENDING is supported allowing for asynchronous
     *     operation.
     * @see
     *     FspFileSystemAddDirInfo
     */
    NTSTATUS (*ReadDirectory)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
        PWSTR Pattern,
        PULONG PBytesTransferred);
} FSP_FILE_SYSTEM_INTERFACE;
typedef struct _FSP_FILE_SYSTEM
{
    UINT16 Version;
    PVOID UserContext;
    WCHAR VolumeName[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    HANDLE VolumeHandle;
    FSP_FILE_SYSTEM_OPERATION_GUARD *EnterOperation, *LeaveOperation;
    FSP_FILE_SYSTEM_OPERATION *Operations[FspFsctlTransactKindCount];
    const FSP_FILE_SYSTEM_INTERFACE *Interface;
    HANDLE DispatcherThread;
    ULONG DispatcherThreadCount;
    NTSTATUS DispatcherResult;
    PWSTR MountPoint;
    LIST_ENTRY MountEntry;
} FSP_FILE_SYSTEM;
/**
 * Create a file system object.
 *
 * @param DevicePath
 *     The name of the control device for this file system. This must be either
 *     FSP_FSCTL_DISK_DEVICE_NAME or FSP_FSCTL_NET_DEVICE_NAME.
 * @param VolumeParams
 *     Volume parameters for the newly created file system.
 * @param Interface
 *     A pointer to the actual operations that actually implement this user mode file system.
 * @param PFileSystem [out]
 *     Pointer that will receive the file system object created on successful return from this
 *     call.
 * @return
 *     STATUS_SUCCESS on error code.
 */
FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem);
/**
 * Delete a file system object.
 *
 * @param FileSystem
 *     The file system object.
 */
FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem);
/**
 * Set the mount point for a file system.
 *
 * This function currently only supports drive letters (X:) as mount points. Refer to the
 * documentation of the DefineDosDevice Windows API to better understand how drive letters are
 * created.
 *
 * @param FileSystem
 *     The file system object.
 * @param MountPoint
 *     The mount point for the new file system. A value of NULL means that the file system should
 *     use the next available drive letter counting downwards from Z: as its mount point.
 * @return
 *     STATUS_SUCCESS on error code.
 */
FSP_API NTSTATUS FspFileSystemSetMountPoint(FSP_FILE_SYSTEM *FileSystem, PWSTR MountPoint);
/**
 * Remove the mount point for a file system.
 *
 * @param FileSystem
 *     The file system object.
 */
FSP_API VOID FspFileSystemRemoveMountPoint(FSP_FILE_SYSTEM *FileSystem);
/**
 * Start the file system dispatcher.
 *
 * The file system dispatcher is used to dispatch operations posted by the FSD to the user mode
 * file system. Once this call starts executing the user mode file system will start receiving
 * file system requests from the kernel.
 *
 * @param FileSystem
 *     The file system object.
 * @param ThreadCount
 *     The number of threads for the file system dispatcher. A value of 0 will create a default
 *     number of threads and should be chosen in most cases.
 * @return
 *     STATUS_SUCCESS on error code.
 */
FSP_API NTSTATUS FspFileSystemStartDispatcher(FSP_FILE_SYSTEM *FileSystem, ULONG ThreadCount);
/**
 * Stop the file system dispatcher.
 *
 * @param FileSystem
 *     The file system object.
 */
FSP_API VOID FspFileSystemStopDispatcher(FSP_FILE_SYSTEM *FileSystem);
/**
 * Send a response to the FSD.
 *
 * This call is not required when the user mode file system performs synchronous processing of
 * requests. It is possible however for the following FSP_FILE_SYSTEM_INTERFACE operations to be
 * processed asynchronously:
 * <ul>
 * <li>Read</li>
 * <li>Write</li>
 * <li>ReadDirectory</li>
 * </ul>
 *
 * These operations are allowed to return STATUS_PENDING to postpone sending a response to the FSD.
 * At a later time the file system can use FspFileSystemSendResponse to send the response.
 *
 * @param FileSystem
 *     The file system object.
 * @param Response
 *     The response buffer.
 */
FSP_API VOID FspFileSystemSendResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response);
static inline
PWSTR FspFileSystemMountPoint(FSP_FILE_SYSTEM *FileSystem)
{
    return FileSystem->MountPoint;
}
static inline
VOID FspFileSystemEnterOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 != FileSystem->EnterOperation)
        FileSystem->EnterOperation(FileSystem, Request, Response);
}
static inline
VOID FspFileSystemLeaveOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response)
{
    if (0 != FileSystem->LeaveOperation)
        FileSystem->LeaveOperation(FileSystem, Request, Response);
}
static inline
VOID FspFileSystemSetOperationGuard(FSP_FILE_SYSTEM *FileSystem,
    FSP_FILE_SYSTEM_OPERATION_GUARD *EnterOperation,
    FSP_FILE_SYSTEM_OPERATION_GUARD *LeaveOperation)
{
    FileSystem->EnterOperation = EnterOperation;
    FileSystem->LeaveOperation = LeaveOperation;
}
static inline
VOID FspFileSystemSetOperation(FSP_FILE_SYSTEM *FileSystem,
    ULONG Index,
    FSP_FILE_SYSTEM_OPERATION *Operation)
{
    FileSystem->Operations[Index] = Operation;
}
static inline
VOID FspFileSystemGetDispatcherResult(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS *PDispatcherResult)
{
    /* 32-bit reads are atomic */
    *PDispatcherResult = FileSystem->DispatcherResult;
    MemoryBarrier();
}
static inline
VOID FspFileSystemSetDispatcherResult(FSP_FILE_SYSTEM *FileSystem,
    NTSTATUS DispatcherResult)
{
    if (NT_SUCCESS(DispatcherResult))
        return;
    InterlockedCompareExchange(&FileSystem->DispatcherResult, DispatcherResult, 0);
}

/*
 * Operations
 */
FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpOverwrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpCleanup(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpClose(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpRead(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpWrite(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpQueryInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpSetInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpFlushBuffers(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpQueryVolumeInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpSetVolumeInformation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpQueryDirectory(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpQuerySecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemOpSetSecurity(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, FSP_FSCTL_TRANSACT_RSP *Response);

/*
 * Helpers
 */
/**
 * Add directory information to a buffer.
 *
 * This is a helper for implementing the ReadDirectory operation.
 *
 * @param DirInfo
 *     The directory information to add. A value of NULL acts as an EOF marker for a ReadDirectory
 *     operation.
 * @param Buffer
 *     Pointer to a buffer that will receive the results of the read operation. This should contain
 *     the same value passed to the ReadDirectory Buffer parameter.
 * @param Length
 *     Length of data to read. This should contain the same value passed to the ReadDirectory
 *     Length parameter.
 * @param PBytesTransferred [out]
 *     Pointer to a memory location that will receive the actual number of bytes read. This should
 *     contain the same value passed to the ReadDirectory PBytesTransferred parameter.
 *     FspFileSystemAddDirInfo uses the value pointed by this parameter to track how much of the
 *     buffer has been used so far.
 * @return
 *     TRUE if the directory information was added, FALSE if there was not enough space to add it.
 * @see
 *     ReadDirectory
 */
FSP_API BOOLEAN FspFileSystemAddDirInfo(FSP_FSCTL_DIR_INFO *DirInfo,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred);

/*
 * Security
 */
FSP_API PGENERIC_MAPPING FspGetFileGenericMapping(VOID);
FSP_API NTSTATUS FspAccessCheckEx(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN CheckParentDirectory, BOOLEAN AllowTraverseCheck,
    UINT32 DesiredAccess, PUINT32 PGrantedAccess,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor);
FSP_API NTSTATUS FspCreateSecurityDescriptor(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PSECURITY_DESCRIPTOR ParentDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor);
FSP_API NTSTATUS FspSetSecurityDescriptor(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    PSECURITY_DESCRIPTOR InputDescriptor,
    PSECURITY_DESCRIPTOR *PSecurityDescriptor);
FSP_API VOID FspDeleteSecurityDescriptor(PSECURITY_DESCRIPTOR SecurityDescriptor,
    NTSTATUS (*CreateFunc)());
static inline
NTSTATUS FspAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request,
    BOOLEAN CheckParentDirectory, BOOLEAN AllowTraverseCheck,
    UINT32 DesiredAccess, PUINT32 PGrantedAccess)
{
    return FspAccessCheckEx(FileSystem, Request,
        CheckParentDirectory, AllowTraverseCheck,
        DesiredAccess, PGrantedAccess,
        0);
}

/*
 * Path Handling
 */
FSP_API VOID FspPathPrefix(PWSTR Path, PWSTR *PPrefix, PWSTR *PRemain, PWSTR Root);
FSP_API VOID FspPathSuffix(PWSTR Path, PWSTR *PRemain, PWSTR *PSuffix, PWSTR Root);
FSP_API VOID FspPathCombine(PWSTR Prefix, PWSTR Suffix);

/*
 * Utility
 */
FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error);
FSP_API VOID FspDebugLog(const char *format, ...);
FSP_API VOID FspDebugLogSD(const char *format, PSECURITY_DESCRIPTOR SecurityDescriptor);
FSP_API VOID FspDebugLogFT(const char *format, PFILETIME FileTime);

#ifdef __cplusplus
}
#endif

#endif
