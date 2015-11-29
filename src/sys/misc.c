/**
 * @file sys/misc.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspCreateGuid(GUID *Guid);
NTSTATUS FspSecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspCreateGuid)
#pragma alloc_text(PAGE, FspSecuritySubjectContextAccessCheck)
#endif

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

NTSTATUS FspSecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_ACCESS_DENIED;
    SECURITY_SUBJECT_CONTEXT SecuritySubjectContext;
    ACCESS_MASK GrantedAccess;

    SeCaptureSubjectContext(&SecuritySubjectContext);
    if (SeAccessCheck(SecurityDescriptor,
        &SecuritySubjectContext, FALSE,
        DesiredAccess, 0, 0, IoGetFileObjectGenericMapping(), AccessMode,
        &GrantedAccess, &Result))
        Result = STATUS_SUCCESS;
    SeReleaseSubjectContext(&SecuritySubjectContext);

    return Result;
}

NTSTATUS FspCreateDeviceObjectList(
    PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount)
{
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    while (STATUS_BUFFER_TOO_SMALL == IoEnumerateDeviceObjectList(DriverObject,
        DeviceObjects, DeviceObjectCount, &DeviceObjectCount))
    {
        if (0 != DeviceObjects)
            ExFreePoolWithTag(DeviceObjects, FSP_TAG);
        DeviceObjects = ExAllocatePoolWithTag(NonPagedPool,
            sizeof *DeviceObjects * DeviceObjectCount, FSP_TAG);
        if (0 == DeviceObjects)
            return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(DeviceObjects, sizeof *DeviceObjects * DeviceObjectCount);
    }

    *PDeviceObjects = DeviceObjects;
    *PDeviceObjectCount = DeviceObjectCount;

    return STATUS_SUCCESS;
}

VOID FspDeleteDeviceObjectList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount)
{
    for (ULONG i = 0; DeviceObjectCount > i; i++)
        ObDereferenceObject(DeviceObjects[i]);

    ExFreePoolWithTag(DeviceObjects, FSP_TAG);
}

NTSTATUS FspLookupDeviceObject(
    PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS Result = STATUS_NO_SUCH_DEVICE;
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    Result = FspCreateDeviceObjectList(DriverObject, &DeviceObjects, &DeviceObjectCount);
    if (!NT_SUCCESS(Result))
        return Result;

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        if (DeviceObjects[i] == DeviceObject)
        {
            Result = STATUS_SUCCESS;
            break;
        }

    FspDeleteDeviceObjectList(DeviceObjects, DeviceObjectCount);

    return Result;
}
