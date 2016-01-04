/**
 * @file winfsp/winfsp.h
 *
 * @copyright 2015 Bill Zissimopoulos
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

/*
 * File System
 */
typedef struct _FSP_FILE_SYSTEM FSP_FILE_SYSTEM;
typedef struct _FSP_FILE_NODE
{
    PVOID UserContext;
    DWORD Flags;
    struct
    {
        ULONG OpenCount;
        ULONG Readers;
        ULONG Writers;
        ULONG Deleters;
        ULONG SharedRead;
        ULONG SharedWrite;
        ULONG SharedDelete;
    } ShareAccess;
} FSP_FILE_NODE;
typedef VOID FSP_FILE_SYSTEM_DISPATCHER(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef NTSTATUS FSP_FILE_SYSTEM_OPERATION(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef struct _FSP_FILE_SYSTEM_INTERFACE
{
    NTSTATUS (*AccessCheck)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, DWORD DesiredAccess, PDWORD PGrantedAccess);
    NTSTATUS (*GetAttributes)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, PDWORD PAttributes);
    NTSTATUS (*SetAttributes)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, DWORD Attributes);
    NTSTATUS (*GetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize);
    NTSTATUS (*SetSecurity)(FSP_FILE_SYSTEM *FileSystem,
        PWSTR FileName, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T SecurityDescriptorSize);
    NTSTATUS (*FileCreate)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE **PFileNode);
    NTSTATUS (*FileOpen)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE **PFileNode);
    NTSTATUS (*FileClose)(FSP_FILE_SYSTEM *FileSystem,
        FSP_FSCTL_TRANSACT_REQ *Request, FSP_FILE_NODE *FileNode);
} FSP_FILE_SYSTEM_INTERFACE;
typedef struct _FSP_FILE_SYSTEM
{
    UINT16 Version;
    WCHAR VolumePath[FSP_FSCTL_VOLUME_NAME_SIZEMAX / sizeof(WCHAR)];
    HANDLE VolumeHandle;
    PVOID UserContext;
    NTSTATUS DispatcherResult;
    FSP_FILE_SYSTEM_DISPATCHER *Dispatcher;
    FSP_FILE_SYSTEM_DISPATCHER *EnterOperation, *LeaveOperation;
    FSP_FILE_SYSTEM_OPERATION *Operations[FspFsctlTransactKindCount];
    const FSP_FILE_SYSTEM_INTERFACE *Interface;
} FSP_FILE_SYSTEM;

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem);
FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem);
FSP_API NTSTATUS FspFileSystemLoop(FSP_FILE_SYSTEM *FileSystem);

FSP_API VOID FspFileSystemDirectDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API VOID FspFileSystemPoolDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
static inline
VOID FspFileSystemSetDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FILE_SYSTEM_DISPATCHER *Dispatcher,
    FSP_FILE_SYSTEM_DISPATCHER *EnterOperation,
    FSP_FILE_SYSTEM_DISPATCHER *LeaveOperation)
{
    FileSystem->Dispatcher = Dispatcher;
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

static inline
VOID FspFileSystemEnterOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 != FileSystem->EnterOperation)
        FileSystem->EnterOperation(FileSystem, Request);
}
static inline
VOID FspFileSystemLeaveOperation(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (0 != FileSystem->LeaveOperation)
        FileSystem->LeaveOperation(FileSystem, Request);
}

FSP_API NTSTATUS FspFileSystemSendResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspFileSystemSendResponseWithStatus(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, NTSTATUS Result);

/*
 * File System Operations
 */
FSP_API NTSTATUS FspFileSystemOpCreate(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);

/*
 * Access Checks
 */
FSP_API PGENERIC_MAPPING FspGetFileGenericMapping(VOID);
FSP_API NTSTATUS FspAccessCheck(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN AllowTraverseCheck, DWORD DesiredAccess,
    PDWORD PGrantedAccess);
FSP_API NTSTATUS FspShareCheck(FSP_FILE_SYSTEM *FileSystem,
    DWORD GrantedAccess, DWORD ShareAccess, FSP_FILE_NODE *FileNode);

/*
 * Path Handling
 */
FSP_API VOID FspPathPrefix(PWSTR Path, PWSTR *PPrefix, PWSTR *PRemain);
FSP_API VOID FspPathSuffix(PWSTR Path, PWSTR *PRemain, PWSTR *PSuffix);
FSP_API VOID FspPathCombine(PWSTR Prefix, PWSTR Suffix);

/*
 * Utility
 */
FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error);
FSP_API VOID FspDebugLog(const char *format, ...);

#endif
