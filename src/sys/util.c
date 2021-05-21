/**
 * @file sys/util.c
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

#include <sys/driver.h>

BOOLEAN FspIsNtDdiVersionAvailable(ULONG Version);
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
static NTSTATUS FspSendIrpCompletion(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context0);
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
VOID FspInitializeSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspExecuteSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem);
static WORKER_THREAD_ROUTINE FspExecuteSynchronousWorkItemRoutine;
VOID FspInitializeDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context);
VOID FspQueueDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem, LARGE_INTEGER Delay);
static KDEFERRED_ROUTINE FspQueueDelayedWorkItemDPC;
BOOLEAN FspSafeMdlCheck(PMDL Mdl);
NTSTATUS FspSafeMdlCreate(PMDL UserMdl, LOCK_OPERATION Operation, FSP_SAFE_MDL **PSafeMdl);
VOID FspSafeMdlCopyBack(FSP_SAFE_MDL *SafeMdl);
VOID FspSafeMdlDelete(FSP_SAFE_MDL *SafeMdl);
NTSTATUS FspIrpHook(PIRP Irp, PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID OwnContext);
VOID FspIrpHookReset(PIRP Irp);
PVOID FspIrpHookContext(PVOID Context);
NTSTATUS FspIrpHookNext(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
VOID FspWgroupInitialize(FSP_WGROUP *Wgroup);
VOID FspWgroupIncrement(FSP_WGROUP *Wgroup);
VOID FspWgroupDecrement(FSP_WGROUP *Wgroup);
VOID FspWgroupSignalPermanently(FSP_WGROUP *Wgroup);
NTSTATUS FspWgroupWait(FSP_WGROUP *Wgroup,
    KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER PTimeout);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspIsNtDdiVersionAvailable)
#pragma alloc_text(PAGE, FspCreateGuid)
#pragma alloc_text(PAGE, FspGetDeviceObjectPointer)
#pragma alloc_text(PAGE, FspRegistryGetValue)
#pragma alloc_text(PAGE, FspSendSetInformationIrp)
#pragma alloc_text(PAGE, FspSendQuerySecurityIrp)
#pragma alloc_text(PAGE, FspSendQueryEaIrp)
#pragma alloc_text(PAGE, FspSendMountmgrDeviceControlIrp)
#pragma alloc_text(PAGE, FspBufferUserBuffer)
#pragma alloc_text(PAGE, FspLockUserBuffer)
#pragma alloc_text(PAGE, FspMapLockedPagesInUserMode)
#pragma alloc_text(PAGE, FspCcInitializeCacheMap)
#pragma alloc_text(PAGE, FspCcSetFileSizes)
#pragma alloc_text(PAGE, FspCcCopyRead)
#pragma alloc_text(PAGE, FspCcCopyWrite)
#pragma alloc_text(PAGE, FspCcMdlRead)
#pragma alloc_text(PAGE, FspCcMdlReadComplete)
#pragma alloc_text(PAGE, FspCcPrepareMdlWrite)
#pragma alloc_text(PAGE, FspCcMdlWriteComplete)
#pragma alloc_text(PAGE, FspCcFlushCache)
#pragma alloc_text(PAGE, FspQuerySecurityDescriptorInfo)
#pragma alloc_text(PAGE, FspEaBufferFromOriginatingProcessValidate)
#pragma alloc_text(PAGE, FspEaBufferFromFileSystemValidate)
#pragma alloc_text(PAGE, FspNotifyInitializeSync)
#pragma alloc_text(PAGE, FspNotifyFullChangeDirectory)
#pragma alloc_text(PAGE, FspNotifyFullReportChange)
#pragma alloc_text(PAGE, FspOplockBreakH)
#pragma alloc_text(PAGE, FspCheckOplock)
#pragma alloc_text(PAGE, FspCheckOplockEx)
#pragma alloc_text(PAGE, FspOplockFsctrl)
#pragma alloc_text(PAGE, FspInitializeSynchronousWorkItem)
#pragma alloc_text(PAGE, FspExecuteSynchronousWorkItem)
#pragma alloc_text(PAGE, FspExecuteSynchronousWorkItemRoutine)
#pragma alloc_text(PAGE, FspInitializeDelayedWorkItem)
#pragma alloc_text(PAGE, FspQueueDelayedWorkItem)
#pragma alloc_text(PAGE, FspSafeMdlCheck)
#pragma alloc_text(PAGE, FspSafeMdlCreate)
#pragma alloc_text(PAGE, FspSafeMdlCopyBack)
#pragma alloc_text(PAGE, FspSafeMdlDelete)
#pragma alloc_text(PAGE, FspIrpHook)
#pragma alloc_text(PAGE, FspIrpHookReset)
// !#pragma alloc_text(PAGE, FspIrpHookContext)
// !#pragma alloc_text(PAGE, FspIrpHookNext)
// !#pragma alloc_text(PAGE, FspWgroupInitialize)
// !#pragma alloc_text(PAGE, FspWgroupIncrement)
// !#pragma alloc_text(PAGE, FspWgroupDecrement)
// !#pragma alloc_text(PAGE, FspWgroupSignalPermanently)
// !#pragma alloc_text(PAGE, FspWgroupWait)
#endif

static const LONG Delays[] =
{
     10/*ms*/ * -10000,
     10/*ms*/ * -10000,
     50/*ms*/ * -10000,
     50/*ms*/ * -10000,
    100/*ms*/ * -10000,
    100/*ms*/ * -10000,
    300/*ms*/ * -10000,
};

PVOID FspAllocatePoolMustSucceed(POOL_TYPE PoolType, SIZE_T Size, ULONG Tag)
{
    // !PAGED_CODE();

    PVOID Result;
    LARGE_INTEGER Delay;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = DEBUGTEST(99) ? ExAllocatePoolWithTag(PoolType, Size, Tag) : 0;
        if (0 != Result)
            return Result;

        Delay.QuadPart = n > i ? Delays[i] : Delays[n - 1];
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
    }
}

PVOID FspAllocateIrpMustSucceed(CCHAR StackSize)
{
    // !PAGED_CODE();

    PIRP Result;
    LARGE_INTEGER Delay;

    for (ULONG i = 0, n = sizeof(Delays) / sizeof(Delays[0]);; i++)
    {
        Result = DEBUGTEST(99) ? IoAllocateIrp(StackSize, FALSE) : 0;
        if (0 != Result)
            return Result;

        Delay.QuadPart = n > i ? Delays[i] : Delays[n - 1];
        KeDelayExecutionThread(KernelMode, FALSE, &Delay);
    }
}

BOOLEAN FspIsNtDdiVersionAvailable(ULONG RequestedVersion)
{
    PAGED_CODE();

    static ULONG Version;

    if (0 == Version)
    {
        RTL_OSVERSIONINFOEXW VersionInfo;
        ULONG TempVersion;

        RtlZeroMemory(&VersionInfo, sizeof VersionInfo);
        VersionInfo.dwOSVersionInfoSize = sizeof VersionInfo;
        RtlGetVersion((PVOID)&VersionInfo);

        TempVersion =
            ((UINT8)VersionInfo.dwMajorVersion << 24) |
            ((UINT8)VersionInfo.dwMinorVersion << 16) |
            ((UINT8)VersionInfo.wServicePackMajor << 8);

        if (10 <= VersionInfo.dwMajorVersion)
        {
            /* see https://docs.microsoft.com/en-us/windows/release-information/ */
            static struct
            {
                ULONG BuildNumber;
                ULONG Subver;
            } Builds[] =
            {
                { 10240, SUBVER(NTDDI_WIN10) },
                { 10586, SUBVER(NTDDI_WIN10_TH2) },
                { 14393, SUBVER(NTDDI_WIN10_RS1) },
                { 15063, SUBVER(NTDDI_WIN10_RS2) },
                { 16299, SUBVER(NTDDI_WIN10_RS3) },
                { 17134, SUBVER(NTDDI_WIN10_RS4) },
                { 17763, SUBVER(NTDDI_WIN10_RS5) },
                { 18362, SUBVER(NTDDI_WIN10_19H1) },
                { 18363, SUBVER(NTDDI_WIN10_19H1) },
                { 19041, 9 },
                { (ULONG)-1, 10 },
            };
            int Lo = 0, Hi = sizeof Builds / sizeof Builds[0] - 1, Mi;

            while (Lo <= Hi)
            {
                Mi = (unsigned)(Lo + Hi) >> 1;

                if (Builds[Mi].BuildNumber < VersionInfo.dwBuildNumber)
                    Lo = Mi + 1;
                else if (Builds[Mi].BuildNumber > VersionInfo.dwBuildNumber)
                    Hi = Mi - 1;
                else
                {
                    Lo = Mi;
                    break;
                }
            }
            Mi = Lo;

            TempVersion |= (UINT8)Builds[Mi].Subver;
        }

        /* thread-safe because multiple threads will compute same value */
        InterlockedExchange((PVOID)&Version, TempVersion);
    }

    return RequestedVersion <= Version;
}

NTSTATUS FspCreateGuid(GUID *Guid)
{
    PAGED_CODE();

    NTSTATUS Result;

    int Retries = 3;
    do
    {
        Result = ExUuidCreate(Guid);
    } while (!NT_SUCCESS(Result) && 0 < --Retries);

    return Result;
}

NTSTATUS FspGetDeviceObjectPointer(PUNICODE_STRING ObjectName, ACCESS_MASK DesiredAccess,
    PULONG PFileNameIndex, PFILE_OBJECT *PFileObject, PDEVICE_OBJECT *PDeviceObject)
{
    PAGED_CODE();

    UNICODE_STRING PartialName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE Handle;
    NTSTATUS Result;

    PartialName.Length = 0;
    PartialName.MaximumLength = ObjectName->Length;
    PartialName.Buffer = ObjectName->Buffer;

    Result = STATUS_NO_SUCH_DEVICE;
    while (PartialName.MaximumLength > PartialName.Length)
    {
        while (PartialName.MaximumLength > PartialName.Length &&
            L'\\' == PartialName.Buffer[PartialName.Length / sizeof(WCHAR)])
            PartialName.Length += sizeof(WCHAR);
        while (PartialName.MaximumLength > PartialName.Length &&
            L'\\' != PartialName.Buffer[PartialName.Length / sizeof(WCHAR)])
            PartialName.Length += sizeof(WCHAR);

        Result = IoGetDeviceObjectPointer(&PartialName, DesiredAccess, PFileObject, PDeviceObject);
        if (NT_SUCCESS(Result))
        {
            *PFileNameIndex = PartialName.Length;
            break;
        }

        InitializeObjectAttributes(&ObjectAttributes, &PartialName, OBJ_KERNEL_HANDLE, 0, 0);
        Result = ZwOpenDirectoryObject(&Handle, 0, &ObjectAttributes);
        if (!NT_SUCCESS(Result))
        {
            Result = ZwOpenSymbolicLinkObject(&Handle, 0, &ObjectAttributes);
            if (!NT_SUCCESS(Result))
            {
                Result = STATUS_NO_SUCH_DEVICE;
                break;
            }
        }
        ZwClose(Handle);
    }

    return Result;
}

NTSTATUS FspRegistryGetValue(PUNICODE_STRING Path, PUNICODE_STRING ValueName,
    PKEY_VALUE_PARTIAL_INFORMATION ValueInformation, PULONG PValueInformationLength)
{
    PAGED_CODE();

    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE Handle = 0;
    NTSTATUS Result;

    InitializeObjectAttributes(&ObjectAttributes,
        Path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, 0);

    Result = ZwOpenKey(&Handle, KEY_QUERY_VALUE, &ObjectAttributes);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = ZwQueryValueKey(Handle, ValueName,
        KeyValuePartialInformation, ValueInformation,
        *PValueInformationLength, PValueInformationLength);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = STATUS_SUCCESS;
        /* NOTE: also converts STATUS_BUFFER_OVERFLOW to STATUS_SUCCESS */

exit:
    if (0 != Handle)
        ZwClose(Handle);

    return Result;
}

typedef struct
{
    IO_STATUS_BLOCK IoStatus;
    KEVENT Event;
} FSP_SEND_IRP_CONTEXT;

NTSTATUS FspSendSetInformationIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    FILE_INFORMATION_CLASS FileInformationClass, PVOID FileInformation, ULONG Length)
{
    PAGED_CODE();

    ASSERT(
        FileAllocationInformation == FileInformationClass ||
        FileEndOfFileInformation == FileInformationClass);

    NTSTATUS Result;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;
    FSP_SEND_IRP_CONTEXT Context;

    if (0 == DeviceObject)
        DeviceObject = IoGetRelatedDeviceObject(FileObject);

    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    if (0 == Irp)
        return STATUS_INSUFFICIENT_RESOURCES;

    IrpSp = IoGetNextIrpStackLocation(Irp);
    Irp->RequestorMode = KernelMode;
    Irp->AssociatedIrp.SystemBuffer = FileInformation;
    IrpSp->MajorFunction = IRP_MJ_SET_INFORMATION;
    IrpSp->FileObject = FileObject;
    IrpSp->Parameters.SetFile.FileInformationClass = FileInformationClass;
    IrpSp->Parameters.SetFile.Length = Length;

    KeInitializeEvent(&Context.Event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(Irp, FspSendIrpCompletion, &Context, TRUE, TRUE, TRUE);

    Result = IoCallDriver(DeviceObject, Irp);
    if (STATUS_PENDING == Result)
        KeWaitForSingleObject(&Context.Event, Executive, KernelMode, FALSE, 0);

    return Context.IoStatus.Status;
}

NTSTATUS FspSendQuerySecurityIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PULONG PLength)
{
    PAGED_CODE();

    NTSTATUS Result;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;
    FSP_SEND_IRP_CONTEXT Context;
    ULONG Length = *PLength;

    *PLength = 0;

    if (0 == DeviceObject)
        DeviceObject = IoGetRelatedDeviceObject(FileObject);

    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    if (0 == Irp)
        return STATUS_INSUFFICIENT_RESOURCES;

    IrpSp = IoGetNextIrpStackLocation(Irp);
    Irp->RequestorMode = KernelMode;
    Irp->AssociatedIrp.SystemBuffer = SecurityDescriptor;
    Irp->UserBuffer = SecurityDescriptor;
    IrpSp->MajorFunction = IRP_MJ_QUERY_SECURITY;
    IrpSp->FileObject = FileObject;
    IrpSp->Parameters.QuerySecurity.SecurityInformation = SecurityInformation;
    IrpSp->Parameters.QuerySecurity.Length = Length;

    KeInitializeEvent(&Context.Event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(Irp, FspSendIrpCompletion, &Context, TRUE, TRUE, TRUE);

    Result = IoCallDriver(DeviceObject, Irp);
    if (STATUS_PENDING == Result)
        KeWaitForSingleObject(&Context.Event, Executive, KernelMode, FALSE, 0);

    *PLength = (ULONG)Context.IoStatus.Information;
    return Context.IoStatus.Status;
}

NTSTATUS FspSendQueryEaIrp(PDEVICE_OBJECT DeviceObject, PFILE_OBJECT FileObject,
    PFILE_GET_EA_INFORMATION GetEa, ULONG GetEaLength,
    PFILE_FULL_EA_INFORMATION Ea, PULONG PEaLength)
{
    PAGED_CODE();

    NTSTATUS Result;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;
    FSP_SEND_IRP_CONTEXT Context;
    ULONG EaLength = *PEaLength;

    *PEaLength = 0;

    if (0 == DeviceObject)
        DeviceObject = IoGetRelatedDeviceObject(FileObject);

    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    if (0 == Irp)
        return STATUS_INSUFFICIENT_RESOURCES;

    IrpSp = IoGetNextIrpStackLocation(Irp);
    Irp->RequestorMode = KernelMode;
    Irp->AssociatedIrp.SystemBuffer = Ea;
    Irp->UserBuffer = Ea;
    IrpSp->MajorFunction = IRP_MJ_QUERY_EA;
    IrpSp->FileObject = FileObject;
    IrpSp->Parameters.QueryEa.Length = EaLength;
    IrpSp->Parameters.QueryEa.EaList = GetEa;
    IrpSp->Parameters.QueryEa.EaListLength = GetEaLength;

    KeInitializeEvent(&Context.Event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(Irp, FspSendIrpCompletion, &Context, TRUE, TRUE, TRUE);

    Result = IoCallDriver(DeviceObject, Irp);
    if (STATUS_PENDING == Result)
        KeWaitForSingleObject(&Context.Event, Executive, KernelMode, FALSE, 0);

    *PEaLength = (ULONG)Context.IoStatus.Information;
    return Context.IoStatus.Status;
}

NTSTATUS FspSendMountmgrDeviceControlIrp(ULONG IoControlCode,
    PVOID SystemBuffer, ULONG InputBufferLength, PULONG POutputBufferLength)
{
    PAGED_CODE();

    ASSERT(METHOD_BUFFERED == (IoControlCode & 3));

    NTSTATUS Result;
    UNICODE_STRING DeviceName;
    PFILE_OBJECT FileObject;
    PDEVICE_OBJECT DeviceObject;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;
    FSP_SEND_IRP_CONTEXT Context;
    ULONG OutputBufferLength = 0;

    if (0 == POutputBufferLength)
        POutputBufferLength = &OutputBufferLength;

    RtlInitUnicodeString(&DeviceName, MOUNTMGR_DEVICE_NAME);
    Result = IoGetDeviceObjectPointer(&DeviceName, FILE_READ_ATTRIBUTES,
        &FileObject, &DeviceObject);
    if (!NT_SUCCESS(Result))
        goto exit;

    Irp = IoAllocateIrp(DeviceObject->StackSize, FALSE);
    if (0 == Irp)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    IrpSp = IoGetNextIrpStackLocation(Irp);
    Irp->RequestorMode = KernelMode;
    Irp->AssociatedIrp.SystemBuffer = SystemBuffer;
    IrpSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
    IrpSp->Parameters.DeviceIoControl.OutputBufferLength = *POutputBufferLength;
    IrpSp->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;
    IrpSp->Parameters.DeviceIoControl.IoControlCode = IoControlCode;

    KeInitializeEvent(&Context.Event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(Irp, FspSendIrpCompletion, &Context, TRUE, TRUE, TRUE);

    Result = IoCallDriver(DeviceObject, Irp);
    if (STATUS_PENDING == Result)
        KeWaitForSingleObject(&Context.Event, Executive, KernelMode, FALSE, 0);

    *POutputBufferLength = (ULONG)Context.IoStatus.Information;
    Result = Context.IoStatus.Status;

exit:
    if (0 != FileObject)
        ObDereferenceObject(FileObject);

    return Result;
}

static NTSTATUS FspSendIrpCompletion(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context0)
{
    // !PAGED_CODE();

    FSP_SEND_IRP_CONTEXT *Context = Context0;

    Context->IoStatus = Irp->IoStatus;
    KeSetEvent(&Context->Event, 1, FALSE);

    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS FspBufferUserBuffer(PIRP Irp, ULONG Length, LOCK_OPERATION Operation)
{
    PAGED_CODE();

    if (0 == Length || 0 != Irp->AssociatedIrp.SystemBuffer)
        return STATUS_SUCCESS;

    if (KernelMode == Irp->RequestorMode &&
        (PUINT8)MM_SYSTEM_RANGE_START <= (PUINT8)Irp->UserBuffer)
    {
        Irp->AssociatedIrp.SystemBuffer = Irp->UserBuffer;
        return STATUS_SUCCESS;
    }

    PVOID SystemBuffer = FspAllocNonPagedExternal(Length);
    if (0 == SystemBuffer)
        return STATUS_INSUFFICIENT_RESOURCES;

    if (IoReadAccess == Operation)
    {
        try
        {
            RtlCopyMemory(SystemBuffer, Irp->UserBuffer, Length);
        }
        except (EXCEPTION_EXECUTE_HANDLER)
        {
            FspFree(SystemBuffer);

            NTSTATUS Result = GetExceptionCode();
            return FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
        }
    }
    else
        RtlZeroMemory(SystemBuffer, Length);

    Irp->AssociatedIrp.SystemBuffer = SystemBuffer;
    Irp->Flags |= (IoReadAccess == Operation ? 0 : IRP_INPUT_OPERATION) |
        IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;

    return STATUS_SUCCESS;
}

NTSTATUS FspLockUserBuffer(PIRP Irp, ULONG Length, LOCK_OPERATION Operation)
{
    PAGED_CODE();

    if (0 == Length || 0 != Irp->MdlAddress)
        return STATUS_SUCCESS;

    PMDL Mdl = IoAllocateMdl(Irp->UserBuffer, Length, FALSE, FALSE, 0);
    if (0 == Mdl)
        return STATUS_INSUFFICIENT_RESOURCES;

    try
    {
        MmProbeAndLockPages(Mdl, Irp->RequestorMode, Operation);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        IoFreeMdl(Mdl);

        NTSTATUS Result = GetExceptionCode();
        return FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
    }

    Irp->MdlAddress = Mdl;

    return STATUS_SUCCESS;
}

NTSTATUS FspMapLockedPagesInUserMode(PMDL Mdl, PVOID *PAddress, ULONG ExtraPriorityFlags)
{
    PAGED_CODE();

    try
    {
        *PAddress = MmMapLockedPagesSpecifyCache(Mdl, UserMode, MmCached, 0, FALSE,
            NormalPagePriority | ExtraPriorityFlags);
        return 0 != *PAddress ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        *PAddress = 0;
        return GetExceptionCode();
    }
}

NTSTATUS FspCcInitializeCacheMap(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes,
    BOOLEAN PinAccess, PCACHE_MANAGER_CALLBACKS Callbacks, PVOID CallbackContext)
{
    PAGED_CODE();

    try
    {
        CcInitializeCacheMap(FileObject, FileSizes, PinAccess, Callbacks, CallbackContext);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcSetFileSizes(PFILE_OBJECT FileObject, PCC_FILE_SIZES FileSizes)
{
    PAGED_CODE();

    try
    {
        CcSetFileSizes(FileObject, FileSizes);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcCopyRead(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    BOOLEAN Wait, PVOID Buffer, PIO_STATUS_BLOCK IoStatus)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        BOOLEAN Success = CcCopyRead(FileObject, FileOffset, Length, Wait, Buffer, IoStatus);
        Result = Success ? STATUS_SUCCESS : STATUS_PENDING;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
        IoStatus->Information = 0;
        IoStatus->Status = Result;
    }

    return Result;
}

NTSTATUS FspCcCopyWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    BOOLEAN Wait, PVOID Buffer)
{
    PAGED_CODE();

    try
    {
        BOOLEAN Success = CcCopyWrite(FileObject, FileOffset, Length, Wait, Buffer);
        return Success ? STATUS_SUCCESS : STATUS_PENDING;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcMdlRead(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    PMDL *PMdlChain, PIO_STATUS_BLOCK IoStatus)
{
    PAGED_CODE();

    try
    {
        CcMdlRead(FileObject, FileOffset, Length, PMdlChain, IoStatus);
        return IoStatus->Status;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcMdlReadComplete(PFILE_OBJECT FileObject, PMDL MdlChain)
{
    PAGED_CODE();

    try
    {
        CcMdlReadComplete(FileObject, MdlChain);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcPrepareMdlWrite(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, ULONG Length,
    PMDL *PMdlChain, PIO_STATUS_BLOCK IoStatus)
{
    PAGED_CODE();

    try
    {
        CcPrepareMdlWrite(FileObject, FileOffset, Length, PMdlChain, IoStatus);
        return IoStatus->Status;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        return GetExceptionCode();
    }
}

NTSTATUS FspCcMdlWriteComplete(PFILE_OBJECT FileObject, PLARGE_INTEGER FileOffset, PMDL MdlChain)
{
    PAGED_CODE();

    try
    {
        CcMdlWriteComplete(FileObject, FileOffset, MdlChain);
        return STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        CcMdlWriteAbort(FileObject, MdlChain);
        return GetExceptionCode();
    }
}

NTSTATUS FspCcFlushCache(PSECTION_OBJECT_POINTERS SectionObjectPointer,
    PLARGE_INTEGER FileOffset, ULONG Length, PIO_STATUS_BLOCK IoStatus)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        CcFlushCache(SectionObjectPointer, FileOffset, Length, IoStatus);
        Result = IoStatus->Status;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();

        IoStatus->Information = 0;
        IoStatus->Status = Result;
    }

    return Result;
}

NTSTATUS FspQuerySecurityDescriptorInfo(SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor, PULONG PLength,
    PSECURITY_DESCRIPTOR ObjectsSecurityDescriptor)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        Result = SeQuerySecurityDescriptorInfo(&SecurityInformation,
            SecurityDescriptor, PLength, &ObjectsSecurityDescriptor);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
        Result = FsRtlIsNtstatusExpected(Result) ? STATUS_INVALID_USER_BUFFER : Result;
    }

    return STATUS_BUFFER_TOO_SMALL == Result ? STATUS_BUFFER_OVERFLOW : Result;
}

NTSTATUS FspEaBufferFromOriginatingProcessValidate(
    PFILE_FULL_EA_INFORMATION Buffer,
    ULONG Length,
    PULONG PErrorOffset)
{
    PAGED_CODE();

    NTSTATUS Result;

    *PErrorOffset = 0;

    Result = IoCheckEaBufferValidity(Buffer, Length, PErrorOffset);
    if (!NT_SUCCESS(Result))
        return Result;

    /* check that the EA names are valid */
    for (PFILE_FULL_EA_INFORMATION Ea = Buffer, EaEnd = (PVOID)((PUINT8)Ea + Length);
        EaEnd > Ea; Ea = FSP_NEXT_EA(Ea, EaEnd))
    {
        STRING Name;

        Name.Length = Name.MaximumLength = Ea->EaNameLength;
        Name.Buffer = Ea->EaName;

        if (!FspEaNameIsValid(&Name))
        {
            *PErrorOffset = (ULONG)((PUINT8)Ea - (PUINT8)Buffer);
            return STATUS_INVALID_EA_NAME;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS FspEaBufferFromFileSystemValidate(
    PFILE_FULL_EA_INFORMATION Buffer,
    ULONG Length,
    PULONG PErrorOffset)
{
    PAGED_CODE();

    PFILE_FULL_EA_INFORMATION LastEa = 0;

    *PErrorOffset = 0;

    /* EA buffers from the user mode file system are allowed to have zero length */
    if (0 == Length)
        return STATUS_SUCCESS;

    /* EA buffers from the user mode file system are allowed to end with NextEntryOffset != 0 */
    for (PFILE_FULL_EA_INFORMATION Ea = Buffer, EaEnd = (PVOID)((PUINT8)Ea + Length);
        EaEnd > Ea; Ea = FSP_NEXT_EA(Ea, EaEnd))
        LastEa = Ea;
    if (0 != LastEa)
        LastEa->NextEntryOffset = 0;

    return IoCheckEaBufferValidity(Buffer, Length, PErrorOffset);
}

NTSTATUS FspNotifyInitializeSync(PNOTIFY_SYNC *NotifySync)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        FsRtlNotifyInitializeSync(NotifySync);
        Result = STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    return Result;
}

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
    PSECURITY_SUBJECT_CONTEXT SubjectContext)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        FsRtlNotifyFullChangeDirectory(
            NotifySync,
            NotifyList,
            FsContext,
            FullDirectoryName,
            WatchTree,
            IgnoreBuffer,
            CompletionFilter,
            NotifyIrp,
            TraverseCallback,
            SubjectContext);
        Result = STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    return Result;
}

NTSTATUS FspNotifyFullReportChange(
    PNOTIFY_SYNC NotifySync,
    PLIST_ENTRY NotifyList,
    PSTRING FullTargetName,
    USHORT TargetNameOffset,
    PSTRING StreamName,
    PSTRING NormalizedParentName,
    ULONG FilterMatch,
    ULONG Action,
    PVOID TargetContext)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        FsRtlNotifyFullReportChange(
            NotifySync,
            NotifyList,
            FullTargetName,
            TargetNameOffset,
            StreamName,
            NormalizedParentName,
            FilterMatch,
            Action,
            TargetContext);
        Result = STATUS_SUCCESS;
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    return Result;
}

NTSTATUS FspOplockBreakH(
    POPLOCK Oplock,
    PIRP Irp,
    ULONG Flags,
    PVOID Context,
    POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine,
    POPLOCK_FS_PREPOST_IRP PostIrpRoutine)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        Result = FsRtlOplockBreakH(
            Oplock,
            Irp,
            Flags,
            Context,
            CompletionRoutine,
            PostIrpRoutine);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    return Result;
}

NTSTATUS FspCheckOplock(
    POPLOCK Oplock,
    PIRP Irp,
    PVOID Context,
    POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine,
    POPLOCK_FS_PREPOST_IRP PostIrpRoutine)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        Result = FsRtlCheckOplock(
            Oplock,
            Irp,
            Context,
            CompletionRoutine,
            PostIrpRoutine);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    return Result;
}

NTSTATUS FspCheckOplockEx(
    POPLOCK Oplock,
    PIRP Irp,
    ULONG Flags,
    PVOID Context,
    POPLOCK_WAIT_COMPLETE_ROUTINE CompletionRoutine,
    POPLOCK_FS_PREPOST_IRP PostIrpRoutine)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        Result = FsRtlCheckOplockEx(
            Oplock,
            Irp,
            Flags,
            Context,
            CompletionRoutine,
            PostIrpRoutine);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    return Result;
}

NTSTATUS FspOplockFsctrl(
    POPLOCK Oplock,
    PIRP Irp,
    ULONG OpenCount)
{
    PAGED_CODE();

    NTSTATUS Result;

    try
    {
        Result = FsRtlOplockFsctrl(
            Oplock,
            Irp,
            OpenCount);
    }
    except (EXCEPTION_EXECUTE_HANDLER)
    {
        Result = GetExceptionCode();
    }

    return Result;
}

VOID FspInitializeSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context)
{
    PAGED_CODE();

    KeInitializeEvent(&SynchronousWorkItem->Event, NotificationEvent, FALSE);
    SynchronousWorkItem->Routine = Routine;
    SynchronousWorkItem->Context = Context;
    ExInitializeWorkItem(&SynchronousWorkItem->WorkQueueItem,
        FspExecuteSynchronousWorkItemRoutine, SynchronousWorkItem);
}

VOID FspExecuteSynchronousWorkItem(FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem)
{
    PAGED_CODE();

    ExQueueWorkItem(&SynchronousWorkItem->WorkQueueItem, CriticalWorkQueue);

    NTSTATUS Result;
    Result = KeWaitForSingleObject(&SynchronousWorkItem->Event, Executive, KernelMode, FALSE, 0);
    ASSERT(STATUS_SUCCESS == Result);
}

static VOID FspExecuteSynchronousWorkItemRoutine(PVOID Context)
{
    PAGED_CODE();

    FSP_SYNCHRONOUS_WORK_ITEM *SynchronousWorkItem = Context;
    SynchronousWorkItem->Routine(SynchronousWorkItem->Context);
    KeSetEvent(&SynchronousWorkItem->Event, 1, FALSE);
}

VOID FspInitializeDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem,
    PWORKER_THREAD_ROUTINE Routine, PVOID Context)
{
    PAGED_CODE();

    KeInitializeTimer(&DelayedWorkItem->Timer);
    KeInitializeDpc(&DelayedWorkItem->Dpc, FspQueueDelayedWorkItemDPC, DelayedWorkItem);
    ExInitializeWorkItem(&DelayedWorkItem->WorkQueueItem, Routine, Context);
}

VOID FspQueueDelayedWorkItem(FSP_DELAYED_WORK_ITEM *DelayedWorkItem, LARGE_INTEGER Delay)
{
    PAGED_CODE();

    KeSetTimer(&DelayedWorkItem->Timer, Delay, &DelayedWorkItem->Dpc);
}

static VOID FspQueueDelayedWorkItemDPC(PKDPC Dpc,
    PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
    // !PAGED_CODE();

    FSP_DELAYED_WORK_ITEM *DelayedWorkItem = DeferredContext;

    ExQueueWorkItem(&DelayedWorkItem->WorkQueueItem, DelayedWorkQueue);
}

BOOLEAN FspSafeMdlCheck(PMDL Mdl)
{
    PAGED_CODE();

    return 0 == MmGetMdlByteOffset(Mdl) && 0 == BYTE_OFFSET(MmGetMdlByteCount(Mdl));
}

NTSTATUS FspSafeMdlCreate(PMDL UserMdl, LOCK_OPERATION Operation, FSP_SAFE_MDL **PSafeMdl)
{
    PAGED_CODE();

    *PSafeMdl = 0;

    PVOID VirtualAddress = MmGetSystemAddressForMdlSafe(UserMdl, NormalPagePriority);
    if (0 == VirtualAddress)
        return STATUS_INSUFFICIENT_RESOURCES;

    NTSTATUS Result;
    ULONG ByteCount = MmGetMdlByteCount(UserMdl);
    ULONG PageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, ByteCount);
    FSP_SAFE_MDL *SafeMdl;
    PMDL TempMdl;
    PPFN_NUMBER UserPfnArray, SafePfnArray, TempPfnArray;
    ULONG ByteOffsetBgn0, ByteOffsetEnd0, ByteOffsetEnd1;
    BOOLEAN Buffer0, Buffer1;
    ULONG BufferPageCount;

    ASSERT(0 != PageCount);
    ASSERT(FlagOn(UserMdl->MdlFlags, MDL_SOURCE_IS_NONPAGED_POOL | MDL_PAGES_LOCKED));

    SafeMdl = FspAllocNonPaged(sizeof *SafeMdl);
    if (0 == SafeMdl)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    RtlZeroMemory(SafeMdl, sizeof *SafeMdl);
    SafeMdl->Operation = Operation;

    SafeMdl->Mdl = IoAllocateMdl(VirtualAddress, ByteCount, FALSE, FALSE, 0);
    if (0 == SafeMdl->Mdl)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
#pragma prefast(suppress:28145, "We are a filesystem: ok to access MdlFlags")
    SafeMdl->Mdl->MdlFlags |= MDL_PAGES_LOCKED;
    UserPfnArray = MmGetMdlPfnArray(UserMdl);
    SafePfnArray = MmGetMdlPfnArray(SafeMdl->Mdl);
    RtlCopyMemory(SafePfnArray, UserPfnArray, PageCount * sizeof(PFN_NUMBER));

    /*
     * Possible cases:
     *
     * ----+---------+---------+----
     *
     *     *---------*         +
     *     +    *----*         +
     *     *----*    +         +
     *     +  *---*  +         +
     *     *--------...--------*
     *     +    *---...--------*
     *     *--------...---*    +
     *     +    *---...---*    +
     */
    if (1 == PageCount)
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = BYTE_OFFSET((PUINT8)VirtualAddress + ByteCount - 1) + 1;
        ByteOffsetEnd1 = 0;
        Buffer0 = 0 != ByteOffsetBgn0 || PAGE_SIZE != ByteOffsetEnd0;
        Buffer1 = FALSE;
    }
    else
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = PAGE_SIZE;
        ByteOffsetEnd1 = BYTE_OFFSET((PUINT8)VirtualAddress + ByteCount - 1) + 1;
        Buffer0 = 0 != ByteOffsetBgn0;
        Buffer1 = PAGE_SIZE != ByteOffsetEnd1;
    }
    BufferPageCount = Buffer0 + Buffer1;

    if (0 < BufferPageCount)
    {
        SafeMdl->Buffer = FspAllocNonPaged(PAGE_SIZE * BufferPageCount);
        if (0 == SafeMdl->Buffer)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        TempMdl = IoAllocateMdl(SafeMdl->Buffer, PAGE_SIZE * BufferPageCount, FALSE, FALSE, 0);
        if (0 == TempMdl)
        {
            Result = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }

        MmBuildMdlForNonPagedPool(TempMdl);

        TempPfnArray = MmGetMdlPfnArray(TempMdl);
        if (IoReadAccess == Operation)
        {
            if (Buffer0)
            {
                RtlZeroMemory((PUINT8)SafeMdl->Buffer, ByteOffsetBgn0);
                RtlCopyMemory((PUINT8)SafeMdl->Buffer + ByteOffsetBgn0,
                    (PUINT8)VirtualAddress, ByteOffsetEnd0 - ByteOffsetBgn0);
                RtlZeroMemory((PUINT8)SafeMdl->Buffer + ByteOffsetEnd0, PAGE_SIZE - ByteOffsetEnd0);
                SafePfnArray[0] = TempPfnArray[0];
            }
            if (Buffer1)
            {
                RtlCopyMemory((PUINT8)SafeMdl->Buffer + (BufferPageCount - 1) * PAGE_SIZE,
                    PAGE_ALIGN((PUINT8)VirtualAddress + (PageCount - 1) * PAGE_SIZE), ByteOffsetEnd1);
                RtlZeroMemory((PUINT8)SafeMdl->Buffer + (BufferPageCount - 1) * PAGE_SIZE + ByteOffsetEnd1,
                    PAGE_SIZE - ByteOffsetEnd1);
                SafePfnArray[PageCount - 1] = TempPfnArray[BufferPageCount - 1];
            }
        }
        else
        {
            RtlZeroMemory((PUINT8)SafeMdl->Buffer, PAGE_SIZE * BufferPageCount);
            if (Buffer0)
                SafePfnArray[0] = TempPfnArray[0];
            if (Buffer1)
                SafePfnArray[PageCount - 1] = TempPfnArray[BufferPageCount - 1];
        }

        IoFreeMdl(TempMdl);
    }

    SafeMdl->UserMdl = UserMdl;
    *PSafeMdl = SafeMdl;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result) && 0 != SafeMdl)
    {
        if (0 != SafeMdl->Buffer)
            FspFree(SafeMdl->Buffer);
        if (0 != SafeMdl->Mdl)
            IoFreeMdl(SafeMdl->Mdl);
        FspFree(SafeMdl);
    }

    return Result;
}

VOID FspSafeMdlCopyBack(FSP_SAFE_MDL *SafeMdl)
{
    PAGED_CODE();

    if (IoReadAccess == SafeMdl->Operation)
        return;

    PVOID VirtualAddress = MmGetSystemAddressForMdlSafe(SafeMdl->UserMdl, NormalPagePriority);
    if (0 == VirtualAddress)
        return; /* should never happen, already checked in FspSafeMdlCreate */

    ULONG ByteCount = MmGetMdlByteCount(SafeMdl->UserMdl);
    ULONG PageCount = ADDRESS_AND_SIZE_TO_SPAN_PAGES(VirtualAddress, ByteCount);
    ULONG ByteOffsetBgn0, ByteOffsetEnd0, ByteOffsetEnd1;
    BOOLEAN Buffer0, Buffer1;
    ULONG BufferPageCount;

    /*
     * Possible cases:
     *
     * ----+---------+---------+----
     *
     *     *---------*         +
     *     +    *----*         +
     *     *----*    +         +
     *     +  *---*  +         +
     *     *--------...--------*
     *     +    *---...--------*
     *     *--------...---*    +
     *     +    *---...---*    +
     */
    if (1 == PageCount)
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = BYTE_OFFSET((PUINT8)VirtualAddress + ByteCount - 1) + 1;
        ByteOffsetEnd1 = 0;
        Buffer0 = 0 != ByteOffsetBgn0 || PAGE_SIZE != ByteOffsetEnd0;
        Buffer1 = FALSE;
    }
    else
    {
        ByteOffsetBgn0 = BYTE_OFFSET(VirtualAddress);
        ByteOffsetEnd0 = PAGE_SIZE;
        ByteOffsetEnd1 = BYTE_OFFSET((PUINT8)VirtualAddress + ByteCount - 1) + 1;
        Buffer0 = 0 != ByteOffsetBgn0;
        Buffer1 = PAGE_SIZE != ByteOffsetEnd1;
    }
    BufferPageCount = Buffer0 + Buffer1;

    if (0 < BufferPageCount)
    {
        if (Buffer0)
            RtlCopyMemory((PUINT8)VirtualAddress,
                (PUINT8)SafeMdl->Buffer + ByteOffsetBgn0, ByteOffsetEnd0 - ByteOffsetBgn0);
        if (Buffer1)
            RtlCopyMemory(PAGE_ALIGN((PUINT8)VirtualAddress + (PageCount - 1) * PAGE_SIZE),
                (PUINT8)SafeMdl->Buffer + (BufferPageCount - 1) * PAGE_SIZE, ByteOffsetEnd1);
    }
}

VOID FspSafeMdlDelete(FSP_SAFE_MDL *SafeMdl)
{
    PAGED_CODE();

    if (0 != SafeMdl->Buffer)
        FspFree(SafeMdl->Buffer);
    if (0 != SafeMdl->Mdl)
        IoFreeMdl(SafeMdl->Mdl);
    FspFree(SafeMdl);
}

typedef struct
{
    PIO_COMPLETION_ROUTINE CompletionRoutine;
    PVOID Context;
    ULONG Control;
    PVOID OwnContext;
} FSP_IRP_HOOK_CONTEXT;

NTSTATUS FspIrpHook(PIRP Irp, PIO_COMPLETION_ROUTINE CompletionRoutine, PVOID OwnContext)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FSP_IRP_HOOK_CONTEXT *HookContext = 0;

    if (0 != IrpSp->CompletionRoutine || 0 != OwnContext)
    {
        HookContext = FspAllocNonPaged(sizeof *HookContext);
        if (0 == HookContext)
            return STATUS_INSUFFICIENT_RESOURCES;

        HookContext->CompletionRoutine = IrpSp->CompletionRoutine;
        HookContext->Context = IrpSp->Context;
        HookContext->Control = FlagOn(IrpSp->Control,
            SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);
        HookContext->OwnContext = OwnContext;
    }

    IrpSp->CompletionRoutine = CompletionRoutine;
    IrpSp->Context = HookContext;
    SetFlag(IrpSp->Control, SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);

    return STATUS_SUCCESS;
}

VOID FspIrpHookReset(PIRP Irp)
{
    PAGED_CODE();

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
    FSP_IRP_HOOK_CONTEXT *HookContext = IrpSp->Context;

    if (0 != HookContext)
    {
        IrpSp->CompletionRoutine = HookContext->CompletionRoutine;
        IrpSp->Context = HookContext->Context;
        ClearFlag(IrpSp->Control, SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);
        SetFlag(IrpSp->Control, HookContext->Control);

        FspFree(HookContext);
    }
    else
    {
        IrpSp->CompletionRoutine = 0;
        IrpSp->Context = 0;
        ClearFlag(IrpSp->Control, SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);
    }
}

PVOID FspIrpHookContext(PVOID Context)
{
    // !PAGED_CODE();

    FSP_IRP_HOOK_CONTEXT *HookContext = Context;
    return 0 != HookContext ? HookContext->OwnContext : 0;
}

NTSTATUS FspIrpHookNext(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    // !PAGED_CODE();

    FSP_IRP_HOOK_CONTEXT *HookContext = Context;
    NTSTATUS Result;

    if (0 != HookContext && 0 != HookContext->CompletionRoutine && (
        (NT_SUCCESS(Irp->IoStatus.Status) && FlagOn(HookContext->Control, SL_INVOKE_ON_SUCCESS)) ||
        (!NT_SUCCESS(Irp->IoStatus.Status) && FlagOn(HookContext->Control, SL_INVOKE_ON_ERROR)) ||
        (Irp->Cancel && FlagOn(HookContext->Control, SL_INVOKE_ON_CANCEL))))
    {
        Result = HookContext->CompletionRoutine(DeviceObject, Irp, HookContext->Context);
    }
    else
    {
        if (Irp->PendingReturned && Irp->CurrentLocation <= Irp->StackCount)
            IoMarkIrpPending(Irp);

        Result = STATUS_SUCCESS;
    }

    if (0 != HookContext)
        FspFree(HookContext);

    return Result;
}

VOID FspWgroupInitialize(FSP_WGROUP *Wgroup)
{
    // !PAGED_CODE();

    KeInitializeEvent(&Wgroup->Event, NotificationEvent, TRUE);
    Wgroup->Count = 0;
    KeInitializeSpinLock(&Wgroup->SpinLock);
}

VOID FspWgroupIncrement(FSP_WGROUP *Wgroup)
{
    // !PAGED_CODE();

    KIRQL Irql;

    KeAcquireSpinLock(&Wgroup->SpinLock, &Irql);
    if (0 <= Wgroup->Count && 1 == ++Wgroup->Count)
        KeClearEvent(&Wgroup->Event);
    KeReleaseSpinLock(&Wgroup->SpinLock, Irql);
}

VOID FspWgroupDecrement(FSP_WGROUP *Wgroup)
{
    // !PAGED_CODE();

    KIRQL Irql;

    KeAcquireSpinLock(&Wgroup->SpinLock, &Irql);
    if (0 < Wgroup->Count && 0 == --Wgroup->Count)
        KeSetEvent(&Wgroup->Event, 1, FALSE);
    KeReleaseSpinLock(&Wgroup->SpinLock, Irql);
}

VOID FspWgroupSignalPermanently(FSP_WGROUP *Wgroup)
{
    // !PAGED_CODE();

    KIRQL Irql;

    KeAcquireSpinLock(&Wgroup->SpinLock, &Irql);
    Wgroup->Count = -1;
    KeSetEvent(&Wgroup->Event, 1, FALSE);
    KeReleaseSpinLock(&Wgroup->SpinLock, Irql);
}

NTSTATUS FspWgroupWait(FSP_WGROUP *Wgroup,
    KPROCESSOR_MODE WaitMode, BOOLEAN Alertable, PLARGE_INTEGER PTimeout)
{
    // !PAGED_CODE();

    return KeWaitForSingleObject(&Wgroup->Event, Executive, WaitMode, Alertable, PTimeout);
}
