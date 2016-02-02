/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
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

#define DRIVER_NAME                     "WinFsp"

/* IoCreateDeviceSecure default SDDL's */
#define FSP_FSCTL_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */
#define FSP_FSVRT_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */

/* private NTSTATUS codes */
#define FSP_STATUS_PRIVATE_BIT          (0x20000000)
#define FSP_STATUS_IOQ_POST             (FSP_STATUS_PRIVATE_BIT | 0x0000)
#define FSP_STATUS_IOQ_POST_BEST_EFFORT (FSP_STATUS_PRIVATE_BIT | 0x0001)

/* misc macros */
#define FSP_ALLOC_INTERNAL_TAG          'IpsF'
#define FSP_ALLOC_EXTERNAL_TAG          'XpsF'
#define FSP_IO_INCREMENT                IO_NETWORK_INCREMENT

/* DEBUGLOG */
#if DBG
#define DEBUGLOG(fmt, ...)              \
    DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

/* DEBUGLOGIRP */
#if DBG
#define DEBUGLOGIRP(Irp, Result)        FspDebugLogIrp(__FUNCTION__, Irp, Result)
#else
#define DEBUGLOGIRP(Irp, Result)        ((void)0)
#endif

/* DEBUGBREAK */
#if DBG
extern __declspec(selectany) int fsp_bp = 1;
#define DEBUGBREAK()                    \
    do                                  \
    {                                   \
        static int bp = 1;              \
        if (bp && fsp_bp && !KD_DEBUGGER_NOT_PRESENT)\
            DbgBreakPoint();            \
    } while (0,0)
#else
#define DEBUGBREAK()                    do {} while (0,0)
#endif

/* DEBUGRANDTEST */
#if DBG
#define DEBUGRANDTEST(Percent, Default) (DebugRandom() <= (Percent) * 0x7fff / 100 ? (Default) : !(Default))
#else
#define DEBUGRANDTEST(Percent, Default) (Default)
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
#define FSP_ENTER_(...)                 \
    DEBUGBREAK();                       \
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
#define FSP_ENTER_NOCRIT_(...)          \
    DEBUGBREAK();                       \
    {                                   \
        __VA_ARGS__
#define FSP_LEAVE_NOCRIT_(...)          \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
        __VA_ARGS__;                    \
    }
#define FSP_ENTER(...)                  \
    NTSTATUS Result = STATUS_SUCCESS; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE(fmt, ...)             \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, NtStatusSym(Result))); return Result
#define FSP_ENTER_MJ(...)               \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);\
    BOOLEAN fsp_device_deref = FALSE; \
    FSP_ENTER_(__VA_ARGS__);            \
    do                                  \
    {                                   \
        if (!FspDeviceReference(IrpSp->DeviceObject))\
        {                               \
            Result = STATUS_CANCELLED;  \
            goto fsp_leave_label;       \
        }                               \
        fsp_device_deref = TRUE;      \
    } while (0,0)
#define FSP_LEAVE_MJ(fmt, ...)          \
    FSP_LEAVE_(                         \
        if (STATUS_PENDING != Result)   \
        {                               \
            ASSERT(0 == (FSP_STATUS_PRIVATE_BIT & Result) ||\
                FSP_STATUS_IOQ_POST == Result || FSP_STATUS_IOQ_POST_BEST_EFFORT == Result);\
            FSP_DEBUGLOG_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
                Irp,                    \
                (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,\
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
                {\
                    DEBUGLOG("FspIoqPostIrpEx = %s", NtStatusSym(Result));\
                    FspIopCompleteIrp(Irp, Result);\
                }\
            }                           \
            else                        \
                FspIopCompleteIrpEx(Irp, Result, fsp_device_deref);\
        }                               \
    );                                  \
    return Result
#define FSP_ENTER_IOC(...)              \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp); (VOID)IrpSp;\
    FSP_ENTER_NOCRIT_(__VA_ARGS__)
#define FSP_LEAVE_IOC(fmt, ...)         \
    FSP_LEAVE_NOCRIT_(                  \
        if (STATUS_PENDING != Result)   \
        {                               \
            ASSERT(0 == (FSP_STATUS_PRIVATE_BIT & Result));\
            FSP_DEBUGLOG_NOCRIT_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
                Irp,                    \
                (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,\
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
FSP_IOCMPL_DISPATCH FspFsvolDirectoryControlComplete;
FSP_IOCMPL_DISPATCH FspFsvolFileSystemControlComplete;
FSP_IOCMPL_DISPATCH FspFsvolFlushBuffersComplete;
FSP_IOCMPL_DISPATCH FspFsvolLockControlComplete;
FSP_IOCMPL_DISPATCH FspFsvolQueryEaComplete;
FSP_IOCMPL_DISPATCH FspFsvolQueryInformationComplete;
FSP_IOCMPL_DISPATCH FspFsvolQuerySecurityComplete;
FSP_IOCMPL_DISPATCH FspFsvolQueryVolumeInformationComplete;
FSP_IOCMPL_DISPATCH FspFsvolReadComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetEaComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetInformationComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetSecurityComplete;
FSP_IOCMPL_DISPATCH FspFsvolSetVolumeInformationComplete;
FSP_IOCMPL_DISPATCH FspFsvolShutdownComplete;
FSP_IOCMPL_DISPATCH FspFsvolWriteComplete;

/* fast I/O and resource acquisition */
FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;
FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;

/* memory allocation */
static inline
PVOID FspAlloc(SIZE_T Size)
{
    return ExAllocatePoolWithTag(PagedPool, Size, FSP_ALLOC_INTERNAL_TAG);
}
static inline
PVOID FspAllocNonPaged(SIZE_T Size)
{
    return ExAllocatePoolWithTag(NonPagedPool, Size, FSP_ALLOC_INTERNAL_TAG);
}
static inline
VOID FspFree(PVOID Pointer)
{
    ExFreePoolWithTag(Pointer, FSP_ALLOC_INTERNAL_TAG);
}
static inline
PVOID FspAllocExternal(SIZE_T Size)
{
    return ExAllocatePoolWithTag(PagedPool, Size, FSP_ALLOC_EXTERNAL_TAG);
}
static inline
PVOID FspAllocNonPagedExternal(SIZE_T Size)
{
    return ExAllocatePoolWithTag(NonPagedPool, Size, FSP_ALLOC_EXTERNAL_TAG);
}
static inline
VOID FspFreeExternal(PVOID Pointer)
{
    ExFreePool(Pointer);
}

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

/* utility */
NTSTATUS FspCreateGuid(GUID *Guid);
NTSTATUS FspCcSetFileSizes(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes);

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

/* IRP context */
#define FspIrpTimestampInfinity         ((ULONG)-1L)
#define FspIrpTimestamp(Irp)            \
    (*(ULONG *)&(Irp)->Tail.Overlay.DriverContext[0])
#define FspIrpDictNext(Irp)             \
    (*(PIRP *)&(Irp)->Tail.Overlay.DriverContext[1])
#define FspIrpRequest(Irp)              \
    (*(FSP_FSCTL_TRANSACT_REQ **)&(Irp)->Tail.Overlay.DriverContext[2])

/* I/O queue */
#define FspIoqTimeout                   ((PIRP)1)
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
    ULONG PendingIrpCapacity, PendingIrpCount;
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
VOID FspIoqRemoveExpired(FSP_IOQ *Ioq);
BOOLEAN FspIoqPostIrpEx(FSP_IOQ *Ioq, PIRP Irp, BOOLEAN BestEffort, NTSTATUS *PResult);
PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp, PLARGE_INTEGER Timeout);
BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint);
BOOLEAN FspIoqRetryCompleteIrp(FSP_IOQ *Ioq, PIRP Irp, NTSTATUS *PResult);
PIRP FspIoqNextCompleteIrp(FSP_IOQ *Ioq, PIRP BoundaryIrp);

/* I/O processing */
#define FSP_FSCTL_WORK                  \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'W', METHOD_NEITHER, FILE_ANY_ACCESS)
#define FSP_FSCTL_WORK_BEST_EFFORT      \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'w', METHOD_NEITHER, FILE_ANY_ACCESS)
#define FspIopCreateRequest(I, F, E, P) \
    FspIopCreateRequestFunnel(I, F, E, 0, FALSE, P)
#define FspIopCreateRequestMustSucceed(I, F, E, P)\
    FspIopCreateRequestFunnel(I, F, E, 0, TRUE, P)
#define FspIopCreateRequestEx(I, F, E, RF, P)\
    FspIopCreateRequestFunnel(I, F, E, RF, FALSE, P)
#define FspIopRequestContext(Request, I)\
    (*FspIopRequestContextAddress(Request, I))
#define FspIopPostWorkRequest(D, R)     FspIopPostWorkRequestFunnel(D, R, FALSE)
#define FspIopPostWorkRequestBestEffort(D, R)\
    FspIopPostWorkRequestFunnel(D, R, TRUE)
#define FspIopCompleteIrp(I, R)         FspIopCompleteIrpEx(I, R, TRUE)
typedef VOID FSP_IOP_REQUEST_FINI(PVOID Context[3]);
NTSTATUS FspIopCreateRequestFunnel(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    BOOLEAN MustSucceed,
    FSP_FSCTL_TRANSACT_REQ **PRequest);
VOID FspIopDeleteRequest(FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopResetRequest(FSP_FSCTL_TRANSACT_REQ *Request, FSP_IOP_REQUEST_FINI *RequestFini);
PVOID *FspIopRequestContextAddress(FSP_FSCTL_TRANSACT_REQ *Request, ULONG I);
NTSTATUS FspIopPostWorkRequestFunnel(PDEVICE_OBJECT DeviceObject,
    FSP_FSCTL_TRANSACT_REQ *Request, BOOLEAN BestEffort);
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceDereference);
VOID FspIopCompleteCanceledIrp(PIRP Irp);
BOOLEAN FspIopRetryPrepareIrp(PIRP Irp, NTSTATUS *PResult);
BOOLEAN FspIopRetryCompleteIrp(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response, NTSTATUS *PResult);
FSP_FSCTL_TRANSACT_RSP *FspIopIrpResponse(PIRP Irp);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
NTSTATUS FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

/* device management */
#define FSP_DEVICE_VOLUME_NAME_LENMAX   (FSP_FSCTL_VOLUME_NAME_SIZEMAX - sizeof(WCHAR))
typedef struct
{
    UINT64 Identifier;
    PVOID Context;
} FSP_DEVICE_GENERIC_TABLE_ELEMENT_DATA;
typedef struct
{
    RTL_BALANCED_LINKS Header;
    FSP_DEVICE_GENERIC_TABLE_ELEMENT_DATA Data;
} FSP_DEVICE_GENERIC_TABLE_ELEMENT;
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
    UINT32 InitDoneFsvrt:1, InitDoneDelRsc:1, InitDoneIoq:1, InitDoneGenTab:1, InitDoneTimer:1,
        InitDoneInfo:1;
    PDEVICE_OBJECT FsctlDeviceObject;
    PDEVICE_OBJECT FsvrtDeviceObject;
    HANDLE MupHandle;
    PVPB SwapVpb;
    FSP_DELAYED_WORK_ITEM DeleteVolumeDelayedWorkItem;
    ERESOURCE DeleteResource;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    UNICODE_STRING VolumePrefix;
    FSP_IOQ *Ioq;
    KSPIN_LOCK ExpirationLock;
    WORK_QUEUE_ITEM ExpirationWorkItem;
    BOOLEAN ExpirationInProgress;
    ERESOURCE GenericTableResource;
    RTL_AVL_TABLE GenericTable;
    PVOID GenericTableElementStorage;
    UNICODE_STRING VolumeName;
    WCHAR VolumeNameBuf[FSP_DEVICE_VOLUME_NAME_LENMAX / sizeof(WCHAR)];
    KSPIN_LOCK InfoSpinLock;
    UINT64 InfoExpirationTime;
    FSP_FSCTL_VOLUME_INFO VolumeInfo;
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
    PUNICODE_STRING DeviceName, DEVICE_TYPE DeviceType,
    PUNICODE_STRING DeviceSddl, LPCGUID DeviceClassGuid,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceCreate(UINT32 Kind, ULONG ExtraSize,
    DEVICE_TYPE DeviceType,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceInitialize(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspDeviceReference(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDereference(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceLockContextTable(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceUnlockContextTable(PDEVICE_OBJECT DeviceObject);
PVOID FspFsvolDeviceLookupContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier);
PVOID FspFsvolDeviceInsertContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier, PVOID Context,
    FSP_DEVICE_GENERIC_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted);
VOID FspFsvolDeviceDeleteContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier,
    PBOOLEAN PDeleted);
VOID FspFsvolGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
BOOLEAN FspFsvolTryGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
VOID FspFsvolSetVolumeInfo(PDEVICE_OBJECT DeviceObject, const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
NTSTATUS FspDeviceCopyList(
    PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount);
VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount);
VOID FspDeviceDeleteAll(VOID);

/* volume management */
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
NTSTATUS FspVolumeTransact(
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
} FSP_FILE_NODE_NONPAGED;
typedef struct
{
    FSRTL_ADVANCED_FCB_HEADER Header;
    FSP_FILE_NODE_NONPAGED *NonPaged;
    /* interlocked access */
    LONG RefCount;
    /* locked access (ContextTable lock) */
    LONG OpenCount;
    SHARE_ACCESS ShareAccess;
    struct
    {
        UINT32 DeleteOnClose:1;
        UINT32 DeletePending:1;
    } Flags;
    /* locked under Header.Resource */
    UINT64 InfoExpirationTime;
    UINT32 FileAttributes;
    UINT32 ReparseTag;
    UINT64 CreationTime;
    UINT64 LastAccessTime;
    UINT64 LastWriteTime;
    UINT64 ChangeTime;
    NTSTATUS CcStatus;
    /* read-only after creation (and insertion in the ContextTable) */
    PDEVICE_OBJECT FsvolDeviceObject;
    UINT64 UserContext;
    UINT64 IndexNumber;
    BOOLEAN IsDirectory;
    FSP_DEVICE_GENERIC_TABLE_ELEMENT ElementStorage;
    UNICODE_STRING FileName;
    WCHAR FileNameBuf[];
} FSP_FILE_NODE;
typedef struct
{
    FSP_FILE_NODE *FileNode;
    UINT64 UserContext2;
} FSP_FILE_DESC;
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
BOOLEAN FspFileNodeTryAcquireSharedF(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags);
BOOLEAN FspFileNodeTryAcquireExclusiveF(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeSetOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner);
VOID FspFileNodeReleaseF(FSP_FILE_NODE *FileNode, ULONG Flags);
VOID FspFileNodeReleaseOwnerF(FSP_FILE_NODE *FileNode, ULONG Flags, PVOID Owner);
FSP_FILE_NODE *FspFileNodeOpen(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    UINT32 GrantedAccess, UINT32 ShareAccess, BOOLEAN DeleteOnClose, NTSTATUS *PResult);
VOID FspFileNodeClose(FSP_FILE_NODE *FileNode, PFILE_OBJECT FileObject,
    PBOOLEAN PDeletePending);
VOID FspFileNodeGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
BOOLEAN FspFileNodeTryGetFileInfo(FSP_FILE_NODE *FileNode, FSP_FSCTL_FILE_INFO *FileInfo);
VOID FspFileNodeSetFileInfo(FSP_FILE_NODE *FileNode, PFILE_OBJECT CcFileObject,
    const FSP_FSCTL_FILE_INFO *FileInfo);
NTSTATUS FspFileDescCreate(FSP_FILE_DESC **PFileDesc);
VOID FspFileDescDelete(FSP_FILE_DESC *FileDesc);
#define FspFileNodeAcquireShared(N,F)   FspFileNodeAcquireSharedF(N, FspFileNodeAcquire ## F)
#define FspFileNodeTryAcquireShared(N,F)    FspFileNodeTryAcquireSharedF(N, FspFileNodeAcquire ## F)
#define FspFileNodeAcquireExclusive(N,F)    FspFileNodeAcquireExclusiveF(N, FspFileNodeAcquire ## F)
#define FspFileNodeTryAcquireExclusive(N,F) FspFileNodeTryAcquireExclusiveF(N, FspFileNodeAcquire ## F)
#define FspFileNodeSetOwner(N,F,P)      FspFileNodeSetOwnerF(N, FspFileNodeAcquire ## F, P)
#define FspFileNodeRelease(N,F)         FspFileNodeReleaseF(N, FspFileNodeAcquire ## F)
#define FspFileNodeReleaseOwner(N,F,P)  FspFileNodeReleaseOwnerF(N, FspFileNodeAcquire ## F, P)

/* debug */
#if DBG
const char *NtStatusSym(NTSTATUS Status);
const char *IrpMajorFunctionSym(UCHAR MajorFunction);
const char *IrpMinorFunctionSym(UCHAR MajorFunction, UCHAR MinorFunction);
const char *IoctlCodeSym(ULONG ControlCode);
const char *FileInformationClassSym(FILE_INFORMATION_CLASS FileInformationClass);
const char *FsInformationClassSym(FS_INFORMATION_CLASS FsInformationClass);
ULONG DebugRandom(VOID);
static inline
VOID FspDebugLogIrp(const char *func, PIRP Irp, NTSTATUS Result)
{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    DbgPrint("[%d] " DRIVER_NAME "!%s: IRP=%p, %s%c, %s%s, IoStatus=%s[%lld]\n",
        KeGetCurrentIrql(),
        func,
        Irp,
        (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,
        Irp->RequestorMode == KernelMode ? 'K' : 'U',
        IrpMajorFunctionSym(IrpSp->MajorFunction),
        IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),
        NtStatusSym(Result),
        (LONGLONG)Irp->IoStatus.Information);
}
#endif

/* extern */
extern PDRIVER_OBJECT FspDriverObject;
extern PDEVICE_OBJECT FspFsctlDiskDeviceObject;
extern PDEVICE_OBJECT FspFsctlNetDeviceObject;
extern FSP_IOPREP_DISPATCH *FspIopPrepareFunction[];
extern FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[];

#endif
