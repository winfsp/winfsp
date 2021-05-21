/**
 * @file sys/silo.c
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

NTSTATUS FspSiloInitialize(FSP_SILO_INIT_CALLBACK Init, FSP_SILO_FINI_CALLBACK Fini);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, FspSiloInitialize)
#endif

typedef PEJOB FSP_PESILO;
typedef PVOID FSP_PSILO_MONITOR;

typedef NTSTATUS (NTAPI *FSP_SILO_MONITOR_CREATE_CALLBACK)(FSP_PESILO Silo);
typedef VOID (NTAPI *FSP_SILO_MONITOR_TERMINATE_CALLBACK)(FSP_PESILO Silo);
typedef VOID (NTAPI *FSP_SILO_CONTEXT_CLEANUP_CALLBACK)(PVOID SiloContext);

#pragma warning(push)
#pragma warning(disable:4201)           /* nameless struct/union */
typedef struct _FSP_SILO_MONITOR_REGISTRATION
{
    UCHAR Version;
    BOOLEAN MonitorHost;
    BOOLEAN MonitorExistingSilos;
    UCHAR Reserved[5];
    union
    {
        PUNICODE_STRING DriverObjectName;
        PUNICODE_STRING ComponentName;
    };
    FSP_SILO_MONITOR_CREATE_CALLBACK CreateCallback;
    FSP_SILO_MONITOR_TERMINATE_CALLBACK TerminateCallback;
} FSP_SILO_MONITOR_REGISTRATION;
#pragma warning(pop)

typedef NTSTATUS FSP_SILO_PsRegisterSiloMonitor(
    FSP_SILO_MONITOR_REGISTRATION *Registration,
    FSP_PSILO_MONITOR *ReturnedMonitor);
typedef NTSTATUS FSP_SILO_PsStartSiloMonitor(
    FSP_PSILO_MONITOR Monitor);
typedef VOID FSP_SILO_PsUnregisterSiloMonitor(
    FSP_PSILO_MONITOR Monitor);
typedef ULONG FSP_SILO_PsGetSiloMonitorContextSlot(
    FSP_PSILO_MONITOR Monitor);
typedef NTSTATUS FSP_SILO_PsCreateSiloContext(
    FSP_PESILO Silo,
    ULONG Size,
    POOL_TYPE PoolType,
    FSP_SILO_CONTEXT_CLEANUP_CALLBACK ContextCleanupCallback,
    PVOID *ReturnedSiloContext);
typedef VOID FSP_SILO_PsDereferenceSiloContext(
    PVOID SiloContext);
typedef NTSTATUS FSP_SILO_PsInsertSiloContext(
    FSP_PESILO Silo,
    ULONG ContextSlot,
    PVOID SiloContext);
typedef NTSTATUS FSP_SILO_PsRemoveSiloContext(
    FSP_PESILO Silo,
    ULONG ContextSlot,
    PVOID *RemovedSiloContext);
typedef NTSTATUS FSP_SILO_PsGetSiloContext(
    FSP_PESILO Silo,
    ULONG ContextSlot,
    PVOID *ReturnedSiloContext);
typedef FSP_PESILO FSP_SILO_PsAttachSiloToCurrentThread(
    FSP_PESILO Silo);
typedef VOID FSP_SILO_PsDetachSiloFromCurrentThread(
    FSP_PESILO PreviousSilo);
typedef FSP_PESILO FSP_SILO_PsGetCurrentServerSilo(
    VOID);
typedef GUID* FSP_SILO_PsGetSiloContainerId(
    FSP_PESILO Silo);

static FSP_SILO_PsRegisterSiloMonitor *FspSiloPsRegisterSiloMonitor;
static FSP_SILO_PsStartSiloMonitor *FspSiloPsStartSiloMonitor;
static FSP_SILO_PsUnregisterSiloMonitor *FspSiloPsUnregisterSiloMonitor;
static FSP_SILO_PsGetSiloMonitorContextSlot *FspSiloPsGetSiloMonitorContextSlot;
static FSP_SILO_PsCreateSiloContext *FspSiloPsCreateSiloContext;
static FSP_SILO_PsDereferenceSiloContext *FspSiloPsDereferenceSiloContext;
static FSP_SILO_PsInsertSiloContext *FspSiloPsInsertSiloContext;
static FSP_SILO_PsRemoveSiloContext *FspSiloPsRemoveSiloContext;
static FSP_SILO_PsGetSiloContext *FspSiloPsGetSiloContext;
static FSP_SILO_PsAttachSiloToCurrentThread *FspSiloPsAttachSiloToCurrentThread;
static FSP_SILO_PsDetachSiloFromCurrentThread *FspSiloPsDetachSiloFromCurrentThread;
static FSP_SILO_PsGetCurrentServerSilo *FspSiloPsGetCurrentServerSilo;
static FSP_SILO_PsGetSiloContainerId *FspSiloPsGetSiloContainerId;
static FSP_PSILO_MONITOR FspSiloMonitor;
static FSP_SILO_INIT_CALLBACK FspSiloInitCallback;
static FSP_SILO_FINI_CALLBACK FspSiloFiniCallback;
static BOOLEAN FspSiloInitDone = FALSE;

static FSP_SILO_GLOBALS FspSiloHostGlobals;

#define FSP_SILO_MONITOR_REGISTRATION_VERSION 1

#define LOAD(n)                         \
    {                                   \
        UNICODE_STRING Name;            \
        RtlInitUnicodeString(&Name, L"" #n);\
        FspSilo ## n =                  \
            (FSP_SILO_ ## n *)(UINT_PTR)MmGetSystemRoutineAddress(&Name);\
        if (0 == FspSilo ## n)          \
            Fail++;                     \
    }
#define CALL(n)                         (FspSilo ## n)

NTSTATUS FspSiloGetGlobals(FSP_SILO_GLOBALS **PGlobals)
{
    FSP_PESILO Silo;
    ULONG ContextSlot;
    FSP_SILO_GLOBALS *Globals;
    NTSTATUS Result;

    if (!FspSiloInitDone ||
        0 == (Silo = CALL(PsGetCurrentServerSilo)()))
    {
        *PGlobals = &FspSiloHostGlobals;
        return STATUS_SUCCESS;
    }

    ContextSlot = CALL(PsGetSiloMonitorContextSlot)(FspSiloMonitor);

    Result = CALL(PsGetSiloContext)(Silo, ContextSlot, &Globals);
    if (!NT_SUCCESS(Result))
    {
        *PGlobals = 0;
        return Result;
    }

    *PGlobals = Globals;
    return STATUS_SUCCESS;
}

VOID FspSiloDereferenceGlobals(FSP_SILO_GLOBALS *Globals)
{
    if (&FspSiloHostGlobals == Globals)
        return;

    CALL(PsDereferenceSiloContext)(Globals);
}

VOID FspSiloGetContainerId(GUID *ContainerId)
{
    FSP_PESILO Silo;
    GUID *Guid;

    if (!FspSiloInitDone ||
        0 == (Silo = CALL(PsGetCurrentServerSilo)()) ||
        0 == (Guid = CALL(PsGetSiloContainerId)(Silo)))
    {
        RtlZeroMemory(ContainerId, sizeof *ContainerId);
        return;
    }

    RtlCopyMemory(ContainerId, Guid, sizeof *ContainerId);
}

static NTSTATUS NTAPI FspSiloMonitorCreateCallback(FSP_PESILO Silo)
{
    ULONG ContextSlot;
    FSP_SILO_GLOBALS *Globals = 0;
    BOOLEAN Inserted = FALSE;
    NTSTATUS Result;

    ASSERT(0 != Silo);

    ContextSlot = CALL(PsGetSiloMonitorContextSlot)(FspSiloMonitor);

    Result = CALL(PsCreateSiloContext)(Silo, sizeof(FSP_SILO_GLOBALS), NonPagedPoolNx, 0, &Globals);
    if (!NT_SUCCESS(Result))
        goto exit;
    RtlZeroMemory(Globals, sizeof(FSP_SILO_GLOBALS));

    /* PsInsertSiloContext adds reference to Globals */
    Result = CALL(PsInsertSiloContext)(Silo, ContextSlot, Globals);
    if (!NT_SUCCESS(Result))
        goto exit;
    Inserted = TRUE;

    if (0 != FspSiloInitCallback)
    {
        FSP_PESILO PreviousSilo = CALL(PsAttachSiloToCurrentThread)(Silo);
        Result = FspSiloInitCallback();
        CALL(PsDetachSiloFromCurrentThread)(PreviousSilo);
    }

exit:
    if (!NT_SUCCESS(Result))
    {
        if (Inserted)
            CALL(PsRemoveSiloContext)(Silo, ContextSlot, 0);
    }

    if (0 != Globals)
        CALL(PsDereferenceSiloContext)(Globals);

    /*
     * Ignore errors and return success. There are two reasons for this:
     *
     * - Returning an error will disallow container creation.
     * - In some cases returning an error will crash Windows with an
     *   unexpected page fault in wcifs.sys.
     */
    return STATUS_SUCCESS;
}

static VOID NTAPI FspSiloMonitorTerminateCallback(FSP_PESILO Silo)
{
    ULONG ContextSlot;
    FSP_SILO_GLOBALS *Globals;
    NTSTATUS Result;

    ASSERT(0 != Silo);

    ContextSlot = CALL(PsGetSiloMonitorContextSlot)(FspSiloMonitor);

    /* if we cannot get the Globals, it must mean that we failed in Create callback */
    Result = CALL(PsGetSiloContext)(Silo, ContextSlot, &Globals);
    if (!NT_SUCCESS(Result))
        return;
    CALL(PsDereferenceSiloContext)(Globals);
    Globals = 0;

    if (0 != FspSiloFiniCallback)
    {
        FSP_PESILO PreviousSilo = CALL(PsAttachSiloToCurrentThread)(Silo);
        FspSiloFiniCallback();
        CALL(PsDetachSiloFromCurrentThread)(PreviousSilo);
    }

    /* PsRemoveSiloContext removes reference to Globals (possibly freeing it) */
    CALL(PsRemoveSiloContext)(Silo, ContextSlot, 0);
}

NTSTATUS FspSiloInitialize(FSP_SILO_INIT_CALLBACK Init, FSP_SILO_FINI_CALLBACK Fini)
{
    NTSTATUS Result = STATUS_SUCCESS;

    if (FspIsNtDdiVersionAvailable(NTDDI_WIN10_RS5))
    {
        ULONG Fail = 0;

        LOAD(PsRegisterSiloMonitor);
        LOAD(PsStartSiloMonitor);
        LOAD(PsUnregisterSiloMonitor);
        LOAD(PsGetSiloMonitorContextSlot);
        LOAD(PsCreateSiloContext);
        LOAD(PsDereferenceSiloContext);
        LOAD(PsInsertSiloContext);
        LOAD(PsRemoveSiloContext);
        LOAD(PsGetSiloContext);
        LOAD(PsAttachSiloToCurrentThread);
        LOAD(PsDetachSiloFromCurrentThread);
        LOAD(PsGetCurrentServerSilo);
        LOAD(PsGetSiloContainerId);

        if (0 == Fail)
        {
            FSP_SILO_MONITOR_REGISTRATION Registration =
            {
                .Version = FSP_SILO_MONITOR_REGISTRATION_VERSION,
                .MonitorHost = FALSE,
                .MonitorExistingSilos = TRUE,
                .DriverObjectName = 0,
                .CreateCallback = FspSiloMonitorCreateCallback,
                .TerminateCallback = FspSiloMonitorTerminateCallback,
            };
            FSP_PSILO_MONITOR Monitor = 0;
            UNICODE_STRING DriverName;

            RtlInitUnicodeString(&DriverName, L"" DRIVER_NAME);
            Registration.DriverObjectName = &DriverName;
            Result = CALL(PsRegisterSiloMonitor)(&Registration, &Monitor);
            if (!NT_SUCCESS(Result))
                goto exit;

            Result = CALL(PsStartSiloMonitor)(Monitor);
            if (!NT_SUCCESS(Result))
                goto exit;

            FspSiloMonitor = Monitor;
            FspSiloInitCallback = Init;
            FspSiloFiniCallback = Fini;

            FspSiloInitDone = TRUE;
            Result = STATUS_SUCCESS;

        exit:
            if (!NT_SUCCESS(Result))
            {
                if (0 != Monitor)
                    CALL(PsUnregisterSiloMonitor)(Monitor);
            }
        }
    }

    return Result;
}

VOID FspSiloFinalize(VOID)
{
    if (!FspSiloInitDone)
        return;

    CALL(PsUnregisterSiloMonitor)(FspSiloMonitor);
}
