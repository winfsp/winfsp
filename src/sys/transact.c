/**
 * @file sys/transact.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static KSTART_ROUTINE FspTransactThread;
NTSTATUS FspTransactThreadStart(FSP_TRANSACT_THREAD *TransactThread,
    FSP_IOQ *TransactIoq, FSP_IOQ *Ioq);
VOID FspTransactThreadStop(FSP_TRANSACT_THREAD *TransactThread);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspTransactThread)
#pragma alloc_text(PAGE, FspTransactThreadStart)
#pragma alloc_text(PAGE, FspTransactThreadStop)
#endif

static VOID FspTransactThread(PVOID StartContext)
{
    PAGED_CODE();

    FSP_TRANSACT_THREAD *TransactThread = StartContext;
    PVOID WaitObjects[2];
    WaitObjects[0] = &TransactThread->Event;
    WaitObjects[1] = FspIoqPendingIrpEvent(TransactThread->TransactIoq);
    for (;;)
    {
        NTSTATUS Result;
        PIRP Irp;

        Result = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, Executive, KernelMode, FALSE, 0, 0);
        if (STATUS_WAIT_0 == Result)
            break; /* stop thread */
        else if (STATUS_WAIT_1 != Result)
            continue; /* retry */

        Irp = FspIoqNextPendingIrp(&TransactThread->TransactIoq, 0);
        if (0 == Irp)
            continue; /* retry */
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS FspTransactThreadStart(FSP_TRANSACT_THREAD *TransactThread,
    FSP_IOQ *TransactIoq, FSP_IOQ *Ioq)
{
    PAGED_CODE();

    NTSTATUS Result;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE ThreadHandle;

    RtlZeroMemory(TransactThread, sizeof *TransactThread);
    KeInitializeEvent(&TransactThread->Event, NotificationEvent, FALSE);
    TransactThread->TransactIoq = TransactIoq;
    TransactThread->Ioq = Ioq;

    InitializeObjectAttributes(&ObjectAttributes, 0, OBJ_KERNEL_HANDLE, 0, 0);
    Result = PsCreateSystemThread(&ThreadHandle, THREAD_ALL_ACCESS, &ObjectAttributes, 0, 0,
        FspTransactThread, TransactThread);
    if (!NT_SUCCESS(Result))
        return Result;

    Result = ObReferenceObjectByHandle(ThreadHandle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode,
        &TransactThread->Thread, 0);
    ASSERT(NT_SUCCESS(Result));

    ZwClose(ThreadHandle);

    return STATUS_SUCCESS;
}

VOID FspTransactThreadStop(FSP_TRANSACT_THREAD *TransactThread)
{
    PAGED_CODE();

    KeSetEvent(&TransactThread->Event, 1, TRUE);
    KeWaitForSingleObject(&TransactThread->Thread, Executive, KernelMode, FALSE, 0);
}
