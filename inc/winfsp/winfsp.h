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
typedef struct _FSP_FILE_SYSTEM_INTERFACE
{
    NTSTATUS (*GetVolumeInfo)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        FSP_FSCTL_VOLUME_INFO *VolumeInfo);
    NTSTATUS (*SetVolumeLabel)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PWSTR VolumeLabel,
        FSP_FSCTL_VOLUME_INFO *VolumeInfo);
    NTSTATUS (*GetSecurityByName)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, PUINT32 PFileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS (*Create)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
        UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
        PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*Open)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PWSTR FileName, BOOLEAN CaseSensitive, UINT32 CreateOptions,
        PVOID *PFileNode, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*Overwrite)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes,
        FSP_FSCTL_FILE_INFO *FileInfo);
    VOID (*Cleanup)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PWSTR FileName, BOOLEAN Delete);
    VOID (*Close)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode);
    NTSTATUS (*Read)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
        PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*Write)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
        BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*Flush)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode);
    NTSTATUS (*GetFileInfo)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*SetBasicInfo)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT32 FileAttributes,
        UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime,
        FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*SetAllocationSize)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT64 AllocationSize,
        FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*SetFileSize)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, UINT64 FileSize,
        FSP_FSCTL_FILE_INFO *FileInfo);
    NTSTATUS (*CanDelete)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PWSTR FileName);
    NTSTATUS (*Rename)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists);
    NTSTATUS (*GetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS (*SetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode,
        SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor);
    NTSTATUS (*ReadDirectory)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request,
        PVOID FileNode, PVOID Buffer, UINT64 Offset, ULONG Length,
        PULONG PBytesTransferred);
} FSP_FILE_SYSTEM_INTERFACE;
typedef struct _FSP_FILE_SYSTEM
{
    UINT16 Version;
    PVOID UserContext;
    WCHAR VolumePrefix[sizeof(((FSP_FSCTL_VOLUME_PARAMS *)0)->Prefix) / sizeof(WCHAR)];
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
FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem);
FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem);
FSP_API NTSTATUS FspFileSystemSetMountPoint(FSP_FILE_SYSTEM *FileSystem, PWSTR MountPoint);
FSP_API VOID FspFileSystemRemoveMountPoint(FSP_FILE_SYSTEM *FileSystem);
FSP_API NTSTATUS FspFileSystemStartDispatcher(FSP_FILE_SYSTEM *FileSystem, ULONG ThreadCount);
FSP_API VOID FspFileSystemStopDispatcher(FSP_FILE_SYSTEM *FileSystem);
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
 * File System Operations
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
 * File System Operations Helpers
 */
FSP_API BOOLEAN FspFileSystemAddDirInfo(FSP_FSCTL_DIR_INFO *DirInfo,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred);

/*
 * Access
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
