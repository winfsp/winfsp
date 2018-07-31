/**
 * @file airfs.cpp
 *
 * @copyright 2015-2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */
/*
 * Airfs is based on Memfs with changes contributed by John Oberschelp.
 * The contributed changes are under joint copyright by Bill Zissimopoulos
 * and John Oberschelp per the Contributor Agreement found at the
 * root of this project.
 */

#include <winfsp/winfsp.h>
#include <sddl.h>
#include <VersionHelpers.h>
#include <cassert>
#include <set>
#include <mutex>
#include <thread>       //  Used by SLOWIO.

#define AIRFS_MAX_PATH 512
FSP_FSCTL_STATIC_ASSERT(AIRFS_MAX_PATH > MAX_PATH,
    "AIRFS_MAX_PATH must be greater than MAX_PATH.");

#define AIRFS_NAME_NORMALIZATION    //  Include name normalization support.
#define AIRFS_REPARSE_POINTS        //  Include reparse points support.
#define AIRFS_NAMED_STREAMS         //  Include alternate data streams support.
#define AIRFS_DIRINFO_BY_NAME       //  Include GetDirInfoByName.
#define AIRFS_SLOWIO                //  Include delayed I/O response support.
#define AIRFS_CONTROL               //  Include DeviceIoControl support.


#define SECTOR_SIZE                   512
#define SECTORS_PER_ALLOCATION_UNIT     1
#define ALLOCATION_UNIT             ( SECTOR_SIZE * SECTORS_PER_ALLOCATION_UNIT )
#define IN_ALLOCATION_UNITS(bytes)  (((bytes) + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT)

enum
{
    AirfsDisk                   = 0x00000000,
    AirfsNet                    = 0x00000001,
    AirfsDeviceMask             = 0x0000000f,
    AirfsCaseInsensitive        = 0x80000000,
    AirfsFlushAndPurgeOnCleanup = 0x40000000,
};

//////////////////////////////////////////////////////////////////////

//  Heap Support

static HANDLE  AirfsHeap = 0;
std::once_flag AirfsHeapInitOnceFlag;

static inline BOOLEAN AirfsHeapInitialize()
{
    std::call_once(AirfsHeapInitOnceFlag, [](){ AirfsHeap = HeapCreate(0, 0, 0); });
    return AirfsHeap != 0;
}

static inline PVOID AirfsHeapAlloc(SIZE_T Size)
{
    return HeapAlloc(AirfsHeap, 0, Size);
}

static inline PVOID AirfsHeapRealloc(PVOID Pointer, SIZE_T RequestedSize)
{
    if (!Pointer)
    {
        if (!RequestedSize) return 0;
        return HeapAlloc(AirfsHeap, 0, RequestedSize);
    }
    if (!RequestedSize) return HeapFree(AirfsHeap, 0, Pointer), 0;
    return HeapReAlloc(AirfsHeap, 0, Pointer, RequestedSize);
}

static inline void AirfsHeapFree(PVOID Pointer)
{
    if (Pointer) HeapFree(AirfsHeap, 0, Pointer);
}

static inline SIZE_T AirfsHeapSize(PVOID Pointer)
{
    if (!Pointer) return 0;
    return HeapSize(AirfsHeap, 0, Pointer);
}

//////////////////////////////////////////////////////////////////////

typedef struct NODE NODE, *NODE_;

struct NODE_LESS
{ 
    bool operator() (const NODE_ NodeA, const NODE_ NodeB) const
    {
        WCHAR* a = (WCHAR*) NodeA;
        WCHAR* b = (WCHAR*) NodeB;
        return (CaseInsensitive ? _wcsicmp(a, b) : wcscmp(a, b)) < 0;
    }
    NODE_LESS(BOOLEAN Insensitive) : CaseInsensitive(Insensitive){}
    BOOLEAN CaseInsensitive;
};

typedef std::set<NODE_, NODE_LESS> NODES, *NODES_;

//////////////////////////////////////////////////////////////////////

struct NODE
{
    WCHAR Name[AIRFS_MAX_PATH];
    NODE_ Parent;
    NODES_ Children;
    FSP_FSCTL_FILE_INFO FileInfo;
    SIZE_T SecurityDescriptorSize;
    PVOID SecurityDescriptor;
    PVOID FileData;
#if defined(AIRFS_REPARSE_POINTS)
    SIZE_T ReparseDataSize;
    PVOID ReparseData;
#endif
    volatile LONG RefCount;
#if defined(AIRFS_NAMED_STREAMS)
    NODES_ Streams;
    BOOLEAN IsAStream;
#endif
};

//////////////////////////////////////////////////////////////////////

typedef struct
{
    FSP_FILE_SYSTEM *FileSystem;
    NODE_ Root;
    ULONG NumNodes;
    ULONG MaxNodes;
    ULONG MaxFileSize;
#ifdef AIRFS_SLOWIO
    ULONG SlowioMaxDelay;
    ULONG SlowioPercentDelay;
    ULONG SlowioRarefyDelay;
    volatile LONG SlowioThreadsRunning;
#endif
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[32];
    BOOLEAN CaseInsensitive;
} AIRFS, *AIRFS_;

//////////////////////////////////////////////////////////////////////

inline UINT64 GetSystemTime()
{
    FILETIME FileTime;
    GetSystemTimeAsFileTime(&FileTime);
    return ((PLARGE_INTEGER)&FileTime)->QuadPart;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS CreateNode(AIRFS_ Airfs, PWSTR Name, NODE_ *PNode)
{
    static UINT64 IndexNumber = 1;
    NODE_ Node = (NODE_) malloc(sizeof *Node);
    if (!Node)
    {
        *PNode = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(Node, 0, sizeof *Node);
    wcscpy_s(Node->Name, sizeof Node->Name / sizeof(WCHAR), Name);
    Node->FileInfo.CreationTime =
    Node->FileInfo.LastAccessTime =
    Node->FileInfo.LastWriteTime =
    Node->FileInfo.ChangeTime = GetSystemTime();
    Node->FileInfo.IndexNumber = IndexNumber++;

    *PNode = Node;
    Airfs->NumNodes++;
    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

void DeleteNode(AIRFS_ Airfs, NODE_ Node)
{
#if defined(AIRFS_REPARSE_POINTS)
    free(Node->ReparseData);
#endif
    AirfsHeapFree(Node->FileData);
    free(Node->SecurityDescriptor);

    if (Node->Children)
    {
        assert(Node->Children->empty());
        delete Node->Children;
    }

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->Streams)
    {
        assert(Node->Streams->empty());
        delete Node->Streams;
    }
#endif

    free(Node);
    Airfs->NumNodes--;
}

//////////////////////////////////////////////////////////////////////

inline void ReferenceNode(NODE_ Node)
{
    InterlockedIncrement(&Node->RefCount);
}

//////////////////////////////////////////////////////////////////////

inline void DereferenceNode(AIRFS_ Airfs, NODE_ Node)
{
    if (InterlockedDecrement(&Node->RefCount) == 0)
        DeleteNode(Airfs, Node);
}

//////////////////////////////////////////////////////////////////////

void GetFileInfo(NODE_ Node, FSP_FSCTL_FILE_INFO *FileInfo)
{
#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream)
    {
        *FileInfo = Node->Parent->FileInfo;
        FileInfo->FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
        //  Named streams cannot be directories.
        FileInfo->AllocationSize = Node->FileInfo.AllocationSize;
        FileInfo->FileSize = Node->FileInfo.FileSize;
    }
    else
#endif
        *FileInfo = Node->FileInfo;
}

//////////////////////////////////////////////////////////////////////

void DumpNodes(NODE_ Node, int indent=0)
{
    UINT32 dir = FILE_ATTRIBUTE_DIRECTORY & Node->FileInfo.FileAttributes;
    printf("%c%3d    ", dir ? 'd' : 'f', (int)Node->RefCount);
    for (int i = indent; i; i--) putchar('\t');
    printf("\"%S\"\n", Node->Name);
    if (Node->Children)
        for (auto& Child : *Node->Children)
             DumpNodes(Child, indent+1);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS CreateNodeSet(BOOLEAN CaseInsensitive, NODES_ *PNodeSet)
{
    try
    {
        *PNodeSet = new NODES(NODE_LESS(CaseInsensitive));
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        *PNodeSet = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
}

//////////////////////////////////////////////////////////////////////

void DeleteAllNodes(AIRFS_ Airfs)
{
    NODE_ Node = Airfs->Root;
    while (Node)
    {
        if (!Node->Children->empty())
        {
            Node = *Node->Children->begin();
        }
        else
        {
#if defined(AIRFS_NAMED_STREAMS)
            if (Node->Streams)
            {
                for (auto Iter = Node->Streams->begin(); Iter != Node->Streams->end(); )
                {
                    NODE_ Stream = *Iter++;
                    DeleteNode(Airfs, Stream);
                }
                delete Node->Streams;
            }
#endif
            NODE_ Parent = Node->Parent;
            DeleteNode(Airfs, Node);
            Node = Parent;
        }
    }
}

//////////////////////////////////////////////////////////////////////

NTSTATUS FindNode(AIRFS_ Airfs, PWSTR Name, PWSTR *BaseName,
    NODE_ *PParent, NODE_ *PNode)
{
    //  Special case root.
    if (Name[0] == 0 || Name[1] == 0)
    {
        if (BaseName) *BaseName = Name;
        if (PParent) *PParent = Airfs->Root;
        *PNode = Airfs->Root;
        return 0;
    }

    WCHAR ParsedName[AIRFS_MAX_PATH];
    wcscpy_s(ParsedName, sizeof ParsedName / sizeof(WCHAR), Name);

    //  From root, for each ancestor...
    NODE_ Ancestor = Airfs->Root;
    WCHAR* fm;
    WCHAR* to = ParsedName;
#if defined(AIRFS_NAMED_STREAMS)
    WCHAR* Colon = 0;
#endif
    for (;;)
    {
        //  Isolate the next base name.
        for (fm = to+1; *fm == L'\\'; fm++) {}
        for (to = fm; *to != L'\0' && *to != L'\\'; to++)
            if (*to == ':'){Colon = to; break;}
        if (*to == 0) break;
        *to = 0;

        //  Find this name.
        auto Iter = Ancestor->Children->find((NODE_)fm);
        if (Iter == Ancestor->Children->end())
        {
            if (PParent) *PParent = 0;
            *PNode = 0;
#if defined(AIRFS_NAMED_STREAMS)
            if (Colon) return STATUS_OBJECT_NAME_NOT_FOUND;
#endif
            return STATUS_OBJECT_PATH_NOT_FOUND;
        }

        Ancestor = *Iter;

#if defined(AIRFS_NAMED_STREAMS)
        if (Colon)
        {
            fm = to+1;
            break;
        }
#endif

        if (!(Ancestor->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (PParent) *PParent = 0;
            *PNode = 0;
            return STATUS_NOT_A_DIRECTORY;
        }
    }

    if (BaseName)
        *BaseName = Name + (  (  (UINT_PTR)fm - (UINT_PTR)ParsedName  )  / sizeof(WCHAR));

    if (PParent)
        *PParent = Ancestor;

#if defined(AIRFS_NAMED_STREAMS)
    if (Colon)
    {
        //  Find the stream, if it exists.
        if (!Ancestor->Streams)
        {
            *PNode = 0;
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        auto Iter = Ancestor->Streams->find((NODE_)fm);
        if (Iter == Ancestor->Streams->end())
        {
            *PNode = 0;
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        *PNode = *Iter;
        return 0;
    }
#endif

    //  Find the directory entry, if it exists.
    auto Iter = Ancestor->Children->find((NODE_)fm);
    if (Iter == Ancestor->Children->end())
    {
        *PNode = 0;
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    *PNode = *Iter;
    return 0;
}

//////////////////////////////////////////////////////////////////////
#if defined(AIRFS_NAMED_STREAMS)

BOOLEAN AddStreamInfo(NODE_ Node, PWSTR StreamName, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    UINT8 StreamInfoBuf[sizeof(FSP_FSCTL_STREAM_INFO) + sizeof Node->Name];
    FSP_FSCTL_STREAM_INFO *StreamInfo = (FSP_FSCTL_STREAM_INFO *)StreamInfoBuf;

    StreamInfo->Size = (UINT16)(sizeof(FSP_FSCTL_STREAM_INFO) + wcslen(StreamName) * sizeof(WCHAR));
    StreamInfo->StreamSize = Node->FileInfo.FileSize;
    StreamInfo->StreamAllocationSize = Node->FileInfo.AllocationSize;
    memcpy(StreamInfo->StreamNameBuf, StreamName, StreamInfo->Size - sizeof(FSP_FSCTL_STREAM_INFO));

    return FspFileSystemAddStreamInfo(StreamInfo, Buffer, Length, PBytesTransferred);
}

#endif
//////////////////////////////////////////////////////////////////////

inline void TouchNode(NODE_ Node)
{
    Node->FileInfo.LastAccessTime =
    Node->FileInfo.LastWriteTime =
    Node->FileInfo.ChangeTime = GetSystemTime();
}

//////////////////////////////////////////////////////////////////////

NTSTATUS InsertNode(AIRFS_ Airfs, NODE_ Parent, NODE_ Node, PBOOLEAN PInserted)
{
    try
    {
#if defined(AIRFS_NAMED_STREAMS)
        if (Node->IsAStream)
        {
            if (!Parent->Streams)
            {
                NTSTATUS Result = CreateNodeSet(Airfs->CaseInsensitive, &Parent->Streams);
                if (Result) return Result;
            }
            *PInserted = Parent->Streams->insert(Node).second;
        }
        else
#endif
            *PInserted = Parent->Children->insert(Node).second;
        if (*PInserted)
        {
            Node->Parent = Parent;
            ReferenceNode(Node);
            TouchNode(Parent);
        }
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        *PInserted = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
}

//////////////////////////////////////////////////////////////////////

void RemoveNode(AIRFS_ Airfs, NODE_ Node)
{
    NODE_ Parent = Node->Parent;
    TouchNode(Parent);

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream)
    {
        if (Parent->Streams)
        {
            auto found = Parent->Streams->find(Node);
            if (found != Parent->Streams->end())
                Parent->Streams->erase(found);
        }
    }
    else
#endif
        Parent->Children->erase(Node);

    DereferenceNode(Airfs, Node);
}

//////////////////////////////////////////////////////////////////////

inline BOOLEAN NodeHasChildren(NODE_ Node)
{
    return Node->Children && !Node->Children->empty();
}

//////////////////////////////////////////////////////////////////////

#ifdef AIRFS_SLOWIO
/*
 * SLOWIO
 *
 * This is included for two uses:
 *
 * 1) For testing winfsp, by allowing Airfs to act more like a non-ram file system,
 *    with some IO taking many milliseconds, and some IO completion delayed.
 *
 * 2) As sample code for how to use winfsp's STATUS_PENDING capabilities.
 * 
 */

inline UINT64 Hash(UINT64 x)
{
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    x =  x ^ (x >> 31);
    return x;
}

inline ULONG PseudoRandom(ULONG to)
{
    //  John Oberschelp's PRNG
    static UINT64 spin = 0;
    InterlockedIncrement(&spin);
    return Hash(spin) % to;
}

inline BOOLEAN SlowioReturnPending(FSP_FILE_SYSTEM *FileSystem)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    if (!Airfs->SlowioMaxDelay)
        return FALSE;
    return PseudoRandom(100) < Airfs->SlowioPercentDelay;
}

inline void SlowioSnooze(FSP_FILE_SYSTEM *FileSystem)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    if (!Airfs->SlowioMaxDelay)
        return;
    ULONG millis = PseudoRandom(Airfs->SlowioMaxDelay + 1) >> PseudoRandom(Airfs->SlowioRarefyDelay + 1);
    Sleep(millis);
}

void SlowioReadThread(
    FSP_FILE_SYSTEM *FileSystem,
    NODE_ Node,
    PVOID Buffer, 
    UINT64 Offset, 
    UINT64 EndOffset,
    UINT64 RequestHint)
{
    SlowioSnooze(FileSystem);

    memcpy(Buffer, (PUINT8)Node->FileData + Offset, (size_t)(EndOffset - Offset));
    UINT32 BytesTransferred = (ULONG)(EndOffset - Offset);

    FSP_FSCTL_TRANSACT_RSP ResponseBuf;
    memset(&ResponseBuf, 0, sizeof ResponseBuf);
    ResponseBuf.Size = sizeof ResponseBuf;
    ResponseBuf.Kind = FspFsctlTransactReadKind;
    ResponseBuf.Hint = RequestHint;                         // IRP that is being completed
    ResponseBuf.IoStatus.Status = STATUS_SUCCESS;
    ResponseBuf.IoStatus.Information = BytesTransferred;    // bytes read
    FspFileSystemSendResponse(FileSystem, &ResponseBuf);

    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    InterlockedDecrement(&Airfs->SlowioThreadsRunning);
}

void SlowioWriteThread(
    FSP_FILE_SYSTEM *FileSystem,
    NODE_ Node,
    PVOID Buffer, 
    UINT64 Offset, 
    UINT64 EndOffset,
    UINT64 RequestHint)
{
    SlowioSnooze(FileSystem);

    memcpy((PUINT8)Node->FileData + Offset, Buffer, (size_t)(EndOffset - Offset));
    UINT32 BytesTransferred = (ULONG)(EndOffset - Offset);

    FSP_FSCTL_TRANSACT_RSP ResponseBuf;
    memset(&ResponseBuf, 0, sizeof ResponseBuf);
    ResponseBuf.Size = sizeof ResponseBuf;
    ResponseBuf.Kind = FspFsctlTransactWriteKind;
    ResponseBuf.Hint = RequestHint;                         // IRP that is being completed
    ResponseBuf.IoStatus.Status = STATUS_SUCCESS;
    ResponseBuf.IoStatus.Information = BytesTransferred;    // bytes written
    GetFileInfo(Node, &ResponseBuf.Rsp.Write.FileInfo);
    FspFileSystemSendResponse(FileSystem, &ResponseBuf);

    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    InterlockedDecrement(&Airfs->SlowioThreadsRunning);
}

void SlowioReadDirectoryThread(
    FSP_FILE_SYSTEM *FileSystem,
    ULONG BytesTransferred,
    UINT64 RequestHint)
{
    SlowioSnooze(FileSystem);

    FSP_FSCTL_TRANSACT_RSP ResponseBuf;
    memset(&ResponseBuf, 0, sizeof ResponseBuf);
    ResponseBuf.Size = sizeof ResponseBuf;
    ResponseBuf.Kind = FspFsctlTransactQueryDirectoryKind;
    ResponseBuf.Hint = RequestHint;                         // IRP that is being completed
    ResponseBuf.IoStatus.Status = STATUS_SUCCESS;
    ResponseBuf.IoStatus.Information = BytesTransferred;    // bytes of directory info read
    FspFileSystemSendResponse(FileSystem, &ResponseBuf);

    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    InterlockedDecrement(&Airfs->SlowioThreadsRunning);
}
#endif
//////////////////////////////////////////////////////////////////////
#if defined(AIRFS_REPARSE_POINTS)

NTSTATUS GetReparsePointByName(FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR Name, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node;

#if defined(AIRFS_NAMED_STREAMS)
    //  GetReparsePointByName will never receive a named stream.
    assert(wcschr(Name, L':') == 0);
#endif

    NTSTATUS Result = FindNode(Airfs, Name, 0, 0, &Node);

    if (!Node)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (!(Node->FileInfo.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return STATUS_NOT_A_REPARSE_POINT;

    if (Buffer)
    {
        if (Node->ReparseDataSize > *PSize)
            return STATUS_BUFFER_TOO_SMALL;

        *PSize = Node->ReparseDataSize;
        memcpy(Buffer, Node->ReparseData, Node->ReparseDataSize);
    }

    return STATUS_SUCCESS;
}

#endif
//////////////////////////////////////////////////////////////////////

NTSTATUS SetAllocSize(FSP_FILE_SYSTEM *FileSystem, NODE_ Node,
    UINT64 RequestedAllocSize)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;

    RequestedAllocSize = IN_ALLOCATION_UNITS(RequestedAllocSize);

    if (Node->FileInfo.AllocationSize != RequestedAllocSize)
    {
        if (RequestedAllocSize > Airfs->MaxFileSize) return STATUS_DISK_FULL;

        //  Reallocate only if the file is made smaller, or if it will not fit in the actual memory footprint.
        size_t ActualSize = AirfsHeapSize(Node->FileData);
        if (RequestedAllocSize < Node->FileInfo.AllocationSize || RequestedAllocSize > ActualSize)
        {
            //  If the file grow request was modest, guess that it might happen again, and grow the file by 50%.
            if (RequestedAllocSize > Node->FileInfo.AllocationSize && RequestedAllocSize <= ActualSize + ActualSize / 8)
                RequestedAllocSize = IN_ALLOCATION_UNITS(ActualSize + ActualSize / 2);

            PVOID FileData = AirfsHeapRealloc(Node->FileData, (size_t)RequestedAllocSize);
            if (!FileData && RequestedAllocSize > 0)
                return STATUS_INSUFFICIENT_RESOURCES;

            Node->FileData = FileData;
        }

        Node->FileInfo.AllocationSize = RequestedAllocSize;
        if (Node->FileInfo.FileSize > RequestedAllocSize)
            Node->FileInfo.FileSize = RequestedAllocSize;
    }

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FileSystem, NODE_ Node,
    UINT64 RequestedFileSize)
{
    if (Node->FileInfo.FileSize != RequestedFileSize)
    {
        if (Node->FileInfo.AllocationSize < RequestedFileSize)
        {
            NTSTATUS Result = SetAllocSize(FileSystem, Node, RequestedFileSize);
            if (!NT_SUCCESS(Result))
                return Result;
        }

        if (Node->FileInfo.FileSize < RequestedFileSize)
            memset((PUINT8)Node->FileData + Node->FileInfo.FileSize, 0,
                (size_t)(RequestedFileSize - Node->FileInfo.FileSize));
        Node->FileInfo.FileSize = RequestedFileSize;
    }

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

BOOLEAN AddDirInfo(NODE_ Node, PWSTR Name, PVOID Buffer, ULONG Length,
    PULONG PBytesTransferred)
{
    UINT8 DirInfoBuf[sizeof(FSP_FSCTL_DIR_INFO) + sizeof Node->Name];
    FSP_FSCTL_DIR_INFO *DirInfo = (FSP_FSCTL_DIR_INFO *)DirInfoBuf;
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;

    if (!Name)
    {
        FspPathSuffix(Node->Name, &Remain, &Suffix, Root);
        Name = Suffix;
        FspPathCombine(Node->Name, Suffix);
    }

    memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(Name) * sizeof(WCHAR));
    DirInfo->FileInfo = Node->FileInfo;
    memcpy(DirInfo->FileNameBuf, Name, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));

    return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetVolumeInfo(FSP_FILE_SYSTEM *FileSystem, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;

    VolumeInfo->TotalSize = Airfs->MaxNodes * (UINT64)Airfs->MaxFileSize;
    VolumeInfo->FreeSize = (Airfs->MaxNodes - Airfs->NumNodes) *
        (UINT64)Airfs->MaxFileSize;
    VolumeInfo->VolumeLabelLength = Airfs->VolumeLabelLength;
    memcpy(VolumeInfo->VolumeLabel, Airfs->VolumeLabel, Airfs->VolumeLabelLength);
    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetVolumeLabel(FSP_FILE_SYSTEM *FileSystem, PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;

    Airfs->VolumeLabelLength = (UINT16)(wcslen(VolumeLabel) * sizeof(WCHAR));
    if (Airfs->VolumeLabelLength > sizeof Airfs->VolumeLabel)
        Airfs->VolumeLabelLength = sizeof Airfs->VolumeLabel;
    memcpy(Airfs->VolumeLabel, VolumeLabel, Airfs->VolumeLabelLength);

    VolumeInfo->TotalSize = Airfs->MaxNodes * Airfs->MaxFileSize;
    VolumeInfo->FreeSize =
        (Airfs->MaxNodes - Airfs->NumNodes) * Airfs->MaxFileSize;
    VolumeInfo->VolumeLabelLength = Airfs->VolumeLabelLength;
    memcpy(VolumeInfo->VolumeLabel, Airfs->VolumeLabel, Airfs->VolumeLabelLength);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetSecurityByName(FSP_FILE_SYSTEM *FileSystem, PWSTR Name, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node;
    NTSTATUS Result = FindNode(Airfs, Name, 0, 0, &Node);
    if (!Node)
    {
#if defined(AIRFS_REPARSE_POINTS)
        if (FspFileSystemFindReparsePoint(FileSystem, GetReparsePointByName, 0,
            Name, PFileAttributes))
        {
            return STATUS_REPARSE;
        }
#endif
        return Result;
    }

#if defined(AIRFS_NAMED_STREAMS)
    UINT32 FileAttributesMask = ~(UINT32)0;
    if (Node->IsAStream)
    {
        FileAttributesMask = ~(UINT32)FILE_ATTRIBUTE_DIRECTORY;
        Node = Node->Parent;
    }

    if (PFileAttributes)
        *PFileAttributes = Node->FileInfo.FileAttributes & FileAttributesMask;
#else
    if (PFileAttributes)
        *PFileAttributes = Node->FileInfo.FileAttributes;
#endif

    if (PSecurityDescriptorSize)
    {
        if (Node->SecurityDescriptorSize > *PSecurityDescriptorSize)
        {
            *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
            return STATUS_BUFFER_OVERFLOW;
        }

        *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
        if (SecurityDescriptor)
            memcpy(SecurityDescriptor, Node->SecurityDescriptor, Node->SecurityDescriptorSize);
    }

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiCreate(FSP_FILE_SYSTEM *FileSystem, PWSTR Name, UINT32 CreateOptions,
    UINT32 GrantedAccess, UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor,
    UINT64 AllocationSize, PVOID *PNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node;
    NODE_ Parent;
    NTSTATUS Result;
    BOOLEAN Inserted;
    PWSTR BaseName;

    if (AIRFS_MAX_PATH <= wcslen(Name))
        return STATUS_OBJECT_NAME_INVALID;

    if (CreateOptions & FILE_DIRECTORY_FILE)
        AllocationSize = 0;

    Result = FindNode(Airfs, Name, &BaseName, &Parent, &Node);

    if (Node)
        return STATUS_OBJECT_NAME_COLLISION;

    if (!Parent)
        return Result;

    if (Airfs->NumNodes >= Airfs->MaxNodes)
        return STATUS_CANNOT_MAKE;

    if (AllocationSize > Airfs->MaxFileSize)
        return STATUS_DISK_FULL;

    Result = CreateNode(Airfs, BaseName, &Node);
    if (!NT_SUCCESS(Result))
        return Result;

#if defined(AIRFS_NAMED_STREAMS)
    Node->IsAStream = BaseName[-1] == L':';
#endif

    if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        Result = CreateNodeSet(Airfs->CaseInsensitive, &Node->Children);
        if (Result)
        {
            DeleteNode(Airfs, Node);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    Node->FileInfo.FileAttributes = (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
        FileAttributes : FileAttributes | FILE_ATTRIBUTE_ARCHIVE;

    if (SecurityDescriptor)
    {
        Node->SecurityDescriptorSize = GetSecurityDescriptorLength(SecurityDescriptor);
        Node->SecurityDescriptor = (PSECURITY_DESCRIPTOR)malloc(Node->SecurityDescriptorSize);
        if (!Node->SecurityDescriptor)
        {
            DeleteNode(Airfs, Node);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        memcpy(Node->SecurityDescriptor, SecurityDescriptor, Node->SecurityDescriptorSize);
    }

    Node->FileInfo.AllocationSize = AllocationSize;
    if (Node->FileInfo.AllocationSize)
    {
        Node->FileData = AirfsHeapAlloc((size_t)Node->FileInfo.AllocationSize);
        if (!Node->FileData)
        {
            DeleteNode(Airfs, Node);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    Result = InsertNode(Airfs, Parent, Node, &Inserted);
    if (!NT_SUCCESS(Result) || !Inserted)
    {
        DeleteNode(Airfs, Node);
        if (NT_SUCCESS(Result))
            Result = STATUS_OBJECT_NAME_COLLISION; //  Should not happen!
        return Result;
    }

    ReferenceNode(Node);
    *PNode = Node;
    GetFileInfo(Node, FileInfo);

#if defined(AIRFS_NAME_NORMALIZATION)
    if (Airfs->CaseInsensitive)
    {
        int ParentPathNumBytes = (int)(  (char*)BaseName - (char*)Name  );
        int NodeNameNumBytes = (int)(  wcslen(Node->Name) * sizeof(WCHAR)  );

        FSP_FSCTL_OPEN_FILE_INFO *OpenFileInfo = FspFileSystemGetOpenFileInfo(FileInfo);
        memcpy(OpenFileInfo->NormalizedName, Name, ParentPathNumBytes);
        memcpy(OpenFileInfo->NormalizedName+ParentPathNumBytes / sizeof(WCHAR), Node->Name, NodeNameNumBytes);
        OpenFileInfo->NormalizedNameSize = ParentPathNumBytes + NodeNameNumBytes;
    }
#endif

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiOpen(FSP_FILE_SYSTEM *FileSystem, PWSTR Name, UINT32 CreateOptions,
    UINT32 GrantedAccess, PVOID *PNode, FSP_FSCTL_FILE_INFO *FileInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node;
    NTSTATUS Result;

    if (AIRFS_MAX_PATH <= wcslen(Name))
        return STATUS_OBJECT_NAME_INVALID;

    PWSTR baseName;
    Result = FindNode(Airfs, Name, &baseName, 0, &Node);
    if (Result) return Result;
    if (!Node)
    {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    ReferenceNode(Node);
    *PNode = Node;
    GetFileInfo(Node, FileInfo);

#if defined(AIRFS_NAME_NORMALIZATION)
    if (Airfs->CaseInsensitive)
    {
        int ParentPathNumBytes = (int)(  (char*)baseName - (char*)Name  );
        int NodeNameNumBytes = (int)(  wcslen(Node->Name) * sizeof(WCHAR)  );

        FSP_FSCTL_OPEN_FILE_INFO *OpenFileInfo = FspFileSystemGetOpenFileInfo(FileInfo);
        memcpy(OpenFileInfo->NormalizedName, Name, ParentPathNumBytes);
        memcpy(OpenFileInfo->NormalizedName+ParentPathNumBytes / sizeof(WCHAR), Node->Name, NodeNameNumBytes);
        OpenFileInfo->NormalizedNameSize = ParentPathNumBytes + NodeNameNumBytes;
    }
#endif

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiOverwrite(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, UINT32 FileAttributes,
    BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize, FSP_FSCTL_FILE_INFO *FileInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    NTSTATUS Result;

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->Streams)
    {
        for (auto Iter = Node->Streams->begin(); Iter != Node->Streams->end(); )
        {
            NODE_ Stream = *Iter++;
            LONG RefCount = Stream->RefCount;
            MemoryBarrier();
            if (RefCount <= 1)
            {
                RemoveNode(Airfs, Stream);
            }
        }
        if (Node->Streams->empty())
        {
            delete Node->Streams;
            Node->Streams = 0;
        }
    }
#endif

    Result = SetAllocSize(FileSystem, Node, AllocationSize);
    if (!NT_SUCCESS(Result))
        return Result;

    if (ReplaceFileAttributes)
        Node->FileInfo.FileAttributes = FileAttributes | FILE_ATTRIBUTE_ARCHIVE;
    else
        Node->FileInfo.FileAttributes |= FileAttributes | FILE_ATTRIBUTE_ARCHIVE;

    Node->FileInfo.FileSize = 0;
    Node->FileInfo.LastAccessTime =
    Node->FileInfo.LastWriteTime =
    Node->FileInfo.ChangeTime = GetSystemTime();

    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

void ApiCleanup(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PWSTR Name, ULONG Flags)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
#if defined(AIRFS_NAMED_STREAMS)
    NODE_ MainNode = Node->IsAStream ? Node->Parent : Node;
#else
    NODE_ MainNode = Node;
#endif

    assert(Flags);  //  FSP_FSCTL_VOLUME_PARAMS::PostCleanupWhenModifiedOnly ensures this.

    if (Flags & FspCleanupSetArchiveBit)
    {
        if (!(MainNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            MainNode->FileInfo.FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    }

    if (Flags & (FspCleanupSetLastAccessTime | FspCleanupSetLastWriteTime | FspCleanupSetChangeTime))
    {
        UINT64 SystemTime = GetSystemTime();

        if (Flags & FspCleanupSetLastAccessTime)
            MainNode->FileInfo.LastAccessTime = SystemTime;
        if (Flags & FspCleanupSetLastWriteTime)
            MainNode->FileInfo.LastWriteTime = SystemTime;
        if (Flags & FspCleanupSetChangeTime)
            MainNode->FileInfo.ChangeTime = SystemTime;
    }

    if (Flags & FspCleanupSetAllocationSize)
    {
        SetAllocSize(FileSystem, Node, Node->FileInfo.FileSize);
    }

    if ((Flags & FspCleanupDelete) && !NodeHasChildren(Node))
    {

#if defined(AIRFS_NAMED_STREAMS)
        if (Node->Streams)
        {
            for (auto Iter = Node->Streams->begin(); Iter != Node->Streams->end(); )
            {
                NODE_ Stream = *Iter++;
                DeleteNode(Airfs, Stream);
            }
            delete Node->Streams;
            Node->Streams = 0;
        }
#endif

        RemoveNode(Airfs, Node);
    }
}

//////////////////////////////////////////////////////////////////////

void ApiClose(FSP_FILE_SYSTEM *FileSystem, PVOID Node0)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    DereferenceNode(Airfs, Node);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiRead(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PVOID Buffer,
    UINT64 Offset, ULONG Length, PULONG PBytesTransferred)
{
    NODE_ Node = (NODE_) Node0;
    UINT64 EndOffset;

    if (Offset >= Node->FileInfo.FileSize)
        return STATUS_END_OF_FILE;

    EndOffset = Offset + Length;
    if (EndOffset > Node->FileInfo.FileSize)
        EndOffset = Node->FileInfo.FileSize;

#ifdef AIRFS_SLOWIO
    if (SlowioReturnPending(FileSystem))
    {
        AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
        try
        {
            InterlockedIncrement(&Airfs->SlowioThreadsRunning);
            std::thread(SlowioReadThread,
                FileSystem, Node, Buffer, Offset, EndOffset,
                FspFileSystemGetOperationContext()->Request->Hint).
                detach();
            return STATUS_PENDING;
        }
        catch (...)
        {
            InterlockedDecrement(&Airfs->SlowioThreadsRunning);
        }
    }
    SlowioSnooze(FileSystem);
#endif

    memcpy(Buffer, (PUINT8)Node->FileData + Offset, (size_t)(EndOffset - Offset));

    *PBytesTransferred = (ULONG)(EndOffset - Offset);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiWrite(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PVOID Buffer,
    UINT64 Offset, ULONG Length, BOOLEAN WriteToEndOfFile,
    BOOLEAN ConstrainedIo, PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    NODE_ Node = (NODE_) Node0;
    UINT64 EndOffset;
    NTSTATUS Result;

    if (ConstrainedIo)
    {
        if (Offset >= Node->FileInfo.FileSize)
            return STATUS_SUCCESS;
        EndOffset = Offset + Length;
        if (EndOffset > Node->FileInfo.FileSize)
            EndOffset = Node->FileInfo.FileSize;
    }
    else
    {
        if (WriteToEndOfFile)
            Offset = Node->FileInfo.FileSize;
        EndOffset = Offset + Length;
        if (EndOffset > Node->FileInfo.FileSize)
        {
            Result = SetFileSize(FileSystem, Node, EndOffset);
            if (!NT_SUCCESS(Result))
                return Result;
        }
    }

#ifdef AIRFS_SLOWIO
    if (SlowioReturnPending(FileSystem))
    {
        AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
        try
        {
            InterlockedIncrement(&Airfs->SlowioThreadsRunning);
            std::thread(SlowioWriteThread,
                FileSystem, Node, Buffer, Offset, EndOffset,
                FspFileSystemGetOperationContext()->Request->Hint).
                detach();
            return STATUS_PENDING;
        }
        catch (...)
        {
            InterlockedDecrement(&Airfs->SlowioThreadsRunning);
        }
    }
    SlowioSnooze(FileSystem);
#endif

    memcpy((PUINT8)Node->FileData + Offset, Buffer, (size_t)(EndOffset - Offset));

    *PBytesTransferred = (ULONG)(EndOffset - Offset);
    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiFlush(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    NODE_ Node = (NODE_) Node0;

    //  Nothing to flush, since we do not cache anything.

    if (Node)
    {
#if 0
#if defined(AIRFS_NAMED_STREAMS)
        if (Node->IsAStream)
            Node->MainNode->FileInfo.LastAccessTime =
            Node->MainNode->FileInfo.LastWriteTime =
            Node->MainNode->FileInfo.ChangeTime = GetSystemTime();
        else
#endif
        Node->FileInfo.LastAccessTime =
        Node->FileInfo.LastWriteTime =
        Node->FileInfo.ChangeTime = GetSystemTime();
#endif

        GetFileInfo(Node, FileInfo);
    }

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetFileInfo(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    NODE_ Node = (NODE_) Node0;

    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetBasicInfo(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    NODE_ Node = (NODE_) Node0;

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream) Node = Node->Parent;
#endif

    if (INVALID_FILE_ATTRIBUTES != FileAttributes)
                         Node->FileInfo.FileAttributes = FileAttributes;
    if (CreationTime   ) Node->FileInfo.CreationTime   = CreationTime;
    if (LastAccessTime ) Node->FileInfo.LastAccessTime = LastAccessTime;
    if (LastWriteTime  ) Node->FileInfo.LastWriteTime  = LastWriteTime;
    if (ChangeTime     ) Node->FileInfo.ChangeTime     = ChangeTime;

    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetFileSize(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    UINT64 NewSize, BOOLEAN SetAllocationSize, FSP_FSCTL_FILE_INFO *FileInfo)
{
    NODE_ Node = (NODE_) Node0;
    NTSTATUS Result = SetAllocationSize
        ? SetAllocSize(FileSystem, Node, NewSize)
        : SetFileSize(FileSystem, Node, NewSize);
    if (!NT_SUCCESS(Result))
        return Result;

    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiCanDelete(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PWSTR Name)
{
    NODE_ Node = (NODE_) Node0;

    if (NodeHasChildren(Node))
        return STATUS_DIRECTORY_NOT_EMPTY;

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiRename(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PWSTR Name,
    PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    NODE_ NewNode;
    BOOLEAN Inserted;
    NTSTATUS Result;
    PWSTR newBaseName;
    NODE_ NewParent;
    Result = FindNode(Airfs, NewFileName, &newBaseName, &NewParent, &NewNode);

    if (NewNode && Node != NewNode)
    {
        if (!ReplaceIfExists)
            return STATUS_OBJECT_NAME_COLLISION;

        if (NewNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return STATUS_ACCESS_DENIED;

        ReferenceNode(NewNode);
        RemoveNode(Airfs, NewNode);
        DereferenceNode(Airfs, NewNode);
    }

    ReferenceNode(Node);
    RemoveNode(Airfs, Node);
    wcscpy_s(Node->Name, sizeof Node->Name / sizeof(WCHAR), newBaseName);
    Result = InsertNode(Airfs, NewParent, Node, &Inserted);
    DereferenceNode(Airfs, Node);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    NODE_ Node = (NODE_) Node0;

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream) Node = Node->Parent;
#endif

    if (Node->SecurityDescriptorSize > *PSecurityDescriptorSize)
    {
        *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
        return STATUS_BUFFER_OVERFLOW;
    }

    *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
    if (SecurityDescriptor)
        memcpy(SecurityDescriptor, Node->SecurityDescriptor, Node->SecurityDescriptorSize);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    NODE_ Node = (NODE_) Node0;
    PSECURITY_DESCRIPTOR NewSecurityDescriptor, SecurityDescriptor;
    SIZE_T SecurityDescriptorSize;
    NTSTATUS Result;

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream) Node = Node->Parent;
#endif

    Result = FspSetSecurityDescriptor(
        Node->SecurityDescriptor,
        SecurityInformation,
        ModificationDescriptor,
        &NewSecurityDescriptor);
    if (!NT_SUCCESS(Result))
        return Result;

    SecurityDescriptorSize = GetSecurityDescriptorLength(NewSecurityDescriptor);
    SecurityDescriptor = (PSECURITY_DESCRIPTOR)malloc(SecurityDescriptorSize);
    if (!SecurityDescriptor)
    {
        FspDeleteSecurityDescriptor(NewSecurityDescriptor, (NTSTATUS (*)())FspSetSecurityDescriptor);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memcpy(SecurityDescriptor, NewSecurityDescriptor, SecurityDescriptorSize);
    FspDeleteSecurityDescriptor(NewSecurityDescriptor, (NTSTATUS (*)())FspSetSecurityDescriptor);

    free(Node->SecurityDescriptor);
    Node->SecurityDescriptorSize = SecurityDescriptorSize;
    Node->SecurityDescriptor = SecurityDescriptor;

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiReadDirectory(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length,
    PULONG PBytesTransferred)
{
    assert(!Pattern);

    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    NODE_ Parent = Node->Parent;
    if (Parent)
    {
        //  If this is not the root directory, add the dot entries.
        if (!Marker)
        {
            if (!AddDirInfo(Node, L".", Buffer, Length, PBytesTransferred))
                return STATUS_SUCCESS;
        }
        if (!Marker || (L'.' == Marker[0] && L'\0' == Marker[1]))
        {
            if (!AddDirInfo(Parent, L"..", Buffer, Length, PBytesTransferred))
                return STATUS_SUCCESS;
            Marker = 0;
        }
    }

    auto Iter = Marker ? Node->Children->upper_bound((NODE_)Marker) : Node->Children->begin();
    for (; Iter != Node->Children->end(); ++Iter)
    {
        NODE_ Node = *Iter;
        if (!AddDirInfo(Node, Node->Name, Buffer, Length, PBytesTransferred))
        {
            goto bufferReady;
        }
    }
    FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

  bufferReady:

#ifdef AIRFS_SLOWIO
    if (SlowioReturnPending(FileSystem))
    {
        try
        {
            InterlockedIncrement(&Airfs->SlowioThreadsRunning);
            std::thread(SlowioReadDirectoryThread,
                FileSystem, *PBytesTransferred,
                FspFileSystemGetOperationContext()->Request->Hint).
                detach();
            return STATUS_PENDING;
        }
        catch (...)
        {
            InterlockedDecrement(&Airfs->SlowioThreadsRunning);
        }
    }
    SlowioSnooze(FileSystem);
#endif

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////
#if defined(AIRFS_DIRINFO_BY_NAME)

NTSTATUS ApiGetDirInfoByName(FSP_FILE_SYSTEM *FileSystem, PVOID ParentNode0,
    PWSTR Name, FSP_FSCTL_DIR_INFO *DirInfo)
{
    NODE_ Parent = (NODE_) ParentNode0;
    NODE_ Node;
    auto Iter = Parent->Children->find((NODE_)Name);
    if (Iter == Parent->Children->end())
        return STATUS_OBJECT_NAME_NOT_FOUND;
    Node = *Iter;

    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(Node->Name) * sizeof(WCHAR));
    DirInfo->FileInfo = Node->FileInfo;
    memcpy(DirInfo->FileNameBuf, Node->Name, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));

    return STATUS_SUCCESS;
}

#endif
//////////////////////////////////////////////////////////////////////
#if defined(AIRFS_REPARSE_POINTS)

NTSTATUS ApiResolveReparsePoints(FSP_FILE_SYSTEM *FileSystem, PWSTR Name,
    UINT32 ReparsePointIndex, BOOLEAN ResolveLastPathComponent,
    PIO_STATUS_BLOCK PIoStatus, PVOID Buffer, PSIZE_T PSize)
{
    return FspFileSystemResolveReparsePoints(FileSystem, GetReparsePointByName, 0,
        Name, ReparsePointIndex, ResolveLastPathComponent,
        PIoStatus, Buffer, PSize);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PWSTR Name, PVOID Buffer, PSIZE_T PSize)
{
    NODE_ Node = (NODE_) Node0;

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream) Node = Node->Parent;
#endif

    if (!(Node->FileInfo.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return STATUS_NOT_A_REPARSE_POINT;

    if (Node->ReparseDataSize > *PSize)
        return STATUS_BUFFER_TOO_SMALL;

    *PSize = Node->ReparseDataSize;
    memcpy(Buffer, Node->ReparseData, Node->ReparseDataSize);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PWSTR Name, PVOID Buffer, SIZE_T Size)
{
    NODE_ Node = (NODE_) Node0;
    PVOID ReparseData;
    NTSTATUS Result;

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream) Node = Node->Parent;
#endif

    if (NodeHasChildren(Node))
        return STATUS_DIRECTORY_NOT_EMPTY;

    if (Node->ReparseData)
    {
        Result = FspFileSystemCanReplaceReparsePoint(
            Node->ReparseData, Node->ReparseDataSize,
            Buffer, Size);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    ReparseData = realloc(Node->ReparseData, Size);
    if (!ReparseData && Size)
        return STATUS_INSUFFICIENT_RESOURCES;

    Node->FileInfo.FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
    Node->FileInfo.ReparseTag = *(PULONG)Buffer;
    //  The first field in a reparse buffer is the reparse tag.
    Node->ReparseDataSize = Size;
    Node->ReparseData = ReparseData;
    memcpy(Node->ReparseData, Buffer, Size);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiDeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PWSTR Name, PVOID Buffer, SIZE_T Size)
{
    NODE_ Node = (NODE_) Node0;
    NTSTATUS Result;

#if defined(AIRFS_NAMED_STREAMS)
    if (Node->IsAStream) Node = Node->Parent;
#endif

    if (Node->ReparseData)
    {
        Result = FspFileSystemCanReplaceReparsePoint(
            Node->ReparseData, Node->ReparseDataSize,
            Buffer, Size);
        if (!NT_SUCCESS(Result))
            return Result;
    }
    else
        return STATUS_NOT_A_REPARSE_POINT;

    free(Node->ReparseData);

    Node->FileInfo.FileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
    Node->FileInfo.ReparseTag = 0;
    Node->ReparseDataSize = 0;
    Node->ReparseData = 0;

    return STATUS_SUCCESS;
}

#endif
//////////////////////////////////////////////////////////////////////
#if defined(AIRFS_NAMED_STREAMS)

NTSTATUS ApiGetStreamInfo(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    NODE_ Node = (NODE_) Node0;

    if (Node->IsAStream) Node = Node->Parent;

    if (!(Node->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        if (!AddStreamInfo(Node, L"", Buffer, Length, PBytesTransferred))
            return STATUS_SUCCESS;

    if (Node->Streams)
    {
        // TODO: how to handle out-of-response-buffer-space condition?
        for (auto Iter = Node->Streams->begin(); Iter != Node->Streams->end(); ++Iter)
        {
            BOOLEAN added = AddStreamInfo(*Iter, (*Iter)->Name, Buffer, Length, PBytesTransferred);
            if (!added) goto done;
        }
    }
    FspFileSystemAddStreamInfo(0, Buffer, Length, PBytesTransferred);

  done:

    return STATUS_SUCCESS;
}

#endif
//////////////////////////////////////////////////////////////////////
#if defined(AIRFS_CONTROL)

NTSTATUS ApiControl(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT32 ControlCode,
    PVOID InputBuffer, ULONG InputBufferLength,
    PVOID OutputBuffer, ULONG OutputBufferLength, PULONG PBytesTransferred)
{
    //  Trivial example: change upper to lower case and vice versa.
    if (CTL_CODE(0x8000 + 'A', 'C', METHOD_BUFFERED, FILE_ANY_ACCESS) == ControlCode)
    {
        if (OutputBufferLength != InputBufferLength) return STATUS_INVALID_PARAMETER;
        for (ULONG i = 0; i < InputBufferLength; i++)
        {
            char c = ((char*)InputBuffer)[i];
            if ((c|0x20) >= 'a' && (c|0x20) <= 'z') c ^= 0x20;
            ((char*)OutputBuffer)[i] = c;
        }
        *PBytesTransferred = InputBufferLength;
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}

#endif
//////////////////////////////////////////////////////////////////////

FSP_FILE_SYSTEM_INTERFACE AirfsInterface =
{
    ApiGetVolumeInfo,
    ApiSetVolumeLabel,
    ApiGetSecurityByName,
    ApiCreate,
    ApiOpen,
    ApiOverwrite,
    ApiCleanup,
    ApiClose,
    ApiRead,
    ApiWrite,
    ApiFlush,
    ApiGetFileInfo,
    ApiSetBasicInfo,
    ApiSetFileSize,
    ApiCanDelete,
    ApiRename,
    ApiGetSecurity,
    ApiSetSecurity,
    ApiReadDirectory,
#if defined(AIRFS_REPARSE_POINTS)
    ApiResolveReparsePoints,
    ApiGetReparsePoint,
    ApiSetReparsePoint,
    ApiDeleteReparsePoint,
#else
    0,
    0,
    0,
    0,
#endif
#if defined(AIRFS_NAMED_STREAMS)
    ApiGetStreamInfo,
#else
    0,
#endif
#if defined(AIRFS_DIRINFO_BY_NAME)
    ApiGetDirInfoByName,
#else
    0,
#endif
#if defined(AIRFS_CONTROL)
    ApiControl,
#else
    0,
#endif
};

//////////////////////////////////////////////////////////////////////

void AirfsDelete(AIRFS_ Airfs)
{
    FspFileSystemDelete(Airfs->FileSystem);

    DeleteAllNodes(Airfs);

    free(Airfs);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS AirfsStart(AIRFS_ Airfs)
{
#ifdef AIRFS_SLOWIO
    Airfs->SlowioThreadsRunning = 0;
#endif

    return FspFileSystemStartDispatcher(Airfs->FileSystem, 0);
}

//////////////////////////////////////////////////////////////////////

void AirfsStop(AIRFS_ Airfs)
{
    FspFileSystemStopDispatcher(Airfs->FileSystem);

#ifdef AIRFS_SLOWIO
    while (Airfs->SlowioThreadsRunning)
        Sleep(1);
#endif
}

//////////////////////////////////////////////////////////////////////

NTSTATUS AirfsCreate(
    ULONG Flags,
    ULONG FileInfoTimeout,
    ULONG MaxNodes,
    ULONG MaxFileSize,
    ULONG SlowioMaxDelay,
    ULONG SlowioPercentDelay,
    ULONG SlowioRarefyDelay,
    PWSTR FileSystemName,
    PWSTR VolumePrefix,
    PWSTR RootSddl,
    AIRFS_ *PAirfs)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    BOOLEAN CaseInsensitive = !!(Flags & AirfsCaseInsensitive);
    BOOLEAN FlushAndPurgeOnCleanup = !!(Flags & AirfsFlushAndPurgeOnCleanup);
    PWSTR DevicePath = AirfsNet == (Flags & AirfsDeviceMask) ?
        L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME;
    AIRFS_ Airfs;
    NODE_ RootNode;
    PSECURITY_DESCRIPTOR RootSecurity;
    ULONG RootSecuritySize;

    *PAirfs = 0;

    if (!AirfsHeapInitialize())
        return STATUS_INSUFFICIENT_RESOURCES;

    if (!RootSddl)
        RootSddl = L"O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(RootSddl, SDDL_REVISION_1,
        &RootSecurity, &RootSecuritySize))
        return FspNtStatusFromWin32(GetLastError());

    Airfs = (AIRFS_) malloc(sizeof *Airfs);
    if (!Airfs)
    {
        LocalFree(RootSecurity);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(Airfs, 0, sizeof *Airfs);
    Airfs->MaxNodes = MaxNodes;
    Airfs->MaxFileSize = IN_ALLOCATION_UNITS(MaxFileSize);

#ifdef AIRFS_SLOWIO
    Airfs->SlowioMaxDelay = SlowioMaxDelay;
    Airfs->SlowioPercentDelay = SlowioPercentDelay;
    Airfs->SlowioRarefyDelay = SlowioRarefyDelay;
#endif

    Airfs->CaseInsensitive = CaseInsensitive;

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.Version = sizeof FSP_FSCTL_VOLUME_PARAMS;
    VolumeParams.SectorSize = SECTOR_SIZE;
    VolumeParams.SectorsPerAllocationUnit = SECTORS_PER_ALLOCATION_UNIT;
    VolumeParams.VolumeCreationTime = GetSystemTime();
    VolumeParams.VolumeSerialNumber = (UINT32)(GetSystemTime() / (10000 * 1000));
    VolumeParams.FileInfoTimeout = FileInfoTimeout;
    VolumeParams.CaseSensitiveSearch = !CaseInsensitive;
    VolumeParams.CasePreservedNames = 1;
    VolumeParams.UnicodeOnDisk = 1;
    VolumeParams.PersistentAcls = 1;
    VolumeParams.ReparsePoints = 1;
    VolumeParams.ReparsePointsAccessCheck = 0;
#if defined(AIRFS_NAMED_STREAMS)
    VolumeParams.NamedStreams = 1;
#endif
    VolumeParams.PostCleanupWhenModifiedOnly = 1;
#if defined(AIRFS_DIRINFO_BY_NAME)
    VolumeParams.PassQueryDirectoryFileName = 1;
#endif
    VolumeParams.FlushAndPurgeOnCleanup = FlushAndPurgeOnCleanup;
#if defined(AIRFS_CONTROL)
    VolumeParams.DeviceControl = 1;
#endif
    if (VolumePrefix)
        wcscpy_s(VolumeParams.Prefix, sizeof VolumeParams.Prefix / sizeof(WCHAR), VolumePrefix);
    wcscpy_s(VolumeParams.FileSystemName, sizeof VolumeParams.FileSystemName / sizeof(WCHAR),
        FileSystemName ? FileSystemName : L"-AIRFS");

    Result = FspFileSystemCreate(DevicePath, &VolumeParams, &AirfsInterface, &Airfs->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        DeleteAllNodes(Airfs);
        free(Airfs);
        LocalFree(RootSecurity);
        return Result;
    }

    Airfs->FileSystem->UserContext = Airfs;
    Airfs->VolumeLabelLength = sizeof L"AIRFS" - sizeof(WCHAR);
    memcpy(Airfs->VolumeLabel, L"AIRFS", Airfs->VolumeLabelLength);

#if 0
    FspFileSystemSetOperationGuardStrategy(Airfs->FileSystem,
        FSP_FILE_SYSTEM_OPERATION_GUARD_STRATEGY_COARSE);
#endif

    //  Create the root directory.
    Result = CreateNode(Airfs, L"", &RootNode);
    if (!NT_SUCCESS(Result))
    {
        AirfsDelete(Airfs);
        LocalFree(RootSecurity);
        return Result;
    }
    Result = CreateNodeSet(Airfs->CaseInsensitive, &RootNode->Children);
    if (Result)
    {
        AirfsDelete(Airfs);
        LocalFree(RootSecurity);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RootNode->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    RootNode->SecurityDescriptor = malloc(RootSecuritySize);
    if (!RootNode->SecurityDescriptor)
    {
        DeleteNode(Airfs, RootNode);
        AirfsDelete(Airfs);
        LocalFree(RootSecurity);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RootNode->SecurityDescriptorSize = RootSecuritySize;
    memcpy(RootNode->SecurityDescriptor, RootSecurity, RootSecuritySize);
    Airfs->Root = RootNode;
    ReferenceNode(RootNode);

    LocalFree(RootSecurity);
    *PAirfs = Airfs;
    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

#define PROGNAME "airfs"

#define info(format, ...)   FspServiceLog(EVENTLOG_INFORMATION_TYPE , format, __VA_ARGS__)
#define warn(format, ...)   FspServiceLog(EVENTLOG_WARNING_TYPE     , format, __VA_ARGS__)
#define fail(format, ...)   FspServiceLog(EVENTLOG_ERROR_TYPE       , format, __VA_ARGS__)

#define argtos(v)   if (arge > ++argp) v = *argp; else goto usage
#define argtol(v)   if (arge > ++argp) v = wcstol_default(*argp, v); else goto usage

static ULONG wcstol_default(wchar_t *w, ULONG deflt)
{
    wchar_t *endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS SvcStart(FSP_SERVICE *Service, ULONG argc, PWSTR *argv)
{
    wchar_t **argp, **arge;
    ULONG DebugFlags = 0;
    PWSTR DebugLogFile = 0;
    ULONG Flags = AirfsDisk;
    ULONG OtherFlags = 0;
    ULONG FileInfoTimeout = INFINITE;
    ULONG MaxNodes = 1024;
    ULONG MaxFileSize = 16 * 1024 * 1024;
    ULONG SlowioMaxDelay = 0;       //  -M: maximum slow IO delay in milliseconds
    ULONG SlowioPercentDelay = 0;   //  -P: percent of slow IO to make pending
    ULONG SlowioRarefyDelay = 0;    //  -R: adjust the rarity of pending slow IO
    PWSTR FileSystemName = 0;
    PWSTR MountPoint = 0;
    PWSTR VolumePrefix = 0;
    PWSTR RootSddl = 0;
    HANDLE DebugLogHandle = INVALID_HANDLE_VALUE;
    AIRFS_ Airfs = 0;
    NTSTATUS Result;

    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?': goto usage;
        case L'd': argtol(DebugFlags); break;
        case L'D': argtos(DebugLogFile); break;
        case L'f': OtherFlags = AirfsFlushAndPurgeOnCleanup; break;
        case L'F': argtos(FileSystemName); break;
        case L'i': OtherFlags = AirfsCaseInsensitive; break;
        case L'm': argtos(MountPoint); break;
        case L'M': argtol(SlowioMaxDelay); break;
        case L'n': argtol(MaxNodes); break;
        case L'P': argtol(SlowioPercentDelay); break;
        case L'R': argtol(SlowioRarefyDelay); break;
        case L'S': argtos(RootSddl); break;
        case L's': argtol(MaxFileSize); break;
        case L't': argtol(FileInfoTimeout); break;
        case L'u':
            argtos(VolumePrefix);
            if (VolumePrefix && L'\0' != VolumePrefix[0])
                Flags = AirfsNet;
            break;
        default:
            goto usage;
        }
    }

    if (arge > argp)
        goto usage;

    if (AirfsDisk == Flags && !MountPoint)
        goto usage;

    if (DebugLogFile)
    {
        if (!wcscmp(L"-", DebugLogFile))
            DebugLogHandle = GetStdHandle(STD_ERROR_HANDLE);
        else
            DebugLogHandle = CreateFileW(
                DebugLogFile,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                0,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                0);
        if (INVALID_HANDLE_VALUE == DebugLogHandle)
        {
            fail(L"cannot open debug log file");
            goto usage;
        }

        FspDebugLogSetHandle(DebugLogHandle);
    }

    Result = AirfsCreate(
        Flags | OtherFlags,
        FileInfoTimeout,
        MaxNodes,
        MaxFileSize,
        SlowioMaxDelay,
        SlowioPercentDelay,
        SlowioRarefyDelay,
        FileSystemName,
        VolumePrefix,
        RootSddl,
        &Airfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create AIRFS");
        goto exit;
    }

    FspFileSystemSetDebugLog(Airfs->FileSystem, DebugFlags);

    if (MountPoint && L'\0' != MountPoint[0])
    {
        Result = FspFileSystemSetMountPoint(Airfs->FileSystem,
            L'*' == MountPoint[0] && L'\0' == MountPoint[1] ? 0 : MountPoint);
        if (!NT_SUCCESS(Result))
        {
            fail(L"cannot mount AIRFS");
            goto exit;
        }
    }

    Result = AirfsStart(Airfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot start AIRFS");
        goto exit;
    }

    MountPoint = FspFileSystemMountPoint(Airfs->FileSystem);

    info(L"%s -t %ld -n %ld -s %ld%s%s%s%s%s%s",
        L"" PROGNAME, FileInfoTimeout, MaxNodes, MaxFileSize,
        RootSddl ? L" -S " : L"", RootSddl ? RootSddl : L"",
        VolumePrefix && L'\0' != VolumePrefix[0] ? L" -u " : L"",
        VolumePrefix && L'\0' != VolumePrefix[0] ? VolumePrefix : L"",
        MountPoint ? L" -m " : L"", MountPoint ? MountPoint : L"");

    Service->UserContext = Airfs;
    Result = STATUS_SUCCESS;

  exit:
    if (!NT_SUCCESS(Result) && Airfs)
        AirfsDelete(Airfs);

    return Result;

  usage:
    static wchar_t usage[] = L""
        "usage: %s OPTIONS\n"
        "\n"
        "options:\n"
        "    -d DebugFlags       [-1: enable all debug logs]\n"
        "    -D DebugLogFile     [file path; use - for stderr]\n"
        "    -i                  [case insensitive file system]\n"
        "    -f                  [flush and purge cache on cleanup]\n"
        "    -t FileInfoTimeout  [millis]\n"
        "    -n MaxNodes\n"
        "    -s MaxFileSize      [bytes]\n"
        "    -M MaxDelay         [maximum slow IO delay in millis]\n"
        "    -P PercentDelay     [percent of slow IO to make pending]\n"
        "    -R RarefyDelay      [adjust the rarity of pending slow IO]\n"
        "    -F FileSystemName\n"
        "    -S RootSddl         [file rights: FA, etc; NO generic rights: GA, etc.]\n"
        "    -u \\Server\\Share  [UNC prefix (single backslash)]\n"
        "    -m MountPoint       [X:|* (required if no UNC prefix)]\n";

    fail(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    AIRFS_ Airfs = (AIRFS_) Service->UserContext;
    
    AirfsStop(Airfs);
    AirfsDelete(Airfs);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

int wmain(int argc, wchar_t **argv)
{
    if (!NT_SUCCESS(FspLoad(0)))
        return ERROR_DELAY_LOAD_FAILED;

    return FspServiceRun(L"" PROGNAME, SvcStart, SvcStop, 0);
}

//////////////////////////////////////////////////////////////////////
