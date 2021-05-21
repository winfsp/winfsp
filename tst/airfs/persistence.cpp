/**
 * @file persistence.cpp
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
/*
 * Airfs is based on Memfs with changes contributed by John Oberschelp.
 * The contributed changes are under joint copyright by Bill Zissimopoulos
 * and John Oberschelp per the Contributor Agreement found at the
 * root of this project.
 */
/*
 * Airfs uses a memory-mapped file per volume to achieve persistence.
 * The primary advantage of this is that volume loads and saves are automatic.
 * The two primary disadvantages, and our workarounds are:
 *   1. We can't use standard containers or memory management,
 *      so the below Rubbertree and Storage functions are used instead.
 *   2. Each process will map the volume to an arbitrary address,
 *      so Where<T> offsets are used in place of pointers.
 */

#include "common.h"

SpinLock StorageLock, AirprintLock, SetLock;

int SizeCmp ( void* key,  NODE_ x)
{
    return *(int32_t*)key - ((int32_t*)x)[-1];
}

int BlockCmp ( void* key,  NODE_ x)
{
    int64_t left  = *(int64_t*)key;
    int64_t right = x->FileOffset;
    return left == right ? 0 : (left < right ? -1 : 1);
}

int CaselessNameCmp ( void* key,  NODE_ x)
{ 
    WCHAR* c1 = (WCHAR*) key;
    WCHAR* c2 = x->Name;
    return _wcsicmp(c1,c2); 
}

int ExactNameCmp ( void* key,  NODE_ x)
{ 
    WCHAR* c1 = (WCHAR*) key;
    WCHAR* c2 = x->Name;
    return wcscmp(c1,c2); 
}

//////////////////////////////////////////////////////////////////////

void Airprint (const char * format, ...)
{
    AirprintLock.Acquire();
    va_list args;
    va_start(args, format);
    char szBuffer[512];
    sprintf_s(szBuffer, 511, "Airfs %5.5f  ----", SystemTime() / 10'000'000.0);
    vsnprintf(szBuffer+25, 511-25, format, args);
    OutputDebugStringA(szBuffer);
    va_end(args);
    AirprintLock.Release();
}

//////////////////////////////////////////////////////////////////////
//
//  Rubbertree (because it is flexible!)
//  Implements a sorted set of elements, using a binary tree.
//  Has a function, Near, that finds nodes at or adjacent to a key.
//  Attach, Find, and Near use splay to improve random access times.
//  First, Last, Next, and Prev do not, to improve sequential access times.
//  Replacing each Where<NODE_> with NODE_ would make this a pointer-based tree.
//  In addition to Left, Right, and Parent references, each node has an Equal
//  reference that may be used to keep "equivalent" nodes. This is used by
//  our memory heap manager's tree of available memory blocks, sorted by size.

//--------------------------------------------------------------------

inline void rotateL(Where<NODE_> &root, NODE_ x)
{
    NODE_ y = x->R, p = x->P;
    if (x->R = y->L) y->L->P = x;
    if (!(y->P = p))
        root = y;
    else
    {
        if (x == p->L) p->L = y;
        else           p->R = y;
    }
    (y->L = x)->P = y;
}

//--------------------------------------------------------------------

inline void rotateR(Where<NODE_> &root, NODE_ y)
{
    NODE_ x = y->L, p = y->P;
    if (y->L = x->R) x->R->P = y;
    if (!(x->P = p))
        root = x;
    else
    {
        if (y == p->L) p->L = x;
        else           p->R = x;
    }
    (x->R = y)->P = x;
}

//--------------------------------------------------------------------

static void splay(Where<NODE_> &root, NODE_ x)
{
    while (NODE_ p = x->P)
    {
        if (!p->P)
        {
            if (p->L == x) rotateR(root, p);
            else           rotateL(root, p);
        } 
        else
        {
            if (p == p->P->R)
            {
                if (p->R == x) rotateL(root, p->P);
                else           rotateR(root, p);
                rotateL(root, x->P); 
            }
            else
            {
                if (p->L == x) rotateR(root, p->P);
                else           rotateL(root, p);
                rotateR(root, x->P);
            }
        }
    }
}

//--------------------------------------------------------------------

inline int seek(Where<NODE_> &root, NODE_ &x, void* key, CompareFunction CMP) 
{
    x = root;
    for (;;)
    {
        int diff = CMP(key, x);
        if (diff < 0)
        {
            if (!x->L) return -1;
            x = x->L;
        }
        else if (diff > 0)
        {
            if (!x->R) return  1;
            x = x->R;
        }
        else           return  0;
    }
}

//--------------------------------------------------------------------

inline NODE_ next(NODE_ x)
{
    if (x->R) { x = x->R; while (x->L) x = x->L; return x; }
    NODE_ p = x->P;
    while (p && x == p->R) { x = p; p = p->P; }
    return p;
}

//--------------------------------------------------------------------

inline NODE_ prev(NODE_ x)
{
    if (x->L) { x = x->L; while (x->R) x = x->R; return x; }
    NODE_ p = x->P;
    while (p && x == p->L) { x = p; p = p->P; }
    return p;
}

//--------------------------------------------------------------------

NODE_ First(NODE_ x)
{
    SetLock.Acquire();
    if (x) while (x->L) x = x->L; 
    SetLock.Release();
    return x;
}

//--------------------------------------------------------------------

NODE_ Last(NODE_ x)
{
    SetLock.Acquire();
    if (x) while (x->R) x = x->R;
    SetLock.Release();
    return x;
}

//--------------------------------------------------------------------
    
NODE_ Next(NODE_ x)
{
    SetLock.Acquire();
    x = next(x);
    SetLock.Release();
    return x;
}

//--------------------------------------------------------------------

NODE_ Prev(NODE_ x)
{
    SetLock.Acquire();
    x = prev(x);
    SetLock.Release();
    return x;
}

//--------------------------------------------------------------------

NODE_ Near(Where<NODE_> &root, void* key, CompareFunction CMP, Neighbor want)
{
    //  Return a node relative to (just <, <=, ==, >=, or >) a key.
    if (!root) return 0;
    SetLock.Acquire();
    NODE_ x;
    int dir = seek(root, x, key, CMP);
    if ((dir == 0 && want == GT) || (dir > 0 && want >= GE)) x = next(x);
    else
    if ((dir == 0 && want == LT) || (dir < 0 && want <= LE)) x = prev(x);
    else
    if (dir != 0 && want == EQ) x = 0;
    if (x) splay(root, x);
    SetLock.Release();
    return x;
}

//--------------------------------------------------------------------

NODE_ Find(Where<NODE_> &root, void* key, CompareFunction CMP)
{
    if (!root) return 0;
    SetLock.Acquire();
    NODE_ x;
    int direction = seek(root, x, key, CMP);
    splay(root, x);
    SetLock.Release();
    return direction?0:x;
}

//--------------------------------------------------------------------

void Attach(Where<NODE_> &root, NODE_ x, CompareFunction CMP, void* key)
{
    SetLock.Acquire();
    if (!root)
    {
        root = x;
        x->P = x->L = x->R = x->E = 0;
        SetLock.Release();
        return;
    }
    NODE_ f;
    int diff = seek(root, f, key, CMP);
    if (!diff)
    {
        if (x->L = f->L) x->L->P = x;
        if (x->R = f->R) x->R->P = x;
        NODE_ p = f->P;
        if (x->P = p) { if (p->L == f) p->L = x; else p->R = x; }
        else root = x;
        (x->E = f)->P = x;
        f->L = f->R = 0;
        splay(root, x);
        SetLock.Release();
        return;
    }
    if (diff < 0) f->L = x; else f->R = x;
    x->P = f;
    x->L = x->R = x->E = 0;
    splay(root, x);
    SetLock.Release();
}

//--------------------------------------------------------------------

void Detach(Where<NODE_> &root, NODE_ x)
{
    SetLock.Acquire();
    NODE_ e = x->E, p = x->P;
    if (p && p->E == x) { if (p->E = e) e->P = p; }
    else if (e)
    {
        if (e->L = x->L) e->L->P = e;
        if (e->R = x->R) e->R->P = e;
        if (e->P = p) { if (p->L == x) p->L = e; else p->R = e; }
        else root = e;
    }
    else if (!x->L)
    {
        if (p) { if ( p->L == x) p->L = x->R; else p->R = x->R; }
        else root = x->R;
        if (x->R) x->R->P = p;
    }
    else if (!x->R)
    {
        if (p) { if ( p->L == x) p->L = x->L; else p->R = x->L; }
        else root = x->L;
        if (x->L) x->L->P = p; 
    }
    else
    {
        e = x->L;
        if (e->R)
        {
            do { e = e->R; } while (e->R);
            if (e->P->R = e->L) e->L->P = e->P;
            (e->L = x->L)->P = e;
        }
        (e->R = x->R)->P = e;
        if (e->P = x->P) { if (e->P->L == x) e->P->L = e; else e->P->R = e; }
        else root = e;
    }
    SetLock.Release();
}

//////////////////////////////////////////////////////////////////////
//
//  Storage Functions for our memory-mapped file-based persistent volumes

//--------------------------------------------------------------------

void* StorageAllocate(AIRFS_ Airfs, int64_t RequestedSize)
{
    if (!RequestedSize) return 0;
    if (RequestedSize + sizeof int32_t > MAXIMUM_ALLOCSIZE) return 0;

    StorageLock.Acquire();
    int32_t RoundedSize = (int32_t) ROUND_UP(RequestedSize, MINIMUM_ALLOCSIZE - sizeof int32_t);
    int32_t SplitableSize = RoundedSize + MINIMUM_ALLOCSIZE;

    //  See if we have a freed node of the size we requested.
    NODE_ NewItem = Near(Airfs->Available, &RoundedSize, SizeCmp, GE);
    if (NewItem)
    {
        int32_t FoundSize = ((int32_t*)NewItem)[-1];
        if (FoundSize < SplitableSize)
        {
            Detach(Airfs->Available, NewItem);
            Airfs->FreeSize -= FoundSize;
            StorageLock.Release();
            return NewItem;
        }
    }

    //  If not, see if we can downsize a larger freed element.
    NewItem = Near(Airfs->Available, &SplitableSize, SizeCmp, GE);
    if (NewItem)
    {
        int32_t FoundSize = ((int32_t*)NewItem)[-1];
        Detach(Airfs->Available, NewItem);
        Airfs->FreeSize -= FoundSize;
        char* Addr = (char*)NewItem + RoundedSize + sizeof int32_t;
        NODE_ Remainder = (NODE_) Addr;
        int32_t RemainderSize = FoundSize - (RoundedSize + sizeof int32_t);
        ((int32_t*)Remainder)[-1] = RemainderSize;
        Attach(Airfs->Available, Remainder, SizeCmp, &RemainderSize);
        Airfs->FreeSize += RemainderSize;
        ((int32_t*)NewItem)[-1] = RoundedSize;
        StorageLock.Release();
        return NewItem;
    }

    //  If not, give up.
    StorageLock.Release();
    return 0;
}

//--------------------------------------------------------------------

void* StorageReallocate(AIRFS_ Airfs, void* OldAlloc, int64_t RequestedSize)
{
    if (!OldAlloc)
    {
        return StorageAllocate(Airfs, RequestedSize);
    }

    if (!RequestedSize)
    {
        StorageFree(Airfs, OldAlloc);
        return 0;
    }

    int32_t OldSize = ((int32_t*)OldAlloc)[-1];
    void* NewAlloc = StorageAllocate(Airfs, RequestedSize);
    if (!NewAlloc) return 0;
    memcpy(NewAlloc, OldAlloc, min(RequestedSize, OldSize));
    StorageFree(Airfs, OldAlloc);
    return NewAlloc;
}

//--------------------------------------------------------------------

void StorageFree(AIRFS_ Airfs, void* r)
{
    if (!r) return;
    StorageLock.Acquire();
    NODE_ release = (NODE_) r;
    int32_t Size = ((int32_t*)r)[-1];
    Attach(Airfs->Available, release, SizeCmp, &Size);
    Airfs->FreeSize += Size;
    StorageLock.Release();
}

//--------------------------------------------------------------------

void StorageAccessFile(StorageFileAccessType Type, NODE_ Node, int64_t AccessOffset, int64_t NumBytes, char* MemoryAddress)
{
    StorageLock.Acquire();

    NODE_ Block = Near(Node->FileBlocks, &AccessOffset, BlockCmp, LE);
    for (;;)
    {
        int32_t BlockSize   = ((int32_t*)Block)[-1];
        int64_t BlockOffset = Block->FileOffset;
        int64_t BlockIndex  = AccessOffset - BlockOffset + FILEBLOCK_OVERHEAD;
        int64_t BlockNum    = min(BlockSize-BlockIndex, NumBytes);

        switch (Type)
        {
          case ZERO  : { memset((char*)Block + BlockIndex, 0, BlockNum); break; }
          case READ  : { memcpy(MemoryAddress, (char*)Block + BlockIndex, BlockNum); break; }
          case WRITE : { memcpy((char*)Block + BlockIndex, MemoryAddress, BlockNum); break; }
        }
        NumBytes -= BlockNum;
        if (!NumBytes) break;
        MemoryAddress += BlockNum;
        AccessOffset += BlockNum;
        Block = Next(Block);
    }

    StorageLock.Release();
}

//--------------------------------------------------------------------

NTSTATUS StorageSetFileCapacity(AIRFS_ Airfs, NODE_ Node, int64_t minimumRequiredCapacity)
{
    StorageLock.Acquire();

    int64_t TargetCapacity = ROUND_UP(minimumRequiredCapacity, ALLOCATION_UNIT);
    NODE_   Block = Last(Node->FileBlocks);
    int32_t BlockSize = Block ? ((int32_t*)Block)[-1] : 0;
    int64_t CurrentCapacity = Block ? Block->FileOffset + BlockSize - FILEBLOCK_OVERHEAD: 0;
    int64_t Add = TargetCapacity - CurrentCapacity;

    while (Add > 0)
    {
        //  Add a block if we can, preferably as large or larger than we need.
        Add += FILEBLOCK_OVERHEAD;
        Block             = Near(Airfs->Available, &Add, SizeCmp, GE);
        if (!Block) Block = Near(Airfs->Available, &Add, SizeCmp, LT);
        Add -= FILEBLOCK_OVERHEAD;
        if (Block)
        {
            Detach(Airfs->Available, Block);
            BlockSize = ((int32_t*)Block)[-1];
            Airfs->FreeSize -= BlockSize;
            Block->FileOffset = CurrentCapacity;
            Attach(Node->FileBlocks, Block, BlockCmp, &CurrentCapacity);
            CurrentCapacity += BlockSize - FILEBLOCK_OVERHEAD;
            Add -= BlockSize - FILEBLOCK_OVERHEAD;
            continue;
        }

        StorageLock.Release();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //  Throw away any trailing blocks that are no longer needed.
    while (Add < 0)
    {
        Block = Last(Node->FileBlocks);
        BlockSize = ((int32_t*)Block)[-1];
        if (BlockSize - FILEBLOCK_OVERHEAD > -Add) break;
        Add += BlockSize - FILEBLOCK_OVERHEAD;
        Detach(Node->FileBlocks, Block);
        Attach(Airfs->Available, Block, SizeCmp, &BlockSize);
        Airfs->FreeSize += BlockSize;
    }

    //  Possibly downsize the last block.
    if (Add < 0)
    {
        Block = Last(Node->FileBlocks);
        int32_t OldBlockSize = ((int32_t*)Block)[-1];
        int32_t NewBlockSize = OldBlockSize - (int32_t) ROUND_DOWN(-Add, MINIMUM_ALLOCSIZE);
        if (NewBlockSize < MINIMUM_ALLOCSIZE) NewBlockSize = MINIMUM_ALLOCSIZE;
        int32_t RemainderBlockSize = OldBlockSize - NewBlockSize - sizeof int32_t;
        if (RemainderBlockSize >= MINIMUM_ALLOCSIZE)  //  i.e. if not too near the end
        {
            char* Addr = (char*)Block + NewBlockSize + sizeof int32_t;
            NODE_ Remainder = (NODE_) Addr;
            ((int32_t*)Remainder)[-1] = RemainderBlockSize;
            Attach(Airfs->Available, Remainder, SizeCmp, &RemainderBlockSize);
            Airfs->FreeSize += RemainderBlockSize;
            ((int32_t*)Block)[-1] = NewBlockSize;
        }
    }

    StorageLock.Release();
    return 0;
}

//--------------------------------------------------------------------

NTSTATUS StorageStartup(AIRFS_ &Airfs, WCHAR* MapName, WCHAR* StorageFileName, int64_t VolumeLength)
{
    HANDLE MapFileHandle = INVALID_HANDLE_VALUE;
    Airfs = 0;

    //  Open.
    if (*StorageFileName)
    {
        MapFileHandle = CreateFileW(StorageFileName, GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE, 0, NULL, OPEN_ALWAYS, NULL, NULL);
        if (MapFileHandle == INVALID_HANDLE_VALUE) return GetLastErrorAsStatus();
    }

    //  Map.
    HANDLE MapHandle = CreateFileMappingW(MapFileHandle, NULL, PAGE_EXECUTE_READWRITE, VolumeLength>>32, VolumeLength & 0xFFFFFFFF, MapName);
    if (!MapHandle) return GetLastErrorAsStatus();

    //  Point.
    char* MappedAddress = (char*) MapViewOfFile(MapHandle, FILE_MAP_ALL_ACCESS, 0, 0, VolumeLength);
    if (!MappedAddress) return GetLastErrorAsStatus();

    //  Keep.
    Airfs = (AIRFS_) MappedAddress;
    Airfs->MapFileHandle = MapFileHandle;
    Airfs->MapHandle = MapHandle;
    Airfs->VolumeLength = VolumeLength;

    return 0;
}

//--------------------------------------------------------------------

NTSTATUS StorageShutdown(AIRFS_ Airfs)
{
    BOOL Ok;
    NTSTATUS Result = 0;
    HANDLE M = Airfs->MapHandle;
    HANDLE F = Airfs->MapFileHandle;

    Ok = FlushViewOfFile(Airfs, 0);  if (!Ok && !Result) Result = GetLastErrorAsStatus();
    if (F != INVALID_HANDLE_VALUE)
    {
        Ok = FlushFileBuffers(F);    if (!Ok && !Result) Result = GetLastErrorAsStatus();
        //  TODO: Set and write a flag something like Airfs->UpdatesCompleted here?
    }
    Ok = UnmapViewOfFile(Airfs);     if (!Ok && !Result) Result = GetLastErrorAsStatus();
    
    if (M)
    {
        Ok = CloseHandle(M);         if (!Ok && !Result) Result = GetLastErrorAsStatus();
    }
    if (F != INVALID_HANDLE_VALUE)
    {
        Ok = CloseHandle(F);         if (!Ok && !Result) Result = GetLastErrorAsStatus();
    }

    return Result;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
