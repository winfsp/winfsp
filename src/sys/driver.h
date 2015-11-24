/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#include <ntifs.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include <winfsp/fsctl.h>

#define DRIVER_NAME                     "WinFsp"

/* IoCreateDeviceSecure default SDDL's */
#define FSP_FSCTL_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */
#define FSP_FSVRT_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */

/* DEBUGLOG */
#if DBG
#define DEBUGLOG(fmt, ...)              \
    DbgPrint(DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

/* FSP_ENTER/FSP_LEAVE */
#if DBG
#define FSP_DEBUGLOG_(fmt, rfmt, ...)   \
    DbgPrint(AbnormalTermination() ?    \
        DRIVER_NAME "!" __FUNCTION__ "(" fmt ") = !AbnormalTermination\n" :\
        DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n",\
        __VA_ARGS__)
#define FSP_DEBUGBRK_()                 \
    do                                  \
    {                                   \
        if (HasDbgBreakPoint(__FUNCTION__))\
            try { DbgBreakPoint(); } except (EXCEPTION_EXECUTE_HANDLER) {}\
    } while (0,0)
#else
#define FSP_DEBUGLOG_(fmt, rfmt, ...)   ((void)0)
#define FSP_DEBUGBRK_()                 ((void)0)
#endif
#define FSP_ENTER_(...)                 \
    FSP_DEBUGBRK_();                    \
    FsRtlEnterFileSystem();             \
    try                                 \
    {                                   \
        __VA_ARGS__
#define FSP_LEAVE_(...)                 \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
    }                                   \
    finally                             \
    {                                   \
        __VA_ARGS__;                    \
        FsRtlExitFileSystem();          \
    }
#define FSP_ENTER(...)                  \
    NTSTATUS Result = STATUS_SUCCESS; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE(fmt, ...)             \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, NtStatusSym(Result))); return Result
#define FSP_ENTER_MJ(...)               \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);\
    FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_MJ(fmt, ...)          \
    FSP_LEAVE_(                         \
        FSP_DEBUGLOG_("%c%c, %s%s, IrpSp->Flags=%x, " fmt, " = %s[%lld]",\
            FspDeviceExtension(DeviceObject)->Kind,\
            Irp->RequestorMode == KernelMode ? 'K' : 'U',\
            IrpMajorFunctionSym(IrpSp->MajorFunction),\
            IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MajorFunction),\
            IrpSp->Flags,               \
            __VA_ARGS__,                \
            NtStatusSym(Result),        \
            !NT_SUCCESS(Result) ? 0 : Irp->IoStatus.Information);\
        if (STATUS_PENDING == Result)   \
            IoMarkIrpPending(Irp);      \
        else                            \
        {                               \
            if (!NT_SUCCESS(Result))    \
                Irp->IoStatus.Information = 0;\
            Irp->IoStatus.Status = Result;\
            IoCompleteRequest(Irp, FSP_IO_INCREMENT);\
        }                               \
    );                                  \
    return Result
#define FSP_ENTER_BOOL(...)             \
    BOOLEAN Result = TRUE; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_BOOL(fmt, ...)        \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, Result ? "TRUE" : "FALSE")); return Result
#define FSP_ENTER_VOID(...)             \
    FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_VOID(fmt, ...)        \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, "", __VA_ARGS__))
#define FSP_RETURN(...)                 \
    do                                  \
    {                                   \
        __VA_ARGS__;                    \
        goto fsp_leave_label;           \
    } while (0,0)

/* misc macros */
#define FSP_TAG                         ' psF'
#define FSP_IO_INCREMENT                IO_NETWORK_INCREMENT

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */

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

/* I/O queue */
typedef struct
{
    KSPIN_LOCK SpinLock;
    int Enabled;
    LIST_ENTRY PendingIrpList, ProcessIrpList;
    IO_CSQ PendingIoCsq, ProcessIoCsq;
} FSP_IOQ;
VOID FspIoqInitialize(FSP_IOQ *Ioq);
VOID FspIoqEnable(FSP_IOQ *Ioq, int Delta);
BOOLEAN FspIoqPostIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq);
BOOLEAN FspIoqProcessIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqRemoveProcessIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint);
VOID FspIoqCancelAll(FSP_IOQ *Ioq);

/* device extensions */
enum
{
    FspFsctlDeviceExtensionKind = 'C',  /* file system control device (e.g. \Device\WinFsp.Disk) */
    FspFsvrtDeviceExtensionKind = 'V',  /* virtual volume device (e.g. \Device\Volume{GUID}) */
    FspFsvolDeviceExtensionKind = 'F',  /* file system volume device (unnamed) */
};
typedef struct
{
    UINT8 Kind;
} FSP_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
} FSP_FSCTL_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    FSP_IOQ Ioq;
    UINT8 SecurityDescriptorBuf[];
} FSP_FSVRT_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
} FSP_FSVOL_DEVICE_EXTENSION;
static inline
FSP_DEVICE_EXTENSION *FspDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSCTL_DEVICE_EXTENSION *FspFsctlDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSVRT_DEVICE_EXTENSION *FspFsvrtDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSVOL_DEVICE_EXTENSION *FspFsvolDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}

/* misc */
NTSTATUS CreateGuid(GUID *Guid);
NTSTATUS SecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode);

/* debug */
#if DBG
BOOLEAN HasDbgBreakPoint(const char *Function);
const char *NtStatusSym(NTSTATUS Status);
const char *IrpMajorFunctionSym(UCHAR MajorFunction);
const char *IrpMinorFunctionSym(UCHAR MajorFunction, UCHAR MinorFunction);
const char *IoctlCodeSym(ULONG ControlCode);
#endif

/* extern */
extern PDEVICE_OBJECT FspFsctlDiskDeviceObject;
extern PDEVICE_OBJECT FspFsctlNetDeviceObject;

#endif
