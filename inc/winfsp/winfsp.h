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

typedef struct _FSP_FILE_SYSTEM FSP_FILE_SYSTEM;
typedef NTSTATUS FSP_FILE_SYSTEM_PR(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef VOID FSP_FILE_SYSTEM_OP(FSP_FILE_SYSTEM *, FSP_FSCTL_TRANSACT_REQ *);
typedef struct _FSP_FILE_SYSTEM
{
    UINT16 Version;
    PVOID UserContext;
    HANDLE VolumeHandle;
    FSP_FILE_SYSTEM_PR *ProcessRequest;
    FSP_FILE_SYSTEM_OP *Operations[FspFsctlTransactKindCount];
} FSP_FILE_SYSTEM;

FSP_API NTSTATUS FspFileSystemCreate(FSP_FILE_SYSTEM_PR *ProcessRequest,
    FSP_FILE_SYSTEM **PFileSystem);
FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem);
FSP_API NTSTATUS FspFileSystemLoop(FSP_FILE_SYSTEM *FileSystem);

FSP_API NTSTATUS FspProcessRequestDirect(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspProcessRequestInPool(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request);
FSP_API NTSTATUS FspProduceResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response);
FSP_API NTSTATUS FspProduceResponseWithStatus(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, NTSTATUS Result);

FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error);
FSP_API VOID FspDebugLog(const char *format, ...);

#endif
