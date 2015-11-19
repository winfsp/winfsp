/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#include <ntifs.h>

#define DRIVER_NAME                     "WinFsp"
#define DEVICE_NAME                     "WinFsp"

/* DEBUGLOG */
#if DBG
#define DEBUGLOG(fmt, ...)              \
    DbgPrint(DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

/* enter/leave*/
#if DBG
#define FSP_DEBUGLOG_(rfmt, r, fmt, ...)\
    DbgPrint(AbnormalTermination() ?    \
        DRIVER_NAME "!" __FUNCTION__ "(" fmt ") = !AbnormalTermination\n" :\
        DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n",\
        __VA_ARGS__, r)
#define FSP_DEBUGBRK_()                 \
    do                                  \
    {                                   \
        if (HasDbgBreakPoint(__FUNCTION__))\
            try { DbgBreakPoint(); } except(EXCEPTION_EXECUTE_HANDLER) {}\
    } while (0,0)
#else
#define FSP_DEBUGLOG_(rfmt, r, fmt, ...)((void)0)
#define FSP_DEBUGBRK_()                 ((void)0)
#endif
#define FSP_ENTER_(...)                 \
    FSP_DEBUGBRK_();                    \
    __VA_ARGS__;                        \
    FsRtlEnterFileSystem();             \
    try                                 \
    {
#define FSP_LEAVE_(rfmt, r, fmt, ...)   \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
    }                                   \
    finally                             \
    {                                   \
        FsRtlExitFileSystem();          \
        FSP_DEBUGLOG_(rfmt, r, fmt, __VA_ARGS__);\
    }
#define FSP_ENTER(...)                  \
    NTSTATUS Result = STATUS_SUCCESS; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE(fmt, ...)             \
    FSP_LEAVE_(" = %s", NtStatusSym(Result), fmt, __VA_ARGS__); return Result
#define FSP_ENTER_MJ(...)               \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);(void)IrpSp;\
    FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_MJ(fmt, ...)          \
    FSP_LEAVE_(" = %s", NtStatusSym(Result),\
        "'%c', %s%s, Flags=%x, " \
        fmt,                            \
        FspDeviceExtension(DeviceObject)->Kind,\
        IrpMajorFunctionSym(IrpSp->MajorFunction),\
        IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MajorFunction),\
        IrpSp->Flags,                   \
        __VA_ARGS__);                   \
    return Result
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

/* types */
enum
{
    FspFileSystemDeviceExtensionKind = 'F',
    FspVolumeDeviceExtensionKind = 'V',
};
typedef struct
{
    UINT8 Kind;
} FSP_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
} FSP_FILE_SYSTEM_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
} FSP_VOLUME_DEVICE_EXTENSION;
static inline
FSP_DEVICE_EXTENSION *FspDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FILE_SYSTEM_DEVICE_EXTENSION *FspFileSystemDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}
static inline
FSP_VOLUME_DEVICE_EXTENSION *FspVolumeDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}

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

/* debug */
#if DBG
BOOLEAN HasDbgBreakPoint(const char *Function);
const char *NtStatusSym(NTSTATUS Status);
const char *IrpMajorFunctionSym(UCHAR MajorFunction);
const char *IrpMinorFunctionSym(UCHAR MajorFunction, UCHAR MinorFunction);
#endif

/* extern */
extern PDEVICE_OBJECT FspFileSystemDeviceObject;

/* I/O increment */
#define FSP_IO_INCREMENT                IO_NETWORK_INCREMENT

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */

#endif
