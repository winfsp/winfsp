/**
 * @file common.h
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

#include <winfsp/winfsp.h>
#include <io.h>
#include <sddl.h>
#include <stdio.h>
#include <stdint.h>

#define PROGNAME "airfs"
#define ROUND_UP(   bytes, units )  (((bytes) + (units) - 1) / (units) * (units))
#define ROUND_DOWN( bytes, units )  (((bytes)              ) / (units) * (units))
#define MINIMUM_ALLOCSIZE 196
#define MAXIMUM_ALLOCSIZE ROUND_DOWN(10*1024*1024, MINIMUM_ALLOCSIZE)
#define SECTOR_SIZE                   512
#define SECTORS_PER_ALLOCATION_UNIT     1
#define ALLOCATION_UNIT ( SECTOR_SIZE * SECTORS_PER_ALLOCATION_UNIT )
#define INFO(format, ...) FspServiceLog(EVENTLOG_INFORMATION_TYPE , format, __VA_ARGS__)
#define WARN(format, ...) FspServiceLog(EVENTLOG_WARNING_TYPE     , format, __VA_ARGS__)
#define FAIL(format, ...) FspServiceLog(EVENTLOG_ERROR_TYPE       , format, __VA_ARGS__)
#define AIRFS_MAX_PATH 512
#define FILEBLOCK_OVERHEAD 40  //  size of ( P + E + L + R + FileOffset ) = 8 * 5 = 40
#define ARG_TO_S(v) if (arge > ++argp) v = *argp; else goto usage
#define ARG_TO_4(v) if (arge > ++argp) v = (int32_t) wcstoll_default(*argp, v); else goto usage
#define ARG_TO_8(v) if (arge > ++argp) v =           wcstoll_default(*argp, v); else goto usage

enum StorageFileAccessType {ZERO=0,READ,WRITE};
enum Neighbor {LT=-2,LE=-1,EQ=0,GE=1,GT=2};

struct  NODE;
typedef NODE* NODE_;

typedef int CompareFunction (void* key,  NODE_);

inline NTSTATUS GetLastErrorAsStatus()
{
    return FspNtStatusFromWin32(GetLastError());
}

inline UINT64 SystemTime()
{
    FILETIME FileTime;
    GetSystemTimeAsFileTime(&FileTime);
    return ((PLARGE_INTEGER)&FileTime)->QuadPart;
}

static int64_t wcstoll_default(wchar_t *w, int64_t deflt)
{
    wchar_t *endp;
    int64_t i = wcstoll(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? i : deflt;
}

//////////////////////////////////////////////////////////////////////
//
//  Where<T> Class: This class manages an offset within our memory-mapped 
//  volume to another location within our memory-mapped volume. Because it is
//  a self-relative offset, this delta is constant regardless of where in
//  memory the file system is mapped, so we can always reobtain its address. 
//  A delta of 0 is the special case for "null".
//

template <class T> class Where
{
    int64_t delta;

  public:

    Where() = default;
   ~Where() = default;
    Where(T      t) : delta(  t      ?(  (char*)t          -(char*)this  ):0) {}
    Where(Where& w) : delta(  w.delta?( ((char*)&w+w.delta)-(char*)this  ):0) {}

      operator bool  () { return delta != 0; }
      operator T     () { return (T)    ( delta?(  (char*)this+delta   ):0); }
    T operator ->    () { return (T)    ( delta?(  (char*)this+delta   ):0); }
      operator void* () { return (void*)( delta?(  (char*)this+delta   ):0); }

    bool operator == (Where& rhs) { return (char*)this+delta == (char*)&rhs+rhs.delta; }
    bool operator != (Where& rhs) { return (char*)this+delta != (char*)&rhs+rhs.delta; }
    bool operator == (T rhs)      { return (char*)this+delta == (char*)rhs; }
    bool operator != (T rhs)      { return (char*)this+delta != (char*)rhs; }

    Where& operator = (Where& rhs) { delta = rhs.delta?(  ((char*)&rhs+rhs.delta) - ((char*)this)  ):0; return *this; }
    Where& operator = (void*  rhs) { delta = rhs      ?(   (char*)rhs             - ((char*)this)  ):0; return *this; }

    char* Address () { return (char*)this+delta; }
};

//////////////////////////////////////////////////////////////////////
//
//  The header for an Airfs volume
//

typedef struct
{
    char         Signature[8];        //  Airfs\0\0\0
    char         MapFormatVersion[4]; //  Major.Minor.Patch.Build
    char         filler[4];
    Where<NODE_> Root;
    Where<NODE_> Available;
    UINT64       VolumeSize;
    UINT64       FreeSize;
    WCHAR        VolumeLabel[32];
    UINT16       VolumeLabelLength;
    UINT16       filler1,filler2,filler3;
    UINT32       CaseInsensitive;
    UINT32       filler4;
    int64_t      VolumeLength;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    FSP_FILE_SYSTEM *FileSystem;
    HANDLE       MapFileHandle;
    HANDLE       MapHandle;
} AIRFS, *AIRFS_;

//////////////////////////////////////////////////////////////////////
//
//  Information per file or directory
//
struct NODE
{
    Where<NODE_> P,L,R,E;  //  Sorted sibling tree: Parent, Left, Right, and Equal
    union
    {
        Where<WCHAR*> Name;
        int64_t FileOffset;
    };
    Where<NODE_>  Parent;
    Where<NODE_>  Children;
    FSP_FSCTL_FILE_INFO FileInfo;
    uint64_t      SecurityDescriptorSize;
    Where<char*>  SecurityDescriptor;
    Where<NODE_>  FileBlocks;
    uint64_t      ReparseDataSize;
    Where<char*>  ReparseData;
    volatile LONG RefCount;
    Where<NODE_>  Streams;
    BOOLEAN       IsAStream;
};

//////////////////////////////////////////////////////////////////////

class SpinLock
{
    LONG   C;  //  Counter
    HANDLE S;  //  Semaphore

  public:

    SpinLock() { C = 0; S = CreateSemaphore(NULL, 0, 1, NULL); }
   ~SpinLock() { CloseHandle(S); }

    void Acquire() { if (_InterlockedIncrement(&C) > 1) WaitForSingleObject(S, INFINITE); }
    void Release() { if (_InterlockedDecrement(&C) > 0) ReleaseSemaphore(S, 1, NULL); }
};

//////////////////////////////////////////////////////////////////////

void Airprint (const char * format, ...);

int           SizeCmp (void* key,  NODE_);
int      ExactNameCmp (void* key,  NODE_);
int   CaselessNameCmp (void* key,  NODE_);

NODE_ Find   (Where<NODE_> &root, void* key, CompareFunction);
NODE_ Near   (Where<NODE_> &root, void* key, CompareFunction, Neighbor);
void  Attach (Where<NODE_> &root, NODE_ attach, CompareFunction, void* key);
void  Detach (Where<NODE_> &root, NODE_ detach);
NODE_ First  (NODE_ start);
NODE_ Last   (NODE_ start);
NODE_ Next   (NODE_);
NODE_ Prev   (NODE_);

NTSTATUS StorageStartup         (AIRFS_ &, WCHAR* MapName, WCHAR* StorageFileName, int64_t Length);
NTSTATUS StorageShutdown        (AIRFS_);
void*    StorageAllocate        (AIRFS_, int64_t RequestedSize);
void*    StorageReallocate      (AIRFS_, void* Reallocate, int64_t RequestedSize);
void     StorageFree            (AIRFS_, void* Release);
NTSTATUS StorageSetFileCapacity (AIRFS_, NODE_, int64_t MinimumRequiredCapacity);
void     StorageAccessFile      (StorageFileAccessType, NODE_, int64_t Offset, int64_t NumBytes, char* Address);

static_assert(AIRFS_MAX_PATH > MAX_PATH, "AIRFS_MAX_PATH must be greater than MAX_PATH.");
static_assert(sizeof NODE + sizeof int32_t == MINIMUM_ALLOCSIZE, "MINIMUM_ALLOCSIZE should be 196.");

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
