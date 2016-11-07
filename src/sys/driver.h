/**
 * @file sys/driver.h
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#define WINFSP_SYS_INTERNAL
#include <ntifs.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include <winfsp/fsctl.h>

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */

#define DRIVER_NAME                     FSP_FSCTL_DRIVER_NAME

/* IoCreateDeviceSecure default SDDL's */
#define FSP_FSCTL_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */
#define FSP_FSVRT_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGX;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ|GENERIC_EXECUTE */

/* private NTSTATUS codes */
#define FSP_STATUS_PRIVATE_BIT          (0x20000000)
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
#else
#define DEBUGTEST(Percent)              (TRUE)
#endif

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
        if (STATUS_PENDING != Result)   \
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
FSP_IOPREP_DISPATCH FspFsvolSetSecurityPrepare;
FSP_IOCMPL_DISPATCH FspFsvolSetSecurityComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetVolumeInformationComplete;
FSP_IOCMPL_DISPATCH FspFsvolShutdownComplete;
FSP_IOPREP_DISPATCH FspFsvolWritePrepare;
FSP_IOCMPL_DISPATCH FspFsvolWriteComplete;

/* fast I/O and resource acquisition callbacks */
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
BOOLEAN FspFileNameIsValid(PUNICODE_STRING Path, PUNICODE_STRING StreamPart, PULONG StreamType);
BOOLEAN FspFileNameIsValidPattern(PUNICODE_STRING Pattern);
VOID FspFileNameSuffix(PUNICODE_STRING Path, PUNICODE_STRING Remain, PUNICODE_STRING Suffix);
#if 0
NTSTATUS FspFileNameUpcase(
    PUNICODE_STRING DestinationName,
    PUNICODE_STRING SourceName,
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
#define FspFileNameCompare(N1,N2,I,U)   (ASSERT(0 == (U)), RtlCompareUnicodeString(N1,N2,I))
#define FspFileNameIsPrefix(N1,N2,I,U)  (ASSERT(0 == (U)), RtlPrefixUnicodeString(N1,N2,I))
#endif
NTSTATUS FspFileNameInExpression(
    PUNICODE_STRING Expression,
    PUNICODE_STRING Name,
    BOOLEAN IgnoreCase,
    PWCH UpcaseTable,
    PBOOLEAN PResult);

/* utility */
PVOID FspAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag);
PVOID FspAllocateIrpMustSucceed(CCHAR StackSize);
NTSTATUS FspCreateGuid(GUID *Guid);
NTSTATUS FspGetDeviceObjectPointer(PUNICODE_STRING ObjectName, ACCESS_MASK DesiredAccess,
    PULONG PFileNameIndex, PFILE_OBJECT *PFileObject, PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspSendSetInformationIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FILE_INFORMATION_CLASS FileInformationClass, PVOID FileInformation, ULONG Length);
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

/* I/O queue */
#define FspIoqTimeout                   ((PIRP)1)
#define FspIoqCancelled                 ((PIRP)2)
#define FspIoqPostIrp(Q, I, R)          FspIoqPostIrpEx(Q, I, FALSE, R)
#define FspIoqPostIrpBestEffort(Q, I, R)FspIoqPostIrpEx(Q, I, TRUE, R)
typedef struct
{
    KSPIN_LOCK SpinLock;
    BOOLEAN Stopped;
    KEVENT PendingIrpEvent;
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
VOID FspIoqStop(FSP_IOQ *Ioq);
BOOLEAN FspIoqStopped(FSP_IOQ *Ioq);
VOID FspIoqRemoveExpired(FSP_IOQ *Ioq, UINT64 InterruptTime);
BOOLEAN FspIoqPostIrpEx(FSP_IOQ *Ioq, PIRP Irp, BOOLEAN BestEffort, NTSTATUS *PResult);
PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp, PLARGE_INTEGER Timeout,
    PIRP CancellableIrp);
ULONG FspIoqPendingIrpCount(FSP_IOQ *Ioq);
BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint);
ULONG FspIoqProcessIrpCount(FSP_IOQ *Ioq);
BOOLEAN FspIoqRetryCompleteIrp(FSP_IOQ *Ioq, PIRP Irp, NTSTATUS *PResult);
PIRP FspIoqNextCompleteIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp);
ULONG FspIoqRetriedIrpCount(FSP_IOQ *Ioq);

/* meta cache */
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
enum
{
    FspIopRequestMustSucceed            = 0x01,
    FspIopRequestNonPaged               = 0x02,
};
#define FspIopCreateRequest(I, F, E, P) \
    FspIopCreateRequestFunnel(I, F, E, 0, 0, P)
#define FspIopCreateRequestMustSucceed(I, F, E, P)\
    FspIopCreateRequestFunnel(I, F, E, 0, FspIopRequestMustSucceed, P)
#define FspIopCreateRequestEx(I, F, E, RF, P)\
    FspIopCreateRequestFunnel(I, F, E, RF, 0, P)
#define FspIopCreateRequestMustSucceedEx(I, F, E, RF, P)\
    FspIopCreateRequestFunnel(I, F, E, RF, FspIopRequestMustSucceed, P)
#define FspIopCreateRequestWorkItem(I, E, RF, P)\
    FspIopCreateRequestFunnel(I, 0, E, RF, FspIopRequestNonPaged, P)
#define FspIopRequestContext(Request, I)\
    (*FspIopRequestContextAddress(Request, I))
#define FspIopPostWorkRequest(D, R)     FspIopPostWorkRequestFunnel(D, R, FALSE)
#define FspIopPostWorkRequestBestEffort(D, R)\
    FspIopPostWorkRequestFunnel(D, R, TRUE)
#define FspIopCompleteIrp(I, R)         FspIopCompleteIrpEx(I, R, TRUE)
#define REQ_ALIGN_SIZE                  16
typedef VOID FSP_IOP_REQUEST_FINI(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4]);
typedef struct
{
    FSP_IOP_REQUEST_FINI *RequestFini;
    PVOID Context[4];
    FSP_FSCTL_TRANSACT_RSP *Response;
    __declspec(align(REQ_ALIGN_SIZE)) UINT8 RequestBuf[];
} FSP_FSCTL_TRANSACT_REQ_HEADER;
static inline
PVOID *FspIopRequestContextAddress(FSP_FSCTL_TRANSACT_REQ *Request, ULONG I)
{
    FSP_FSCTL_TRANSACT_REQ_HEADER *RequestHeader = (PVOID)((PUINT8)Request - sizeof *RequestHeader);
    return &RequestHeader->Context[I];
}
NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    ULONG Flags, FSP_FSCTL_TRANSACT_REQ **PRequest);
VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopResetRequest(FSP_FSCTL_TRANSACT_REQ *Request, FSP_IOP_REQUEST_FINI *RequestFini);
NTSTATUS FspIopPostWorkRequestFunnel(PDEVICE_OBJECT DeviceObject,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN BestEffort);
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceDereference);
VOID FspIopCompleteCanceledIrp(PIRP Irp);
BOOLEAN FspIopRetryPrepareIrp(PIRP Irp, NTSTATUS *PResult);
BOOLEAN FspIopRetryCompleteIrp(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response, NTSTATUS *PResult);
FSP_FSCTL_TRANSACT_RSP *FspIopIrpResponse(PIRP Irp);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
NTSTATUS FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);
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

/* work queue processing */
enum
{
    FspWqRequestWorkRoutine             = 3,
};
typedef NTSTATUS FSP_WQ_REQUEST_WORK(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp,
    BOOLEAN CanWait);
NTSTATUS FspWqCreateAndPostIrpWorkItem(PIRP Irp,
    FSP_WQ_REQUEST_WORK *WorkRoutine, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN CreateAndPost);
VOID FspWqPostIrpWorkItem(PIRP Irp);
#define FspWqCreateIrpWorkItem(I, RW, RF)\
    FspWqCreateAndPostIrpWorkItem(I, RW, RF, FALSE)
#define FspWqRepostIrpWorkItem(I, RW, RF)\
    FspWqCreateAndPostIrpWorkItem(I, RW, RF, TRUE)

/* device management */
enum
{
    FspFsvolDeviceSecurityCacheCapacity = 100,
    FspFsvolDeviceSecurityCacheItemSizeMax = 4096,
    FspFsvolDeviceDirInfoCacheCapacity = 100,
    FspFsvolDeviceDirInfoCacheItemSizeMax = FSP_FSCTL_ALIGN_UP(16384, PAGE_SIZE),
    FspFsvolDeviceStreamInfoCacheCapacity = 100,
    FspFsvolDeviceStreamInfoCacheItemSizeMax = FSP_FSCTL_ALIGN_UP(16384, PAGE_SIZE),
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
    FspFsvrtDeviceExtensionKind = '\0trV',  /* virtual volume device (e.g. \Device\Volume{GUID}) */
    FspFsvolDeviceExtensionKind = '\0loV',  /* file system volume device (unnamed) */
};
typedef struct
{
    KSPIN_LOCK SpinLock;
    LONG RefCount;
    UINT32 Kind;
} FSP_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    UINT32 InitDoneFsvrt:1, InitDoneIoq:1, InitDoneSec:1, InitDoneDir:1, InitDoneStrm:1,
        InitDoneCtxTab:1, InitDoneTimer:1, InitDoneInfo:1, InitDoneNotify:1;
    PDEVICE_OBJECT FsctlDeviceObject;
    PDEVICE_OBJECT FsvrtDeviceObject;
    HANDLE MupHandle;
    PVPB SwapVpb;
    FSP_DELAYED_WORK_ITEM DeleteVolumeDelayedWorkItem;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    UNICODE_STRING VolumePrefix;
    FSP_IOQ *Ioq;
    FSP_META_CACHE *SecurityCache;
    FSP_META_CACHE *DirInfoCache;
    FSP_META_CACHE *StreamInfoCache;
    KSPIN_LOCK ExpirationLock;
    WORK_QUEUE_ITEM ExpirationWorkItem;
    BOOLEAN ExpirationInProgress;
    ERESOURCE FileRenameResource;
    ERESOURCE ContextTableResource;
    RTL_AVL_TABLE ContextByNameTable;
    PVOID ContextByNameTableElementStorage;
    UNICODE_STRING VolumeName;
    WCHAR VolumeNameBuf[FSP_FSCTL_VOLUME_NAME_SIZE / sizeof(WCHAR)];
    KSPIN_LOCK InfoSpinLock;
    UINT64 InfoExpirationTime;
    FSP_FSCTL_VOLUME_INFO VolumeInfo;
    PNOTIFY_SYNC NotifySync;
    LIST_ENTRY NotifyList;
} FSP_FSVOL_DEVICE_EXTENSION;
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
VOID FspFsvolDeviceFileRenameAcquireExclusive(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameSetOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
VOID FspFsvolDeviceFileRenameRelease(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameReleaseOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
VOID FspFsvolDeviceLockContextTable(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceUnlockContextTable(PDEVICE_OBJECT DeviceObject);
NTSTATUS FspFsvolDeviceCopyContextByNameList(PDEVICE_OBJECT DeviceObject,
    PVOID **PContexts, PULONG PContextCount);
VOID FspFsvolDeviceDeleteContextByNameList(PVOID *Contexts, ULONG ContextCount);
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
#define FspFsvolDeviceStoppedStatus(DeviceObject)\
    STATUS_VOLUME_DISMOUNTED
    //(FILE_DEVICE_DISK_FILE_SYSTEM == (DeviceObject)->DeviceType ?\
    //    STATUS_VOLUME_DISMOUNTED : STATUS_DEVICE_NOT_CONNECTED)

/* volume management */
#define FspVolumeTransactEarlyTimeout   (1 * 10000ULL)
NTSTATUS FspVolumeCreate(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
VOID FspVolumeDelete(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeMount(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeRedirQueryPathEx(
    PDEVICE_OBJECT FsvolDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetName(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeGetNameList(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeTransact(
    PDEVICE_OBJECT FsctlDeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
NTSTATUS FspVolumeStop(
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
    UINT64 DirInfo;
    UINT64 StreamInfo;
} FSP_FILE_NODE_NONPAGED;
typedef struct FSP_FILE_NODE
{
    FSRTL_ADVANCED_FCB_HEADER Header;
    FSP_FILE_NODE_NONPAGED *NonPaged;
    /* interlocked access */
    LONG RefCount;
    UINT32 DeletePending;
    /* locked under FSP_FSVOL_DEVICE_EXTENSION::ContextTableResource */
    LONG OpenCount;                     /* ContextTable ref count */
    LONG HandleCount;                   /* HANDLE count (CREATE/CLEANUP) */
    SHARE_ACCESS ShareAccess;
    ULONG MainFileDenyDeleteCount;      /* number of times main file is denying delete */
    ULONG StreamDenyDeleteCount;        /* number of times open streams are denying delete */
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
    ULONG FileInfoChangeNumber;
    UINT64 Security;
    ULONG SecurityChangeNumber;
    ULONG DirInfoChangeNumber;
    ULONG StreamInfoChangeNumber;
    BOOLEAN TruncateOnClose;
    FILE_LOCK FileLock;
    struct
    {
        PVOID LazyWriteThread;
        union
        {
            PIRP TopLevelIrp;
            UINT32 TopFlags;
        } CcFlush;
    } Tls;
    /* read-only after creation (and insertion in the ContextTable) */
    PDEVICE_OBJECT FsvolDeviceObject;
    UINT64 UserContext;
    UINT64 IndexNumber;
    BOOLEAN IsDirectory;
    BOOLEAN IsRootDirectory;
    struct FSP_FILE_NODE *MainFileNode; /* this becomes invalid after our last desc close */
    WCHAR FileNameBuf[];
} FSP_FILE_NODE;
typedef struct
{
    FSP_FILE_NODE *FileNode;
    UINT64 UserContext2;
    BOOLEAN CaseSensitive;
    BOOLEAN HasTraversePrivilege;
    BOOLEAN DeleteOnClose;
    BOOLEAN DirectoryHasSuchFile;
    UNICODE_STRING DirectoryPattern;
    UINT64 DirectoryOffset;
    UINT64 DirInfo;
    ULONG DirInfoCacheHint;
    /* stream support */
    HANDLE MainFileHandle;
    PFILE_OBJECT MainFileObject;
} FSP_FILE_DESC;
NTSTATUS FspFileNodeCopyList(PDEVICE_OBJECT DeviceObject,
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
FSP_FILE_NODE *FspFileNodeOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    UINT32 GrantedAccess, UINT32 ShareAccess, NTSTATUS *PResult);
VOID FspFileNodeCleanup(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    PBOOLEAN PDeletePending);
VOID FspFileNodeCleanupComplete(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject);
VOID FspFileNodeClose(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject);
NTSTATUS FspFileNodeFlushAndPurgeCache(FSP_FILE_NODE *FileNode,
    UINT64 FlushOffset64, ULONG FlushLength, BOOLEAN FlushAndPurge);
VOID FspFileNodeRename(FSP_FILE_NODE *FileNode, PUNICODE_STRING NewFileName);
BOOLEAN FspFileNodeHasOpenHandles(PDEVICE_OBJECT FsvolDeviceObject,
    PUNICODE_STRING FileName, BOOLEAN SubpathOnly);
VOID FspFileNodeGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTryGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
VOID FspFileNodeSetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTrySetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo, ULONG InfoChangeNumber);
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
VOID FspFileNodeNotifyChange(FSP_FILE_NODE *FileNode, ULONG Filter, ULONG Action);
NTSTATUS FspFileNodeProcessLockIrp(FSP_FILE_NODE *FileNode, PIRP Irp);
NTSTATUS FspFileDescCreate(FSP_FILE_DESC **PFileDesc);
VOID FspFileDescDelete(FSP_FILE_DESC *FileDesc);
NTSTATUS FspFileDescResetDirectoryPattern(FSP_FILE_DESC *FileDesc,
    PUNICODE_STRING FileName, BOOLEAN Reset);
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
static __forceinline
BOOLEAN FspMainFileOpenCheck(PIRP Irp)
{
    extern const GUID FspMainFileOpenEcpGuid;
    NTSTATUS Result;
    PECP_LIST ExtraCreateParameters = 0;
    PVOID ExtraCreateParameter = 0;

    Result = FsRtlGetEcpListFromIrp(Irp, &ExtraCreateParameters);
    if (!NT_SUCCESS(Result) || 0 == ExtraCreateParameters)
        return FALSE;

    Result = FsRtlFindExtraCreateParameter(ExtraCreateParameters,
        &FspMainFileOpenEcpGuid, &ExtraCreateParameter, 0);
    if (!NT_SUCCESS(Result) || 0 == ExtraCreateParameter)
        return FALSE;

    if (FsRtlIsEcpFromUserMode(ExtraCreateParameter))
        return FALSE;

    return TRUE;
}
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
#define FspFileNodeUnlockAll(N,F,P)     FsRtlFastUnlockAll(&(N)->FileLock, F, P, N)

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
extern PDEVICE_OBJECT FspFsctlDiskDeviceObject;
extern PDEVICE_OBJECT FspFsctlNetDeviceObject;
extern FAST_IO_DISPATCH FspFastIoDispatch;
extern CACHE_MANAGER_CALLBACKS FspCacheManagerCallbacks;
extern FSP_IOPREP_DISPATCH *FspIopPrepareFunction[];
extern FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[];
extern ERESOURCE FspDeviceGlobalResource;
extern WCHAR FspFileDescDirectoryPatternMatchAll[];
extern const GUID FspMainFileOpenEcpGuid;
extern FSP_MV_CcCoherencyFlushAndPurgeCache *FspMvCcCoherencyFlushAndPurgeCache;
extern ULONG FspMvMdlMappingNoWrite;

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

#endif
