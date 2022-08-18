/**
 * @file sys/devtimer.c
 *
 * @copyright 2015-2022 Bill Zissimopoulos
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

/*
 * IoTimer Emulation.
 *
 * This is required because IoInitializeTimer and friends is missing from Windows on ARM64.
 */

static LIST_ENTRY FspDeviceTimerList;
static KSPIN_LOCK FspDeviceTimerLock;
static KDPC FspDeviceTimerDpc;
static KTIMER FspDeviceTimer;

static KDEFERRED_ROUTINE FspDeviceTimerRoutine;

NTSTATUS FspDeviceInitializeAllTimers(VOID)
{
    LARGE_INTEGER DueTime;
    LONG Period;

    InitializeListHead(&FspDeviceTimerList);
    KeInitializeSpinLock(&FspDeviceTimerLock);

    KeInitializeDpc(&FspDeviceTimerDpc, FspDeviceTimerRoutine, 0);
    KeInitializeTimerEx(&FspDeviceTimer, SynchronizationTimer);

    DueTime.QuadPart = 1000/*ms*/ * -10000;
    Period = 1000/*ms*/;
    KeSetTimerEx(&FspDeviceTimer, DueTime, Period, &FspDeviceTimerDpc);

    return STATUS_SUCCESS;
}

VOID FspDeviceFinalizeAllTimers(VOID)
{
    KeCancelTimer(&FspDeviceTimer);
    KeFlushQueuedDpcs();

#if DBG
    KIRQL Irql;
    KeAcquireSpinLock(&FspDeviceTimerLock, &Irql);
    ASSERT(IsListEmpty(&FspDeviceTimerList));
    KeReleaseSpinLock(&FspDeviceTimerLock, Irql);
#endif
}

static VOID FspDeviceTimerRoutine(
    PKDPC Dpc,
    PVOID DeferredContext,
    PVOID SystemArgument1,
    PVOID SystemArgument2)
{
    FSP_DEVICE_TIMER *Timer;
    PLIST_ENTRY ListEntry;
    KIRQL Irql;

    KeAcquireSpinLock(&FspDeviceTimerLock, &Irql);
    for (ListEntry = FspDeviceTimerList.Flink;
        &FspDeviceTimerList != ListEntry;
        ListEntry = ListEntry->Flink)
    {
        Timer = CONTAINING_RECORD(ListEntry, FSP_DEVICE_TIMER, ListEntry);
        Timer->TimerRoutine(Timer->DeviceObject, Timer->Context);
    }
    KeReleaseSpinLock(&FspDeviceTimerLock, Irql);
}

NTSTATUS FspDeviceInitializeTimer(PDEVICE_OBJECT DeviceObject,
    PIO_TIMER_ROUTINE TimerRoutine, PVOID Context)
{
    FSP_DEVICE_EXTENSION *DeviceExtension;

    DeviceExtension = FspDeviceExtension(DeviceObject);

    DeviceExtension->DeviceTimer.TimerRoutine = TimerRoutine;
    DeviceExtension->DeviceTimer.DeviceObject = DeviceObject;
    DeviceExtension->DeviceTimer.Context = Context;

    return STATUS_SUCCESS;
}

VOID FspDeviceStartTimer(PDEVICE_OBJECT DeviceObject)
{
    FSP_DEVICE_EXTENSION *DeviceExtension;
    KIRQL Irql;

    DeviceExtension = FspDeviceExtension(DeviceObject);

    KeAcquireSpinLock(&FspDeviceTimerLock, &Irql);
    InsertTailList(&FspDeviceTimerList, &DeviceExtension->DeviceTimer.ListEntry);
    KeReleaseSpinLock(&FspDeviceTimerLock, Irql);
}

VOID FspDeviceStopTimer(PDEVICE_OBJECT DeviceObject)
{
    FSP_DEVICE_EXTENSION *DeviceExtension;
    KIRQL Irql;

    DeviceExtension = FspDeviceExtension(DeviceObject);

    KeAcquireSpinLock(&FspDeviceTimerLock, &Irql);
    RemoveEntryList(&DeviceExtension->DeviceTimer.ListEntry);
    KeReleaseSpinLock(&FspDeviceTimerLock, Irql);
}
