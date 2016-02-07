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
BOOLEAN FspDeviceReference(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDereference(PDEVICE_OBJECT DeviceObject);
_IRQL_requires_(DISPATCH_LEVEL)
static BOOLEAN FspDeviceReferenceAtDpcLevel(PDEVICE_OBJECT DeviceObject);
_IRQL_requires_(DISPATCH_LEVEL)
static VOID FspDeviceDereferenceFromDpcLevel(PDEVICE_OBJECT DeviceObject);
static NTSTATUS FspFsvolDeviceInit(PDEVICE_OBJECT DeviceObject);
static VOID FspFsvolDeviceFini(PDEVICE_OBJECT DeviceObject);
static IO_TIMER_ROUTINE FspFsvolDeviceTimerRoutine;
static WORKER_THREAD_ROUTINE FspFsvolDeviceExpirationRoutine;
VOID FspFsvolDeviceFileRenameAcquireShared(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameAcquireExclusive(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameSetOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
VOID FspFsvolDeviceFileRenameRelease(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceFileRenameReleaseOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner);
VOID FspFsvolDeviceLockContextTable(PDEVICE_OBJECT DeviceObject);
VOID FspFsvolDeviceUnlockContextTable(PDEVICE_OBJECT DeviceObject);
PVOID FspFsvolDeviceLookupContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier);
PVOID FspFsvolDeviceInsertContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier, PVOID Context,
    FSP_DEVICE_CONTEXT_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted);
VOID FspFsvolDeviceDeleteContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier,
    PBOOLEAN PDeleted);
static RTL_AVL_COMPARE_ROUTINE FspFsvolDeviceCompareContext;
static RTL_AVL_ALLOCATE_ROUTINE FspFsvolDeviceAllocateContext;
static RTL_AVL_FREE_ROUTINE FspFsvolDeviceFreeContext;
PVOID FspFsvolDeviceLookupContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName);
PVOID FspFsvolDeviceLookupChildContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName);
PVOID FspFsvolDeviceInsertContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName, PVOID Context,
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted);
VOID FspFsvolDeviceDeleteContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    PBOOLEAN PDeleted);
static RTL_AVL_COMPARE_ROUTINE FspFsvolDeviceCompareContextByName;
static RTL_AVL_ALLOCATE_ROUTINE FspFsvolDeviceAllocateContextByName;
static RTL_AVL_FREE_ROUTINE FspFsvolDeviceFreeContextByName;
VOID FspFsvolGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
BOOLEAN FspFsvolTryGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo);
VOID FspFsvolSetVolumeInfo(PDEVICE_OBJECT DeviceObject, const FSP_FSCTL_VOLUME_INFO *VolumeInfo);
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
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameAcquireShared)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameAcquireExclusive)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameSetOwner)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameRelease)
#pragma alloc_text(PAGE, FspFsvolDeviceFileRenameReleaseOwner)
#pragma alloc_text(PAGE, FspFsvolDeviceLockContextTable)
#pragma alloc_text(PAGE, FspFsvolDeviceUnlockContextTable)
#pragma alloc_text(PAGE, FspFsvolDeviceLookupContext)
#pragma alloc_text(PAGE, FspFsvolDeviceInsertContext)
#pragma alloc_text(PAGE, FspFsvolDeviceDeleteContext)
#pragma alloc_text(PAGE, FspFsvolDeviceCompareContext)
#pragma alloc_text(PAGE, FspFsvolDeviceAllocateContext)
#pragma alloc_text(PAGE, FspFsvolDeviceFreeContext)
#pragma alloc_text(PAGE, FspFsvolDeviceLookupContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceLookupChildContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceInsertContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceDeleteContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceCompareContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceAllocateContextByName)
#pragma alloc_text(PAGE, FspFsvolDeviceFreeContextByName)
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

BOOLEAN FspDeviceReference(PDEVICE_OBJECT DeviceObject)
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

VOID FspDeviceDereference(PDEVICE_OBJECT DeviceObject)
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
static BOOLEAN FspDeviceReferenceAtDpcLevel(PDEVICE_OBJECT DeviceObject)
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
static VOID FspDeviceDereferenceFromDpcLevel(PDEVICE_OBJECT DeviceObject)
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
    IrpTimeout.QuadPart = FsvolDeviceExtension->VolumeParams.IrpTimeout * 10000ULL;
        /* convert millis to nanos */
    Result = FspIoqCreate(
        FsvolDeviceExtension->VolumeParams.IrpCapacity, &IrpTimeout, FspIopCompleteCanceledIrp,
        &FsvolDeviceExtension->Ioq);
    if (!NT_SUCCESS(Result))
        return Result;
    FsvolDeviceExtension->InitDoneIoq = 1;

    /* initialize our context table */
    ExInitializeResourceLite(&FsvolDeviceExtension->FileRenameResource);
    ExInitializeResourceLite(&FsvolDeviceExtension->ContextTableResource);
    RtlInitializeGenericTableAvl(&FsvolDeviceExtension->ContextTable,
        FspFsvolDeviceCompareContext,
        FspFsvolDeviceAllocateContext,
        FspFsvolDeviceFreeContext,
        0);
    RtlInitializeGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable,
        FspFsvolDeviceCompareContextByName,
        FspFsvolDeviceAllocateContextByName,
        FspFsvolDeviceFreeContextByName,
        0);
    FsvolDeviceExtension->InitDoneCtxTab = 1;

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

    /* initialize the volume information */
    KeInitializeSpinLock(&FsvolDeviceExtension->InfoSpinLock);
    FsvolDeviceExtension->InitDoneInfo = 1;

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
     * references our DeviceObject before queueing work items.
     */
    if (FsvolDeviceExtension->InitDoneTimer)
        IoStopTimer(DeviceObject);

    /* delete the Ioq */
    if (FsvolDeviceExtension->InitDoneIoq)
        FspIoqDelete(FsvolDeviceExtension->Ioq);

    if (FsvolDeviceExtension->InitDoneCtxTab)
    {
        /*
         * FspDeviceFreeContext/FspDeviceFreeContextByName is a no-op, so it is not necessary
         * to enumerate and delete all entries in the ContextTable.
         */

        ExDeleteResourceLite(&FsvolDeviceExtension->ContextTableResource);
        ExDeleteResourceLite(&FsvolDeviceExtension->FileRenameResource);
    }

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
     * This routine runs at DPC level. Reference our DeviceObject and queue a work item
     * so that we can do our processing at Passive level. Only do so if the work item
     * is not already in flight (otherwise we could requeue the same work item).
     */

    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    
    if (!FspDeviceReferenceAtDpcLevel(DeviceObject))
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
        FspDeviceDereferenceFromDpcLevel(DeviceObject);
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

    FspDeviceDereference(DeviceObject);
}

VOID FspFsvolDeviceFileRenameAcquireShared(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    ExAcquireResourceSharedLite(&FsvolDeviceExtension->FileRenameResource, TRUE);
}

VOID FspFsvolDeviceFileRenameAcquireExclusive(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->FileRenameResource, TRUE);
}

VOID FspFsvolDeviceFileRenameSetOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    Owner = (PVOID)((UINT_PTR)Owner | 3);

    ExSetResourceOwnerPointer(&FsvolDeviceExtension->FileRenameResource, Owner);
}

VOID FspFsvolDeviceFileRenameRelease(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    ExReleaseResourceLite(&FsvolDeviceExtension->FileRenameResource);
}

VOID FspFsvolDeviceFileRenameReleaseOwner(PDEVICE_OBJECT DeviceObject, PVOID Owner)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);

    Owner = (PVOID)((UINT_PTR)Owner | 3);

    if (ExIsResourceAcquiredLite(&FsvolDeviceExtension->FileRenameResource))
        ExReleaseResourceLite(&FsvolDeviceExtension->FileRenameResource);
    else
        ExReleaseResourceForThreadLite(&FsvolDeviceExtension->FileRenameResource, (ERESOURCE_THREAD)Owner);
}

VOID FspFsvolDeviceLockContextTable(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->ContextTableResource, TRUE);
}

VOID FspFsvolDeviceUnlockContextTable(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    ExReleaseResourceLite(&FsvolDeviceExtension->ContextTableResource);
}

PVOID FspFsvolDeviceLookupContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_CONTEXT_TABLE_ELEMENT_DATA *Result;

    Result = RtlLookupElementGenericTableAvl(&FsvolDeviceExtension->ContextTable, &Identifier);

    return 0 != Result ? Result->Context : 0;
}

PVOID FspFsvolDeviceInsertContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier, PVOID Context,
    FSP_DEVICE_CONTEXT_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_CONTEXT_TABLE_ELEMENT_DATA *Result, Element = { 0 };

    ASSERT(0 != ElementStorage);
    Element.Identifier = Identifier;
    Element.Context = Context;

    FsvolDeviceExtension->ContextTableElementStorage = ElementStorage;
    Result = RtlInsertElementGenericTableAvl(&FsvolDeviceExtension->ContextTable,
        &Element, sizeof Element, PInserted);
    FsvolDeviceExtension->ContextTableElementStorage = 0;

    ASSERT(0 != Result);

    return Result->Context;
}

VOID FspFsvolDeviceDeleteContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier,
    PBOOLEAN PDeleted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    BOOLEAN Deleted;

    Deleted = RtlDeleteElementGenericTableAvl(&FsvolDeviceExtension->ContextTable, &Identifier);

    if (0 != PDeleted)
        *PDeleted = Deleted;
}

static RTL_GENERIC_COMPARE_RESULTS NTAPI FspFsvolDeviceCompareContext(
    PRTL_AVL_TABLE Table, PVOID FirstElement, PVOID SecondElement)
{
    PAGED_CODE();

    UINT64 FirstIdentifier = *(PUINT64)FirstElement;
    UINT64 SecondIdentifier = *(PUINT64)SecondElement;

    if (FirstIdentifier < SecondIdentifier)
        return GenericLessThan;
    else
    if (SecondIdentifier < FirstIdentifier)
        return GenericGreaterThan;
    else
        return GenericEqual;
}

static PVOID NTAPI FspFsvolDeviceAllocateContext(
    PRTL_AVL_TABLE Table, CLONG ByteSize)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        CONTAINING_RECORD(Table, FSP_FSVOL_DEVICE_EXTENSION, ContextTable);

    ASSERT(sizeof(FSP_DEVICE_CONTEXT_TABLE_ELEMENT) == ByteSize);

    return FsvolDeviceExtension->ContextTableElementStorage;
}

static VOID NTAPI FspFsvolDeviceFreeContext(
    PRTL_AVL_TABLE Table, PVOID Buffer)
{
    PAGED_CODE();
}

PVOID FspFsvolDeviceLookupContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA *Result;

    Result = RtlLookupElementGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable, &FileName);

    return 0 != Result ? Result->Context : 0;
}

PVOID FspFsvolDeviceLookupChildContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    BOOLEAN CaseInsensitive = 0 == FsvolDeviceExtension->VolumeParams.CaseSensitiveSearch;
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA *Result;
    PVOID RestartKey;

    Result = RtlLookupFirstMatchingElementGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable,
        &FileName, &RestartKey);
    if (0 == Result)
        return 0;

    Result = RtlEnumerateGenericTableWithoutSplayingAvl(&FsvolDeviceExtension->ContextByNameTable,
        &RestartKey);
    if (0 == Result)
        return 0;

    if (RtlPrefixUnicodeString(FileName, Result->FileName, CaseInsensitive) &&
        FileName->Length < Result->FileName->Length &&
        '\\' == Result->FileName->Buffer[FileName->Length / sizeof(WCHAR)])
        return Result->Context;
    else
        return 0;
}

PVOID FspFsvolDeviceInsertContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName, PVOID Context,
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT *ElementStorage, PBOOLEAN PInserted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT_DATA *Result, Element = { 0 };

    ASSERT(0 != ElementStorage);
    Element.FileName = FileName;
    Element.Context = Context;

    FsvolDeviceExtension->ContextByNameTableElementStorage = ElementStorage;
    Result = RtlInsertElementGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable,
        &Element, sizeof Element, PInserted);
    FsvolDeviceExtension->ContextByNameTableElementStorage = 0;

    ASSERT(0 != Result);

    return Result->Context;
}

VOID FspFsvolDeviceDeleteContextByName(PDEVICE_OBJECT DeviceObject, PUNICODE_STRING FileName,
    PBOOLEAN PDeleted)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    BOOLEAN Deleted;

    Deleted = RtlDeleteElementGenericTableAvl(&FsvolDeviceExtension->ContextByNameTable, &FileName);

    if (0 != PDeleted)
        *PDeleted = Deleted;
}

static RTL_GENERIC_COMPARE_RESULTS NTAPI FspFsvolDeviceCompareContextByName(
    PRTL_AVL_TABLE Table, PVOID FirstElement, PVOID SecondElement)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        CONTAINING_RECORD(Table, FSP_FSVOL_DEVICE_EXTENSION, ContextByNameTable);
    BOOLEAN CaseInsensitive = 0 == FsvolDeviceExtension->VolumeParams.CaseSensitiveSearch;
    PUNICODE_STRING FirstFileName = *(PUNICODE_STRING *)FirstElement;
    PUNICODE_STRING SecondFileName = *(PUNICODE_STRING *)SecondElement;
    LONG ComparisonResult;

    ComparisonResult = RtlCompareUnicodeString(FirstFileName, SecondFileName, CaseInsensitive);

    if (0 > ComparisonResult)
        return GenericLessThan;
    else
    if (0 < ComparisonResult)
        return GenericGreaterThan;
    else
        return GenericEqual;
}

static PVOID NTAPI FspFsvolDeviceAllocateContextByName(
    PRTL_AVL_TABLE Table, CLONG ByteSize)
{
    PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =
        CONTAINING_RECORD(Table, FSP_FSVOL_DEVICE_EXTENSION, ContextByNameTable);

    ASSERT(sizeof(FSP_DEVICE_CONTEXT_BY_NAME_TABLE_ELEMENT) == ByteSize);

    return FsvolDeviceExtension->ContextByNameTableElementStorage;
}

static VOID NTAPI FspFsvolDeviceFreeContextByName(
    PRTL_AVL_TABLE Table, PVOID Buffer)
{
    PAGED_CODE();
}

VOID FspFsvolGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_VOLUME_INFO VolumeInfoNp;
    KIRQL Irql;

    KeAcquireSpinLock(&FsvolDeviceExtension->InfoSpinLock, &Irql);
    VolumeInfoNp = FsvolDeviceExtension->VolumeInfo;
    KeReleaseSpinLock(&FsvolDeviceExtension->InfoSpinLock, Irql);

    *VolumeInfo = VolumeInfoNp;
}

#pragma warning(push)
#pragma warning(disable:4701) /* disable idiotic warning! */
BOOLEAN FspFsvolTryGetVolumeInfo(PDEVICE_OBJECT DeviceObject, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_VOLUME_INFO VolumeInfoNp;
    KIRQL Irql;
    BOOLEAN Result;

    KeAcquireSpinLock(&FsvolDeviceExtension->InfoSpinLock, &Irql);
    if (0 < FsvolDeviceExtension->InfoExpirationTime &&
        KeQueryInterruptTime() < FsvolDeviceExtension->InfoExpirationTime)
    {
        VolumeInfoNp = FsvolDeviceExtension->VolumeInfo;
        Result = TRUE;
    }
    else
        Result = FALSE;
    KeReleaseSpinLock(&FsvolDeviceExtension->InfoSpinLock, Irql);

    if (Result)
        *VolumeInfo = VolumeInfoNp;

    return Result;
}
#pragma warning(pop)

VOID FspFsvolSetVolumeInfo(PDEVICE_OBJECT DeviceObject, const FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    // !PAGED_CODE();

    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    FSP_FSCTL_VOLUME_INFO VolumeInfoNp = *VolumeInfo;
    KIRQL Irql;
    UINT64 FileInfoTimeout = FsvolDeviceExtension->VolumeParams.FileInfoTimeout * 10000ULL;

    KeAcquireSpinLock(&FsvolDeviceExtension->InfoSpinLock, &Irql);
    FsvolDeviceExtension->VolumeInfo = VolumeInfoNp;
    FsvolDeviceExtension->InfoExpirationTime = 0 != FileInfoTimeout ?
        KeQueryInterruptTime() + FileInfoTimeout : 0;
    KeReleaseSpinLock(&FsvolDeviceExtension->InfoSpinLock, Irql);
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
