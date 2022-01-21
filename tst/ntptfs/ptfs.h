/**
 * @file ptfs.h
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

#ifndef PTFS_H_INCLUDED
#define PTFS_H_INCLUDED

#include <winfsp/winfsp.h>
#include <strsafe.h>

#define PROGNAME                        "ptfs"

#define info(format, ...)               FspServiceLog(EVENTLOG_INFORMATION_TYPE, format, __VA_ARGS__)
#define warn(format, ...)               FspServiceLog(EVENTLOG_WARNING_TYPE, format, __VA_ARGS__)
#define fail(format, ...)               FspServiceLog(EVENTLOG_ERROR_TYPE, format, __VA_ARGS__)


/*
 * PTFS
 */

enum
{
    PtfsReparsePoints = 0x00000010,
    PtfsNamedStreams = 0x00000040,
    PtfsExtendedAttributes = 0x00000100,
    PtfsWslFeatures = 0x04000000,
    PtfsFlushAndPurgeOnCleanup = 0x00004000,
    PtfsSetAllocationSizeOnCleanup = 0x00010000,                // reuse UmFileContextIsUserContext2
    PtfsAttributesMask =
        PtfsReparsePoints |
        PtfsNamedStreams |
        PtfsExtendedAttributes |
        PtfsWslFeatures |
        PtfsFlushAndPurgeOnCleanup |
        PtfsSetAllocationSizeOnCleanup,
};
typedef struct
{
    FSP_FILE_SYSTEM *FileSystem;
    BOOLEAN HasSecurityPrivilege;
    HANDLE RootHandle;
    ULONG RootPrefixLength;
    ULONG FsAttributeMask;
    ULONG FsAttributes;
    UINT64 AllocationUnit;
} PTFS;
NTSTATUS PtfsCreate(
    PWSTR RootPath,
    ULONG FileInfoTimeout,
    ULONG FsAttributeMask,
    PWSTR VolumePrefix,
    PWSTR MountPoint,
    UINT32 DebugFlags,
    PTFS **PPtfs);
VOID PtfsDelete(PTFS *Ptfs);


/*
 * Lower file system
 */

NTSTATUS LfsCreateFile(
    PHANDLE PHandle,
    ACCESS_MASK DesiredAccess,
    HANDLE RootHandle,
    PWSTR FileName,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength);
NTSTATUS LfsOpenFile(
    PHANDLE PHandle,
    ACCESS_MASK DesiredAccess,
    HANDLE RootHandle,
    PWSTR FileName,
    ULONG OpenOptions);
NTSTATUS LfsGetFileInfo(
    HANDLE Handle,
    ULONG RootPrefixLength,
    FSP_FSCTL_FILE_INFO *FileInfo);
NTSTATUS LfsReadFile(
    HANDLE Handle,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length,
    PULONG PBytesTransferred);
NTSTATUS LfsWriteFile(
    HANDLE Handle,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length,
    PULONG PBytesTransferred);
NTSTATUS LfsQueryDirectoryFile(
    HANDLE Handle,
    PVOID Buffer,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    BOOLEAN ReturnSingleEntry,
    PWSTR FileName,
    BOOLEAN RestartScan,
    PULONG PBytesTransferred);
NTSTATUS LfsFsControlFile(
    HANDLE Handle,
    ULONG FsControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG PBytesTransferred);
static inline
ULONG LfsGetEaSize(ULONG EaSize)
{
    return 0 != EaSize ? EaSize - 4 : 0;
}


/*
 * Missing NTDLL definitions
 */

typedef struct
{
    ACCESS_MASK AccessFlags;
} FILE_ACCESS_INFORMATION;
typedef struct
{
    ULONG AlignmentRequirement;
} FILE_ALIGNMENT_INFORMATION;
typedef struct
{
    LARGE_INTEGER AllocationSize;
} FILE_ALLOCATION_INFORMATION;
typedef struct
{
    ULONG FileAttributes;
    ULONG ReparseTag;
} FILE_ATTRIBUTE_TAG_INFORMATION;
typedef struct
{
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
} FILE_BASIC_INFORMATION;
typedef struct
{
    BOOLEAN DeleteFile;
} FILE_DISPOSITION_INFORMATION;
typedef struct
{
    ULONG Flags;
} FILE_DISPOSITION_INFORMATION_EX;
typedef struct
{
    ULONG EaSize;
} FILE_EA_INFORMATION;
typedef struct
{
    LARGE_INTEGER EndOfFile;
} FILE_END_OF_FILE_INFORMATION;
typedef struct
{
    LARGE_INTEGER IndexNumber;
} FILE_INTERNAL_INFORMATION;
typedef struct
{
    ULONG Mode;
} FILE_MODE_INFORMATION;
typedef struct
{
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_NAME_INFORMATION;
typedef struct
{
    LARGE_INTEGER CurrentByteOffset;
} FILE_POSITION_INFORMATION;
typedef struct
{
    union
    {
        BOOLEAN ReplaceIfExists;
        ULONG Flags;
    } DUMMYUNIONNAME;
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_RENAME_INFORMATION;
typedef struct
{
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG NumberOfLinks;
    BOOLEAN DeletePending;
    BOOLEAN Directory;
} FILE_STANDARD_INFORMATION;
typedef struct
{
    ULONG NextEntryOffset;
    ULONG StreamNameLength;
    LARGE_INTEGER StreamSize;
    LARGE_INTEGER StreamAllocationSize;
    WCHAR StreamName[1];
} FILE_STREAM_INFORMATION;
typedef struct
{
    FILE_BASIC_INFORMATION BasicInformation;
    FILE_STANDARD_INFORMATION StandardInformation;
    FILE_INTERNAL_INFORMATION InternalInformation;
    FILE_EA_INFORMATION EaInformation;
    FILE_ACCESS_INFORMATION AccessInformation;
    FILE_POSITION_INFORMATION PositionInformation;
    FILE_MODE_INFORMATION ModeInformation;
    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
    FILE_NAME_INFORMATION NameInformation;
} FILE_ALL_INFORMATION;

typedef struct
{
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    ULONG EaSize;
    CCHAR ShortNameLength;
    WCHAR ShortName[12];
    LARGE_INTEGER FileId;
    WCHAR FileName[1];
} FILE_ID_BOTH_DIR_INFORMATION;

typedef struct
{
    ULONG FileSystemAttributes;
    LONG MaximumComponentNameLength;
    ULONG FileSystemNameLength;
    WCHAR FileSystemName[1];
} FILE_FS_ATTRIBUTE_INFORMATION;
typedef struct
{
    LARGE_INTEGER TotalAllocationUnits;
    LARGE_INTEGER AvailableAllocationUnits;
    ULONG SectorsPerAllocationUnit;
    ULONG BytesPerSector;
} FILE_FS_SIZE_INFORMATION;

NTSTATUS NTAPI NtFlushBuffersFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock);

NTSTATUS NTAPI NtFsControlFile(
    HANDLE FileHandle,
    HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG FsControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength);

NTSTATUS NTAPI NtQueryEaFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer,
    ULONG Length,
    BOOLEAN ReturnSingleEntry,
    PVOID EaList,
    ULONG EaListLength,
    PULONG EaIndex,
    BOOLEAN RestartScan);

NTSTATUS NTAPI NtQueryDirectoryFile(
    HANDLE FileHandle,
    HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    BOOLEAN ReturnSingleEntry,
    PUNICODE_STRING FileName,
    BOOLEAN RestartScan);

NTSTATUS NTAPI NtQueryInformationFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass);

NTSTATUS NTAPI NtQuerySecurityObject(
    HANDLE Handle,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    ULONG Length,
    PULONG LengthNeeded);

NTSTATUS NTAPI NtQueryVolumeInformationFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FsInformation,
    ULONG Length,
    ULONG FsInformationClass);

NTSTATUS NTAPI NtReadFile(
    HANDLE FileHandle,
    HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key);

NTSTATUS NTAPI NtSetEaFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer,
    ULONG Length);

NTSTATUS NTAPI NtSetInformationFile(
    HANDLE FileHandle,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID FileInformation,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass);

NTSTATUS NTAPI NtSetSecurityObject(
    HANDLE Handle,
    SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR SecurityDescriptor);

NTSTATUS NTAPI NtWriteFile(
    HANDLE FileHandle,
    HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER ByteOffset,
    PULONG Key);

#endif
