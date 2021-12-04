/**
 * @file sys/driver.h
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#define WINFSP_SYS_INTERNAL

#define POOL_NX_OPTIN                   1
#include <ntifs.h>
#include <mountdev.h>
#include <ntddstor.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include <winfsp/fsctl.h>
#include <winfsp/fsext.h>

#include <shared/ku/config.h>

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */

#define DRIVER_NAME                     FSP_FSCTL_DRIVER_NAME

#if _WIN64
#define FSP_REGKEY                      "\\Registry\\Machine\\Software\\WOW6432Node\\" FSP_FSCTL_PRODUCT_NAME
#else
#define FSP_REGKEY                      "\\Registry\\Machine\\Software\\" FSP_FSCTL_PRODUCT_NAME
#endif

/* IoCreateDeviceSecure default SDDL's */
#define FSP_FSCTL_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */
#define FSP_FSVRT_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGX;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ|GENERIC_EXECUTE */

/* private NTSTATUS codes */
#define FSP_STATUS_PRIVATE_BIT          (0x20000000)
#define FSP_STATUS_IGNORE_BIT           (0x10000000)
#define FSP_STATUS_IOQ_POST             (FSP_STATUS_PRIVATE_BIT | 0x0000)
#define FSP_STATUS_IOQ_POST_BEST_EFFORT (FSP_STATUS_PRIVATE_BIT | 0x0001)

/* misc macros */
#define FSP_ALLOC_INTERNAL_TAG          'IpsF'
#define FSP_ALLOC_EXTERNAL_TAG          'XpsF'
#define FSP_IO_INCREMENT                IO_NETWORK_INCREMENT

/* debug */
#if DBG
enum
{
    fsp_debug_bp_generic                = 0x00000001,   /* generic breakpoint switch */
    fsp_debug_bp_drvrld                 = 0x00000002,   /* DriverEntry/Unload breakpoint switch */
    fsp_debug_bp_ioentr                 = 0x00000004,   /* I/O entry breakpoint switch */
    fsp_debug_bp_ioprep                 = 0x00000008,   /* I/O prepare breakpoint switch */
    fsp_debug_bp_iocmpl                 = 0x00000010,   /* I/O complete breakpoint switch */
    fsp_debug_bp_iocall                 = 0x00000020,   /* I/O callback breakpoint switch */
    fsp_debug_bp_iorecu                 = 0x00000040,   /* I/O recursive breakpoint switch */
    fsp_debug_dt                        = 0x01000000,   /* DEBUGTEST switch */
    fsp_debug_dp                        = 0x10000000,   /* DbgPrint switch */
};
extern __declspec(selectany) int fsp_debug =
    fsp_debug_bp_drvrld | fsp_debug_dt;
const char *NtStatusSym(NTSTATUS Status);
const char *IrpMajorFunctionSym(UCHAR MajorFunction);
const char *IrpMinorFunctionSym(UCHAR MajorFunction, UCHAR MinorFunction);
const char *IoctlCodeSym(ULONG ControlCode);
const char *FileInformationClassSym(FILE_INFORMATION_CLASS FileInformationClass);
const char *FsInformationClassSym(FS_INFORMATION_CLASS FsInformationClass);
const char *DeviceExtensionKindSym(UINT32 Kind);
ULONG DebugRandom(VOID);
VOID FspDebugLogIrp(const char *func, PIRP Irp, NTSTATUS Result);
#endif

/* DbgPrint */
#if DBG
#define DbgPrint(...)                   \
    ((void)((fsp_debug & fsp_debug_dp) ? DbgPrint(__VA_ARGS__) : 0))
#endif

/* DEBUGLOG */
#if DBG
#define DEBUGLOG(fmt, ...)              \
    DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__)
#define DEBUGLOGIRP(Irp, Result)        FspDebugLogIrp(__FUNCTION__, Irp, Result)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#define DEBUGLOGIRP(Irp, Result)        ((void)0)
#endif

/* DEBUGBREAK */
#if DBG
#define DEBUGBREAK_CRIT()               \
    do                                  \
    {                                   \
        static int bp = 1;              \
        if (bp && !KD_DEBUGGER_NOT_PRESENT)\
            DbgBreakPoint();            \
    } while (0,0)
#define DEBUGBREAK()                    \
    do                                  \
    {                                   \
        static int bp = 1;              \
        if (bp && (fsp_debug & fsp_debug_bp_generic) && !KD_DEBUGGER_NOT_PRESENT)\
            DbgBreakPoint();            \
    } while (0,0)
#define DEBUGBREAK_EX(category)         \
    do                                  \
    {                                   \
        static int bp = 1;              \
        if (bp && (fsp_debug & fsp_debug_bp_ ## category) && !KD_DEBUGGER_NOT_PRESENT)\
            DbgBreakPoint();            \
    } while (0,0)
#else
#define DEBUGBREAK_CRIT()               do {} while (0,0)
#define DEBUGBREAK()                    do {} while (0,0)
#define DEBUGBREAK_EX(category)         do {} while (0,0)
#endif

/* DEBUGTEST */
#if DBG
#define DEBUGTEST(Percent)              \
    (0 == (fsp_debug & fsp_debug_dt) || DebugRandom() <= (Percent) * 0x7fff / 100)
#define DEBUGTEST_EX(C, Percent, Deflt) \
    (0 != (fsp_debug & fsp_debug_dt) && (C) ? (DebugRandom() <= (Percent) * 0x7fff / 100) : (Deflt))
#else
#define DEBUGTEST(Percent)              (TRUE)
#define DEBUGTEST_EX(C, Percent, Deflt) (Deflt)
#endif

/* trace */
#if FSP_TRACE_ENABLED
VOID FspTraceInitialize(VOID);
VOID FspTraceFinalize(VOID);
VOID FspTrace(const char *file, int line, const char *func);
VOID FspTraceNtStatus(const char *file, int line, const char *func, NTSTATUS Status);
#define FSP_TRACE_INIT()                \
    FspTraceInitialize()
#define FSP_TRACE_FINI()                \
    FspTraceFinalize()
#define FSP_TRACE()                     \
    FspTrace(                           \
        __FILE__,                       \
        __LINE__,                       \
        __FUNCTION__)
#define FSP_TRACE_NTSTATUS(Status)      \
    FspTraceNtStatus(                   \
        __FILE__,                       \
        __LINE__,                       \
        __FUNCTION__,                   \
        Status)
#else
#define FSP_TRACE_INIT()                \
    ((VOID)0)
#define FSP_TRACE_FINI()                \
    ((VOID)0)
#define FSP_TRACE()                     \
    ((VOID)0)
#define FSP_TRACE_NTSTATUS(Result)      \
    ((VOID)0)
#endif
#undef STATUS_INSUFFICIENT_RESOURCES
#define STATUS_INSUFFICIENT_RESOURCES   (FSP_TRACE_NTSTATUS(0xC000009AL), (NTSTATUS)0xC000009AL)

/* FSP_ENTER/FSP_LEAVE */
#if DBG
#define FSP_DEBUGLOG_(fmt, rfmt, ...)   \
    DbgPrint(AbnormalTermination() ?    \
        "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ") = *AbnormalTermination*\n" :\
        "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n",\
        KeGetCurrentIrql(), __VA_ARGS__)
#define FSP_DEBUGLOG_NOCRIT_(fmt, rfmt, ...)\
    DbgPrint(                           \
        "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n",\
        KeGetCurrentIrql(), __VA_ARGS__)
#else
#define FSP_DEBUGLOG_(fmt, rfmt, ...)   ((void)0)
#define FSP_DEBUGLOG_NOCRIT_(fmt, rfmt, ...)((void)0)
#endif
#define FSP_ENTER_(bpcat, ...)          \
    DEBUGBREAK_EX(bpcat);               \
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
#define FSP_ENTER_NOCRIT_(bpcat, ...)   \
    DEBUGBREAK_EX(bpcat);               \
    {                                   \
        __VA_ARGS__
#define FSP_LEAVE_NOCRIT_(...)          \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
        __VA_ARGS__;                    \
    }
#define FSP_ENTER(...)                  \
    NTSTATUS Result = STATUS_SUCCESS; FSP_ENTER_(iocall, __VA_ARGS__)
#define FSP_LEAVE(fmt, ...)             \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, NtStatusSym(Result))); return Result
#define FSP_ENTER_DRV(...)              \
    NTSTATUS Result = STATUS_SUCCESS; FSP_ENTER_(drvrld, __VA_ARGS__)
#define FSP_LEAVE_DRV(fmt, ...)         \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, NtStatusSym(Result))); return Result
#define FSP_ENTER_MJ(...)               \
    if (FspFsmupDeviceExtensionKind == FspDeviceExtension(DeviceObject)->Kind)\
        return FspMupHandleIrp(DeviceObject, Irp);\
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);\
    BOOLEAN fsp_device_deref = FALSE;   \
    PIRP fsp_top_level_irp = IoGetTopLevelIrp();\
    FSP_ENTER_(ioentr, __VA_ARGS__);    \
    do                                  \
    {                                   \
        if (0 != fsp_top_level_irp)     \
            FspPropagateTopFlags(Irp, fsp_top_level_irp);\
        IoSetTopLevelIrp(Irp);          \
        if (!FspDeviceReference(DeviceObject))\
        {                               \
            Result = STATUS_CANCELLED;  \
            goto fsp_leave_label;       \
        }                               \
        fsp_device_deref = TRUE;        \
    } while (0,0)
#define FSP_LEAVE_MJ(fmt, ...)          \
    FSP_LEAVE_(                         \
        if (STATUS_PENDING != Result && !(FSP_STATUS_IGNORE_BIT & Result))\
        {                               \
            ASSERT(0 == (FSP_STATUS_PRIVATE_BIT & Result) ||\
                FSP_STATUS_IOQ_POST == Result || FSP_STATUS_IOQ_POST_BEST_EFFORT == Result);\
            FSP_DEBUGLOG_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
                Irp,                    \
                DeviceExtensionKindSym(FspDeviceExtension(IrpSp->DeviceObject)->Kind),\
                Irp->RequestorMode == KernelMode ? 'K' : 'U',\
                IrpMajorFunctionSym(IrpSp->MajorFunction),\
                IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),\
                __VA_ARGS__,            \
                NtStatusSym(Result),    \
                (LONGLONG)Irp->IoStatus.Information);\
            if (FSP_STATUS_PRIVATE_BIT & Result)\
            {                           \
                FSP_FSVOL_DEVICE_EXTENSION *fsp_leave_FsvolDeviceExtension =\
                    FspFsvolDeviceExtension(DeviceObject);\
                if (!FspIoqPostIrpEx(fsp_leave_FsvolDeviceExtension->Ioq, Irp,\
                    FSP_STATUS_IOQ_POST_BEST_EFFORT == Result, &Result))\
                {                       \
                    DEBUGLOG("FspIoqPostIrpEx = %s", NtStatusSym(Result));\
                    FspIopCompleteIrp(Irp, Result);\
                }                       \
            }                           \
            else                        \
                FspIopCompleteIrpEx(Irp, Result, fsp_device_deref);\
        }                               \
        else                            \
            Result &= ~FSP_STATUS_IGNORE_BIT;\
        IoSetTopLevelIrp(fsp_top_level_irp);\
    );                                  \
    return Result
#define FSP_ENTER_IOC(...)              \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp); (VOID)IrpSp;\
    FSP_ENTER_NOCRIT_(iocmpl, __VA_ARGS__)
#define FSP_LEAVE_IOC(fmt, ...)         \
    FSP_LEAVE_NOCRIT_(                  \
        if (STATUS_PENDING != Result)   \
        {                               \
            ASSERT(0 == (FSP_STATUS_PRIVATE_BIT & Result));\
            FSP_DEBUGLOG_NOCRIT_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
                Irp,                    \
                DeviceExtensionKindSym(FspDeviceExtension(IrpSp->DeviceObject)->Kind),\
                Irp->RequestorMode == KernelMode ? 'K' : 'U',\
                IrpMajorFunctionSym(IrpSp->MajorFunction),\
                IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),\
                __VA_ARGS__,            \
                NtStatusSym(Result),    \
                (LONGLONG)Irp->IoStatus.Information);\
            FspIopCompleteIrp(Irp, Result);\
        }                               \
    );                                  \
    return Result
#define FSP_ENTER_BOOL(...)             \
    BOOLEAN Result = TRUE; FSP_ENTER_(iocall, __VA_ARGS__)
#define FSP_LEAVE_BOOL(fmt, ...)        \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, Result ? "TRUE" : "FALSE")); return Result
#define FSP_ENTER_VOID(...)             \
    FSP_ENTER_(iocall, __VA_ARGS__)
#define FSP_LEAVE_VOID(fmt, ...)        \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, "", __VA_ARGS__))
#define FSP_RETURN(...)                 \
    do                                  \
    {                                   \
        __VA_ARGS__;                    \
        goto fsp_leave_label;           \
    } while (0,0)

/* missing typedef */
typedef const void *PCVOID;

/* driver major functions */
_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_max_(APC_LEVEL)
    /* see https://msdn.microsoft.com/en-us/library/windows/hardware/ff540124(v=vs.85).aspx */
_IRQL_requires_same_
typedef NTSTATUS FSP_DRIVER_DISPATCH(
    _In_ struct _DEVICE_OBJECT *DeviceObject, _Inout_ struct _IRP *Irp);
_Dispatch_type_(IRP_MJ_CLEANUP)         FSP_DRIVER_DISPATCH FspCleanup;
_Dispatch_type_(IRP_MJ_CLOSE)           FSP_DRIVER_DISPATCH FspClose;
_Dispatch_type_(IRP_MJ_CREATE)          FSP_DRIVER_DISPATCH FspCreate;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)  FSP_DRIVER_DISPATCH FspDeviceControl;
_Dispatch_type_(IRP_MJ_DIRECTORY_CONTROL) FSP_DRIVER_DISPATCH FspDirectoryControl;
_Dispatch_type_(IRP_MJ_FILE_SYSTEM_CONTROL) FSP_DRIVER_DISPATCH FspFileSystemControl;
_Dispatch_type_(IRP_MJ_FLUSH_BUFFERS)   FSP_DRIVER_DISPATCH FspFlushBuffers;
_Dispatch_type_(IRP_MJ_LOCK_CONTROL)    FSP_DRIVER_DISPATCH FspLockControl;
_Dispatch_type_(IRP_MJ_QUERY_EA)        FSP_DRIVER_DISPATCH FspQueryEa;
_Dispatch_type_(IRP_MJ_QUERY_INFORMATION) FSP_DRIVER_DISPATCH FspQueryInformation;
_Dispatch_type_(IRP_MJ_QUERY_SECURITY)  FSP_DRIVER_DISPATCH FspQuerySecurity;
_Dispatch_type_(IRP_MJ_QUERY_VOLUME_INFORMATION) FSP_DRIVER_DISPATCH FspQueryVolumeInformation;
_Dispatch_type_(IRP_MJ_READ)            FSP_DRIVER_DISPATCH FspRead;
_Dispatch_type_(IRP_MJ_SET_EA)          FSP_DRIVER_DISPATCH FspSetEa;
_Dispatch_type_(IRP_MJ_SET_INFORMATION) FSP_DRIVER_DISPATCH FspSetInformation;
_Dispatch_type_(IRP_MJ_SET_SECURITY)    FSP_DRIVER_DISPATCH FspSetSecurity;
_Dispatch_type_(IRP_MJ_SET_VOLUME_INFORMATION) FSP_DRIVER_DISPATCH FspSetVolumeInformation;
_Dispatch_type_(IRP_MJ_SHUTDOWN)        FSP_DRIVER_DISPATCH FspShutdown;
_Dispatch_type_(IRP_MJ_WRITE)           FSP_DRIVER_DISPATCH FspWrite;

/* I/O processing functions */
_IRQL_requires_max_(APC_LEVEL)
_IRQL_requires_same_
typedef NTSTATUS FSP_IOPREP_DISPATCH(
    _Inout_ PIRP Irp, _Inout_ FSP_FSCTL_TRANSACT_REQ *Request);
_IRQL_requires_max_(APC_LEVEL)
_IRQL_requires_same_
typedef NTSTATUS FSP_IOCMPL_DISPATCH(
    _Inout_ PIRP Irp, _In_ const FSP_FSCTL_TRANSACT_RSP *Response);
FSP_IOCMPL_DISPATCH FspFsvolCleanupComplete;
FSP_IOCMPL_DISPATCH FspFsvolCloseComplete;
FSP_IOPREP_DISPATCH FspFsvolCreatePrepare;
FSP_IOCMPL_DISPATCH FspFsvolCreateComplete;
FSP_IOCMPL_DISPATCH FspFsvolDeviceControlComplete;
FSP_IOPREP_DISPATCH FspFsvolDirectoryControlPrepare;
FSP_IOCMPL_DISPATCH FspFsvolDirectoryControlComplete;
FSP_IOCMPL_DISPATCH FspFsvolFileSystemControlComplete;
FSP_IOCMPL_DISPATCH FspFsvolFlushBuffersComplete;
FSP_IOCMPL_DISPATCH FspFsvolLockControlComplete;
FSP_IOCMPL_DISPATCH FspFsvolQueryEaComplete;
FSP_IOCMPL_DISPATCH FspFsvolQueryInformationComplete;
FSP_IOCMPL_DISPATCH FspFsvolQuerySecurityComplete;
FSP_IOCMPL_DISPATCH FspFsvolQueryVolumeInformationComplete;
FSP_IOPREP_DISPATCH FspFsvolReadPrepare;
FSP_IOCMPL_DISPATCH FspFsvolReadComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetEaComplete;
FSP_IOPREP_DISPATCH FspFsvolSetInformationPrepare;
FSP_IOCMPL_DISPATCH FspFsvolSetInformationComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetSecurityComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetVolumeInformationComplete;
FSP_IOPREP_DISPATCH FspFsvolWritePrepare;
FSP_IOCMPL_DISPATCH FspFsvolWriteComplete;

/* fast I/O and resource acquisition callbacks */
FAST_IO_QUERY_BASIC_INFO FspFastIoQueryBasicInfo;
FAST_IO_QUERY_STANDARD_INFO FspFastIoQueryStandardInfo;
FAST_IO_QUERY_NETWORK_OPEN_INFO FspFastIoQueryNetworkOpenInfo;
FAST_IO_QUERY_OPEN FspFastIoQueryOpen;
FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;
FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;
BOOLEAN FspAcquireForLazyWrite(
    PVOID Context,
    BOOLEAN Wait);
VOID FspReleaseFromLazyWrite(
    PVOID Context);
BOOLEAN FspAcquireForReadAhead(
    PVOID Context,
    BOOLEAN Wait);
VOID FspReleaseFromReadAhead(
    PVOID Context);
VOID FspPropagateTopFlags(PIRP Irp, PIRP TopLevelIrp);

/* memory allocation */
#define FspAlloc(Size)                  ExAllocatePoolWithTag(PagedPool, Size, FSP_ALLOC_INTERNAL_TAG)
#define FspAllocNonPaged(Size)          ExAllocatePoolWithTag(NonPagedPool, Size, FSP_ALLOC_INTERNAL_TAG)
#define FspAllocMustSucceed(Size)       FspAllocatePoolMustSucceed(PagedPool, Size, FSP_ALLOC_INTERNAL_TAG)
#define FspFree(Pointer)                ExFreePoolWithTag(Pointer, FSP_ALLOC_INTERNAL_TAG)
#define FspAllocExternal(Size)          ExAllocatePoolWithTag(PagedPool, Size, FSP_ALLOC_EXTERNAL_TAG)
#define FspAllocNonPagedExternal(Size)  ExAllocatePoolWithTag(NonPagedPool, Size, FSP_ALLOC_EXTERNAL_TAG)
#define FspFreeExternal(Pointer)        ExFreePool(Pointer)

/* hash mix */
/* Based on the MurmurHash3 fmix32/fmix64 function:
 * See: https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp?r=152#68
 */
static inline
UINT32 FspHashMix32(UINT32 h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}
static inline
UINT64 FspHashMix64(UINT64 k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}
static inline
ULONG FspHashMixPointer(PVOID Pointer)
{
#if _WIN64
    return (ULONG)FspHashMix64((UINT64)Pointer);
#else
    return (ULONG)FspHashMix32((UINT32)Pointer);
#endif
}

/* timeouts */
#define FspTimeoutInfinity32            ((UINT32)-1L)
#define FspTimeoutInfinity64            ((UINT64)-1LL)
static inline
UINT64 FspTimeoutFromMillis(UINT32 Millis)
{
    /* if Millis is 0 or -1 then sign-extend else 10000ULL * Millis */
    return 1 >= Millis + 1 ? (INT64)(INT32)Millis : 10000ULL * Millis;
}
static inline
UINT64 FspExpirationTimeFromMillis(UINT32 Millis)
{
    /* if Millis is 0 or -1 then sign-extend else KeQueryInterruptTime() + 10000ULL * Millis */
    return 1 >= Millis + 1 ? (INT64)(INT32)Millis : KeQueryInterruptTime() + 10000ULL * Millis;
}
static inline
UINT64 FspExpirationTimeFromTimeout(UINT64 Timeout)
{
    /* if Timeout is 0 or -1 then Timeout else KeQueryInterruptTime() + Timeout */
    return 1 >= Timeout + 1 ? Timeout : KeQueryInterruptTime() + Timeout;
}
static inline
BOOLEAN FspExpirationTimeValid(UINT64 ExpirationTime)
{
    /* if ExpirationTime is 0 or -1 then ExpirationTime else KeQueryInterruptTime() < ExpirationTime */
    return 1 >= ExpirationTime + 1 ? (0 != ExpirationTime) : (KeQueryInterruptTime() < ExpirationTime);
}
static inline
BOOLEAN FspExpirationTimeValidEx(UINT64 ExpirationTime, UINT64 CurrentTime)
{
    /* if ExpirationTime is 0 or -1 then ExpirationTime else CurrentTime < ExpirationTime */
    return 1 >= ExpirationTime + 1 ? (0 != ExpirationTime) : (CurrentTime < ExpirationTime);
}
static inline
BOOLEAN FspExpirationTimeValid2(UINT64 ExpirationTime, UINT64 CurrentTime)
{
    return CurrentTime < ExpirationTime;
}

/* names */
enum
{
    FspFileNameStreamTypeNone           = 0,
    FspFileNameStreamTypeData           = 1,
};
BOOLEAN FspFileNameIsValid(PUNICODE_STRING Path, ULONG MaxComponentLength,
    PUNICODE_STRING StreamPart, PULONG StreamType);
BOOLEAN FspFileNameIsValidPattern(PUNICODE_STRING Pattern, ULONG MaxComponentLength);
BOOLEAN FspEaNameIsValid(PSTRING Name);
VOID FspFileNameSuffix(PUNICODE_STRING Path, PUNICODE_STRING Remain, PUNICODE_STRING Suffix);
#if 0
NTSTATUS FspFileNameUpcase(
    PUNICODE_STRING DestinationName,
    PUNICODE_STRING SourceName,
    PCWCH UpcaseTable);
VOID FspEaNameUpcase(
    PSTRING DestinationName,
    PSTRING SourceName,
    PCWCH UpcaseTable);
LONG FspFileNameCompare(
    PUNICODE_STRING Name1,
    PUNICODE_STRING Name2,
    BOOLEAN IgnoreCase,
    PCWCH UpcaseTable);
BOOLEAN FspFileNameIsPrefix(
    PCUNICODE_STRING Name1,
    PCUNICODE_STRING Name2,
    BOOLEAN IgnoreCase,
    PCWCH UpcaseTable);
#else
#define FspFileNameUpcase(D,S,U)        (ASSERT(0 == (U)), RtlUpcaseUnicodeString(D,S,FALSE))
#define FspEaNameUpcase(D,S,U)          (ASSERT(0 == (U)), RtlUpperString(D,S))
#define FspFileNameCompare(N1,N2,I,U)   (ASSERT(0 == (U)), RtlCompareUnicodeString(N1,N2,I))
#define FspFileNameIsPrefix(N1,N2,I,U)  (ASSERT(0 == (U)), RtlPrefixUnicodeString(N1,N2,I))
#endif
NTSTATUS FspFileNameInExpression(
    PUNICODE_STRING Expression,
    PUNICODE_STRING Name,
    BOOLEAN IgnoreCase,
    PWCH UpcaseTable,
    PBOOLEAN PResult);

/* UUID5 creation (ku) */
NTSTATUS FspUuid5Make(const UUID *Namespace, const VOID *Buffer, ULONG Size, UUID *Uuid);

/* utility */
PVOID FspAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag);
PVOID FspAllocateIrpMustSucceed(CCHAR StackSize);
BOOLEAN FspIsNtDdiVersionAvailable(ULONG RequestedVersion);
NTSTATUS FspCreateGuid(GUID *Guid);
NTSTATUS FspGetDeviceObjectPointer(PUNICODE_STRING ObjectName, ACCESS_MASK DesiredAccess,
    PULONG PFileNameIndex, PFILE_OBJECT *PFileObject, PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspRegistryGetValue(PUNICODE_STRING Path, PUNICODE_STRING ValueName,
    PKEY_VALUE_PARTIAL_INFORMATION ValueInformation, PULONG PValueInformationLength);
NTSTATUS FspSendSetInformationIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FILE_INFORMATION_CLASS FileInformationClass, PVOID FileInformation, ULONG Length);
NTSTATUS FspSendQuerySecurityIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PULONG PLength);
NTSTATUS FspSendQueryEaIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    PFILE_GET_EA_INFORMATION GetEa, ULONG GetEaLength,
    PFILE_FULL_EA_INFORMATION Ea, PULONG PEaLength);
NTSTATUS FspSendMountmgrDeviceControlIrp(ULONG IoControlCode,
    PVOID SystemBuffer, ULONG InputBufferLength, PULONG POutputBufferLength);
NTSTATUS FspBufferUserBuffer(PIRP Irp, ULONG Length, LOCK_OPERATION Operation);
NTSTATUS FspLockUserBuffer(PIRP Irp, ULONG Length, LOCK_OPERATION Operation);
NTSTATUS FspMapLockedPagesInUserMode(PMDL Mdl, PVOID *PAddress, ULONG ExtraPriorityFlags);
NTSTATUS FspCcInitializeCacheMap(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes,
    BOOLEAN PinAccess, PCACHE_MANAGER_CALLBACKS Callbacks, PVOID CallbackContext);
NTSTATUS FspCcSetFileSizes(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes);
NTSTATUS FspCcCopyRead(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    BOOLEAN Wait, PVOID Buffer, PIO_STATUS_BLOCK IoStatus);
NTSTATUS FspCcCopyWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    BOOLEAN Wait, PVOID Buffer);
NTSTATUS FspCcMdlRead(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    PMDL *PMdlChain, PIO_STATUS_BLOCK IoStatus);
NTSTATUS FspCcMdlReadComplete(PFILE_OBJECT FileObject, PMDL MdlChain);
NTSTATUS FspCcPrepareMdlWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    PMDL *PMdlChain, PIO_STATUS_BLOCK IoStatus);
NTSTATUS FspCcMdlWriteComplete(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, PMDL MdlChain);
NTSTATUS FspCcFlushCache(PSECTION_OBJECT_POINTERS SectionObjectPointer,
    PLARGE_INTEGER FileOffset, ULONG Length, PIO_STATUS_BLOCK IoStatus);
NTSTATUS FspQuerySecurityDescriptorInfo(SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PULONG PLength,
    PSECURITY_DESCRIPTOR ObjectsSecurityDescriptor);
NTSTATUS FspEaBufferFromOriginatingProcessValidate(
    PFILE_FULL_EA_INFORMATION Buffer,
    ULONG Length,
    PULONG PErrorOffset);
NTSTATUS FspEaBufferFromFileSystemValidate(
    PFILE_FULL_EA_INFORMATION Buffer,
    ULONG Length,
    PULONG PErrorOffset);
NTSTATUS FspNotifyInitializeSync(PNOTIFY_SYNC *NotifySync);
NTSTATUS FspNotifyFullChangeDirectory(
    PNOTIFY_SYNC NotifySync,
    PLIST_ENTRY NotifyList,
    PVOID FsContext,
    PSTRING FullDirectoryName,
    BOOLEAN WatchTree,
    BOOLEAN IgnoreBuffer,
    ULONG CompletionFilter,
    PIRP NotifyIrp,
    PCHECK_FOR_TRAVERSE_ACCESS TraverseCallback,
    PSECURITY_SUBJECT_CONTEXT SubjectContext);
NTSTATUS FspNotifyFullReportChange(
    PNOTIFY_SYNC NotifySync,
    PLIST_ENTRY NotifyList,
    PSTRING FullTargetName,
    USHORT TargetNameOffset,
    PSTRING StreamName,
    PSTRING NormalizedParentName,
    ULONG FilterMatch,
    ULONG Action,
    PVOID TargetContext);
NTSTATUS FspOplockBreakH(
    POPLOCK Oplock,
    PIRP Irp,
    ULONG Flags,
    PVOID Context,
    POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine,
    POPLOCK_FS_PREPOST_IRP PostIrpRoutine);
NTSTATUS FspCheckOplock(
    POPLOCK Oplock,
    PIRP Irp,
    PVOID Context,
    POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine,
    POPLOCK_FS_PREPOST_IRP PostIrpRoutine);
NTSTATUS FspCheckOplockEx(
    POPLOCK Oplock,
    PIRP Irp,
    ULONG Flags,
    PVOID Context,
    POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine,
    POPLOCK_FS_PREPOST_IRP PostIrpRoutine);
NTSTATUS FspOplockFsctrl(
    POPLOCK Oplock,
    PIRP Irp,
    ULONG OpenCount);
#define FspNotifyUninitializeSync(NS)\
    FsRtlNotifyUninitializeSync(NS)
#define FspNotifyCleanupAll(NS, NL)\
    FsRtlNotifyCleanupAll(NS, NL)
#define FspNotifyChangeDirectory(NS, NL, FC, FN, WT, CF, I)\
    FspNotifyFullChangeDirectory(NS, NL, FC, (PSTRING)(FN), WT, FALSE, CF, I, 0, 0)
#define FspNotifyCleanup(NS, NL, FC)\
    FsRtlNotifyCleanup(NS, NL, FC)
#define FspNotifyDeletePending(NS, NL, FC)\
    FspNotifyFullChangeDirectory(NS, NL, FC, 0, 0, FALSE, 0, 0, 0, 0)
#define FspNotifyReportChange(NS, NL, FN, FO, NP, F, A)\
    FspNotifyFullReportChange(NS, NL, (PSTRING)(FN), FO, 0, (PSTRING)(NP), F, A, 0)
#define FSP_NEXT_EA(Ea, EaEnd)          \
    (0 != (Ea)->NextEntryOffset ? (PVOID)((PUINT8)(Ea) + (Ea)->NextEntryOffset) : (EaEnd))

/* utility: synchronous work queue */
typedef struct
{
    KEVENT Event;
    PWORKER_THREAD_ROUTINE Routine;
    PVOID Context;
    WORK_QUEUE_ITEM WorkQueueItem;
} FSP_SYNCHRONOUS_WORK_ITEM;
VOID FspInitializeSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspExecuteSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem);

/* utility: delayed work queue */
typedef struct
{
    KTIMER Timer;
    KDPC Dpc;
    WORK_QUEUE_ITEM WorkQueueItem;
} FSP_DELAYED_WORK_ITEM;
VOID FspInitializeDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspQueueDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem, LARGE_INTEGER Delay);

/* utility: safe MDL */
typedef struct
{
    PMDL Mdl;
    PVOID Buffer;
    PMDL UserMdl;
    LOCK_OPERATION Operation;
} FSP_SAFE_MDL;
BOOLEAN FspSafeMdlCheck(PMDL Mdl);
NTSTATUS FspSafeMdlCreate(PMDL UserMdl, LOCK_OPERATION Operation, FSP_SAFE_MDL **PSafeMdl);
VOID FspSafeMdlCopyBack(FSP_SAFE_MDL *SafeMdl);
VOID FspSafeMdlDelete(FSP_SAFE_MDL *SafeMdl);

/* utility: hook IRP completion */
NTSTATUS FspIrpHook(PIRP Irp, PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID OwnContext);
VOID FspIrpHookReset(PIRP Irp);
PVOID FspIrpHookContext(PVOID Context);
NTSTATUS FspIrpHookNext(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

/* utility: wait groups
 *
 * A Wait Group is a synchronization primitive that encapsulates a counter.
 * A Wait Group is considered signaled when the counter is 0 and non-signaled
 * when the counter is non-0. (Wait Group functionality is similar to Golang's
 * sync.WaitGroup.)
 *
 * Wait Groups must always be allocated in non-paged storage.
 */
typedef struct
{
    KEVENT Event;
    LONG Count;
    KSPIN_LOCK SpinLock;
} FSP_WGROUP;
VOID FspWgroupInitialize(FSP_WGROUP *Wgroup);
VOID FspWgroupIncrement(FSP_WGROUP *Wgroup);
VOID FspWgroupDecrement(FSP_WGROUP *Wgroup);
VOID FspWgroupSignalPermanently(FSP_WGROUP *Wgroup);
NTSTATUS FspWgroupWait(FSP_WGROUP *Wgroup,
    KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER PTimeout);

/* silos */
typedef struct
{
    PDEVICE_OBJECT FsctlDiskDeviceObject;
    PDEVICE_OBJECT FsctlNetDeviceObject;
    PDEVICE_OBJECT FsmupDeviceObject;
    HANDLE MupHandle;
    WCHAR FsmupDeviceNameBuf[64];
} FSP_SILO_GLOBALS;
typedef NTSTATUS (*FSP_SILO_INIT_CALLBACK)(VOID);
typedef VOID (*FSP_SILO_FINI_CALLBACK)(VOID);
NTSTATUS FspSiloGetGlobals(FSP_SILO_GLOBALS **PGlobals);
VOID FspSiloDereferenceGlobals(FSP_SILO_GLOBALS *Globals);
VOID FspSiloGetContainerId(GUID *ContainerId);
NTSTATUS FspSiloInitialize(FSP_SILO_INIT_CALLBACK Init, FSP_SILO_FINI_CALLBACK Fini);
VOID FspSiloFinalize(VOID);

/* process buffers */
#define FspProcessBufferSizeMax         (64 * 1024)
NTSTATUS FspProcessBufferInitialize(VOID);
VOID FspProcessBufferFinalize(VOID);
VOID FspProcessBufferCollect(HANDLE ProcessId);
NTSTATUS FspProcessBufferAcquire(SIZE_T BufferSize, PVOID *PBufferCookie, PVOID *PBuffer);
VOID FspProcessBufferRelease(PVOID BufferCookie, PVOID Buffer);

/* IRP context */
#define FspIrpTimestampInfinity         ((ULONG)-1L)
#define FspIrpTimestamp(Irp)            \
    (*(ULONG *)&(Irp)->Tail.Overlay.DriverContext[0])
#define FspIrpDictNext(Irp)             \
    (*(PIRP *)&(Irp)->Tail.Overlay.DriverContext[1])
static inline
FSP_FSCTL_TRANSACT_REQ *FspIrpRequest(PIRP Irp)
{
    return (PVOID)((UINT_PTR)Irp->Tail.Overlay.DriverContext[2] & ~0xf);
}
static inline
VOID FspIrpSetRequest(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request)
{
    ASSERT(0 == ((UINT_PTR)Request & 0xf));
    ULONG Flags = (ULONG)((UINT_PTR)Irp->Tail.Overlay.DriverContext[2] & 0xf);
    Irp->Tail.Overlay.DriverContext[2] = (PVOID)((UINT_PTR)Request | Flags);
}
static inline
ULONG FspIrpFlags(PIRP Irp)
{
    return (ULONG)((UINT_PTR)Irp->Tail.Overlay.DriverContext[2] & 3);
}
static inline
VOID FspIrpSetFlags(PIRP Irp, ULONG Flags)
{
    ASSERT(3 >= Flags);
    FSP_FSCTL_TRANSACT_REQ *Request = (PVOID)((UINT_PTR)Irp->Tail.Overlay.DriverContext[2] & ~3);
    Irp->Tail.Overlay.DriverContext[2] = (PVOID)((UINT_PTR)Request | Flags);
}
static inline
ULONG FspIrpTopFlags(PIRP Irp)
{
    return (ULONG)((UINT_PTR)Irp->Tail.Overlay.DriverContext[2] & 0xc) >> 2;
}
static inline
VOID FspIrpSetTopFlags(PIRP Irp, ULONG Flags)
{
    ASSERT(3 >= Flags);
    FSP_FSCTL_TRANSACT_REQ *Request = (PVOID)((UINT_PTR)Irp->Tail.Overlay.DriverContext[2] & ~0xc);
    Irp->Tail.Overlay.DriverContext[2] = (PVOID)((UINT_PTR)Request | (Flags << 2));
}

/*
 * Queued Events
 *
 * Queued Events are an implementation of SynchronizationEvent's using
 * a KQUEUE. The reason we do this is because a KQUEUE has some desirable
 * properties:
 *
 * - It has a LIFO wait discipline, which is advantageous in many situations.
 * - It can limit the numbers of threads that can be satisfied concurrently.
 *
 * Queued Events must always be allocated in non-paged storage.
 *
 * Here is how Queued Events work. A queued event consists of a KQUEUE and a
 * spin lock. There is also a LIST_ENTRY which is used as a dummy item to
 * place in the KQUEUE.
 *
 * The KQUEUE is guaranteed to contain either 0 or 1 items. When the KQUEUE
 * contains 0 items the queued event is considered non-signaled. When the
 * KQUEUE contains 1 items the queued event is considered signaled.
 *
 * To transition from the non-signaled to the signaled state, we acquire the
 * spin lock and then insert the dummy item in the KQUEUE using KeInsertQueue.
 * To transition from the signaled to the non-signaled state, we simply (wait
 * and) remove the dummy item from the KQUEUE using KeRemoveQueue (without
 * the use of the spin lock).
 *
 * EventSet:
 *     AcquireSpinLock
 *     if (0 == KeReadState())          // if KQUEUE is empty
 *         KeInsertQueue(DUMMY);
 *     ReleaseSpinLock
 *
 * EventWait:
 *     KeRemoveQueue();                 // (wait and) remove item
 *
 * First notice that EventSet is serialized by the use of the spin lock. This
 * guarantees that the dummy item can be only inserted ONCE in the KQUEUE
 * and that the only possible signaled state transitions for EventSet are 0->1
 * and 1->1. This is how KeSetEvent works for a SynchronizationEvent.
 *
 * Second notice that EventWait is not protected by the spin lock, which means
 * that it can happen at any time including concurrently with EventSet or
 * another EventWait. Notice also that for EventWait the only possible
 * transitions are 1->0 or 0->0 (0->block->0). This is how
 * KeWaitForSingleObject works for a SynchronizationEvent.
 *
 * We now have to consider what happens when we have one EventSet concurrently
 * with one or more EventWait's:
 *
 * 1.  The EventWait(s) happen before KeReadState. If the KQUEUE has an
 *     item one EventWait gets satisfied, otherwise it blocks. In this case
 *     KeReadState will read the KQUEUE's state as 0 and KeInsertQueue will
 *     insert the dummy item, which will unblock the EventWait.
 *
 * 2.  The EventWait(s) happen after KeReadState, but before KeInsertQueue.
 *     If the dummy item was already in the KQUEUE the KeReadState test will
 *     fail and KeInsertQueue will not be executed, but EventWait will be
 *     satisfied immediately. If the dummy item was not in the KQUEUE the
 *     KeReadState will succeed and EventWait will momentarily block until
 *     KeInsertQueue releases it.
 *
 * 3.  The EventWait(s) happen after KeInsertQueue. In this case the dummy
 *     item in is the KQUEUE already and one EventWait will be satisfied
 *     immediately.
 *
 * A final note: Queued Events cannot cleanly support an EventClear operation.
 * The obvious choice of using KeRemoveQueue with a 0 timeout is insufficient
 * because it would associate the current thread with the KQUEUE and that is
 * not desirable. KeRundownQueue cannot be used either because it
 * disassociates all threads from the KQUEUE.
 */
typedef struct
{
    KQUEUE Queue;
    LIST_ENTRY DummyEntry;
    KSPIN_LOCK SpinLock;
} FSP_QEVENT;
static inline
VOID FspQeventInitialize(FSP_QEVENT *Qevent, ULONG ThreadCount)
{
    KeInitializeQueue(&Qevent->Queue, ThreadCount);
    RtlZeroMemory(&Qevent->DummyEntry, sizeof Qevent->DummyEntry);
    KeInitializeSpinLock(&Qevent->SpinLock);
}
static inline
VOID FspQeventFinalize(FSP_QEVENT *Qevent)
{
    KeRundownQueue(&Qevent->Queue);
}
static inline
VOID FspQeventSetNoLock(FSP_QEVENT *Qevent)
{
    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    if (0 == KeReadStateQueue(&Qevent->Queue))
        KeInsertQueue(&Qevent->Queue, &Qevent->DummyEntry);
}
static inline
VOID FspQeventSet(FSP_QEVENT *Qevent)
{
    KIRQL Irql;
    KeAcquireSpinLock(&Qevent->SpinLock, &Irql);
    FspQeventSetNoLock(Qevent);
    KeReleaseSpinLock(&Qevent->SpinLock, Irql);
}
static inline
NTSTATUS FspQeventWait(FSP_QEVENT *Qevent,
    KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER PTimeout)
{
    PLIST_ENTRY ListEntry;
    KeRemoveQueueEx(&Qevent->Queue, WaitMode, Alertable, PTimeout, &ListEntry, 1);
    if (ListEntry == &Qevent->DummyEntry)
        return STATUS_SUCCESS;
    return (NTSTATUS)(UINT_PTR)ListEntry;
}
static inline
NTSTATUS FspQeventCancellableWait(FSP_QEVENT *Qevent,
    PLARGE_INTEGER PTimeout, PIRP Irp)
{
    NTSTATUS Result;
    UINT64 ExpirationTime = 0, InterruptTime;
    if (0 != PTimeout && 0 > PTimeout->QuadPart)
        ExpirationTime = KeQueryInterruptTime() - PTimeout->QuadPart;
retry:
    Result = FspQeventWait(Qevent, KernelMode, TRUE, PTimeout);
    if (STATUS_ALERTED == Result)
    {
        if (PsIsThreadTerminating(PsGetCurrentThread()))
            return STATUS_THREAD_IS_TERMINATING;
        if (0 != Irp && Irp->Cancel)
            return STATUS_CANCELLED;
        if (0 != ExpirationTime)
        {
            InterruptTime = KeQueryInterruptTime();
            if (ExpirationTime <= InterruptTime)
                return STATUS_TIMEOUT;
            PTimeout->QuadPart = (INT64)InterruptTime - (INT64)ExpirationTime;
        }
        goto retry;
    }
    return Result;
}

/* I/O queue */
#define FSP_IOQ_USE_QEVENT
#define FSP_IOQ_PROCESS_NO_CANCEL
#define FspIoqTimeout                   ((PIRP)1)
#define FspIoqCancelled                 ((PIRP)2)
#define FspIoqPostIrp(Q, I, R)          FspIoqPostIrpEx(Q, I, FALSE, R)
#define FspIoqPostIrpBestEffort(Q, I, R)FspIoqPostIrpEx(Q, I, TRUE, R)
typedef struct
{
    KSPIN_LOCK SpinLock;
    BOOLEAN Stopped;
#if defined(FSP_IOQ_USE_QEVENT)
    FSP_QEVENT PendingIrpEvent;
#else
    KEVENT PendingIrpEvent;
#endif
    LIST_ENTRY PendingIrpList, ProcessIrpList, RetriedIrpList;
    IO_CSQ PendingIoCsq, ProcessIoCsq, RetriedIoCsq;
    ULONG IrpTimeout;
    ULONG PendingIrpCapacity, PendingIrpCount, ProcessIrpCount, RetriedIrpCount;
    VOID (*CompleteCanceledIrp)(PIRP Irp);
    ULONG ProcessIrpBucketCount;
    PVOID ProcessIrpBuckets[];
} FSP_IOQ;
NTSTATUS FspIoqCreate(
    ULONG IrpCapacity, PLARGE_INTEGER IrpTimeout, VOID (*CompleteCanceledIrp)(PIRP Irp),
    FSP_IOQ **PIoq);
VOID FspIoqDelete(FSP_IOQ *Ioq);
VOID FspIoqStop(FSP_IOQ *Ioq, BOOLEAN CancelIrps);
BOOLEAN FspIoqStopped(FSP_IOQ *Ioq);
VOID FspIoqRemoveExpired(FSP_IOQ *Ioq, UINT64 InterruptTime);
BOOLEAN FspIoqPostIrpEx(FSP_IOQ *Ioq, PIRP Irp, BOOLEAN BestEffort, NTSTATUS *PResult);
PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp, PLARGE_INTEGER Timeout,
    PIRP CancellableIrp);
ULONG FspIoqPendingIrpCount(FSP_IOQ *Ioq);
BOOLEAN FspIoqPendingAboveWatermark(FSP_IOQ *Ioq, ULONG Watermark);
BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint);
ULONG FspIoqProcessIrpCount(FSP_IOQ *Ioq);
BOOLEAN FspIoqRetryCompleteIrp(FSP_IOQ *Ioq, PIRP Irp, NTSTATUS *PResult);
PIRP FspIoqNextCompleteIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp);
ULONG FspIoqRetriedIrpCount(FSP_IOQ *Ioq);

/* meta cache */
enum
{
    FspMetaCacheItemHeaderSize = MEMORY_ALLOCATION_ALIGNMENT,
};
typedef struct
{
    KSPIN_LOCK SpinLock;
    UINT64 MetaTimeout;
    ULONG MetaCapacity, ItemCount;
    ULONG ItemSizeMax;
    UINT64 ItemIndex;
    LIST_ENTRY ItemList;
    ULONG ItemBucketCount;
    PVOID ItemBuckets[];
} FSP_META_CACHE;
NTSTATUS FspMetaCacheCreate(
    ULONG MetaCapacity, ULONG ItemSizeMax, PLARGE_INTEGER MetaTimeout,
    FSP_META_CACHE **PMetaCache);
VOID FspMetaCacheDelete(FSP_META_CACHE *MetaCache);
VOID FspMetaCacheInvalidateExpired(FSP_META_CACHE *MetaCache, UINT64 ExpirationTime);
BOOLEAN FspMetaCacheReferenceItemBuffer(FSP_META_CACHE *MetaCache, UINT64 ItemIndex,
    PCVOID *PBuffer, PULONG PSize);
VOID FspMetaCacheDereferenceItemBuffer(PCVOID Buffer);
UINT64 FspMetaCacheAddItem(FSP_META_CACHE *MetaCache, PCVOID Buffer, ULONG Size);
VOID FspMetaCacheInvalidateItem(FSP_META_CACHE *MetaCache, UINT64 ItemIndex);

/* I/O processing */
#define FSP_FSCTL_WORK                  \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'W', METHOD_NEITHER, FILE_ANY_ACCESS)
#define FSP_FSCTL_WORK_BEST_EFFORT      \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'w', METHOD_NEITHER, FILE_ANY_ACCESS)
#define FSP_FSCTL_TRANSACT_REQ_ALIGNMENT 16
enum
{
    FspIopCreateRequestMustSucceedFlag  = 0x01,
    FspIopCreateRequestNonPagedFlag     = 0x02,
    FspIopCreateRequestWorkItemFlag     = 0x04,
};
enum
{
    FspIopRequestExtraContext           = 4,
};
typedef VOID FSP_IOP_REQUEST_FINI(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4]);
typedef NTSTATUS FSP_IOP_REQUEST_WORK(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
typedef struct
{
    FSP_IOP_REQUEST_WORK *WorkRoutine;
    WORK_QUEUE_ITEM WorkQueueItem;
} FSP_FSCTL_TRANSACT_REQ_WORK_ITEM;
typedef struct
{
    FSP_IOP_REQUEST_FINI *RequestFini;
    PVOID Context[4 + 1/*FspIopRequestExtraContext*/];
    FSP_FSCTL_TRANSACT_RSP *Response;
    FSP_FSCTL_TRANSACT_REQ_WORK_ITEM *WorkItem;
    __declspec(align(FSP_FSCTL_TRANSACT_REQ_ALIGNMENT)) UINT8 RequestBuf[];
} FSP_FSCTL_TRANSACT_REQ_HEADER;
FSP_FSCTL_STATIC_ASSERT(sizeof(FSP_FSCTL_TRANSACT_REQ_HEADER) <= 64,
    "sizeof(FSP_FSCTL_TRANSACT_REQ_HEADER) assumed less or equal to 64; "
    "see FSP_FSCTL_TRANSACT_REQ_SIZEMAX");
static inline
PVOID *FspIopRequestContextAddress(FSP_FSCTL_TRANSACT_REQ *Request, ULONG I)
{
    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);
    return &RequestHeader->Context[I];
}
static inline
FSP_FSCTL_TRANSACT_REQ_WORK_ITEM *FspIopRequestWorkItem(FSP_FSCTL_TRANSACT_REQ *Request)
{
    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);
    return RequestHeader->WorkItem;
}
NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    ULONG Flags, FSP_FSCTL_TRANSACT_REQ **PRequest);
NTSTATUS FspIopCreateRequestWorkItem(FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopResetRequest(FSP_FSCTL_TRANSACT_REQ *Request, FSP_IOP_REQUEST_FINI *RequestFini);
NTSTATUS FspIopPostWorkRequestFunnel(PDEVICE_OBJECT DeviceObject,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN BestEffort);
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceDereference);
VOID FspIopCompleteCanceledIrp(PIRP Irp);
BOOLEAN FspIopRetryPrepareIrp(PIRP Irp, NTSTATUS *PResult);
BOOLEAN FspIopRetryCompleteIrp(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response, NTSTATUS *PResult);
VOID FspIopSetIrpResponse(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);
FSP_FSCTL_TRANSACT_RSP *FspIopIrpResponse(PIRP Irp);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
NTSTATUS FspIopDispatchComplete(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response);
static inline
VOID FspIrpDeleteRequest(PIRP Irp)
{
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    if (0 != Request)
    {
        FspIopDeleteRequest(Request);
        FspIrpSetRequest(Irp, 0);
    }
}
#define FspIopCreateRequest(I, F, E, P) \
    FspIopCreateRequestFunnel(I, F, E, 0, 0, P)
#define FspIopCreateRequestMustSucceed(I, F, E, P)\
    FspIopCreateRequestFunnel(I, F, E, 0, FspIopCreateRequestMustSucceedFlag, P)
#define FspIopCreateRequestEx(I, F, E, RF, P)\
    FspIopCreateRequestFunnel(I, F, E, RF, 0, P)
#define FspIopCreateRequestMustSucceedEx(I, F, E, RF, P)\
    FspIopCreateRequestFunnel(I, F, E, RF, FspIopCreateRequestMustSucceedFlag, P)
#define FspIopCreateRequestAndWorkItem(I, E, RF, P)\
    FspIopCreateRequestFunnel(I, 0, E, RF, FspIopCreateRequestWorkItemFlag, P)
#define FspIopRequestContext(Request, I)\
    (*FspIopRequestContextAddress(Request, I))
#define FspIopPostWorkRequest(D, R)     FspIopPostWorkRequestFunnel(D, R, FALSE)
#define FspIopPostWorkRequestBestEffort(D, R)\
    FspIopPostWorkRequestFunnel(D, R, TRUE)
#define FspIopCompleteIrp(I, R)         FspIopCompleteIrpEx(I, R, TRUE)

/* work queue processing */
NTSTATUS FspWqCreateAndPostIrpWorkItem(PIRP Irp,
    FSP_IOP_REQUEST_WORK *WorkRoutine, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN CreateAndPost);
VOID FspWqPostIrpWorkItem(PIRP Irp);
#define FspWqCreateIrpWorkItem(I, RW, RF)\
    FspWqCreateAndPostIrpWorkItem(I, RW, RF, FALSE)
#define FspWqRepostIrpWorkItem(I, RW, RF)\
    FspWqCreateAndPostIrpWorkItem(I, RW, RF, TRUE)

/* file system statistics */
typedef struct
{
    FILESYSTEM_STATISTICS Base;
    FAT_STATISTICS Specific;            /* pretend that we are FAT when it comes to stats */
    /* align to 64 bytes */
    __declspec(align(64)) UINT8 EndOfStruct[];
} FSP_STATISTICS;
NTSTATUS FspStatisticsCreate(FSP_STATISTICS **PStatistics);
VOID FspStatisticsDelete(FSP_STATISTICS *Statistics);
NTSTATUS FspStatisticsCopy(FSP_STATISTICS *Statistics, PVOID Buffer, PULONG PLength);
#define FspStatistics(S)                (&(S)[KeGetCurrentProcessorNumber() % FspProcessorCount])
#define FspStatisticsInc(S,F)           ((S)->F++)
#define FspStatisticsAdd(S,F,V)         ((S)->F += (V))

/* device management */
enum
{
    FspFsvolDeviceSecurityCacheCapacity = 100,
    FspFsvolDeviceSecurityCacheItemSizeMax = 4096,
    FspFsvolDeviceDirInfoCacheCapacity = 100,
    FspFsvolDeviceDirInfoCacheItemSizeMax = FSP_FSCTL_ALIGN_UP(16384, PAGE_SIZE),
    FspFsvolDeviceStreamInfoCacheCapacity = 100,
    FspFsvolDeviceStreamInfoCacheItemSizeMax = FSP_FSCTL_ALIGN_UP(16384, PAGE_SIZE),
    FspFsvolDeviceEaCacheCapacity = 100,
    FspFsvolDeviceEaCacheItemSizeMax = FSP_FSCTL_ALIGN_UP(16384, PAGE_SIZE),
};
typedef struct
{
    PUNICODE_STRING FileName;
    PVOID Context;
} FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA;
typedef struct
{
    RTL_BALANCED_LINKS Header;
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA Data;
} FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT;
typedef struct
{
    PVOID RestartKey;
    ULONG DeleteCount;
} FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY;
enum
{
    FspFsctlDeviceExtensionKind = '\0ltC',  /* file system control device (e.g. \Device\WinFsp.Disk) */
    FspFsmupDeviceExtensionKind = '\0puM',  /* our own MUP device (linked to \Device\WinFsp.Mup) */
    FspFsvrtDeviceExtensionKind = '\0trV',  /* virtual volume device (e.g. \Device\Volume{GUID}) */
    FspFsvolDeviceExtensionKind = '\0loV',  /* file system volume device (unnamed) */
};
typedef struct
{
    KSPIN_LOCK SpinLock;
    LONG RefCount;
    UINT32 Kind;
    GUID SiloContainerId;
} FSP_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    UINT32 InitDoneFsvrt:1, InitDoneIoq:1, InitDoneSec:1, InitDoneDir:1, InitDoneStrm:1, InitDoneEa:1,
        InitDoneCtxTab:1, InitDoneTimer:1, InitDoneInfo:1, InitDoneNotify:1, InitDoneStat:1,
        InitDoneFsext;
    PDEVICE_OBJECT FsctlDeviceObject;
    PDEVICE_OBJECT FsvrtDeviceObject;
    PDEVICE_OBJECT FsvolDeviceObject;
    PVPB SwapVpb;
    FSP_DELAYED_WORK_ITEM DeleteVolumeDelayedWorkItem;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    FSP_FSEXT_PROVIDER *Provider;
    UNICODE_STRING VolumePrefix;
    UNICODE_PREFIX_TABLE_ENTRY VolumePrefixEntry;
#if defined(FSP_CFG_REJECT_EARLY_IRP)
    LONG ReadyToAcceptIrp;
#endif
    FSP_IOQ *Ioq;
    FSP_META_CACHE *SecurityCache;
    FSP_META_CACHE *DirInfoCache;
    FSP_META_CACHE *StreamInfoCache;
    FSP_META_CACHE *EaCache;
    KSPIN_LOCK ExpirationLock;
    WORK_QUEUE_ITEM ExpirationWorkItem;
    BOOLEAN ExpirationInProgress;
    ERESOURCE FileRenameResource;
    ERESOURCE ContextTableResource;
    LIST_ENTRY ContextList;
    RTL_AVL_TABLE ContextByNameTable;
    PVOID ContextByNameTableElementStorage;
    UNICODE_STRING VolumeName;
    WCHAR VolumeNameBuf[FSP_FSCTL_VOLUME_NAME_SIZE / sizeof(WCHAR)];
    KSPIN_LOCK InfoSpinLock;
    UINT64 InfoExpirationTime;
    FSP_FSCTL_VOLUME_INFO VolumeInfo;
    LONG VolumeNotifyLock;
    FSP_WGROUP VolumeNotifyWgroup;
    PNOTIFY_SYNC NotifySync;
    LIST_ENTRY NotifyList;
    FSP_STATISTICS *Statistics;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 FsextData[];
} FSP_FSVOL_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    UINT16 SectorSize;
    LONG IsMountdev;
    BOOLEAN Persistent;
    GUID UniqueId;
    UNICODE_STRING VolumeName;
    WCHAR VolumeNameBuf[FSP_FSCTL_VOLUME_NAME_SIZE / sizeof(WCHAR)];
} FSP_FSVRT_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    UINT32 InitDonePfxTab:1;
    ERESOURCE PrefixTableResource;
    UNICODE_PREFIX_TABLE PrefixTable;
    UNICODE_PREFIX_TABLE ClassTable;
} FSP_FSMUP_DEVICE_EXTENSION;
static inline
FSP_DEVICE_EXTENSION *FspDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSVOL_DEVICE_EXTENSION *FspFsvolDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    ASSERT(FspFsvolDeviceExtensionKind == ((FSP_DEVICE_EXTENSION *)DeviceObject->DeviceExtension)->Kind);
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSVRT_DEVICE_EXTENSION *FspFsvrtDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    ASSERT(FspFsvrtDeviceExtensionKind == ((FSP_DEVICE_EXTENSION *)DeviceObject->DeviceExtension)->Kind);
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSMUP_DEVICE_EXTENSION *FspFsmupDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    ASSERT(FspFsmupDeviceExtensionKind == ((FSP_DEVICE_EXTENSION *)DeviceObject->DeviceExtension)->Kind);
    return DeviceObject->DeviceExtension;
}
NTSTATUS FspDeviceCreateSecure(UINT32 Kind, ULONG ExtraSize,
    PUNICODE_STRING DeviceName, DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics,
    PUNICODE_STRING DeviceSddl, LPCGUID DeviceClassGuid,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceCreate(UINT32 Kind, ULONG ExtraSize,
    DEVICE_TYPE DeviceType, ULONG DeviceCharacteristics,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceInitialize(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspDeviceReference(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDereference(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameAcquireShared(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspFsvolDeviceFileRenameTryAcquireShared(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameAcquireExclusive(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspFsvolDeviceFileRenameTryAcquireExclusive(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameSetOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
VOID FspFsvolDeviceFileRenameRelease(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameReleaseOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
BOOLEAN FspFsvolDeviceFileRenameIsAcquiredExclusive(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceLockContextTable(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceUnlockContextTable(PDEVICE_OBJECT DeviceObject);
NTSTATUS FspFsvolDeviceCopyContextList(PDEVICE_OBJECT DeviceObject,
    PVOID **PContexts, PULONG PContextCount);
NTSTATUS FspFsvolDeviceCopyContextByNameList(PDEVICE_OBJECT DeviceObject,
    PVOID **PContexts, PULONG PContextCount);
VOID FspFsvolDeviceDeleteContextList(PVOID *Contexts, ULONG ContextCount);
PVOID FspFsvolDeviceEnumerateContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    BOOLEAN NextFlag, FSP_DEVICE_CONTEXT_BY_NAME_TABLE_RESTART_KEY *RestartKey);
PVOID FspFsvolDeviceLookupContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName);
PVOID FspFsvolDeviceInsertContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName, PVOID Context,
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted);
VOID FspFsvolDeviceDeleteContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    PBOOLEAN PDeleted);
VOID FspFsvolDeviceGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
BOOLEAN FspFsvolDeviceTryGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
VOID FspFsvolDeviceSetVolumeInfo(PDEVICE_OBJECT DeviceObject, const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
VOID FspFsvolDeviceInvalidateVolumeInfo(PDEVICE_OBJECT DeviceObject);
#if defined(FSP_CFG_REJECT_EARLY_IRP)
static inline
BOOLEAN FspFsvolDeviceReadyToAcceptIrp(PDEVICE_OBJECT DeviceObject)
{
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    if (!FsvolDeviceExtension->VolumeParams.RejectIrpPriorToTransact0)
        return TRUE;
    return 0 != InterlockedCompareExchange(&FsvolDeviceExtension->ReadyToAcceptIrp, 0, 0);
}
static inline
VOID FspFsvolDeviceSetReadyToAcceptIrp(PDEVICE_OBJECT DeviceObject)
{
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    if (!FsvolDeviceExtension->VolumeParams.RejectIrpPriorToTransact0)
        return;
    InterlockedExchange(&FsvolDeviceExtension->ReadyToAcceptIrp, 1);
}
#endif
static inline
BOOLEAN FspFsvolDeviceVolumePrefixInString(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING String)
{
    return RtlPrefixUnicodeString(&FspFsvolDeviceExtension(DeviceObject)->VolumePrefix, String,
        TRUE);
}
NTSTATUS FspDeviceCopyList(
    PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount);
VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount);
VOID FspDeviceDeleteAll(VOID);
static inline
VOID FspDeviceGlobalLock(VOID)
{
    extern ERESOURCE FspDeviceGlobalResource;
    ExAcquireResourceExclusiveLite(&FspDeviceGlobalResource, TRUE);
}
static inline
VOID FspDeviceGlobalUnlock(VOID)
{
    extern ERESOURCE FspDeviceGlobalResource;
    ExReleaseResourceLite(&FspDeviceGlobalResource);
}
#define FspFsvolDeviceStatistics(DeviceObject)\
    FspStatistics(FspFsvolDeviceExtension(DeviceObject)->Statistics)
#define FspFsvolDeviceStoppedStatus(DeviceObject)\
    STATUS_VOLUME_DISMOUNTED
    //(FILE_DEVICE_DISK_FILE_SYSTEM == (DeviceObject)->DeviceType ?\
    //    STATUS_VOLUME_DISMOUNTED : STATUS_DEVICE_NOT_CONNECTED)

/* fsext */
FSP_FSEXT_PROVIDER *FspFsextProvider(UINT32 FsextControlCode, PNTSTATUS PLoadResult);

/* process buffers conditional usage */
static inline
BOOLEAN FspReadIrpShouldUseProcessBuffer(PIRP Irp, SIZE_T BufferSize)
{
    ASSERT(0 != Irp);
#if DBG
    return DEBUGTEST(50) ||
#else
    return FspProcessBufferSizeMax >= BufferSize ||
#endif
        FspFsvolDeviceExtension(IoGetCurrentIrpStackLocation(Irp)->DeviceObject)->
            VolumeParams.AlwaysUseDoubleBuffering;
}
static inline
BOOLEAN FspWriteIrpShouldUseProcessBuffer(PIRP Irp, SIZE_T BufferSize)
{
    ASSERT(0 != Irp);
#if DBG
    return DEBUGTEST(50);
#else
    return FspProcessBufferSizeMax >= BufferSize;
#endif
}
#if 0
static inline
BOOLEAN FspQueryDirectoryIrpShouldUseProcessBuffer(PIRP Irp, SIZE_T BufferSize)
{
    return FspReadIrpShouldUseProcessBuffer(Irp, BufferSize);
}
#endif

/* mountdev */
NTSTATUS FspMountdevQueryDeviceName(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspMountdevQueryUniqueId(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
BOOLEAN FspMountdevDeviceControl(
    PDEVICE_OBJECT FsvrtDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    PNTSTATUS PResult);
NTSTATUS FspMountdevMake(
    PDEVICE_OBJECT FsvrtDeviceObject, PDEVICE_OBJECT FsvolDeviceObject,
    BOOLEAN Persistent);
VOID FspMountdevFini(
    PDEVICE_OBJECT FsvrtDeviceObject);

/* fsmup */
NTSTATUS FspMupRegister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject);
VOID FspMupUnregister(
    PDEVICE_OBJECT FsmupDeviceObject, PDEVICE_OBJECT FsvolDeviceObject);
NTSTATUS FspMupHandleIrp(
    PDEVICE_OBJECT FsmupDeviceObject, PIRP Irp);

/* volume management */
NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeMount(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeMakeMountdev(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetName(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetNameList(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeTransactFsext(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeStop(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeNotify(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeWork(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);

/* file objects */
#define FspFileNodeKind(FileNode)       \
    (((FSP_FILE_NODE *)FileNode)->Header.NodeTypeCode)
#define FspFileNodeIsValid(FileNode)    \
    (0 != (FileNode) && FspFileNodeFileKind == ((FSP_FILE_NODE *)FileNode)->Header.NodeTypeCode)
enum
{
    FspFileNodeFileKind                 = 'BZ',
};
enum
{
    FspFileNodeSharingViolationGeneral  = 'G',
    FspFileNodeSharingViolationMainFile = 'M',
    FspFileNodeSharingViolationStream   = 'S',
};
enum
{
    FspFileNodeAcquireMain              = 1,
    FspFileNodeAcquirePgio              = 2,
    FspFileNodeAcquireFull              = 3,
};
typedef struct
{
    ERESOURCE Resource;
    ERESOURCE PagingIoResource;
    FAST_MUTEX HeaderFastMutex;
    SECTION_OBJECT_POINTERS SectionObjectPointers;
    KSPIN_LOCK NpInfoSpinLock;          /* allows to invalidate non-page Info w/o resources acquired */
    UINT64 Security;
    UINT64 DirInfo;
    UINT64 StreamInfo;
    UINT64 Ea;
} FSP_FILE_NODE_NONPAGED;
typedef struct FSP_FILE_NODE
{
    FSRTL_ADVANCED_FCB_HEADER Header;
    FSP_FILE_NODE_NONPAGED *NonPaged;
    /* interlocked access */
    LONG RefCount;
    UINT32 DeletePending;
    /* locked under FSP_FSVOL_DEVICE_EXTENSION::ContextTableResource */
    LONG ActiveCount;                   /* CREATE w/o CLOSE count */
    LONG OpenCount;                     /* ContextTable ref count */
    LONG HandleCount;                   /* HANDLE count (CREATE/CLEANUP) */
    SHARE_ACCESS ShareAccess;
    ULONG MainFileDenyDeleteCount;      /* number of times main file is denying delete */
    ULONG StreamDenyDeleteCount;        /* number of times open streams are denying delete */
    LIST_ENTRY ActiveEntry;
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT ContextByNameElementStorage;
    /* locked under FSP_FSVOL_DEVICE_EXTENSION::FileRenameResource or Header.Resource */
    UNICODE_STRING FileName;
    PWSTR ExternalFileName;
    /* locked under Header.Resource */
    UINT64 FileInfoExpirationTime, BasicInfoExpirationTime;
    UINT32 FileAttributes;
    UINT32 ReparseTag;
    UINT64 CreationTime;
    UINT64 LastAccessTime;
    UINT64 LastWriteTime;
    UINT64 ChangeTime;
    UINT32 EaSize;
    ULONG FileInfoChangeNumber;
    ULONG SecurityChangeNumber;
    ULONG DirInfoChangeNumber;
    ULONG StreamInfoChangeNumber;
    ULONG EaChangeNumber;
    ULONG EaChangeCount;
    BOOLEAN TruncateOnClose;
    BOOLEAN PosixDelete;
    FILE_LOCK FileLock;
#if (NTDDI_VERSION < NTDDI_WIN8)
    OPLOCK Oplock;
#endif
    struct
    {
        PVOID LazyWriteThread;
        union
        {
            PIRP TopLevelIrp;
            UINT32 TopFlags;
        } CcFlush;
        BOOLEAN CreateSection;
    } Tls;
    /* read-only after creation (and insertion in the ContextTable) */
    PDEVICE_OBJECT FsvolDeviceObject;
    UINT64 UserContext;
    UINT64 IndexNumber;
    BOOLEAN IsDirectory;
    BOOLEAN IsRootDirectory;
    struct FSP_FILE_NODE *MainFileNode;
    WCHAR FileNameBuf[];
} FSP_FILE_NODE;
typedef struct
{
    FSP_FILE_NODE *FileNode;
    UINT64 UserContext2;
    UINT32 GrantedAccess;
    UINT32
        CaseSensitive:1, HasTraversePrivilege:1, DeleteOnClose:1, PosixDelete:1,
        DidSetMetadata:1,
        DidSetFileAttributes:1, DidSetReparsePoint:1, DidSetSecurity:1,
        DidSetCreationTime:1, DidSetLastAccessTime:1, DidSetLastWriteTime:1, DidSetChangeTime:1,
        DirectoryHasSuchFile:1;
    NTSTATUS DispositionStatus;
    UNICODE_STRING DirectoryPattern;
    UNICODE_STRING DirectoryMarker;
    UINT64 DirInfo;
    ULONG DirInfoCacheHint;
    ULONG EaIndex;
    ULONG EaChangeCount;
    /* stream support */
    HANDLE MainFileHandle;
    PFILE_OBJECT MainFileObject;
} FSP_FILE_DESC;
NTSTATUS FspFileNodeCopyActiveList(PDEVICE_OBJECT DeviceObject,
    FSP_FILE_NODE ***PFileNodes, PULONG PFileNodeCount);
NTSTATUS FspFileNodeCopyOpenList(PDEVICE_OBJECT DeviceObject,
    FSP_FILE_NODE ***PFileNodes, PULONG PFileNodeCount);
VOID FspFileNodeDeleteList(FSP_FILE_NODE **FileNodes, ULONG FileNodeCount);
NTSTATUS FspFileNodeCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_NODE **PFileNode);
VOID FspFileNodeDelete(FSP_FILE_NODE *FileNode);
static inline
VOID FspFileNodeReference(FSP_FILE_NODE *FileNode)
{
    InterlockedIncrement(&FileNode->RefCount);
}
static inline
VOID FspFileNodeDereference(FSP_FILE_NODE *FileNode)
{
    LONG RefCount = InterlockedDecrement(&FileNode->RefCount);
    if (0 == RefCount)
        FspFileNodeDelete(FileNode);
}
VOID FspFileNodeAcquireSharedF(FSP_FILE_NODE *FileNode, ULONG Flags);
BOOLEAN FspFileNodeTryAcquireSharedF(FSP_FILE_NODE *FileNode, ULONG Flags, BOOLEAN Wait);
VOID FspFileNodeAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags);
BOOLEAN FspFileNodeTryAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags, BOOLEAN Wait);
VOID FspFileNodeConvertExclusiveToSharedF(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeSetOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner);
VOID FspFileNodeReleaseF(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeReleaseOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner);
static inline
VOID FspFileNodeAcquireSharedForeign(FSP_FILE_NODE *FileNode)
{
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
    ExAcquireResourceSharedLite(FileNode->Header.Resource, TRUE);
}
static inline
VOID FspFileNodeAcquireExclusiveForeign(FSP_FILE_NODE *FileNode)
{
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
    ExAcquireResourceExclusiveLite(FileNode->Header.Resource, TRUE);
}
static inline
VOID FspFileNodeReleaseForeign(FSP_FILE_NODE *FileNode)
{
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
    ExReleaseResourceLite(FileNode->Header.Resource);
}
NTSTATUS FspFileNodeOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    UINT32 GrantedAccess, UINT32 AdditionalGrantedAccess, UINT32 ShareAccess,
    FSP_FILE_NODE **POpenedFileNode, PULONG PSharingViolationReason);
VOID FspFileNodeCleanup(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject, PULONG PCleanupFlags);
VOID FspFileNodeCleanupFlush(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject);
VOID FspFileNodeCleanupComplete(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject, BOOLEAN Delete);
VOID FspFileNodeClose(FSP_FILE_NODE *FileNode,
    PFILE_OBJECT FileObject,    /* non-0 to remove share access */
    BOOLEAN HandleCleanup);     /* TRUE to decrement handle count */
NTSTATUS FspFileNodeFlushAndPurgeCache(FSP_FILE_NODE *FileNode,
    UINT64 FlushOffset64, ULONG FlushLength, BOOLEAN FlushAndPurge);
VOID FspFileNodeOverwriteStreams(FSP_FILE_NODE *FileNode);
NTSTATUS FspFileNodeCheckBatchOplocksOnAllStreams(
    PDEVICE_OBJECT FsvolDeviceObject,
    PIRP OplockIrp,
    FSP_FILE_NODE *FileNode,
    ULONG AcquireFlags,
    PUNICODE_STRING StreamFileName);
NTSTATUS FspFileNodeRenameCheck(PDEVICE_OBJECT FsvolDeviceObject, PIRP OplockIrp,
    FSP_FILE_NODE *FileNode, ULONG AcquireFlags,
    PUNICODE_STRING FileName, BOOLEAN CheckingOldName,
    BOOLEAN PosixRename);
VOID FspFileNodeRename(FSP_FILE_NODE *FileNode, PUNICODE_STRING NewFileName);
VOID FspFileNodeGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTryGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTryGetFileInfoByName(PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp,
    PUNICODE_STRING FileName, FSP_FSCTL_FILE_INFO *FileInfo);
VOID FspFileNodeSetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, BOOLEAN TruncateOnClose);
BOOLEAN FspFileNodeTrySetFileInfoAndSecurityOnOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo,
    const PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG SecurityDescriptorSize,
    BOOLEAN TruncateOnClose);
BOOLEAN FspFileNodeTrySetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, ULONG InfoChangeNumber);
VOID FspFileNodeInvalidateFileInfo(FSP_FILE_NODE *FileNode);
static inline
ULONG FspFileNodeFileInfoChangeNumber(FSP_FILE_NODE *FileNode)
{
    if (0 != FileNode->MainFileNode)
        return (FileNode->MainFileNode->FileInfoChangeNumber & 0xfffff) |
            ((FileNode->FileInfoChangeNumber & 0xfff) << 20);
    else
        return FileNode->FileInfoChangeNumber & 0xfffff;
}
BOOLEAN FspFileNodeReferenceSecurity(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetSecurity(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG SecurityChangeNumber);
VOID FspFileNodeInvalidateSecurity(FSP_FILE_NODE *FileNode);
static inline
ULONG FspFileNodeSecurityChangeNumber(FSP_FILE_NODE *FileNode)
{
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
    return FileNode->SecurityChangeNumber;
}
BOOLEAN FspFileNodeReferenceDirInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetDirInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG DirInfoChangeNumber);
static inline
ULONG FspFileNodeDirInfoChangeNumber(FSP_FILE_NODE *FileNode)
{
    return FileNode->DirInfoChangeNumber;
}
VOID FspFileNodeInvalidateParentDirInfo(FSP_FILE_NODE *FileNode);
BOOLEAN FspFileNodeReferenceStreamInfo(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetStreamInfo(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG StreamInfoChangeNumber);
static inline
ULONG FspFileNodeStreamInfoChangeNumber(FSP_FILE_NODE *FileNode)
{
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
    return FileNode->StreamInfoChangeNumber;
}
VOID FspFileNodeInvalidateStreamInfo(FSP_FILE_NODE *FileNode);
BOOLEAN FspFileNodeReferenceEa(FSP_FILE_NODE *FileNode, PCVOID *PBuffer, PULONG PSize);
VOID FspFileNodeSetEa(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size);
BOOLEAN FspFileNodeTrySetEa(FSP_FILE_NODE *FileNode, PCVOID Buffer, ULONG Size,
    ULONG EaChangeNumber);
VOID FspFileNodeInvalidateEa(FSP_FILE_NODE *FileNode);
static inline
ULONG FspFileNodeEaChangeNumber(FSP_FILE_NODE *FileNode)
{
    if (0 != FileNode->MainFileNode)
        FileNode = FileNode->MainFileNode;
    return FileNode->EaChangeNumber;
}
VOID FspFileNodeNotifyChange(FSP_FILE_NODE *FileNode, ULONG Filter, ULONG Action,
    BOOLEAN InvalidateCaches);
VOID FspFileNodeInvalidateCachesAndNotifyChangeByName(PDEVICE_OBJECT FsvolDeviceObject,
    PUNICODE_STRING FileName, ULONG Filter, ULONG Action,
    BOOLEAN InvalidateParentCaches);
NTSTATUS FspFileNodeProcessLockIrp(FSP_FILE_NODE *FileNode, PIRP Irp);
NTSTATUS FspFileDescCreate(FSP_FILE_DESC **PFileDesc);
VOID FspFileDescDelete(FSP_FILE_DESC *FileDesc);
NTSTATUS FspFileDescResetDirectory(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName, BOOLEAN RestartScan, BOOLEAN IndexSpecified);
NTSTATUS FspFileDescSetDirectoryMarker(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName);
NTSTATUS FspMainFileOpen(
    PDEVICE_OBJECT FsvolDeviceObject,
    PDEVICE_OBJECT DeviceObjectHint,
    PUNICODE_STRING MainFileName, BOOLEAN CaseSensitive,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    ULONG FileAttributes,
    ULONG Disposition,
    PHANDLE PMainFileHandle,
    PFILE_OBJECT *PMainFileObject);
NTSTATUS FspMainFileClose(
    HANDLE MainFileHandle,
    PFILE_OBJECT MainFileObject);
#define FspFileNodeAcquireShared(N,F)   FspFileNodeAcquireSharedF(N, FspFileNodeAcquire ## F)
#define FspFileNodeTryAcquireShared(N,F)    FspFileNodeTryAcquireSharedF(N, FspFileNodeAcquire ## F, FALSE)
#define FspFileNodeAcquireExclusive(N,F)    FspFileNodeAcquireExclusiveF(N, FspFileNodeAcquire ## F)
#define FspFileNodeTryAcquireExclusive(N,F) FspFileNodeTryAcquireExclusiveF(N, FspFileNodeAcquire ## F, FALSE)
#define FspFileNodeConvertExclusiveToShared(N,F)    FspFileNodeConvertExclusiveToSharedF(N, FspFileNodeAcquire ## F)
#define FspFileNodeSetOwner(N,F,P)      FspFileNodeSetOwnerF(N, FspFileNodeAcquire ## F, P)
#define FspFileNodeRelease(N,F)         FspFileNodeReleaseF(N, FspFileNodeAcquire ## F)
#define FspFileNodeReleaseOwner(N,F,P)  FspFileNodeReleaseOwnerF(N, FspFileNodeAcquire ## F, P)
#define FspFileNodeDereferenceSecurity(P)   FspMetaCacheDereferenceItemBuffer(P)
#define FspFileNodeDereferenceDirInfo(P)    FspMetaCacheDereferenceItemBuffer(P)
#define FspFileNodeDereferenceStreamInfo(P) FspMetaCacheDereferenceItemBuffer(P)
#define FspFileNodeDereferenceEa(P)         FspMetaCacheDereferenceItemBuffer(P)
#define FspFileNodeUnlockAll(N,F,P)     FsRtlFastUnlockAll(&(N)->FileLock, F, P, N)
#if (NTDDI_VERSION < NTDDI_WIN8)
#define FspFileNodeAddrOfOplock(N)      (&(N)->Oplock)
#else
#define FspFileNodeAddrOfOplock(N)      (&(N)->Header.Oplock)
#endif

/* oplock support */
typedef struct
{
    FSP_FILE_NODE *FileNode;
    ULONG AcquireFlags;
    PVOID PrepareContext;
} FSP_FILE_NODE_OPLOCK_CONTEXT;
static inline
NTSTATUS FspFileNodeOplockFsctl(FSP_FILE_NODE *FileNode, PIRP Irp, ULONG OpenCount)
{
    return FspOplockFsctrl(FspFileNodeAddrOfOplock(FileNode), Irp, OpenCount);
}
static inline
BOOLEAN FspFileNodeOplockIsBatch(FSP_FILE_NODE *FileNode)
{
    return FsRtlCurrentBatchOplock(FspFileNodeAddrOfOplock(FileNode));
}
static inline
BOOLEAN FspFileNodeOplockIsHandle(FSP_FILE_NODE *FileNode)
{
    return FsRtlCurrentOplockH(FspFileNodeAddrOfOplock(FileNode));
}
static inline
NTSTATUS FspFileNodeOplockCheck(FSP_FILE_NODE *FileNode, PIRP Irp)
{
    return FspCheckOplock(FspFileNodeAddrOfOplock(FileNode), Irp, 0, 0, 0);
}
static inline
NTSTATUS FspFileNodeOplockCheckEx(FSP_FILE_NODE *FileNode, PIRP Irp, ULONG Flags)
{
    return FspCheckOplockEx(FspFileNodeAddrOfOplock(FileNode), Irp, Flags, 0, 0, 0);
}
static inline
NTSTATUS FspFileNodeOplockBreakHandle(FSP_FILE_NODE *FileNode, PIRP Irp, ULONG Flags)
{
    return FspOplockBreakH(FspFileNodeAddrOfOplock(FileNode), Irp, Flags, 0, 0, 0);
}
static inline
NTSTATUS FspFileNodeOplockCheckAsyncEx(
    FSP_FILE_NODE *FileNode, ULONG AcquireFlags, PVOID PrepareContext,
    PIRP Irp,
    POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine,
    POPLOCK_FS_PREPOST_IRP PostIrpRoutine)
{
    FSP_FILE_NODE_OPLOCK_CONTEXT OplockContext;
    NTSTATUS Result;
    OplockContext.FileNode = FileNode;
    OplockContext.AcquireFlags = AcquireFlags;
    OplockContext.PrepareContext = PrepareContext;
    Result = FspCheckOplock(
        FspFileNodeAddrOfOplock(FileNode),
        Irp,
        &OplockContext,
        CompletionRoutine,
        PostIrpRoutine);
#if DBG
    if (DEBUGTEST_EX(STATUS_SUCCESS == Result, 10, FALSE))
    {
        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0;
        PostIrpRoutine(&OplockContext, Irp);
        CompletionRoutine(&OplockContext, Irp);
        Result = STATUS_PENDING;
    }
#endif
    return Result;
}
static inline
PVOID FspFileNodeReleaseForOplock(FSP_FILE_NODE_OPLOCK_CONTEXT *OplockContext)
{
    FspFileNodeReleaseF(OplockContext->FileNode, OplockContext->AcquireFlags);
    return OplockContext->PrepareContext;
}
VOID FspFileNodeOplockPrepare(PVOID Context, PIRP Irp);
VOID FspFileNodeOplockComplete(PVOID Context, PIRP Irp);
#define FspFileNodeOplockCheckAsync(FileNode, AcquireFlags, PrepareContext, Irp)\
    FspFileNodeOplockCheckAsyncEx(FileNode, AcquireFlags, (PVOID)(UINT_PTR)PrepareContext, Irp,\
        FspFileNodeOplockComplete,FspFileNodeOplockPrepare)

/* multiversion support */
typedef
NTKERNELAPI
VOID
FSP_MV_CcCoherencyFlushAndPurgeCache(
    _In_ PSECTION_OBJECT_POINTERS SectionObjectPointer,
    _In_opt_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_opt_ ULONG Flags
    );

/* extern */
extern PDRIVER_OBJECT FspDriverObject;
extern FAST_IO_DISPATCH FspFastIoDispatch;
extern CACHE_MANAGER_CALLBACKS FspCacheManagerCallbacks;
extern FSP_IOPREP_DISPATCH *FspIopPrepareFunction[];
extern FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[];
extern ERESOURCE FspDeviceGlobalResource;
extern WCHAR FspFileDescDirectoryPatternMatchAll[];
extern const GUID FspMainFileOpenEcpGuid;
extern ULONG FspProcessorCount;
extern FSP_MV_CcCoherencyFlushAndPurgeCache *FspMvCcCoherencyFlushAndPurgeCache;
extern ULONG FspMvMdlMappingNoWrite;
extern BOOLEAN FspHasReparsePointCaseSensitivityFix;

/*
 * Fixes
 */

/* RtlEqualMemory: this is defined as memcmp, which does not exist on Win7 x86! */
#undef RtlEqualMemory
static inline
LOGICAL RtlEqualMemory(const VOID *Source1, const VOID *Source2, SIZE_T Length)
{
    return Length == RtlCompareMemory(Source1, Source2, Length);
}

/* FILE_STAT_INFORMATION and FILE_STAT_LX_INFORMATION are missings on some WDK's. */
typedef struct
{
    LARGE_INTEGER FileId;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
    ULONG ReparseTag;
    ULONG NumberOfLinks;
    ACCESS_MASK EffectiveAccess;
} FSP_FILE_STAT_INFORMATION, *PFSP_FILE_STAT_INFORMATION;
typedef struct
{
    LARGE_INTEGER FileId;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG FileAttributes;
    ULONG ReparseTag;
    ULONG NumberOfLinks;
    ACCESS_MASK EffectiveAccess;
    ULONG LxFlags;
    ULONG LxUid;
    ULONG LxGid;
    ULONG LxMode;
    ULONG LxDeviceIdMajor;
    ULONG LxDeviceIdMinor;
} FSP_FILE_STAT_LX_INFORMATION, *PFSP_FILE_STAT_LX_INFORMATION;

/* ATOMIC_CREATE_ECP_CONTEXT is missing on some WDK's */
#define ATOMIC_CREATE_ECP_IN_FLAG_REPARSE_POINT_SPECIFIED   0x0002
#define ATOMIC_CREATE_ECP_OUT_FLAG_REPARSE_POINT_SET        0x0002
#define ATOMIC_CREATE_ECP_IN_FLAG_BEST_EFFORT               0x0100
typedef struct
{
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
} FSP_FILE_TIMESTAMPS, *PFSP_FILE_TIMESTAMPS;
typedef struct
{
    USHORT Size;
    USHORT InFlags;
    USHORT OutFlags;
    USHORT ReparseBufferLength;
    PREPARSE_DATA_BUFFER ReparseBuffer;
    LONGLONG FileSize;
    LONGLONG ValidDataLength;
    PFSP_FILE_TIMESTAMPS FileTimestamps;
    ULONG FileAttributes;
    ULONG UsnSourceInfo;
    USN Usn;
    ULONG SuppressFileAttributeInheritanceMask;
    ULONG InOpFlags;
    ULONG OutOpFlags;
    ULONG InGenFlags;
    ULONG OutGenFlags;
    ULONG CaseSensitiveFlagsMask;
    ULONG InCaseSensitiveFlags;
    ULONG OutCaseSensitiveFlags;
} FSP_ATOMIC_CREATE_ECP_CONTEXT, *PFSP_ATOMIC_CREATE_ECP_CONTEXT;

#endif
