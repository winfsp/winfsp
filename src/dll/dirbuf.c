/**
 * @file dll/dirbuf.c
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

#include <dll/library.h>

#define RETURN(R, B)                    \
    do                                  \
    {                                   \
        if (0 != PResult)               \
            *PResult = R;               \
        return B;                       \
    } while (0,0)

typedef struct
{
    SRWLOCK Lock;
    ULONG Capacity, LoMark, HiMark;
    PUINT8 Buffer;
} FSP_FILE_SYSTEM_DIRECTORY_BUFFER;

static int FspFileSystemDirectoryBufferFileNameCmp(PWSTR a, int alen, PWSTR b, int blen)
{
    int len, res;

    if (-1 == alen)
        alen = (int)lstrlenW(a);
    if (-1 == blen)
        blen = (int)lstrlenW(b);

    len = alen < blen ? alen : blen;

    /* order "." and ".." first */
    switch (alen)
    {
    case 1:
        if (L'.' == a[0])
            a = L"\1";
        break;
    case 2:
        if (L'.' == a[0] && L'.' == a[1])
            a = L"\1\1";
        break;
    }

    /* order "." and ".." first */
    switch (blen)
    {
    case 1:
        if (L'.' == b[0])
            b = L"\1";
        break;
    case 2:
        if (L'.' == b[0] && L'.' == b[1])
            b = L"\1\1";
        break;
    }

    res = invariant_wcsncmp(a, b, len);

    if (0 == res)
        res = alen - blen;

    return res;
}

/*
 * Binary search
 * "I wish I had the standard library!"
 */
static BOOLEAN FspFileSystemSearchDirectoryBuffer(FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer,
    PWSTR Marker, int MarkerLen, PULONG PIndexNum)
{
    PULONG Index = (PULONG)(DirBuffer->Buffer + DirBuffer->HiMark);
    ULONG Count = (DirBuffer->Capacity - DirBuffer->HiMark) / sizeof(ULONG);
    FSP_FSCTL_DIR_INFO *DirInfo;
    int Lo = 0, Hi = Count - 1, Mi;
    int CmpResult;

    while (Lo <= Hi)
    {
        Mi = (unsigned)(Lo + Hi) >> 1;

        DirInfo = (PVOID)(DirBuffer->Buffer + Index[Mi]);
        CmpResult = FspFileSystemDirectoryBufferFileNameCmp(
            DirInfo->FileNameBuf, (DirInfo->Size - sizeof *DirInfo) / sizeof(WCHAR),
            Marker, MarkerLen);

        if (0 > CmpResult)
            Lo = Mi + 1;
        else if (0 < CmpResult)
            Hi = Mi - 1;
        else
        {
            *PIndexNum = Mi;
            return TRUE;
        }
    }

    *PIndexNum = Hi;
    return FALSE;
}

/*
 * Quick sort
 * "I wish I had the standard library!"
 *
 * Based on Sedgewick's Algorithms in C++; multiple editions.
 *
 * Implements a non-recursive quicksort with tail-end recursion eliminated
 * and median-of-three partitioning.
 */

#define less(a, b)                      FspFileSystemDirectoryBufferLess(Buffer, a, b)
#define exch(a, b)                      { ULONG t = a; a = b; b = t; }
#define compexch(a, b)                  if (less(b, a)) exch(a, b)
#define push(i)                         (stack[stackpos++] = (i))
#define pop()                           (stack[--stackpos])

static __forceinline
int FspFileSystemDirectoryBufferLess(PUINT8 Buffer, int a, int b)
{
    FSP_FSCTL_DIR_INFO *DirInfoA = (FSP_FSCTL_DIR_INFO *)(Buffer + a);
    FSP_FSCTL_DIR_INFO *DirInfoB = (FSP_FSCTL_DIR_INFO *)(Buffer + b);
    return 0 > FspFileSystemDirectoryBufferFileNameCmp(
        DirInfoA->FileNameBuf, (DirInfoA->Size - sizeof *DirInfoA) / sizeof(WCHAR),
        DirInfoB->FileNameBuf, (DirInfoB->Size - sizeof *DirInfoB) / sizeof(WCHAR));
}

static __forceinline
int FspFileSystemPartitionDirectoryBuffer(PUINT8 Buffer, PULONG Index, int l, int r)
{
    int i = l - 1, j = r;
    ULONG v = Index[r];

    for (;;)
    {
        while (less(Index[++i], v))
            ;

        while (less(v, Index[--j]))
            if (j == l)
                break;

        if (i >= j)
            break;

        exch(Index[i], Index[j]);
    }

    exch(Index[i], Index[r]);

    return i;
}

static VOID FspFileSystemQSortDirectoryBuffer(PUINT8 Buffer, PULONG Index, int l, int r)
{
    int stack[64], stackpos = 0;
    int i;

    for (;;)
    {
        while (r > l)
        {
#if 0
            exch(Index[(l + r) / 2], Index[r - 1]);
            compexch(Index[l], Index[r - 1]);
            compexch(Index[l], Index[r]);
            compexch(Index[r - 1], Index[r]);

            i = FspFileSystemPartitionDirectoryBuffer(Buffer, Index, l + 1, r - 1);
#else
            i = FspFileSystemPartitionDirectoryBuffer(Buffer, Index, l, r);
#endif

            if (i - l > r - i)
            {
                push(l); push(i - 1);
                l = i + 1;
            }
            else
            {
                push(i + 1); push(r);
                r = i - 1;
            }
        }

        if (0 == stackpos)
            break;

        r = pop(); l = pop();
    }
}

#undef push
#undef pop
#undef less
#undef compexch
#undef exch

static inline VOID FspFileSystemSortDirectoryBuffer(FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer)
{
    PUINT8 Buffer = DirBuffer->Buffer;
    PULONG Index = (PULONG)(DirBuffer->Buffer + DirBuffer->HiMark);
    ULONG Count = (DirBuffer->Capacity - DirBuffer->HiMark) / sizeof(ULONG);

    FspFileSystemQSortDirectoryBuffer(Buffer, Index, 0, Count - 1);
}

FSP_API BOOLEAN FspFileSystemAcquireDirectoryBuffer(PVOID *PDirBuffer,
    BOOLEAN Reset, PNTSTATUS PResult)
{
    FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer = *PDirBuffer;
    MemoryBarrier();

    if (0 == DirBuffer)
    {
        static SRWLOCK CreateLock = SRWLOCK_INIT;
        FSP_FILE_SYSTEM_DIRECTORY_BUFFER *NewDirBuffer;

        NewDirBuffer = MemAlloc(sizeof *NewDirBuffer);
        if (0 == NewDirBuffer)
            RETURN(STATUS_INSUFFICIENT_RESOURCES, FALSE);
        memset(NewDirBuffer, 0, sizeof *NewDirBuffer);
        InitializeSRWLock(&NewDirBuffer->Lock);
        AcquireSRWLockExclusive(&NewDirBuffer->Lock);

        AcquireSRWLockExclusive(&CreateLock);
        DirBuffer = *PDirBuffer;
        MemoryBarrier();
        if (0 == DirBuffer)
            *PDirBuffer = DirBuffer = NewDirBuffer;
        ReleaseSRWLockExclusive(&CreateLock);

        if (DirBuffer == NewDirBuffer)
            RETURN(STATUS_SUCCESS, TRUE);

        ReleaseSRWLockExclusive(&NewDirBuffer->Lock);
        MemFree(NewDirBuffer);
    }

    if (Reset)
    {
        AcquireSRWLockExclusive(&DirBuffer->Lock);

        DirBuffer->LoMark = 0;
        DirBuffer->HiMark = DirBuffer->Capacity;

        RETURN(STATUS_SUCCESS, TRUE);
    }

    RETURN(STATUS_SUCCESS, FALSE);
}

FSP_API BOOLEAN FspFileSystemFillDirectoryBuffer(PVOID *PDirBuffer,
    FSP_FSCTL_DIR_INFO *DirInfo, PNTSTATUS PResult)
{
    /* assume that FspFileSystemAcquireDirectoryBuffer has been called */

    FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer = *PDirBuffer;
    ULONG Capacity, LoMark, HiMark;
    PUINT8 Buffer;

    if (0 == DirInfo)
        RETURN(STATUS_INVALID_PARAMETER, FALSE);

    for (;;)
    {
        LoMark = DirBuffer->LoMark;
        HiMark = DirBuffer->HiMark;
        Buffer = DirBuffer->Buffer;

        if (FspFileSystemAddDirInfo(DirInfo,
            Buffer,
            HiMark > sizeof(ULONG) ? HiMark - sizeof(ULONG)/*space for new index entry*/ : HiMark,
            &LoMark))
        {
            HiMark -= sizeof(ULONG);
            *(PULONG)(Buffer + HiMark) = DirBuffer->LoMark;

            DirBuffer->LoMark = LoMark;
            DirBuffer->HiMark = HiMark;

            RETURN (STATUS_SUCCESS, TRUE);
        }

        if (0 == Buffer)
        {
            Buffer = MemAlloc(Capacity = 512);
            if (0 == Buffer)
                RETURN(STATUS_INSUFFICIENT_RESOURCES, FALSE);

            HiMark = Capacity;
        }
        else
        {
            Buffer = MemRealloc(Buffer, Capacity = DirBuffer->Capacity * 2);
            if (0 == Buffer)
                RETURN(STATUS_INSUFFICIENT_RESOURCES, FALSE);

            ULONG IndexSize = DirBuffer->Capacity - HiMark;
            ULONG NewHiMark = Capacity - IndexSize;

            memmove(Buffer + NewHiMark, Buffer + HiMark, IndexSize);
            HiMark = NewHiMark;
        }

        DirBuffer->Capacity = Capacity;
        DirBuffer->LoMark = LoMark;
        DirBuffer->HiMark = HiMark;
        DirBuffer->Buffer = Buffer;
    }
}

FSP_API VOID FspFileSystemReleaseDirectoryBuffer(PVOID *PDirBuffer)
{
    /* assume that FspFileSystemAcquireDirectoryBuffer has been called */

    FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer = *PDirBuffer;

    /* eliminate invalidated entries from the index */
    PULONG Index = (PULONG)(DirBuffer->Buffer + DirBuffer->HiMark);
    ULONG Count = (DirBuffer->Capacity - DirBuffer->HiMark) / sizeof(ULONG);
    ULONG I, J;
    for (I = Count - 1, J = Count; I < Count; I--)
    {
        if (FspFileSystemDirectoryBufferEntryInvalid == Index[I])
            continue;
        Index[--J] = Index[I];
    }
    DirBuffer->HiMark = (ULONG)((PUINT8)&Index[J] - DirBuffer->Buffer);

    FspFileSystemSortDirectoryBuffer(DirBuffer);

    ReleaseSRWLockExclusive(&DirBuffer->Lock);
}

FSP_API VOID FspFileSystemReadDirectoryBuffer(PVOID *PDirBuffer,
    PWSTR Marker,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer = *PDirBuffer;
    MemoryBarrier();

    if (0 != DirBuffer)
    {
        AcquireSRWLockShared(&DirBuffer->Lock);

        PULONG Index = (PULONG)(DirBuffer->Buffer + DirBuffer->HiMark);
        ULONG Count = (DirBuffer->Capacity - DirBuffer->HiMark) / sizeof(ULONG);
        ULONG IndexNum;
        FSP_FSCTL_DIR_INFO *DirInfo;

        if (0 == Marker)
            IndexNum = 0;
        else
        {
            FspFileSystemSearchDirectoryBuffer(DirBuffer,
                Marker, lstrlenW(Marker),
                &IndexNum);
            IndexNum++;
        }

        for (; IndexNum < Count; IndexNum++)
        {
            DirInfo = (PVOID)(DirBuffer->Buffer + Index[IndexNum]);
            if (!FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred))
            {
                ReleaseSRWLockShared(&DirBuffer->Lock);
                return;
            }
        }

        ReleaseSRWLockShared(&DirBuffer->Lock);
    }

    FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);
}

FSP_API VOID FspFileSystemDeleteDirectoryBuffer(PVOID *PDirBuffer)
{
    FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer = *PDirBuffer;
    MemoryBarrier();

    if (0 != DirBuffer)
    {
        MemFree(DirBuffer->Buffer);
        MemFree(DirBuffer);
        *PDirBuffer = 0;
    }
}

VOID FspFileSystemPeekInDirectoryBuffer(PVOID *PDirBuffer,
    PUINT8 *PBuffer, PULONG *PIndex, PULONG PCount)
{
    /* assume that FspFileSystemAcquireDirectoryBuffer has been called */

    FSP_FILE_SYSTEM_DIRECTORY_BUFFER *DirBuffer = *PDirBuffer;

    *PBuffer = DirBuffer->Buffer;
    *PIndex = (PULONG)(DirBuffer->Buffer + DirBuffer->HiMark);
    *PCount = (DirBuffer->Capacity - DirBuffer->HiMark) / sizeof(ULONG);
}
