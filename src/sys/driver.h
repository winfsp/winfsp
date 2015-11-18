/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#include <ntifs.h>

#define DRIVER_NAME                     "winfsp"

/* DEBUGLOG */
#if DBG
#define DEBUGLOG(fmt, ...)              \
    DbgPrint(DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

/* enter/leave*/
#if DBG
#define FSP_DEBUGLOG(rfmt, r, fmt, ...) \
    DbgPrint(AbnormalTermination() ?    \
        DRIVER_NAME "!" __FUNCTION__ "(" fmt ") = !AbnormalTermination\n" :\
        DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n",\
        __VA_ARGS__, r)
#else
#define FSP_DEBUGLOG(rfmt, r, fmt, ...) ((void)0)
#endif
#if DBG
BOOLEAN HasDbgBreakPoint(const char *Function);
const char *NtStatusSym(NTSTATUS Status);
#define FSP_ENTER_(...)                 \
    if (HasDbgBreakPoint(__FUNCTION__)) \
        try { DbgBreakPoint(); } except(EXCEPTION_EXECUTE_HANDLER) {}\
    __VA_ARGS__;                        \
    try                                 \
    {
#define FSP_LEAVE_(rfmt, r, fmt, ...)   \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
    }                                   \
    finally                             \
    {                                   \
        FSP_DEBUGLOG(rfmt, r, fmt, __VA_ARGS__);\
    }
#else
#define FSP_ENTER_(...)                 \
    __VA_ARGS__;                        \
    {
#define FSP_LEAVE_(rfmt, r, fmt, ...)   \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
    }
#endif
#define FSP_ENTER(...)                  \
    NTSTATUS Result = STATUS_SUCCESS; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE(fmt, ...)             \
    FSP_LEAVE_(" = %s", NtStatusSym(Result), fmt, __VA_ARGS__); return Result
#define FSP_ENTER_BOOL(...)             \
    BOOLEAN Result = TRUE; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_BOOL(fmt, ...)        \
    FSP_LEAVE_(" = %s", Result ? "TRUE" : "FALSE", fmt, __VA_ARGS__); return Result
#define FSP_ENTER_VOID(...)             \
    FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_VOID(fmt, ...)        \
    FSP_LEAVE_("", 0, fmt, __VA_ARGS__)
#define FSP_RETURN(...)                 \
    do                                  \
    {                                   \
        __VA_ARGS__;                    \
        goto fsp_leave_label;           \
    } while (0,0)

/* driver major functions */
DRIVER_DISPATCH FspCleanup;
DRIVER_DISPATCH FspClose;
DRIVER_DISPATCH FspCreate;
DRIVER_DISPATCH FspDeviceControl;
DRIVER_DISPATCH FspDirectoryControl;
DRIVER_DISPATCH FspFileSystemControl;
DRIVER_DISPATCH FspFlushBuffers;
DRIVER_DISPATCH FspLockControl;
DRIVER_DISPATCH FspQueryEa;
DRIVER_DISPATCH FspQueryInformation;
DRIVER_DISPATCH FspQuerySecurity;
DRIVER_DISPATCH FspQueryVolumeInformation;
DRIVER_DISPATCH FspRead;
DRIVER_DISPATCH FspSetEa;
DRIVER_DISPATCH FspSetInformation;
DRIVER_DISPATCH FspSetSecurity;
DRIVER_DISPATCH FspSetVolumeInformation;
DRIVER_DISPATCH FspShutdown;
DRIVER_DISPATCH FspWrite;

/* fast I/O */
FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;

/* resource acquisition */
FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;

/* extern */
extern PDEVICE_OBJECT FspDeviceObject;

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */

#endif
