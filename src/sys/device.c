/**
 * @file sys/device.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS FspDeviceCreateList(
    PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount);
VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount);
NTSTATUS FspDeviceOwned(
    PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT DeviceObject);
static VOID FspFsctlDeviceDeleteObject(PDEVICE_OBJECT DeviceObject);
static VOID FspFsvrtDeviceDeleteObject(PDEVICE_OBJECT DeviceObject);
static VOID FspFsvolDeviceDeleteObject(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDeleteObject(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceDeleteObjects(PDRIVER_OBJECT DriverObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspDeviceCreateList)
#pragma alloc_text(PAGE, FspDeviceDeleteList)
#pragma alloc_text(PAGE, FspDeviceOwned)
#pragma alloc_text(PAGE, FspFsctlDeviceDeleteObject)
#pragma alloc_text(PAGE, FspFsvrtDeviceDeleteObject)
#pragma alloc_text(PAGE, FspFsvolDeviceDeleteObject)
#pragma alloc_text(PAGE, FspDeviceDeleteObject)
#pragma alloc_text(PAGE, FspDeviceDeleteObjects)
#endif

NTSTATUS FspDeviceCreateList(
    PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount)
{
    PAGED_CODE();

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

VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount)
{
    PAGED_CODE();

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        ObDereferenceObject(DeviceObjects[i]);

    ExFreePoolWithTag(DeviceObjects, FSP_TAG);
}

NTSTATUS FspDeviceOwned(
    PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_NO_SUCH_DEVICE;
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    Result = FspDeviceCreateList(DriverObject, &DeviceObjects, &DeviceObjectCount);
    if (!NT_SUCCESS(Result))
        return Result;

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        if (DeviceObjects[i] == DeviceObject)
        {
            Result = STATUS_SUCCESS;
            break;
        }

    FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);

    return Result;
}

static VOID FspFsctlDeviceDeleteObject(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    IoDeleteDevice(DeviceObject);
}

static VOID FspFsvrtDeviceDeleteObject(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    IoDeleteDevice(DeviceObject);
}

static VOID FspFsvolDeviceDeleteObject(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    IoDeleteDevice(DeviceObject);
}

VOID FspDeviceDeleteObject(PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FspFsvolDeviceDeleteObject(DeviceObject);
        break;
    case FspFsvrtDeviceExtensionKind:
        FspFsvrtDeviceDeleteObject(DeviceObject);
        break;
    case FspFsctlDeviceExtensionKind:
        FspFsctlDeviceDeleteObject(DeviceObject);
        break;
    }
}

VOID FspDeviceDeleteObjects(PDRIVER_OBJECT DriverObject)
{
    PAGED_CODE();

    NTSTATUS Result;
    PDEVICE_OBJECT *DeviceObjects = 0;
    ULONG DeviceObjectCount = 0;

    Result = FspDeviceCreateList(DriverObject, &DeviceObjects, &DeviceObjectCount);
    if (!NT_SUCCESS(Result))
        return;

    for (ULONG i = 0; DeviceObjectCount > i; i++)
        FspDeviceDeleteObject(DeviceObjects[i]);

    FspDeviceDeleteList(DeviceObjects, DeviceObjectCount);
}
