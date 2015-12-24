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

/* DEBUGBREAK */
#if DBG
extern __declspec(selectany) int bpglobal = 1;
#define DEBUGBREAK()                    \
    do                                  \
    {                                   \
        static int bp = 1;          \
        if (bp && bpglobal && !KD_DEBUGGER_NOT_PRESENT)\
            DbgBreakPoint();            \
    } while (0,0)
#else
#define DEBUGBREAK()                    do {} while (0,0)
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
    BOOLEAN fsp_device_release = FALSE; \
    FSP_ENTER_(__VA_ARGS__);            \
    do                                  \
    {                                   \
        if (!FspDeviceRetain(IrpSp->DeviceObject))\
        {                               \
            Result = STATUS_CANCELLED;  \
            goto fsp_leave_label;       \
        }                               \
        fsp_device_release = TRUE;      \
    } while (0,0)
#define FSP_LEAVE_MJ(fmt, ...)          \
    FSP_LEAVE_(                         \
        FSP_DEBUGLOG_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
            Irp,                        \
            (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,\
            Irp->RequestorMode == KernelMode ? 'K' : 'U',\
            IrpMajorFunctionSym(IrpSp->MajorFunction),\
            IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),\
            __VA_ARGS__,                \
            NtStatusSym(Result),        \
            (LONGLONG)Irp->IoStatus.Information);\
        if (STATUS_PENDING == Result)   \
        {                               \
            if (0 == (IrpSp->Control & SL_PENDING_RETURNED))\
            {                           \
                /* if the IRP has not been marked pending already */\
                FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =\
                    FspFsvolDeviceExtension(DeviceObject);\
                if (!FspIoqPostIrp(&FsvolDeviceExtension->Ioq, Irp, &Result))\
                    FspIopCompleteIrp(Irp, Result);\
            }                           \
        }                               \
        else                            \
            FspIopCompleteIrpEx(Irp, Result, fsp_device_release);\
    );                                  \
    return Result
#define FSP_ENTER_IOC(...)              \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp); (VOID)IrpSp;\
    FSP_ENTER_NOCRIT_(__VA_ARGS__)
#define FSP_LEAVE_IOC(fmt, ...)         \
    FSP_LEAVE_NOCRIT_(                  \
        FSP_DEBUGLOG_NOCRIT_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
            Irp,                        \
            (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,\
            Irp->RequestorMode == KernelMode ? 'K' : 'U',\
            IrpMajorFunctionSym(IrpSp->MajorFunction),\
            IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),\
            __VA_ARGS__,                \
            NtStatusSym(Result),        \
            (LONGLONG)Irp->IoStatus.Information);\
        FspIopCompleteIrp(Irp, Result);\
    )
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
typedef VOID FSP_IOCMPL_DISPATCH(
    _Inout_ PIRP Irp, _In_ const FSP_FSCTL_TRANSACT_RSP *Response);
FSP_IOPREP_DISPATCH FspFsvolCreatePrepare;
FSP_IOCMPL_DISPATCH FspFsvolCleanupComplete;
FSP_IOCMPL_DISPATCH FspFsvolCloseComplete;
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

/* utility: GUIDs */
NTSTATUS FspCreateGuid(GUID *Guid);

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
#define FspIrpTimestamp(Irp)            \
    (*(ULONGLONG *)&(Irp)->Tail.Overlay.DriverContext[0])
    /* FspIrpTimestamp() uses up DriverContext[0] and [1] in 32-bit builds */
#define FspIrpRequest(Irp)              \
    (*(FSP_FSCTL_TRANSACT_REQ **)&(Irp)->Tail.Overlay.DriverContext[2])

/* I/O queue */
#define FspIoqTimeout                   ((PIRP)1)
typedef struct
{
    KSPIN_LOCK SpinLock;
    BOOLEAN Stopped;
    KEVENT PendingIrpEvent;
    LIST_ENTRY PendingIrpList, ProcessIrpList;
    IO_CSQ PendingIoCsq, ProcessIoCsq;
    LARGE_INTEGER IrpTimeout;
    ULONG PendingIrpCapacity, PendingIrpCount;
    VOID (*CompleteCanceledIrp)(PIRP Irp);
} FSP_IOQ;
VOID FspIoqInitialize(FSP_IOQ *Ioq,
    PLARGE_INTEGER IrpTimeout, ULONG IrpCapacity, VOID (*CompleteCanceledIrp)(PIRP Irp));
VOID FspIoqStop(FSP_IOQ *Ioq);
BOOLEAN FspIoqStopped(FSP_IOQ *Ioq);
VOID FspIoqRemoveExpired(FSP_IOQ *Ioq);
BOOLEAN FspIoqPostIrp(FSP_IOQ *Ioq, PIRP Irp, NTSTATUS *PResult);
PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, PLARGE_INTEGER Timeout);
BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint);

/* I/O processing */
#define FSP_FSCTL_WORK                  \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'W', METHOD_NEITHER, FILE_ANY_ACCESS)
#define FspIopRequestContext(Request, I)\
    (*FspIopRequestContextAddress(Request, I))
#define FspIopCreateRequest(I, F, E, P) FspIopCreateRequestEx(I, F, E, 0, P)
typedef VOID FSP_IOP_REQUEST_FINI(PVOID Context[3]);
NTSTATUS FspIopCreateRequestEx(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_IOP_REQUEST_FINI *RequestFini,
    FSP_FSCTL_TRANSACT_REQ **PRequest);
PVOID *FspIopRequestContextAddress(FSP_FSCTL_TRANSACT_REQ *Request, ULONG I);
NTSTATUS FspIopPostWorkRequest(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopCompleteIrpEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceRelease);
static inline
VOID FspIopCompleteIrp(PIRP Irp, NTSTATUS Result)
{
    FspIopCompleteIrpEx(Irp, Result, TRUE);
}
VOID FspIopCompleteCanceledIrp(PIRP Irp);
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

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
    PDEVICE_OBJECT FsctlDeviceObject;
    PDEVICE_OBJECT FsvrtDeviceObject;
    HANDLE MupHandle;
    PVPB SwapVpb;
    FSP_DELAYED_WORK_ITEM DeleteVolumeDelayedWorkItem;
    ERESOURCE DeleteResource;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    FSP_IOQ Ioq;
    KSPIN_LOCK ExpirationLock;
    WORK_QUEUE_ITEM ExpirationWorkItem;
    BOOLEAN ExpirationInProgress;
    FAST_MUTEX GenericTableFastMutex;
    RTL_AVL_TABLE GenericTable;
    PVOID GenericTableElementStorage;
    UNICODE_STRING VolumeName;
    WCHAR VolumeNameBuf[FSP_DEVICE_VOLUME_NAME_LENMAX / sizeof(WCHAR)];
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
VOID FspDeviceInitComplete(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspDeviceRetain(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceRelease(PDEVICE_OBJECT DeviceObject);
_IRQL_raises_(APC_LEVEL)
_IRQL_saves_global_(OldIrql, DeviceObject)
VOID FspFsvolDeviceLockContext(PDEVICE_OBJECT DeviceObject);
_IRQL_requires_(APC_LEVEL)
_IRQL_restores_global_(OldIrql, DeviceObject)
VOID FspFsvolDeviceUnlockContext(PDEVICE_OBJECT DeviceObject);
PVOID FspFsvolDeviceLookupContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier);
PVOID FspFsvolDeviceInsertContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier, PVOID Context,
    FSP_DEVICE_GENERIC_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted);
VOID FspFsvolDeviceDeleteContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier,
    PBOOLEAN PDeleted);
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
#define FspFileContextKind(FsContext)   \
    (((FSP_FILE_CONTEXT *)FsContext)->Header.NodeTypeCode)
#define FspFileContextIsValid(FsContext)\
    (0 != (FsContext) && FspFileContextFileKind == ((FSP_FILE_CONTEXT *)FsContext)->Header.NodeTypeCode)
enum
{
    FspFileContextFileKind = 'BZ',
};
typedef struct
{
    ERESOURCE Resource;
    ERESOURCE PagingIoResource;
    FAST_MUTEX HeaderFastMutex;
} FSP_FILE_CONTEXT_NONPAGED;
typedef struct
{
    FSRTL_ADVANCED_FCB_HEADER Header;
    FSP_FILE_CONTEXT_NONPAGED *NonPaged;
    /* interlocked access */
    LONG RefCount;
#if 0
    /* protected by Header.Resource */
    LONG OpenCount;
    SHARE_ACCESS ShareAccess;
    BOOLEAN DeletePending;              /* FileDispositionInformation */
    BOOLEAN DeleteOnClose;              /* FILE_DELETE_ON_CLOSE */
    FSP_DEVICE_GENERIC_TABLE_ELEMENT ElementStorage;
    UINT64 UserContext;
#endif
    /* read-only after creation */
    PDEVICE_OBJECT FsvolDeviceObject;
    UNICODE_STRING FileName;
    WCHAR FileNameBuf[];
} FSP_FILE_CONTEXT;
NTSTATUS FspFileContextCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_CONTEXT **PFsContext);
VOID FspFileContextDelete(FSP_FILE_CONTEXT *Context);
static inline
VOID FspFileContextRetain(FSP_FILE_CONTEXT *Context)
{
    InterlockedIncrement(&Context->RefCount);
}
static inline
VOID FspFileContextRelease(FSP_FILE_CONTEXT *Context)
{
    LONG RefCount = InterlockedDecrement(&Context->RefCount);
    if (0 == RefCount)
        FspFileContextDelete(Context);
}

/* debug */
#if DBG
const char *NtStatusSym(NTSTATUS Status);
const char *IrpMajorFunctionSym(UCHAR MajorFunction);
const char *IrpMinorFunctionSym(UCHAR MajorFunction, UCHAR MinorFunction);
const char *IoctlCodeSym(ULONG ControlCode);
#endif

/* extern */
extern PDRIVER_OBJECT FspDriverObject;
extern PDEVICE_OBJECT FspFsctlDiskDeviceObject;
extern PDEVICE_OBJECT FspFsctlNetDeviceObject;
extern FSP_IOPREP_DISPATCH *FspIopPrepareFunction[];
extern FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[];

#if 0
typedef struct
{
    FSRTL_ADVANCED_FCB_HEADER Header;
    FSP_FILE_CONTEXT_NONPAGED *NonPaged;
    /* interlocked access */
    LONG RefCount;
    /* protected by Header.Resource */
    LONG OpenCount;
    SHARE_ACCESS ShareAccess;
    BOOLEAN DeletePending;              /* FileDispositionInformation */
    BOOLEAN DeleteOnClose;              /* FILE_DELETE_ON_CLOSE */
    /* read-only after creation */
    FSP_DEVICE_GENERIC_TABLE_ELEMENT ElementStorage;
    PDEVICE_OBJECT FsvolDeviceObject;
    UINT64 UserContext;
    UNICODE_STRING FileName;
    WCHAR FileNameBuf[];
} FSP_FILE_CONTEXT;
NTSTATUS FspFileContextCreate(PDEVICE_OBJECT DeviceObject,
    ULONG ExtraSize, FSP_FILE_CONTEXT **PFsContext);
VOID FspFileContextDelete(FSP_FILE_CONTEXT *Context);
static inline
VOID FspFileContextOpen(FSP_FILE_CONTEXT *Context)
{
    ASSERT(0 == Context->OpenCount || ExIsResourceAcquiredExclusiveLite(Context->Header.Resource));
    Context->OpenCount++;
}
static inline
LONG FspFileContextClose(FSP_FILE_CONTEXT *Context)
{
    ASSERT(ExIsResourceAcquiredExclusiveLite(Context->Header.Resource));
    ASSERT(0 < Context->OpenCount);
    return --Context->OpenCount;
}

#endif

#endif
