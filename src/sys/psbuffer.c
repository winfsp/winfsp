/**
 * @file sys/psbuffer.c
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

#define SafeGetCurrentProcessId()       (PsGetProcessId(PsGetCurrentProcess()))

#define FspProcessBufferCountMax        (2 >= FspProcessorCount ? 2 : (8 <= FspProcessorCount ? 8 : FspProcessorCount))
#define ProcessBufferBucketCount        61  /* are you going to have that many file systems? */

typedef struct _FSP_PROCESS_BUFFER_ITEM
{
    struct _FSP_PROCESS_BUFFER_ITEM *DictNext;
    struct _FSP_PROCESS_BUFFER_LIST_ENTRY *BufferList;
    ULONG BufferCount;
    HANDLE ProcessId;
} FSP_PROCESS_BUFFER_ITEM;

typedef struct _FSP_PROCESS_BUFFER_LIST_ENTRY
{
    struct _FSP_PROCESS_BUFFER_LIST_ENTRY *Next;
    PVOID Buffer;
} FSP_PROCESS_BUFFER_LIST_ENTRY;

static KSPIN_LOCK ProcessBufferLock;
static FSP_PROCESS_BUFFER_ITEM *ProcessBufferBuckets[ProcessBufferBucketCount];

static VOID FspProcessBufferNotifyRoutine(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create);

static inline FSP_PROCESS_BUFFER_ITEM *FspProcessBufferLookupItemAtDpcLevel(HANDLE ProcessId)
{
    FSP_PROCESS_BUFFER_ITEM *Item = 0;
    ULONG HashIndex = FspHashMixPointer(ProcessId) % ProcessBufferBucketCount;
    for (FSP_PROCESS_BUFFER_ITEM *ItemX = ProcessBufferBuckets[HashIndex]; ItemX; ItemX = ItemX->DictNext)
        if (ItemX->ProcessId == ProcessId)
        {
            Item = ItemX;
            break;
        }
    return Item;
}

static inline VOID FspProcessBufferAddItemAtDpcLevel(FSP_PROCESS_BUFFER_ITEM *Item)
{
    ULONG HashIndex = FspHashMixPointer(Item->ProcessId) % ProcessBufferBucketCount;
#if DBG
    for (FSP_PROCESS_BUFFER_ITEM *ItemX = ProcessBufferBuckets[HashIndex]; ItemX; ItemX = ItemX->DictNext)
        ASSERT(ItemX->ProcessId != Item->ProcessId);
#endif
    Item->DictNext = ProcessBufferBuckets[HashIndex];
    ProcessBufferBuckets[HashIndex] = Item;
}

static inline FSP_PROCESS_BUFFER_ITEM *FspProcessBufferRemoveItemAtDpcLevel(HANDLE ProcessId)
{
    FSP_PROCESS_BUFFER_ITEM *Item = 0;
    ULONG HashIndex = FspHashMixPointer(ProcessId) % ProcessBufferBucketCount;
    for (FSP_PROCESS_BUFFER_ITEM **P = &ProcessBufferBuckets[HashIndex]; *P; P = &(*P)->DictNext)
        if ((*P)->ProcessId == ProcessId)
        {
            Item = *P;
            *P = (*P)->DictNext;
            break;
        }
    return Item;
}

static inline VOID FspProcessBufferReuseEntry(HANDLE ProcessId,
    FSP_PROCESS_BUFFER_LIST_ENTRY *BufferEntry)
{
    KIRQL Irql;
    FSP_PROCESS_BUFFER_ITEM *Item;

    KeAcquireSpinLock(&ProcessBufferLock, &Irql);

    Item = FspProcessBufferLookupItemAtDpcLevel(ProcessId);

    if (0 != Item)
    {
        BufferEntry->Next = Item->BufferList;
        Item->BufferList = BufferEntry;
    }

    KeReleaseSpinLock(&ProcessBufferLock, Irql);

    if (0 == Item)
    {
        if (0 != BufferEntry->Buffer)
        {
            SIZE_T BufferSize = 0;
            ZwFreeVirtualMemory(ZwCurrentProcess(), &BufferEntry->Buffer, &BufferSize, MEM_RELEASE);
        }

        FspFree(BufferEntry);
    }
}

NTSTATUS FspProcessBufferInitialize(VOID)
{
    KeInitializeSpinLock(&ProcessBufferLock);

    return PsSetCreateProcessNotifyRoutine(FspProcessBufferNotifyRoutine, FALSE);
}

VOID FspProcessBufferFinalize(VOID)
{
    PsSetCreateProcessNotifyRoutine(FspProcessBufferNotifyRoutine, TRUE);
}

static VOID FspProcessBufferNotifyRoutine(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create)
{
    if (!Create)
        FspProcessBufferCollect(ProcessId);
}

VOID FspProcessBufferCollect(HANDLE ProcessId)
{
    KIRQL Irql;
    FSP_PROCESS_BUFFER_ITEM *Item = 0;

    KeAcquireSpinLock(&ProcessBufferLock, &Irql);

    Item = FspProcessBufferRemoveItemAtDpcLevel(ProcessId);

    KeReleaseSpinLock(&ProcessBufferLock, Irql);

    if (0 != Item)
    {
        DEBUGLOG("pid=%ld", (ULONG)(UINT_PTR)ProcessId);

        for (FSP_PROCESS_BUFFER_LIST_ENTRY *P = Item->BufferList, *Next; P; P = Next)
        {
            Next = P->Next;
            FspFree(P);
        }

        FspFree(Item);
    }
}

NTSTATUS FspProcessBufferAcquire(SIZE_T BufferSize, PVOID *PBufferCookie, PVOID *PBuffer)
{
    if (FspProcessBufferSizeMax >= BufferSize)
    {
        HANDLE ProcessId = SafeGetCurrentProcessId();
        KIRQL Irql;
        FSP_PROCESS_BUFFER_ITEM *Item, *NewItem;
        FSP_PROCESS_BUFFER_LIST_ENTRY *BufferEntry = 0;
        BOOLEAN AllocNoReuse;
        NTSTATUS Result;

        KeAcquireSpinLock(&ProcessBufferLock, &Irql);

        Item = FspProcessBufferLookupItemAtDpcLevel(ProcessId);

        if (0 != Item)
        {
            BufferEntry = Item->BufferList;
            if (0 != BufferEntry)
                Item->BufferList = BufferEntry->Next;
        }

        AllocNoReuse = 0 == BufferEntry &&
            (0 != Item && FspProcessBufferCountMax <= Item->BufferCount);

        KeReleaseSpinLock(&ProcessBufferLock, Irql);

        if (AllocNoReuse)
            goto alloc_no_reuse;

        if (0 == BufferEntry)
        {
            *PBufferCookie = 0;
            *PBuffer = 0;

            BufferEntry = FspAllocNonPaged(sizeof *BufferEntry);
            if (0 == BufferEntry)
                return STATUS_INSUFFICIENT_RESOURCES;
            RtlZeroMemory(BufferEntry, sizeof *BufferEntry);

            NewItem = FspAllocNonPaged(sizeof *NewItem);
            if (0 == NewItem)
            {
                FspFree(BufferEntry);
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            RtlZeroMemory(NewItem, sizeof *NewItem);

            KeAcquireSpinLock(&ProcessBufferLock, &Irql);

            Item = FspProcessBufferLookupItemAtDpcLevel(ProcessId);

            if (0 == Item)
            {
                Item = NewItem;
                NewItem = 0;
                Item->BufferCount = 1;
                Item->ProcessId = ProcessId;
                FspProcessBufferAddItemAtDpcLevel(Item);
            }
            else if (FspProcessBufferCountMax > Item->BufferCount)
                Item->BufferCount++;
            else
                AllocNoReuse = TRUE;

            KeReleaseSpinLock(&ProcessBufferLock, Irql);

            if (0 != NewItem)
                FspFree(NewItem);

            if (AllocNoReuse)
            {
                FspFree(BufferEntry);
                goto alloc_no_reuse;
            }
        }

        if (0 == BufferEntry->Buffer)
        {
            BufferSize = FspProcessBufferSizeMax;
            Result = ZwAllocateVirtualMemory(ZwCurrentProcess(),
                &BufferEntry->Buffer, 0, &BufferSize, MEM_COMMIT, PAGE_READWRITE);
            if (!NT_SUCCESS(Result))
            {
                /* failed to allocate actual buffer; reuse BufferEntry */
                FspProcessBufferReuseEntry(ProcessId, BufferEntry);

                return Result;
            }
        }

        *PBufferCookie = BufferEntry;
        *PBuffer = BufferEntry->Buffer;

        return STATUS_SUCCESS;
    }
    else
    {
    alloc_no_reuse:
        *PBufferCookie = 0;
        *PBuffer = 0;
        return ZwAllocateVirtualMemory(ZwCurrentProcess(),
            PBuffer, 0, &BufferSize, MEM_COMMIT, PAGE_READWRITE);
    }
}

VOID FspProcessBufferRelease(PVOID BufferCookie, PVOID Buffer)
{
    if (0 != BufferCookie)
    {
        HANDLE ProcessId = SafeGetCurrentProcessId();
        FSP_PROCESS_BUFFER_LIST_ENTRY *BufferEntry = BufferCookie;

        ASSERT(Buffer == BufferEntry->Buffer);

        FspProcessBufferReuseEntry(ProcessId, BufferEntry);
    }
    else
    {
        SIZE_T BufferSize = 0;
        ZwFreeVirtualMemory(ZwCurrentProcess(), &Buffer, &BufferSize, MEM_RELEASE);
    }
}
