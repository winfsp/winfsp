/**
 * @file sys/device.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspDeviceCreateSecure(UINT32 Kind, ULONG ExtraSize,
    PUNICODE_STRING DeviceName, DEVICE_TYPE DeviceType,
    PUNICODE_STRING DeviceSddl, LPCGUID DeviceClassGuid,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceCreate(UINT32 Kind, ULONG ExtraSize,
    DEVICE_TYPE DeviceType,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceInitialize(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspDeviceRetain(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceRelease(PDEVICE_OBJECT DeviceObject);
_IRQL_requires_(DISPATCH_LEVEL)
static BOOLEAN FspDeviceRetainAtDpcLevel(PDEVICE_OBJECT DeviceObject);
_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspDeviceReleaseFromDpcLevel(PDEVICE_OBJECT DeviceObject);
static NTSTATUS FspFsvolDeviceInit(PDEVICE_OBJECT DeviceObject);
static VOID FspFsvolDeviceFini(PDEVICE_OBJECT DeviceObject);
static IO_TIMER_ROUTINE FspFsvolDeviceTimerRoutine;
static WORKER_THREAD_ROUTINE FspFsvolDeviceExpirationRoutine;
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
static RTL_AVL_COMPARE_ROUTINE FspFsvolDeviceCompareElement;
static RTL_AVL_ALLOCATE_ROUTINE FspFsvolDeviceAllocateElement;
static RTL_AVL_FREE_ROUTINE FspFsvolDeviceFreeElement;
NTSTATUS FspDeviceCopyList(
    PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount);
VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount);
VOID FspDeviceDeleteAll(VOID);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspDeviceCreateSecure)
#pragma alloc_text(PAGE, FspDeviceCreate)
#pragma alloc_text(PAGE, FspDeviceInitialize)
#pragma alloc_text(PAGE, FspDeviceDelete)
#pragma alloc_text(PAGE, FspFsvolDeviceInit)
#pragma alloc_text(PAGE, FspFsvolDeviceFini)
#pragma alloc_text(PAGE, FspFsvolDeviceLockContext)
#pragma alloc_text(PAGE, FspFsvolDeviceUnlockContext)
#pragma alloc_text(PAGE, FspFsvolDeviceLookupContext)
#pragma alloc_text(PAGE, FspFsvolDeviceInsertContext)
#pragma alloc_text(PAGE, FspFsvolDeviceDeleteContext)
#pragma alloc_text(PAGE, FspFsvolDeviceCompareElement)
#pragma alloc_text(PAGE, FspFsvolDeviceAllocateElement)
#pragma alloc_text(PAGE, FspFsvolDeviceFreeElement)
#pragma alloc_text(PAGE, FspDeviceCopyList)
#pragma alloc_text(PAGE, FspDeviceDeleteList)
#pragma alloc_text(PAGE, FspDeviceDeleteAll)
#endif

NTSTATUS FspDeviceCreateSecure(UINT32 Kind, ULONG ExtraSize,
    PUNICODE_STRING DeviceName, DEVICE_TYPE DeviceType,
    PUNICODE_STRING DeviceSddl, LPCGUID DeviceClassGuid,
    PDEVICE_OBJECT *PDeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    ULONG DeviceExtensionSize;
    PDEVICE_OBJECT DeviceObject;
    FSP_DEVICE_EXTENSION *DeviceExtension;

    *PDeviceObject = 0;

    switch (Kind)
    {
    case FspFsvolDeviceExtensionKind:
        DeviceExtensionSize = sizeof(FSP_FSVOL_DEVICE_EXTENSION);
        break;
    case FspFsvrtDeviceExtensionKind:
    case FspFsctlDeviceExtensionKind:
        DeviceExtensionSize = sizeof(FSP_DEVICE_EXTENSION);
        break;
    default:
        ASSERT(0);
        return STATUS_INVALID_PARAMETER;
    }

    if (0 != DeviceSddl)
        Result = IoCreateDeviceSecure(FspDriverObject,
            DeviceExtensionSize + ExtraSize, DeviceName, DeviceType,
            FILE_DEVICE_SECURE_OPEN, FALSE,
            DeviceSddl, DeviceClassGuid,
            &DeviceObject);
    else
        Result = IoCreateDevice(FspDriverObject,
            DeviceExtensionSize + ExtraSize, DeviceName, DeviceType,
            0, FALSE,
            &DeviceObject);
    if (!NT_SUCCESS(Result))
        return Result;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeInitializeSpinLock(&DeviceExtension->SpinLock);
    DeviceExtension->RefCount = 1;
    DeviceExtension->Kind = Kind;

    *PDeviceObject = DeviceObject;

    return Result;
}

NTSTATUS FspDeviceCreate(UINT32 Kind, ULONG ExtraSize,
    DEVICE_TYPE DeviceType,
    PDEVICE_OBJECT *PDeviceObject)
{
    PAGED_CODE();

    return FspDeviceCreateSecure(Kind, ExtraSize, 0, DeviceType, 0, 0, PDeviceObject);
}

NTSTATUS FspDeviceInitialize(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_DEVICE_EXTENSION *DeviceExtension = FspDeviceExtension(DeviceObject);

    switch (DeviceExtension->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        Result = FspFsvolDeviceInit(DeviceObject);
        break;
    case FspFsvrtDeviceExtensionKind:
    case FspFsctlDeviceExtensionKind:
        Result = STATUS_SUCCESS;
        break;
    default:
        ASSERT(0);
        return STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(Result))
        ClearFlag(DeviceObject->Flags, DO_DEVICE_INITIALIZING);

    return Result;
}

VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_DEVICE_EXTENSION *DeviceExtension = FspDeviceExtension(DeviceObject);

    switch (DeviceExtension->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FspFsvolDeviceFini(DeviceObject);
        break;
    case FspFsvrtDeviceExtensionKind:
    case FspFsctlDeviceExtensionKind:
        break;
    default:
        ASSERT(0);
        return;
    }

    IoDeleteDevice(DeviceObject);
}

BOOLEAN FspDeviceRetain(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Result;
    FSP_DEVICE_EXTENSION *DeviceExtension;
    KIRQL Irql;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);
    Result = 0 != DeviceExtension->RefCount;
    if (Result)
        DeviceExtension->RefCount++;
    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    return Result;
}

VOID FspDeviceRelease(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Delete = FALSE;
    FSP_DEVICE_EXTENSION *DeviceExtension;
    KIRQL Irql;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLock(&DeviceExtension->SpinLock, &Irql);
    if (0 != DeviceExtension->RefCount)
    {
        DeviceExtension->RefCount--;
        Delete = 0 == DeviceExtension->RefCount;
    }
    KeReleaseSpinLock(&DeviceExtension->SpinLock, Irql);

    if (Delete)
        FspDeviceDelete(DeviceObject);
}

_IRQL_requires_(DISPATCH_LEVEL)
static BOOLEAN FspDeviceRetainAtDpcLevel(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Result;
    FSP_DEVICE_EXTENSION *DeviceExtension;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);
    Result = 0 != DeviceExtension->RefCount;
    if (Result)
        DeviceExtension->RefCount++;
    KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

    return Result;
}

_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspDeviceReleaseFromDpcLevel(PDEVICE_OBJECT DeviceObject)
{
    // !PAGED_CODE();

    BOOLEAN Delete = FALSE;
    FSP_DEVICE_EXTENSION *DeviceExtension;

    DeviceExtension = FspDeviceExtension(DeviceObject);
    KeAcquireSpinLockAtDpcLevel(&DeviceExtension->SpinLock);
    if (0 != DeviceExtension->RefCount)
    {
        DeviceExtension->RefCount--;
        Delete = 0 == DeviceExtension->RefCount;
    }
    KeReleaseSpinLockFromDpcLevel(&DeviceExtension->SpinLock);

    ASSERT(!Delete);
}

static NTSTATUS FspFsvolDeviceInit(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    LARGE_INTEGER IrpTimeout;

    /*
     * Volume device initialization is a mess, because of the different ways of
     * creating/initializing different resources. So we will use some bits just
     * to track what has been initialized!
     */

    /* is there a virtual disk? */
    if (0 != FsvolDeviceExtension->FsvrtDeviceObject)
    {
        /* allocate a spare VPB so that we can be mounted on the virtual disk */
        FsvolDeviceExtension->SwapVpb = FspAllocNonPagedExternal(sizeof *FsvolDeviceExtension->SwapVpb);
        if (0 == FsvolDeviceExtension->SwapVpb)
            return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(FsvolDeviceExtension->SwapVpb, sizeof *FsvolDeviceExtension->SwapVpb);

        /* reference the virtual disk device so that it will not go away while we are using it */
        ObReferenceObject(FsvolDeviceExtension->FsvrtDeviceObject);
        FsvolDeviceExtension->InitDoneFsvrt = 1;
    }

    /* initialize our delete lock */
    ExInitializeResourceLite(&FsvolDeviceExtension->DeleteResource);
    FsvolDeviceExtension->InitDoneDelRsc = 1;

    /* create our Ioq */
    IrpTimeout.QuadPart = FsvolDeviceExtension->VolumeParams.IrpTimeout * 10000;
        /* convert millis to nanos */
    Result = FspIoqCreate(
        FsvolDeviceExtension->VolumeParams.IrpCapacity, &IrpTimeout, FspIopCompleteCanceledIrp,
        &FsvolDeviceExtension->Ioq);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneIoq = 1;

    /* initialize our generic table */
    ExInitializeFastMutex(&FsvolDeviceExtension->GenericTableFastMutex);
    RtlInitializeGenericTableAvl(&FsvolDeviceExtension->GenericTable,
        FspFsvolDeviceCompareElement, FspFsvolDeviceAllocateElement, FspFsvolDeviceFreeElement, 0);
    FsvolDeviceExtension->InitDoneGenTab = 1;

    /* initialize our timer routine and start our expiration timer */
#pragma prefast(suppress:28133, "We are a filesystem: we do not have AddDevice")
    Result = IoInitializeTimer(DeviceObject, FspFsvolDeviceTimerRoutine, 0);
    if (!NT_SUCCESS(Result))
        return Result;
    KeInitializeSpinLock(&FsvolDeviceExtension->ExpirationLock);
    ExInitializeWorkItem(&FsvolDeviceExtension->ExpirationWorkItem,
        FspFsvolDeviceExpirationRoutine, DeviceObject);
    IoStartTimer(DeviceObject);
    FsvolDeviceExtension->InitDoneTimer = 1;

    return STATUS_SUCCESS;
}

static VOID FspFsvolDeviceFini(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    /*
     * First things first: stop our timer.
     *
     * Our IoTimer routine will NOT be called again after IoStopTimer() returns.
     * However a work item may be in flight. For this reason our IoTimer routine
     * retains our DeviceObject before queueing work items.
     */
    if (FsvolDeviceExtension->InitDoneTimer)
        IoStopTimer(DeviceObject);

#if 0
    /* FspDeviceFreeElement is now a no-op, so this is no longer necessary */
    /*
     * Enumerate and delete all entries in the GenericTable.
     * There is no need to protect accesses to the table as we are in the device destructor.
     */
    if (FsvolDeviceExtension->InitDoneGenTab)
    {
        FSP_DEVICE_GENERIC_TABLE_ELEMENT_DATA *Element;
        while (0 != (Element = RtlGetElementGenericTableAvl(&FsvolDeviceExtension->GenericTable, 0)))
            RtlDeleteElementGenericTableAvl(&FsvolDeviceExtension->GenericTable, &Element->Identifier);
    }
#endif

    /* delete the Ioq */
    if (FsvolDeviceExtension->InitDoneIoq)
        FspIoqDelete(FsvolDeviceExtension->Ioq);

    /* finalize our delete lock */
    if (FsvolDeviceExtension->InitDoneDelRsc)
        ExDeleteResourceLite(&FsvolDeviceExtension->DeleteResource);

    /* is there a virtual disk? */
    if (FsvolDeviceExtension->InitDoneFsvrt)
    {
        /* dereference the virtual volume device so that it can now go away */
        ObDereferenceObject(FsvolDeviceExtension->FsvrtDeviceObject);

        /* free the spare VPB if we still have it */
        if (0 != FsvolDeviceExtension->SwapVpb)
            FspFreeExternal(FsvolDeviceExtension->SwapVpb);
    }
}

static VOID FspFsvolDeviceTimerRoutine(PDEVICE_OBJECT DeviceObject, PVOID Context)
{
    // !PAGED_CODE();

    /*
     * This routine runs at DPC level. Retain our DeviceObject and queue a work item
     * so that we can do our processing at Passive level. Only do so if the work item
     * is not already in flight (otherwise we could requeue the same work item).
     */

    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    
    if (!FspDeviceRetainAtDpcLevel(DeviceObject))
        return;

    BOOLEAN ExpirationInProgress;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    KeAcquireSpinLockAtDpcLevel(&FsvolDeviceExtension->ExpirationLock);
    ExpirationInProgress = FsvolDeviceExtension->ExpirationInProgress;
    if (!ExpirationInProgress)
    {
        FsvolDeviceExtension->ExpirationInProgress = TRUE;
        ExQueueWorkItem(&FsvolDeviceExtension->ExpirationWorkItem, DelayedWorkQueue);
    }
    KeReleaseSpinLockFromDpcLevel(&FsvolDeviceExtension->ExpirationLock);

    if (ExpirationInProgress)
        FspDeviceReleaseFromDpcLevel(DeviceObject);
}

static VOID FspFsvolDeviceExpirationRoutine(PVOID Context)
{
    // !PAGED_CODE();

    PDEVICE_OBJECT DeviceObject = Context;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    KIRQL Irql;

    FspIoqRemoveExpired(FsvolDeviceExtension->Ioq);

    KeAcquireSpinLock(&FsvolDeviceExtension->ExpirationLock, &Irql);
    FsvolDeviceExtension->ExpirationInProgress = FALSE;
    KeReleaseSpinLock(&FsvolDeviceExtension->ExpirationLock, Irql);

    FspDeviceRelease(DeviceObject);
}

_IRQL_raises_(APC_LEVEL)
_IRQL_saves_global_(OldIrql, DeviceObject)
VOID FspFsvolDeviceLockContext(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    ExAcquireFastMutex(&FsvolDeviceExtension->GenericTableFastMutex);
}

_IRQL_requires_(APC_LEVEL)
_IRQL_restores_global_(OldIrql, DeviceObject)
VOID FspFsvolDeviceUnlockContext(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    ExReleaseFastMutex(&FsvolDeviceExtension->GenericTableFastMutex);
}

PVOID FspFsvolDeviceLookupContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_GENERIC_TABLE_ELEMENT_DATA *Result;

    Result = RtlLookupElementGenericTableAvl(&FsvolDeviceExtension->GenericTable, &Identifier);

    return 0 != Result ? Result->Context : 0;
}

PVOID FspFsvolDeviceInsertContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier, PVOID Context,
    FSP_DEVICE_GENERIC_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_GENERIC_TABLE_ELEMENT_DATA *Result, Element = { 0 };

    ASSERT(0 != ElementStorage);
    Element.Identifier = Identifier;
    Element.Context = Context;

    FsvolDeviceExtension->GenericTableElementStorage = ElementStorage;
    Result = RtlInsertElementGenericTableAvl(&FsvolDeviceExtension->GenericTable,
        &Element, sizeof Element, PInserted);
    FsvolDeviceExtension->GenericTableElementStorage = 0;

    ASSERT(0 != Result);

    return Result->Context;
}

VOID FspFsvolDeviceDeleteContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier,
    PBOOLEAN PDeleted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    BOOLEAN Deleted;

    Deleted = RtlDeleteElementGenericTableAvl(&FsvolDeviceExtension->GenericTable, &Identifier);

    if (0 != PDeleted)
        *PDeleted = Deleted;
}

static RTL_GENERIC_COMPARE_RESULTS NTAPI FspFsvolDeviceCompareElement(
    PRTL_AVL_TABLE Table, PVOID FirstElement, PVOID SecondElement)
{
    PAGED_CODE();

    if (FirstElement < SecondElement)
        return GenericLessThan;
    else
    if (SecondElement < FirstElement)
        return GenericGreaterThan;
    else
        return GenericEqual;
}

static PVOID NTAPI FspFsvolDeviceAllocateElement(
    PRTL_AVL_TABLE Table, CLONG ByteSize)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        CONTAINING_RECORD(Table, FSP_FSVOL_DEVICE_EXTENSION, GenericTable);

    ASSERT(sizeof(FSP_DEVICE_GENERIC_TABLE_ELEMENT) == ByteSize);

    return FsvolDeviceExtension->GenericTableElementStorage;
}

static VOID NTAPI FspFsvolDeviceFreeElement(
    PRTL_AVL_TABLE Table, PVOID Buffer)
{
    PAGED_CODE();
}

NTSTATUS FspDeviceCopyList(
    PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount)
{
    PAGED_CODE();

    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    while (STATUS_BUFFER_TOO_SMALL == IoEnumerateDeviceObjectList(FspDriverObject,
        DeviceObjects, sizeof *DeviceObjects * DeviceObjectCount, &DeviceObjectCount))
    {
        if (0 != DeviceObjects)
            FspFree(DeviceObjects);
        DeviceObjects = FspAllocNonPaged(sizeof *DeviceObjects * DeviceObjectCount);
        if (0 == DeviceObjects)
            return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(DeviceObjects, sizeof *DeviceObjects * DeviceObjectCount);
    }

    *PDeviceObjects = DeviceObjects;
    *PDeviceObjectCount = DeviceObjectCount;

    return STATUS_SUCCESS;
}

VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount)
{
    PAGED_CODE();

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        ObDereferenceObject(DeviceObjects[i]);

    FspFree(DeviceObjects);
}

VOID FspDeviceDeleteAll(VOID)
{
    PAGED_CODE();

    NTSTATUS Result;
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    Result = FspDeviceCopyList(&DeviceObjects, &DeviceObjectCount);
    if (!NT_SUCCESS(Result))
        return;

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        FspDeviceDelete(DeviceObjects[i]);

    FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
}
