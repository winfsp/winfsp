/**
 * @file winfsp/fsctl.h
 *
 * @copyright 2015-2017 Bill Zissimopoulos
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

#ifndef WINFSP_FSCTL_H_INCLUDED
#define WINFSP_FSCTL_H_INCLUDED

#include <devioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* static_assert is a C++11 feature, but seems to work with C on MSVC 2015 */
#if defined(WINFSP_SYS_INTERNAL) || defined(WINFSP_DLL_INTERNAL)
#define FSP_FSCTL_STATIC_ASSERT(e,m)    static_assert(e,m)
#else
#define FSP_FSCTL_STATIC_ASSERT(e,m)    static_assert(1,"")
#endif

#define FSP_FSCTL_DRIVER_NAME           "WinFsp"
#define FSP_FSCTL_DISK_DEVICE_NAME      "WinFsp.Disk"
#define FSP_FSCTL_NET_DEVICE_NAME       "WinFsp.Net"

// {6F9D25FA-6DEE-4A9D-80F5-E98E14F35E54}
extern const __declspec(selectany) GUID FspFsctlDeviceClassGuid =
    { 0x6f9d25fa, 0x6dee, 0x4a9d, { 0x80, 0xf5, 0xe9, 0x8e, 0x14, 0xf3, 0x5e, 0x54 } };
// {B48171C3-DD50-4852-83A3-344C50D93B17}
extern const __declspec(selectany) GUID FspFsvrtDeviceClassGuid =
    { 0xb48171c3, 0xdd50, 0x4852, { 0x83, 0xa3, 0x34, 0x4c, 0x50, 0xd9, 0x3b, 0x17 } };

/* alignment macros */
#define FSP_FSCTL_ALIGN_UP(x, s)        (((x) + ((s) - 1L)) & ~((s) - 1L))
#define FSP_FSCTL_DEFAULT_ALIGNMENT     8
#define FSP_FSCTL_DEFAULT_ALIGN_UP(x)   FSP_FSCTL_ALIGN_UP(x, FSP_FSCTL_DEFAULT_ALIGNMENT)
#define FSP_FSCTL_DECLSPEC_ALIGN        __declspec(align(FSP_FSCTL_DEFAULT_ALIGNMENT))

/* fsctl device codes */
#define FSP_FSCTL_VOLUME_NAME           \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'N', METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSP_FSCTL_VOLUME_LIST           \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'L', METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSP_FSCTL_TRANSACT              \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'T', METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSP_FSCTL_TRANSACT_BATCH        \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 't', METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define FSP_FSCTL_STOP                  \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'S', METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSP_FSCTL_VOLUME_PARAMS_PREFIX  "\\VolumeParams="

#define FSP_FSCTL_VOLUME_NAME_SIZE      (64 * sizeof(WCHAR))
#define FSP_FSCTL_VOLUME_PREFIX_SIZE    (192 * sizeof(WCHAR))
#define FSP_FSCTL_VOLUME_FSNAME_SIZE    (16 * sizeof(WCHAR))
#define FSP_FSCTL_VOLUME_NAME_SIZEMAX   (FSP_FSCTL_VOLUME_NAME_SIZE + FSP_FSCTL_VOLUME_PREFIX_SIZE)
FSP_FSCTL_STATIC_ASSERT(FSP_FSCTL_VOLUME_NAME_SIZEMAX <= 260 * sizeof(WCHAR),
    "Max volume name size is greater than MAX_PATH.");

#define FSP_FSCTL_TRANSACT_PATH_SIZEMAX (1024 * sizeof(WCHAR))

#define FSP_FSCTL_TRANSACT_REQ_SIZEMAX  (16 * 1024 - 64)    /* 64: size for internal request header */
#define FSP_FSCTL_TRANSACT_RSP_SIZEMAX  (16 * 1024)
#define FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMAX   (FSP_FSCTL_TRANSACT_REQ_SIZEMAX - sizeof(FSP_FSCTL_TRANSACT_REQ))
#define FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX   (FSP_FSCTL_TRANSACT_RSP_SIZEMAX - sizeof(FSP_FSCTL_TRANSACT_RSP))
#define FSP_FSCTL_TRANSACT_BATCH_BUFFER_SIZEMIN (64 * 1024)
#define FSP_FSCTL_TRANSACT_BUFFER_SIZEMIN       FSP_FSCTL_TRANSACT_REQ_SIZEMAX

#define FSP_FSCTL_TRANSACT_REQ_TOKEN_HANDLE(T)  ((HANDLE)((T) & 0xffffffff))
#define FSP_FSCTL_TRANSACT_REQ_TOKEN_PID(T)     ((UINT32)(((T) >> 32) & 0xffffffff))

/* marshalling */
#pragma warning(push)
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
enum
{
    FspFsctlTransactReservedKind = 0,
    FspFsctlTransactCreateKind,
    FspFsctlTransactOverwriteKind,
    FspFsctlTransactCleanupKind,
    FspFsctlTransactCloseKind,
    FspFsctlTransactReadKind,
    FspFsctlTransactWriteKind,
    FspFsctlTransactQueryInformationKind,
    FspFsctlTransactSetInformationKind,
    FspFsctlTransactQueryEaKind,
    FspFsctlTransactSetEaKind,
    FspFsctlTransactFlushBuffersKind,
    FspFsctlTransactQueryVolumeInformationKind,
    FspFsctlTransactSetVolumeInformationKind,
    FspFsctlTransactQueryDirectoryKind,
    FspFsctlTransactFileSystemControlKind,
    FspFsctlTransactDeviceControlKind,
    FspFsctlTransactShutdownKind,
    FspFsctlTransactLockControlKind,
    FspFsctlTransactQuerySecurityKind,
    FspFsctlTransactSetSecurityKind,
    FspFsctlTransactQueryStreamInformationKind,
    FspFsctlTransactKindCount,
};
enum
{
    FspFsctlTransactTimeoutMinimum = 1000,
    FspFsctlTransactTimeoutMaximum = 10000,
    FspFsctlTransactTimeoutDefault = 1000,
    FspFsctlIrpTimeoutMinimum = 60000,
    FspFsctlIrpTimeoutMaximum = 600000,
    FspFsctlIrpTimeoutDefault = 300000,
    FspFsctlIrpTimeoutDebug = 142,      /* special value for IRP timeout testing */
    FspFsctlIrpCapacityMinimum = 100,
    FspFsctlIrpCapacityMaximum = 1000,
    FspFsctlIrpCapacityDefault = 1000,
};
typedef struct
{
    UINT16 Version;                     /* set to 0 */
    /* volume information */
    UINT16 SectorSize;
    UINT16 SectorsPerAllocationUnit;
    UINT16 MaxComponentLength;          /* maximum file name component length (bytes) */
    UINT64 VolumeCreationTime;
    UINT32 VolumeSerialNumber;
    /* I/O timeouts, capacity, etc. */
    UINT32 TransactTimeout;             /* FSP_FSCTL_TRANSACT timeout (millis; 1 sec - 10 sec) */
    UINT32 IrpTimeout;                  /* pending IRP timeout (millis; 1 min - 10 min) */
    UINT32 IrpCapacity;                 /* maximum number of pending IRP's (100 - 1000)*/
    UINT32 FileInfoTimeout;             /* FileInfo/Security/VolumeInfo timeout (millis) */
    /* FILE_FS_ATTRIBUTE_INFORMATION::FileSystemAttributes */
    UINT32 CaseSensitiveSearch:1;       /* file system supports case-sensitive file names */
    UINT32 CasePreservedNames:1;        /* file system preserves the case of file names */
    UINT32 UnicodeOnDisk:1;             /* file system supports Unicode in file names */
    UINT32 PersistentAcls:1;            /* file system preserves and enforces access control lists */
    UINT32 ReparsePoints:1;             /* file system supports reparse points */
    UINT32 ReparsePointsAccessCheck:1;  /* file system performs reparse point access checks */
    UINT32 NamedStreams:1;              /* file system supports named streams */
    UINT32 HardLinks:1;                 /* unimplemented; set to 0 */
    UINT32 ExtendedAttributes:1;        /* unimplemented; set to 0 */
    UINT32 ReadOnlyVolume:1;
    /* kernel-mode flags */
    UINT32 PostCleanupWhenModifiedOnly:1;   /* post Cleanup when a file was modified/deleted */
    UINT32 PassQueryDirectoryPattern:1;     /* pass Pattern during QueryDirectory operations */
    UINT32 AlwaysUseDoubleBuffering:1;
    UINT32 PassQueryDirectoryFileName:1;    /* pass FileName during QueryDirectory (GetDirInfoByName) */
    UINT32 KmReservedFlags:2;
    /* user-mode flags */
    UINT32 UmFileContextIsUserContext2:1;   /* user mode: FileContext parameter is UserContext2 */
    UINT32 UmFileContextIsFullContext:1;    /* user mode: FileContext parameter is FullContext */
    UINT32 UmReservedFlags:14;
    WCHAR Prefix[FSP_FSCTL_VOLUME_PREFIX_SIZE / sizeof(WCHAR)]; /* UNC prefix (\Server\Share) */
    WCHAR FileSystemName[FSP_FSCTL_VOLUME_FSNAME_SIZE / sizeof(WCHAR)];
} FSP_FSCTL_VOLUME_PARAMS;
typedef struct
{
    UINT64 TotalSize;
    UINT64 FreeSize;
    UINT16 VolumeLabelLength;
    WCHAR VolumeLabel[32];
} FSP_FSCTL_VOLUME_INFO;
typedef struct
{
    UINT32 FileAttributes;
    UINT32 ReparseTag;
    UINT64 AllocationSize;
    UINT64 FileSize;
    UINT64 CreationTime;
    UINT64 LastAccessTime;
    UINT64 LastWriteTime;
    UINT64 ChangeTime;
    UINT64 IndexNumber;
    UINT32 HardLinks;                   /* unimplemented: set to 0 */
} FSP_FSCTL_FILE_INFO;
typedef struct
{
    FSP_FSCTL_FILE_INFO FileInfo;
    PWSTR NormalizedName;
    UINT16 NormalizedNameSize;
} FSP_FSCTL_OPEN_FILE_INFO;
typedef struct
{
    UINT16 Size;
    FSP_FSCTL_FILE_INFO FileInfo;
    UINT8 Padding[24];
        /* make struct as big as FILE_ID_BOTH_DIR_INFORMATION; allows for in-place copying */
    WCHAR FileNameBuf[];
} FSP_FSCTL_DIR_INFO;
typedef struct
{
    UINT16 Size;
    UINT64 StreamSize;
    UINT64 StreamAllocationSize;
    WCHAR StreamNameBuf[];
} FSP_FSCTL_STREAM_INFO;
typedef struct
{
    UINT64 UserContext;
    UINT64 UserContext2;
} FSP_FSCTL_TRANSACT_FULL_CONTEXT;
typedef struct
{
    UINT16 Offset;
    UINT16 Size;
} FSP_FSCTL_TRANSACT_BUF;
typedef struct
{
    UINT16 Version;
    UINT16 Size;
    UINT32 Kind;
    UINT64 Hint;
    union
    {
        struct
        {
            UINT32 CreateOptions;       /* Disposition: high 8 bits; Options: low 24 bits */
            UINT32 FileAttributes;      /* file attributes for new files */
            FSP_FSCTL_TRANSACT_BUF SecurityDescriptor; /* security descriptor for new files */
            UINT64 AllocationSize;      /* initial allocation size */
            UINT64 AccessToken;         /* request access token (PID,HANDLE) */
            UINT32 DesiredAccess;       /* FILE_{READ_DATA,WRITE_DATA,etc.} */
            UINT32 GrantedAccess;       /* FILE_{READ_DATA,WRITE_DATA,etc.} */
            UINT32 ShareAccess;         /* FILE_SHARE_{READ,WRITE,DELETE} */
            FSP_FSCTL_TRANSACT_BUF Ea;  /* reserved; not currently implemented */
            UINT32 UserMode:1;          /* request originated in user mode */
            UINT32 HasTraversePrivilege:1;  /* requestor has TOKEN_HAS_TRAVERSE_PRIVILEGE */
            UINT32 HasBackupPrivilege:1;    /* requestor has TOKEN_HAS_BACKUP_PRIVILEGE */
            UINT32 HasRestorePrivilege:1;   /* requestor has TOKEN_HAS_RESTORE_PRIVILEGE */
            UINT32 OpenTargetDirectory:1;   /* open target dir and report FILE_{EXISTS,DOES_NOT_EXIST} */
            UINT32 CaseSensitive:1;         /* FileName comparisons should be case-sensitive */
            UINT32 HasTrailingBackslash:1;  /* FileName had trailing backslash */
            UINT32 ReservedFlags:25;
            UINT16 NamedStream;             /* request targets named stream; colon offset in FileName */
        } Create;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT32 FileAttributes;      /* file attributes for overwritten/superseded files */
            UINT64 AllocationSize;      /* allocation size for overwritten/superseded files */
            UINT32 Supersede:1;         /* 0: FILE_OVERWRITE operation, 1: FILE_SUPERSEDE operation */
        } Overwrite;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT32 Delete:1;            /* file must be deleted */
            UINT32 SetAllocationSize:1;
            UINT32 SetArchiveBit:1;
            UINT32 SetLastAccessTime:1;
            UINT32 SetLastWriteTime:1;
            UINT32 SetChangeTime:1;
        } Cleanup;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
        } Close;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT64 Address;
            UINT64 Offset;
            UINT32 Length;
            UINT32 Key;
        } Read;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT64 Address;
            UINT64 Offset;
            UINT32 Length;
            UINT32 Key;
            UINT32 ConstrainedIo:1;
        } Write;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
        } QueryInformation;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT32 FileInformationClass;
            union
            {
                struct
                {
                    UINT64 AllocationSize;
                } Allocation;
                struct
                {
                    UINT32 FileAttributes;
                    UINT64 CreationTime;
                    UINT64 LastAccessTime;
                    UINT64 LastWriteTime;
                    UINT64 ChangeTime;
                } Basic;
                struct
                {
                    UINT32 Delete:1;
                } Disposition;
                struct
                {
                    UINT64 FileSize;
                } EndOfFile;
                struct
                {
                    FSP_FSCTL_TRANSACT_BUF NewFileName;
                    UINT64 AccessToken; /* request access token (PID,HANDLE) */
                } Rename;
            } Info;
        } SetInformation;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
        } FlushBuffers;
        struct
        {
            UINT32 FsInformationClass;
            union
            {
                struct
                {
                    FSP_FSCTL_TRANSACT_BUF VolumeLabel;
                } Label;
            } Info;
        } SetVolumeInformation;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT64 Address;
            UINT32 Length;
            FSP_FSCTL_TRANSACT_BUF Pattern;
            FSP_FSCTL_TRANSACT_BUF Marker;
            UINT32 CaseSensitive:1;     /* FileName comparisons should be case-sensitive */
            UINT32 PatternIsFileName:1; /* Pattern does not contain wildcards */
        } QueryDirectory;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT32 FsControlCode;
            FSP_FSCTL_TRANSACT_BUF Buffer;
            UINT16 TargetOnFileSystem;  /* the target of the symbolic link is on this file system */
        } FileSystemControl;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
        } QuerySecurity;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
            UINT32 SecurityInformation;
            FSP_FSCTL_TRANSACT_BUF SecurityDescriptor;
        } SetSecurity;
        struct
        {
            UINT64 UserContext;
            UINT64 UserContext2;
        } QueryStreamInformation;
    } Req;
    FSP_FSCTL_TRANSACT_BUF FileName;
        /* Create,Cleanup,SetInformation{Disposition,Rename},FileSystemControl{ReparsePoint} */
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 Buffer[];
} FSP_FSCTL_TRANSACT_REQ;
typedef struct
{
    UINT16 Version;
    UINT16 Size;
    UINT32 Kind;
    UINT64 Hint;
    struct
    {
        UINT32 Information;
        UINT32 Status;
    } IoStatus;
    union
    {
        union
        {
            /* IoStatus.Status == STATUS_SUCCESS */
            struct
            {
                UINT64 UserContext;     /* user context associated with file node */
                UINT64 UserContext2;    /* user context associated with file descriptor (handle) */
                UINT32 GrantedAccess;   /* FILE_{READ_DATA,WRITE_DATA,etc.} */
                FSP_FSCTL_FILE_INFO FileInfo;
                FSP_FSCTL_TRANSACT_BUF FileName;
                UINT32 DisableCache:1;
            } Opened;
            /* IoStatus.Status == STATUS_REPARSE */
            struct
            {
                FSP_FSCTL_TRANSACT_BUF Buffer;
            } Reparse;
        } Create;
        struct
        {
            FSP_FSCTL_FILE_INFO FileInfo;
        } Overwrite;
        struct
        {
            FSP_FSCTL_FILE_INFO FileInfo;
        } Write;
        struct
        {
            FSP_FSCTL_FILE_INFO FileInfo;
        } QueryInformation;
        struct
        {
            FSP_FSCTL_FILE_INFO FileInfo;       /* valid: File{Allocation,Basic,EndOfFile}Information */
        } SetInformation;
        struct
        {
            FSP_FSCTL_FILE_INFO FileInfo;       /* valid when flushing file (not volume) */
        } FlushBuffers;
        struct
        {
            FSP_FSCTL_VOLUME_INFO VolumeInfo;
        } QueryVolumeInformation;
        struct
        {
            FSP_FSCTL_VOLUME_INFO VolumeInfo;
        } SetVolumeInformation;
        struct
        {
            FSP_FSCTL_TRANSACT_BUF Buffer;
        } FileSystemControl;
        struct
        {
            FSP_FSCTL_TRANSACT_BUF SecurityDescriptor;
        } QuerySecurity;
        struct
        {
            FSP_FSCTL_TRANSACT_BUF SecurityDescriptor;  /* Size==0 means no security descriptor returned */
        } SetSecurity;
        struct
        {
            FSP_FSCTL_TRANSACT_BUF Buffer;
        } QueryStreamInformation;
    } Rsp;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 Buffer[];
} FSP_FSCTL_TRANSACT_RSP;
#pragma warning(pop)
FSP_FSCTL_STATIC_ASSERT(FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX > FSP_FSCTL_TRANSACT_PATH_SIZEMAX,
    "FSP_FSCTL_TRANSACT_RSP_BUFFER_SIZEMAX must be greater than FSP_FSCTL_TRANSACT_PATH_SIZEMAX "
    "to detect when a normalized name has been set during a Create/Open request.");
static inline BOOLEAN FspFsctlTransactCanProduceRequest(
    FSP_FSCTL_TRANSACT_REQ *Request, PVOID RequestBufEnd)
{
    return (PUINT8)Request + FSP_FSCTL_TRANSACT_REQ_SIZEMAX <= (PUINT8)RequestBufEnd;
}
static inline FSP_FSCTL_TRANSACT_REQ *FspFsctlTransactProduceRequest(
    FSP_FSCTL_TRANSACT_REQ *Request, SIZE_T RequestSize)
{
    PVOID NextRequest = (PUINT8)Request + FSP_FSCTL_DEFAULT_ALIGN_UP(RequestSize);
    return (FSP_FSCTL_TRANSACT_REQ *)NextRequest;
}
static inline FSP_FSCTL_TRANSACT_REQ *FspFsctlTransactConsumeRequest(
    FSP_FSCTL_TRANSACT_REQ *Request, PVOID RequestBufEnd)
{
    if ((PUINT8)Request + sizeof(Request->Size) > (PUINT8)RequestBufEnd ||
        sizeof(FSP_FSCTL_TRANSACT_REQ) > Request->Size)
        return 0;
    PVOID NextRequest = (PUINT8)Request + FSP_FSCTL_DEFAULT_ALIGN_UP(Request->Size);
    return NextRequest <= RequestBufEnd ? (FSP_FSCTL_TRANSACT_REQ *)NextRequest : 0;
}
static inline BOOLEAN FspFsctlTransactCanProduceResponse(
    FSP_FSCTL_TRANSACT_RSP *Response, PVOID ResponseBufEnd)
{
    return (PUINT8)Response + FSP_FSCTL_TRANSACT_RSP_SIZEMAX <= (PUINT8)ResponseBufEnd;
}
static inline FSP_FSCTL_TRANSACT_RSP *FspFsctlTransactProduceResponse(
    FSP_FSCTL_TRANSACT_RSP *Response, SIZE_T ResponseSize)
{
    PVOID NextResponse = (PUINT8)Response + FSP_FSCTL_DEFAULT_ALIGN_UP(ResponseSize);
    return (FSP_FSCTL_TRANSACT_RSP *)NextResponse;
}
static inline FSP_FSCTL_TRANSACT_RSP *FspFsctlTransactConsumeResponse(
    FSP_FSCTL_TRANSACT_RSP *Response, PVOID ResponseBufEnd)
{
    if ((PUINT8)Response + sizeof(Response->Size) > (PUINT8)ResponseBufEnd ||
        sizeof(FSP_FSCTL_TRANSACT_RSP) > Response->Size)
        return 0;
    PVOID NextResponse = (PUINT8)Response + FSP_FSCTL_DEFAULT_ALIGN_UP(Response->Size);
    return NextResponse <= ResponseBufEnd ? (FSP_FSCTL_TRANSACT_RSP *)NextResponse : 0;
}

#if !defined(WINFSP_SYS_INTERNAL)
FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    PWCHAR VolumeNameBuf, SIZE_T VolumeNameSize,
    PHANDLE PVolumeHandle);
FSP_API NTSTATUS FspFsctlTransact(HANDLE VolumeHandle,
    PVOID ResponseBuf, SIZE_T ResponseBufSize,
    PVOID RequestBuf, SIZE_T *PRequestBufSize,
    BOOLEAN Batch);
FSP_API NTSTATUS FspFsctlStop(HANDLE VolumeHandle);
FSP_API NTSTATUS FspFsctlGetVolumeList(PWSTR DevicePath,
    PWCHAR VolumeListBuf, PSIZE_T PVolumeListSize);
FSP_API NTSTATUS FspFsctlPreflight(PWSTR DevicePath);
#endif

#ifdef __cplusplus
}
#endif

#endif
