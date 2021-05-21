/**
 * @file airfs.cpp
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

#include "common.h"

enum
{
    AirfsDisk                   = 0x00000000,
    AirfsNet                    = 0x00000001,
    AirfsDeviceMask             = 0x0000000f,
    AirfsCaseInsensitive        = 0x80000000,
    AirfsFlushAndPurgeOnCleanup = 0x40000000,
};

//////////////////////////////////////////////////////////////////////

NTSTATUS CreateNode(AIRFS_ Airfs, PWSTR Name, NODE_ *PNode)
{
    static UINT64 IndexNumber = 1;

    NODE_ Node = (NODE_) StorageAllocate(Airfs, sizeof NODE);

    if (!Node)
    {
        *PNode = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(Node, 0, sizeof NODE);

    size_t NameNumBytes = (wcslen(Name)+1) * sizeof WCHAR;
    WCHAR* NameAllocation = (WCHAR*) StorageAllocate(Airfs, NameNumBytes);
    if (!NameAllocation)
    {
        StorageFree(Airfs, Node);
        *PNode = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memcpy(NameAllocation, Name, NameNumBytes);
    Node->Name = NameAllocation;

    Node->FileInfo.CreationTime =
    Node->FileInfo.LastAccessTime =
    Node->FileInfo.LastWriteTime =
    Node->FileInfo.ChangeTime = SystemTime();
    Node->FileInfo.IndexNumber = IndexNumber++;

    *PNode = Node;
    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

void DeleteNode(AIRFS_ Airfs, NODE_ Node)
{
    NODE_ Block;
    while (Block = Node->FileBlocks)
    {
        Detach(Node->FileBlocks, Block);
        StorageFree(Airfs, Block);
    }

    StorageFree(Airfs, Node->Name);
    StorageFree(Airfs, Node->ReparseData);
    StorageFree(Airfs, Node->SecurityDescriptor);
    StorageFree(Airfs, Node);
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
    if (Node->IsAStream)
    {
        *FileInfo = Node->Parent->FileInfo;
        FileInfo->FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
        //  Named streams cannot be directories.
        FileInfo->AllocationSize = Node->FileInfo.AllocationSize;
        FileInfo->FileSize = Node->FileInfo.FileSize;
    }
    else
        *FileInfo = Node->FileInfo;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS FindNode(AIRFS_ Airfs, PWSTR Name, PWSTR *BaseName, NODE_ *PParent, NODE_ *PNode)
{
    CompareFunction* NodeCmp = Airfs->CaseInsensitive ? CaselessNameCmp : ExactNameCmp;

    //  Special case root.
    if (Name[0] == 0 || Name[1] == 0)
    {
        if (BaseName) *BaseName = Name;
        if (PParent) *PParent = Airfs->Root;
        *PNode = Airfs->Root;
        return 0;
    }

    WCHAR ParsedName[AIRFS_MAX_PATH];
    wcscpy_s(ParsedName, sizeof ParsedName / sizeof WCHAR, Name);

    //  From root, for each ancestor...
    NODE_ Ancestor = Airfs->Root;
    WCHAR* fm;
    WCHAR* to = ParsedName;
    WCHAR* Colon = 0;
    for (;;)
    {
        //  Isolate the next base name.
        for (fm = to+1; *fm == L'\\'; fm++) {}
        for (to = fm; *to != L'\0' && *to != L'\\'; to++)
            if (*to == ':') {Colon = to; break;}
        if (*to == 0) break;
        *to = 0;

        //  Find this name.
        NODE_ Child = Find(Ancestor->Children, fm, NodeCmp);
        if (!Child)
        {
            if (PParent) *PParent = 0;
            *PNode = 0;
            if (Colon) return STATUS_OBJECT_NAME_NOT_FOUND;
            return STATUS_OBJECT_PATH_NOT_FOUND;
        }

        Ancestor = Child;

        if (Colon)
        {
            fm = to+1;
            break;
        }

        if (!(Ancestor->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (PParent) *PParent = 0;
            *PNode = 0;
            return STATUS_NOT_A_DIRECTORY;
        }
    }

    if (BaseName)
        *BaseName = Name + (  (  (UINT_PTR)fm - (UINT_PTR)ParsedName  )  / sizeof WCHAR);

    if (PParent)
        *PParent = Ancestor;

    if (Colon)
    {
        //  Find the stream, if it exists.
        if (!Ancestor->Streams)
        {
            *PNode = 0;
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        NODE_ Stream = Find(Ancestor->Streams, fm, NodeCmp);
        if (!Stream)
        {
            *PNode = 0;
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        *PNode = Stream;
        return 0;
    }

    //  Find the directory entry, if it exists.
    NODE_ Found = Find(Ancestor->Children, fm, NodeCmp);
    if (!Found)
    {
        *PNode = 0;
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    *PNode = Found;
    return 0;
}

//////////////////////////////////////////////////////////////////////

BOOLEAN AddStreamInfo(NODE_ Node, PWSTR StreamName, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    UINT8 StreamInfoBuf[sizeof FSP_FSCTL_STREAM_INFO + AIRFS_MAX_PATH * sizeof WCHAR];
    FSP_FSCTL_STREAM_INFO *StreamInfo = (FSP_FSCTL_STREAM_INFO *)StreamInfoBuf;

    StreamInfo->Size = (UINT16)(sizeof FSP_FSCTL_STREAM_INFO + wcslen(StreamName) * sizeof WCHAR);
    StreamInfo->StreamSize = Node->FileInfo.FileSize;
    StreamInfo->StreamAllocationSize = Node->FileInfo.AllocationSize;
    memcpy(StreamInfo->StreamNameBuf, StreamName, StreamInfo->Size - sizeof FSP_FSCTL_STREAM_INFO);

    return FspFileSystemAddStreamInfo(StreamInfo, Buffer, Length, PBytesTransferred);
}

//////////////////////////////////////////////////////////////////////

inline void TouchNode(NODE_ Node)
{
    Node->FileInfo.LastAccessTime =
    Node->FileInfo.LastWriteTime =
    Node->FileInfo.ChangeTime = SystemTime();
}

//////////////////////////////////////////////////////////////////////

void InsertNode(AIRFS_ Airfs, NODE_ Parent, NODE_ Node)
{
    CompareFunction* NodeCmp = Airfs->CaseInsensitive ? CaselessNameCmp : ExactNameCmp;

    WCHAR* key = Node->Name;
    if (Node->IsAStream) Attach(Parent->Streams  , Node, NodeCmp, key);
    else                 Attach(Parent->Children , Node, NodeCmp, key);

    Node->Parent = Parent;
    ReferenceNode(Node);
    TouchNode(Parent);
}

//////////////////////////////////////////////////////////////////////

void RemoveNode(AIRFS_ Airfs, NODE_ Node)
{
    NODE_ Parent = Node->Parent;
    TouchNode(Parent);

    if (Node->IsAStream)
    {
        if (Parent->Streams) Detach(Parent->Streams, Node);
    }
    else
        Detach(Parent->Children, Node);

    DereferenceNode(Airfs, Node);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS GetReparsePointByName(FSP_FILE_SYSTEM *FileSystem, PVOID Context,
    PWSTR Name, BOOLEAN IsDirectory, PVOID Buffer, PSIZE_T PSize)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node;

    NTSTATUS Result = FindNode(Airfs, Name, 0, 0, &Node);

    if (!Node) return STATUS_OBJECT_NAME_NOT_FOUND;
    if (!(Node->FileInfo.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) return STATUS_NOT_A_REPARSE_POINT;

    if (Buffer)
    {
        if (Node->ReparseDataSize > *PSize) return STATUS_BUFFER_TOO_SMALL;
        *PSize = Node->ReparseDataSize;
        memcpy(Buffer, Node->ReparseData.Address(), Node->ReparseDataSize);
    }

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS SetAllocSize(AIRFS_ Airfs, NODE_ Node, UINT64 RequestedAllocSize)
{
    NTSTATUS Result = StorageSetFileCapacity(Airfs, Node, RequestedAllocSize);
    if (!Result)
    {
        Node->FileInfo.AllocationSize = RequestedAllocSize;
        if (Node->FileInfo.FileSize > Node->FileInfo.AllocationSize) 
            Node->FileInfo.FileSize = Node->FileInfo.AllocationSize;

    }
    return Result;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS SetFileSize(AIRFS_ Airfs, NODE_ Node, UINT64 RequestedFileSize)
{
    if (Node->FileInfo.FileSize != RequestedFileSize)
    {
        if (Node->FileInfo.AllocationSize < RequestedFileSize)
        {
            NTSTATUS Result = SetAllocSize(Airfs, Node, RequestedFileSize);
            if (!NT_SUCCESS(Result))
                return Result;
        }

        if (Node->FileInfo.FileSize < RequestedFileSize)
            StorageAccessFile(ZERO, Node, Node->FileInfo.FileSize, RequestedFileSize - Node->FileInfo.FileSize, 0);

        Node->FileInfo.FileSize = RequestedFileSize;
    }

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

BOOLEAN AddDirInfo(NODE_ Node, PWSTR Name, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    size_t NameBytes = wcslen(Name) * sizeof WCHAR;
    UINT8 DirInfoBuf[sizeof FSP_FSCTL_DIR_INFO + AIRFS_MAX_PATH * sizeof WCHAR];
    FSP_FSCTL_DIR_INFO *DirInfo = (FSP_FSCTL_DIR_INFO *)DirInfoBuf;
    DirInfo->Size = (UINT16)(sizeof FSP_FSCTL_DIR_INFO + NameBytes);
    DirInfo->FileInfo = Node->FileInfo;
    memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    memcpy(DirInfo->FileNameBuf, Name, NameBytes + sizeof WCHAR);

    return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetVolumeInfo(FSP_FILE_SYSTEM *FileSystem, FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;

    VolumeInfo->TotalSize = Airfs->VolumeSize;
    VolumeInfo->FreeSize = Airfs->FreeSize;
    VolumeInfo->VolumeLabelLength = Airfs->VolumeLabelLength;
    memcpy(VolumeInfo->VolumeLabel, Airfs->VolumeLabel, Airfs->VolumeLabelLength);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetVolumeLabel(FSP_FILE_SYSTEM *FileSystem, PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;

    Airfs->VolumeLabelLength = (UINT16)(wcslen(VolumeLabel) * sizeof WCHAR);
    if (Airfs->VolumeLabelLength > sizeof Airfs->VolumeLabel)
        Airfs->VolumeLabelLength = sizeof Airfs->VolumeLabel;
    memcpy(Airfs->VolumeLabel, VolumeLabel, Airfs->VolumeLabelLength);

    VolumeInfo->TotalSize = Airfs->VolumeSize;
    VolumeInfo->FreeSize = Airfs->FreeSize;
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
        if (FspFileSystemFindReparsePoint(FileSystem, GetReparsePointByName, 0,
            Name, PFileAttributes))
        {
            return STATUS_REPARSE;
        }
        return Result;
    }

    UINT32 FileAttributesMask = ~(UINT32)0;
    if (Node->IsAStream)
    {
        FileAttributesMask = ~(UINT32)FILE_ATTRIBUTE_DIRECTORY;
        Node = Node->Parent;
    }

    if (PFileAttributes)
        *PFileAttributes = Node->FileInfo.FileAttributes & FileAttributesMask;

    if (PSecurityDescriptorSize)
    {
        if (Node->SecurityDescriptorSize > *PSecurityDescriptorSize)
        {
            *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
            return STATUS_BUFFER_OVERFLOW;
        }

        *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
        if (SecurityDescriptor)
            memcpy(SecurityDescriptor, Node->SecurityDescriptor.Address(), Node->SecurityDescriptorSize);
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
    PWSTR BaseName;

    if (AIRFS_MAX_PATH <= wcslen(Name))
        return STATUS_OBJECT_NAME_INVALID;

    if (CreateOptions & FILE_DIRECTORY_FILE)
        AllocationSize = 0;

    Result = FindNode(Airfs, Name, &BaseName, &Parent, &Node);
    if (Node) return STATUS_OBJECT_NAME_COLLISION;
    if (!Parent) return Result;

    Result = CreateNode(Airfs, BaseName, &Node);
    if (!NT_SUCCESS(Result)) return Result;

    Node->IsAStream = BaseName[-1] == L':';

    Node->FileInfo.FileAttributes = (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
        FileAttributes : FileAttributes | FILE_ATTRIBUTE_ARCHIVE;

    if (SecurityDescriptor)
    {
        Node->SecurityDescriptorSize = GetSecurityDescriptorLength(SecurityDescriptor);
        Node->SecurityDescriptor = StorageAllocate(Airfs, (int)Node->SecurityDescriptorSize);
        if (!Node->SecurityDescriptor)
        {
            DeleteNode(Airfs, Node);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        memcpy(Node->SecurityDescriptor.Address(), SecurityDescriptor, Node->SecurityDescriptorSize);
    }

    if (AllocationSize)
    {
        NTSTATUS Result = SetAllocSize(Airfs, Node, AllocationSize);
        if (Result)
        {
            DeleteNode(Airfs, Node);
            return Result;
        }
    }

    InsertNode(Airfs, Parent, Node);
    ReferenceNode(Node);
    *PNode = Node;
    GetFileInfo(Node, FileInfo);

    if (Airfs->CaseInsensitive)
    {
        int ParentPathNumBytes = (int)(  (char*)BaseName - (char*)Name  );
        int NodeNameNumBytes = (int)(  wcslen(Node->Name) * sizeof WCHAR  );

        FSP_FSCTL_OPEN_FILE_INFO *OpenFileInfo = FspFileSystemGetOpenFileInfo(FileInfo);
        memcpy(OpenFileInfo->NormalizedName, Name, ParentPathNumBytes);
        memcpy(OpenFileInfo->NormalizedName+ParentPathNumBytes / sizeof WCHAR, Node->Name, NodeNameNumBytes);
        OpenFileInfo->NormalizedNameSize = ParentPathNumBytes + NodeNameNumBytes;
    }

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

    if (Airfs->CaseInsensitive)
    {
        int ParentPathNumBytes = (int)(  (char*)baseName - (char*)Name  );
        int NodeNameNumBytes = (int)(  wcslen(Node->Name) * sizeof WCHAR  );

        FSP_FSCTL_OPEN_FILE_INFO *OpenFileInfo = FspFileSystemGetOpenFileInfo(FileInfo);
        memcpy(OpenFileInfo->NormalizedName, Name, ParentPathNumBytes);
        memcpy(OpenFileInfo->NormalizedName+ParentPathNumBytes / sizeof WCHAR, Node->Name, NodeNameNumBytes);
        OpenFileInfo->NormalizedNameSize = ParentPathNumBytes + NodeNameNumBytes;
    }

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiOverwrite(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, UINT32 FileAttributes,
    BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize, FSP_FSCTL_FILE_INFO *FileInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;

    for (NODE_ Stream = First(Node->Streams); Stream; )
    {
        NODE_ NextStream = Next(Stream);
        LONG RefCount = Stream->RefCount;
        MemoryBarrier();
        if (RefCount <= 1)
        {
            RemoveNode(Airfs, Stream);
        }
        Stream = NextStream;
    }

    NTSTATUS Result = SetAllocSize(Airfs, Node, AllocationSize);
    if (!NT_SUCCESS(Result))
        return Result;

    if (ReplaceFileAttributes) Node->FileInfo.FileAttributes  = FileAttributes | FILE_ATTRIBUTE_ARCHIVE;
    else                       Node->FileInfo.FileAttributes |= FileAttributes | FILE_ATTRIBUTE_ARCHIVE;

    Node->FileInfo.FileSize = 0;
    Node->FileInfo.LastAccessTime =
    Node->FileInfo.LastWriteTime =
    Node->FileInfo.ChangeTime = SystemTime();

    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

void ApiCleanup(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PWSTR Name, ULONG Flags)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;

    NODE_ MainNode = Node->IsAStream ? Node->Parent : Node;

    if (Flags & FspCleanupSetArchiveBit)
    {
        if (!(MainNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
              MainNode->FileInfo.FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    }

    if (Flags & (FspCleanupSetLastAccessTime | FspCleanupSetLastWriteTime | FspCleanupSetChangeTime))
    {
        UINT64 Time = SystemTime();
        if (Flags & FspCleanupSetLastAccessTime ) MainNode->FileInfo.LastAccessTime = Time;
        if (Flags & FspCleanupSetLastWriteTime  ) MainNode->FileInfo.LastWriteTime  = Time;
        if (Flags & FspCleanupSetChangeTime     ) MainNode->FileInfo.ChangeTime     = Time;
    }

    if (Flags & FspCleanupSetAllocationSize)
    {
        SetAllocSize(Airfs, Node, Node->FileInfo.FileSize);
    }

    if ((Flags & FspCleanupDelete) && !Node->Children)
    {
        NODE_ Stream;
        while (Stream = Node->Streams)
        {
            Detach(Node->Streams, Stream);
            DeleteNode(Airfs, Stream);
        }
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
    UINT64 BegOffset, ULONG Length, PULONG PBytesTransferred)
{
    NODE_ Node = (NODE_) Node0;
    UINT64 EndOffset;

    if (BegOffset >= Node->FileInfo.FileSize)
        return STATUS_END_OF_FILE;

    EndOffset = BegOffset + Length;
    if (EndOffset > Node->FileInfo.FileSize)
        EndOffset = Node->FileInfo.FileSize;

    StorageAccessFile(READ, Node, BegOffset, EndOffset - BegOffset, (char*)Buffer);

    *PBytesTransferred = (ULONG)(EndOffset - BegOffset);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiWrite(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PVOID Buffer,
    UINT64 BegOffset, ULONG Length, BOOLEAN WriteToEndOfFile,
    BOOLEAN ConstrainedIo, PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO *FileInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    UINT64 EndOffset;
    NTSTATUS Result;

    if (ConstrainedIo)
    {
        if (BegOffset >= Node->FileInfo.FileSize)
            return STATUS_SUCCESS;
        EndOffset = BegOffset + Length;
        if (EndOffset > Node->FileInfo.FileSize)
            EndOffset = Node->FileInfo.FileSize;
    }
    else
    {
        if (WriteToEndOfFile)
            BegOffset = Node->FileInfo.FileSize;
        EndOffset = BegOffset + Length;
        if (EndOffset > Node->FileInfo.FileSize)
        {
            Result = SetFileSize(Airfs, Node, EndOffset);
            if (!NT_SUCCESS(Result))
                return Result;
        }
    }

    StorageAccessFile(WRITE, Node, BegOffset, EndOffset - BegOffset, (char*)Buffer);

    *PBytesTransferred = (ULONG)(EndOffset - BegOffset);
    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiFlush(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, FSP_FSCTL_FILE_INFO *FileInfo)
{
    NODE_ Node = (NODE_) Node0;

    if (Node) GetFileInfo(Node, FileInfo);

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

    if (Node->IsAStream) Node = Node->Parent;

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
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    NTSTATUS Result = SetAllocationSize
        ? SetAllocSize (Airfs, Node, NewSize)
        : SetFileSize  (Airfs, Node, NewSize);
    if (!NT_SUCCESS(Result))
        return Result;

    GetFileInfo(Node, FileInfo);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiCanDelete(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PWSTR Name)
{
    NODE_ Node = (NODE_) Node0;

    if (Node->Children)
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
    NTSTATUS Result;
    PWSTR NewBaseName;
    NODE_ NewParent;
    Result = FindNode(Airfs, NewFileName, &NewBaseName, &NewParent, &NewNode);

    //  Create the replacement name.
    size_t NewBaseNameNumBytes = (wcslen(NewBaseName)+1) * sizeof WCHAR;
    WCHAR* NewBaseNameAlloc = (WCHAR*) StorageAllocate(Airfs, NewBaseNameNumBytes);
    if (!NewBaseNameAlloc)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memcpy(NewBaseNameAlloc, NewBaseName, NewBaseNameNumBytes);

    if (NewNode && Node != NewNode)
    {
        if (!ReplaceIfExists)
        {
            StorageFree(Airfs, NewBaseNameAlloc);
            return STATUS_OBJECT_NAME_COLLISION;
        }

        if (NewNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            StorageFree(Airfs, NewBaseNameAlloc);
            return STATUS_ACCESS_DENIED;
        }

        ReferenceNode(NewNode);
        RemoveNode(Airfs, NewNode);
        DereferenceNode(Airfs, NewNode);
    }

    ReferenceNode(Node);
    RemoveNode(Airfs, Node);

    StorageFree(Airfs, Node->Name);
    Node->Name = NewBaseNameAlloc;

    InsertNode(Airfs, NewParent, Node);
    DereferenceNode(Airfs, Node);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T *PSecurityDescriptorSize)
{
    NODE_ Node = (NODE_) Node0;

    if (Node->IsAStream) Node = Node->Parent;

    if (Node->SecurityDescriptorSize > *PSecurityDescriptorSize)
    {
        *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
        return STATUS_BUFFER_OVERFLOW;
    }

    *PSecurityDescriptorSize = Node->SecurityDescriptorSize;
    if (SecurityDescriptor)
        memcpy(SecurityDescriptor, Node->SecurityDescriptor.Address(), Node->SecurityDescriptorSize);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetSecurity(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    PSECURITY_DESCRIPTOR NewSecurityDescriptor, MallocSecurityDescriptor;

    if (Node->IsAStream) Node = Node->Parent;

    NTSTATUS Result = FspSetSecurityDescriptor(
        Node->SecurityDescriptor.Address(),
        SecurityInformation,
        ModificationDescriptor,
        &NewSecurityDescriptor);
    if (!NT_SUCCESS(Result))
        return Result;

    uint64_t SecurityDescriptorSize = GetSecurityDescriptorLength(NewSecurityDescriptor);
    MallocSecurityDescriptor = (PSECURITY_DESCRIPTOR) malloc(SecurityDescriptorSize);
    if (!MallocSecurityDescriptor)
    {
        FspDeleteSecurityDescriptor(NewSecurityDescriptor, (NTSTATUS (*)())FspSetSecurityDescriptor);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memcpy(MallocSecurityDescriptor, NewSecurityDescriptor, SecurityDescriptorSize);
    FspDeleteSecurityDescriptor(NewSecurityDescriptor, (NTSTATUS (*)())FspSetSecurityDescriptor);

    NODE_ NewHunk = (NODE_) StorageReallocate(Airfs, Node->SecurityDescriptor, SecurityDescriptorSize);
    if (!NewHunk && SecurityDescriptorSize)
    {
        free(MallocSecurityDescriptor);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Node->SecurityDescriptorSize = SecurityDescriptorSize;
    if (!SecurityDescriptorSize) Node->SecurityDescriptor = 0;
    else
    {
        Node->SecurityDescriptor = NewHunk;
        memcpy(Node->SecurityDescriptor.Address(), MallocSecurityDescriptor, SecurityDescriptorSize);
    }

    free(MallocSecurityDescriptor);
    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiReadDirectory(FSP_FILE_SYSTEM *FileSystem, PVOID Node0, PWSTR Pattern, 
    PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;

    CompareFunction* NodeCmp = Airfs->CaseInsensitive ? CaselessNameCmp : ExactNameCmp;

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

    //  Find the next Child, even if the Marker child no longer exists.
    NODE_ Child;
    if (!Marker) Child = First(Node->Children);
    else         Child = Near(Node->Children, Marker, NodeCmp, GT);

    for (; Child; Child = Next(Child))
    {
        if (!AddDirInfo(Child, Child->Name, Buffer, Length, PBytesTransferred))
            goto bufferReady;
    }
    FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

  bufferReady:

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetDirInfoByName(FSP_FILE_SYSTEM *FileSystem, PVOID ParentNode0,
    PWSTR Name, FSP_FSCTL_DIR_INFO *DirInfo)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;

    CompareFunction* NodeCmp = Airfs->CaseInsensitive ? CaselessNameCmp : ExactNameCmp;

    NODE_ Parent = (NODE_) ParentNode0;
    NODE_ Node = Find(Parent->Children, Name, NodeCmp);
    if (!Node)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    DirInfo->Size = (UINT16)(sizeof FSP_FSCTL_DIR_INFO + wcslen(Node->Name) * sizeof WCHAR);
    DirInfo->FileInfo = Node->FileInfo;
    memcpy(DirInfo->FileNameBuf, Node->Name, DirInfo->Size - sizeof FSP_FSCTL_DIR_INFO);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

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

    if (Node->IsAStream) Node = Node->Parent;

    if (!(Node->FileInfo.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return STATUS_NOT_A_REPARSE_POINT;

    if (Node->ReparseDataSize > *PSize)
        return STATUS_BUFFER_TOO_SMALL;

    *PSize = Node->ReparseDataSize;
    memcpy(Buffer, Node->ReparseData.Address(), Node->ReparseDataSize);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiSetReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PWSTR Name, PVOID Buffer, SIZE_T Size)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    NTSTATUS Result;

    if (Node->IsAStream) Node = Node->Parent;

    if (Node->Children)
        return STATUS_DIRECTORY_NOT_EMPTY;

    if (Node->ReparseData)
    {
        Result = FspFileSystemCanReplaceReparsePoint(
            Node->ReparseData.Address(), Node->ReparseDataSize,
            Buffer, Size);
        if (!NT_SUCCESS(Result))
            return Result;
    }

    NODE_ ReparseData = (NODE_) StorageReallocate(Airfs, Node->ReparseData, Size);
    if (!ReparseData && Size)
        return STATUS_INSUFFICIENT_RESOURCES;

    Node->FileInfo.FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
    Node->FileInfo.ReparseTag = *(PULONG)Buffer;
    //  The first field in a reparse buffer is the reparse tag.
    Node->ReparseDataSize = Size;
    Node->ReparseData = ReparseData;
    memcpy(Node->ReparseData.Address(), Buffer, Size);

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiDeleteReparsePoint(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PWSTR Name, PVOID Buffer, SIZE_T Size)
{
    AIRFS_ Airfs = (AIRFS_) FileSystem->UserContext;
    NODE_ Node = (NODE_) Node0;
    NTSTATUS Result;

    if (Node->IsAStream) Node = Node->Parent;

    if (Node->ReparseData)
    {
        Result = FspFileSystemCanReplaceReparsePoint(
            Node->ReparseData.Address(), Node->ReparseDataSize,
            Buffer, Size);
        if (!NT_SUCCESS(Result))
            return Result;
    }
    else
        return STATUS_NOT_A_REPARSE_POINT;

    StorageFree(Airfs, Node->ReparseData);

    Node->FileInfo.FileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
    Node->FileInfo.ReparseTag = 0;
    Node->ReparseDataSize = 0;
    Node->ReparseData = 0;

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiGetStreamInfo(FSP_FILE_SYSTEM *FileSystem, PVOID Node0,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    NODE_ Node = (NODE_) Node0;

    if (Node->IsAStream) Node = Node->Parent;

    if (!(Node->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        if (!AddStreamInfo(Node, L"", Buffer, Length, PBytesTransferred))
            return STATUS_SUCCESS;

    for (NODE_ Stream = First(Node->Streams); Stream; Stream = Next(Stream))
    {
        BOOLEAN added = AddStreamInfo(Stream, Stream->Name, Buffer, Length, PBytesTransferred);
        if (!added) goto done;
    }

    FspFileSystemAddStreamInfo(0, Buffer, Length, PBytesTransferred);

  done:

    return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS ApiControl(FSP_FILE_SYSTEM *FileSystem,
    PVOID FileContext, UINT32 ControlCode,
    PVOID InputBuffer, ULONG InputBufferLength,
    PVOID OutputBuffer, ULONG OutputBufferLength, PULONG PBytesTransferred)
{
    //  Trivial example: Perform a ROT13 translation on alphas.
    if (CTL_CODE(0x8000 + 'M', 'R', METHOD_BUFFERED, FILE_ANY_ACCESS) == ControlCode)
    {
        if (OutputBufferLength != InputBufferLength) return STATUS_INVALID_PARAMETER;
        for (ULONG i = 0; i < InputBufferLength; i++)
        {
            char c = ((char*)InputBuffer)[i];
            if (('A' <= c && c <= 'M') || ('a' <= c && c <= 'm')) c += 13;
            else
            if (('N' <= c && c <= 'Z') || ('n' <= c && c <= 'z')) c -= 13;
            ((char*)OutputBuffer)[i] = c;
        }
        *PBytesTransferred = InputBufferLength;
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_DEVICE_REQUEST;
}

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
    ApiResolveReparsePoints,
    ApiGetReparsePoint,
    ApiSetReparsePoint,
    ApiDeleteReparsePoint,
    ApiGetStreamInfo,
    ApiGetDirInfoByName,
    ApiControl,
};

//////////////////////////////////////////////////////////////////////

void AirfsDelete(AIRFS_ Airfs)
{
    FspFileSystemDelete(Airfs->FileSystem);
    StorageShutdown(Airfs);
}

//////////////////////////////////////////////////////////////////////

NTSTATUS AirfsCreate(
    PWSTR StorageFileName,
    PWSTR MapName,
    ULONG Flags,
    ULONG FileInfoTimeout,
    UINT64 VolumeSize,
    PWSTR FileSystemName,
    PWSTR VolumePrefix,
    PWSTR RootSddl,
    AIRFS_ *PAirfs)
{
    NTSTATUS Result;
    BOOLEAN CaseInsensitive = !!(Flags & AirfsCaseInsensitive);
    BOOLEAN FlushAndPurgeOnCleanup = !!(Flags & AirfsFlushAndPurgeOnCleanup);
    PWSTR DevicePath = AirfsNet == (Flags & AirfsDeviceMask) ?
        L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME;
    AIRFS_ Airfs;
    PSECURITY_DESCRIPTOR RootSecurity = 0;
    ULONG RootSecuritySize;

    *PAirfs = 0;

    boolean StorageFileExists = *StorageFileName && (_waccess(StorageFileName, 0) != -1);

    Result = StorageStartup(Airfs, MapName, StorageFileName, VolumeSize);
    if (Result) return Result;

    boolean ShouldFormat = !StorageFileExists || memcmp(Airfs->Signature, "Airfs\0\0\0", 8);

    if (ShouldFormat)
    {
        memcpy(Airfs->Signature,"Airfs\0\0\0"  "\1\0\0\0"  "\0\0\0\0", 16);

        if (!RootSddl)
            RootSddl = L"O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;WD)";
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(RootSddl, SDDL_REVISION_1,
            &RootSecurity, &RootSecuritySize))
            return GetLastErrorAsStatus();

        Airfs->VolumeSize = ROUND_DOWN(VolumeSize, ALLOCATION_UNIT);
        Airfs->CaseInsensitive = CaseInsensitive;
        Airfs->VolumeLabelLength = sizeof L"AIRFS" - sizeof WCHAR;
        memcpy(Airfs->VolumeLabel, L"AIRFS", Airfs->VolumeLabelLength);

        FSP_FSCTL_VOLUME_PARAMS V;
        memset(&V, 0, sizeof V);
        V.Version = sizeof FSP_FSCTL_VOLUME_PARAMS;
        V.SectorSize = SECTOR_SIZE;
        V.SectorsPerAllocationUnit = SECTORS_PER_ALLOCATION_UNIT;
        V.VolumeCreationTime = SystemTime();
        V.VolumeSerialNumber = (UINT32)(SystemTime() / (10000 * 1000));
        V.FileInfoTimeout = FileInfoTimeout;
        V.CaseSensitiveSearch = !CaseInsensitive;
        V.CasePreservedNames = 1;
        V.UnicodeOnDisk = 1;
        V.PersistentAcls = 1;
        V.ReparsePoints = 1;
        V.ReparsePointsAccessCheck = 0;
        V.NamedStreams = 1;
        V.PostCleanupWhenModifiedOnly = 1;
        V.PassQueryDirectoryFileName = 1;
        V.FlushAndPurgeOnCleanup = FlushAndPurgeOnCleanup;
        V.DeviceControl = 1;
        wcscpy_s(V.Prefix, sizeof V.Prefix / sizeof WCHAR, VolumePrefix);
        wcscpy_s(V.FileSystemName, sizeof V.FileSystemName / sizeof WCHAR,
            FileSystemName ? FileSystemName : L"-AIRFS");
        Airfs->VolumeParams = V;

        //  Set up the available storage in chunks.
        Airfs->FreeSize = 0;
        Airfs->Available = 0;
        uint64_t to = 4096;
        for (;;)
        {
            uint64_t fm = to + sizeof int32_t;
            to =  fm + MAXIMUM_ALLOCSIZE;
            if (to > Airfs->VolumeSize - MINIMUM_ALLOCSIZE) to = Airfs->VolumeSize;
            int32_t Size = (int32_t)( to - fm );
            char* Addr = (char*)Airfs + fm;// + sizeof int32_t;
            NODE_ NewItem = (NODE_) Addr;
            ((int32_t*)NewItem)[-1] = Size;
            Attach(Airfs->Available, NewItem, SizeCmp, &Size);
            Airfs->FreeSize += Size;
            if (to == Airfs->VolumeSize) break;
        }

        //  Create the root directory.
        Airfs->Root = 0;
        NODE_ RootNode;
        Result = CreateNode(Airfs, L"", &RootNode);
        if (!NT_SUCCESS(Result))
        {
            AirfsDelete(Airfs);
            LocalFree(RootSecurity);
            return Result;
        }
        RootNode->P = RootNode->L = RootNode->R = RootNode->E = RootNode->Parent = 0;
        RootNode->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        RootNode->SecurityDescriptor = StorageAllocate(Airfs, RootSecuritySize);
        if (!RootNode->SecurityDescriptor)
        {
            DeleteNode(Airfs, RootNode);
            AirfsDelete(Airfs);
            LocalFree(RootSecurity);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        RootNode->SecurityDescriptorSize = RootSecuritySize;
        memcpy(RootNode->SecurityDescriptor.Address(), RootSecurity, RootSecuritySize);
        Airfs->Root = RootNode;
        ReferenceNode(RootNode);
        LocalFree(RootSecurity);
    }

    Result = FspFileSystemCreate(DevicePath, &Airfs->VolumeParams, &AirfsInterface, &Airfs->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        LocalFree(RootSecurity);
        return Result;
    }

    Airfs->FileSystem->UserContext = Airfs;

    *PAirfs = Airfs;

    return STATUS_SUCCESS;
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
    PWSTR StorageFileName = L"";
    PWSTR MapName = L"";
    UINT64 VolumeSize = 16LL * 1024 * 1024;
    PWSTR FileSystemName = 0;
    PWSTR MountPoint = 0;
    PWSTR VolumePrefix = L"";
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
        case L'd': ARG_TO_4(DebugFlags); break;
        case L'D': ARG_TO_S(DebugLogFile); break;
        case L'f': OtherFlags = AirfsFlushAndPurgeOnCleanup; break;
        case L'F': ARG_TO_S(FileSystemName); break;
        case L'i': OtherFlags = AirfsCaseInsensitive; break;
        case L'm': ARG_TO_S(MountPoint); break;
        case L'N': ARG_TO_S(StorageFileName);  break;
        case L'n': ARG_TO_S(MapName);  break;
        case L'S': ARG_TO_S(RootSddl); break;
        case L's': ARG_TO_8(VolumeSize); break;
        case L't': ARG_TO_4(FileInfoTimeout); break;
        case L'u':
            ARG_TO_S(VolumePrefix);
            if (*VolumePrefix) Flags = AirfsNet;
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
            FAIL(L"cannot open debug log file");
            goto usage;
        }

        FspDebugLogSetHandle(DebugLogHandle);
    }

    Result = AirfsCreate(
        StorageFileName,
        MapName,
        Flags | OtherFlags,
        FileInfoTimeout,
        VolumeSize,
        FileSystemName,
        VolumePrefix,
        RootSddl,
        &Airfs);
    
    if (!NT_SUCCESS(Result))
    {
        FAIL(L"cannot create AIRFS");
        goto exit;
    }

    FspFileSystemSetDebugLog(Airfs->FileSystem, DebugFlags);

    if (MountPoint && L'\0' != MountPoint[0])
    {
        Result = FspFileSystemSetMountPoint(Airfs->FileSystem,
            L'*' == MountPoint[0] && L'\0' == MountPoint[1] ? 0 : MountPoint);
        if (!NT_SUCCESS(Result))
        {
            FAIL(L"cannot mount AIRFS");
            goto exit;
        }
    }

    Result = FspFileSystemStartDispatcher(Airfs->FileSystem, 0);
    if (!NT_SUCCESS(Result))
    {
        FAIL(L"cannot start AIRFS");
        goto exit;
    }

    MountPoint = FspFileSystemMountPoint(Airfs->FileSystem);

    WCHAR buffer[1024];
    _snwprintf_s(buffer, 1024, L"%S%S%s%S%s -t %ld -s %lld%S%s%S%s%S%s",
        PROGNAME, 
        *StorageFileName ? " -N " : "", *StorageFileName ? StorageFileName : L"", 
        *MapName         ? " -n " : "", *MapName         ? MapName         : L"",
        FileInfoTimeout, VolumeSize,
        RootSddl         ? " -S " : "", RootSddl         ? RootSddl        : L"",
        *VolumePrefix    ? " -u " : "", *VolumePrefix    ? VolumePrefix    : L"",
        MountPoint       ? " -m " : "", MountPoint       ? MountPoint      : L"");
    INFO(buffer);

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
        "    -f                  [flush and purge cache on cleanup]\n"
        "    -F FileSystemName\n"
        "    -i                  [case insensitive file system]\n"
        "    -m MountPoint       [X:|* (required if no UNC prefix)]\n"
        "    -n MapName          [(ex) \"Local\\Airfs\"]\n"
        "    -N StorageFileName  [\"\": in memory only]\n"
        "    -s VolumeSize       [bytes]\n"
        "    -S RootSddl         [file rights: FA, etc; NO generic rights: GA, etc.]\n"
        "    -t FileInfoTimeout  [millis]\n"
        "    -u \\Server\\Share    [UNC prefix (single backslash)]\n";

    FAIL(usage, L"" PROGNAME);

    return STATUS_UNSUCCESSFUL;
}

//////////////////////////////////////////////////////////////////////

NTSTATUS SvcStop(FSP_SERVICE *Service)
{
    AIRFS_ Airfs = (AIRFS_) Service->UserContext;
    FspFileSystemStopDispatcher(Airfs->FileSystem);
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
//////////////////////////////////////////////////////////////////////
