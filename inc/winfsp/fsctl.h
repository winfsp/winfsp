/**
 * @file winfsp/fsctl.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_FSCTL_H_INCLUDED
#define WINFSP_FSCTL_H_INCLUDED

#include <devioctl.h>

// {6F9D25FA-6DEE-4A9D-80F5-E98E14F35E54}
extern const __declspec(selectany) GUID FspFsctlDeviceClassGuid =
    { 0x6f9d25fa, 0x6dee, 0x4a9d, { 0x80, 0xf5, 0xe9, 0x8e, 0x14, 0xf3, 0x5e, 0x54 } };
// {B48171C3-DD50-4852-83A3-344C50D93B17}
extern const __declspec(selectany) GUID FspFsvrtDeviceClassGuid =
    { 0xb48171c3, 0xdd50, 0x4852, { 0x83, 0xa3, 0x34, 0x4c, 0x50, 0xd9, 0x3b, 0x17 } };

#define FSP_FSCTL_DISK_DEVICE_NAME      "WinFsp.Disk"
#define FSP_FSCTL_NET_DEVICE_NAME       "WinFsp.Net"

/* alignment macros */
#define FSP_FSCTL_ALIGN_UP(x, s)        (((x) + ((s) - 1L)) & ~((s) - 1L))
#define FSP_FSCTL_DEFAULT_ALIGNMENT     (16)
#define FSP_FSCTL_DECLSPEC_ALIGN        __declspec(align(16))

/* fsctl device codes */
#define FSP_FSCTL_CREATE                \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'C', METHOD_BUFFERED, FILE_ANY_ACCESS)

/* fsvrt device codes */
#define FSP_FSCTL_DELETE                \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'D', METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSP_FSCTL_TRANSACT              \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0x800 + 'T', METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSP_FSCTL_CREATE_BUFFER_SIZE    128
#define FSP_FSCTL_TRANSACT_BUFFER_SIZE  (16 * 1024)

#define FSP_FSCTL_VOLUME_PARAMS_SIZE    \
    FSP_FSCTL_ALIGN_UP(sizeof(FSP_FSCTL_VOLUME_PARAMS), FSP_FSCTL_DEFAULT_ALIGNMENT)
#define FSP_FSCTL_TRANSACT_REQ_SIZEMAX  \
    FSP_FSCTL_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_REQ) + 1024 * sizeof(WCHAR), FSP_FSCTL_DEFAULT_ALIGNMENT)
#define FSP_FSCTL_TRANSACT_RSP_SIZEMAX  \
    FSP_FSCTL_ALIGN_UP(sizeof(FSP_FSCTL_TRANSACT_RSP) + 1024 * sizeof(WCHAR), FSP_FSCTL_DEFAULT_ALIGNMENT)

/* marshalling */
#pragma warning(push)
#pragma warning(disable:4200)           /* zero-sized array in struct/union */
typedef struct
{
    UINT16 Version;
    UINT16 SectorSize;
    UINT32 SerialNumber;
    BOOLEAN EaSupported;
} FSP_FSCTL_VOLUME_PARAMS;
typedef struct
{
    UINT16 Version;
    UINT16 Size;
    UINT64 Hint;
    UINT8 Kind;
    union
    {
        struct
        {
            UINT8 Placeholder;
            WCHAR FileName[];
        } Create;
    } Req;
} FSP_FSCTL_TRANSACT_REQ;
typedef struct
{
    UINT16 Version;
    UINT16 Size;
    UINT64 Hint;
    struct
    {
        UINT32 Status;
        UINT64 Information;
    } IoStatus;
    UINT8 Kind;
    union
    {
        UINT8 Placeholder; // !!!: REMOVE
    } Req;
} FSP_FSCTL_TRANSACT_RSP;
#pragma warning(pop)
static inline FSP_FSCTL_TRANSACT_REQ *FspFsctlTransactProduceRequest(
    FSP_FSCTL_TRANSACT_REQ *Request, SIZE_T RequestSize, PVOID RequestBufEnd)
{
    PVOID NextRequest = (PUINT8)Request + FSP_FSCTL_ALIGN_UP(RequestSize, FSP_FSCTL_DEFAULT_ALIGNMENT);
    return NextRequest <= RequestBufEnd ? NextRequest : 0;
}
static inline const FSP_FSCTL_TRANSACT_REQ *FspFsctlTransactConsumeRequest(
    const FSP_FSCTL_TRANSACT_REQ *Request, PVOID RequestBufEnd)
{
    if ((PUINT8)Request + sizeof(Request->Size) > (PUINT8)RequestBufEnd ||
        sizeof(FSP_FSCTL_TRANSACT_REQ) > Request->Size)
        return 0;
    PVOID NextRequest = (PUINT8)Request + FSP_FSCTL_ALIGN_UP(Request->Size, FSP_FSCTL_DEFAULT_ALIGNMENT);
    return NextRequest <= RequestBufEnd ? NextRequest : 0;
}
static inline FSP_FSCTL_TRANSACT_RSP *FspFsctlTransactProduceResponse(
    FSP_FSCTL_TRANSACT_RSP *Response, SIZE_T ResponseSize, PVOID ResponseBufEnd)
{
    PVOID NextResponse = (PUINT8)Response + FSP_FSCTL_ALIGN_UP(ResponseSize, FSP_FSCTL_DEFAULT_ALIGNMENT);
    return NextResponse <= ResponseBufEnd ? NextResponse : 0;
}
static inline const FSP_FSCTL_TRANSACT_RSP *FspFsctlTransactConsumeResponse(
    const FSP_FSCTL_TRANSACT_RSP *Response, PVOID ResponseBufEnd)
{
    if ((PUINT8)Response + sizeof(Response->Size) > (PUINT8)ResponseBufEnd ||
        sizeof(FSP_FSCTL_TRANSACT_RSP) > Response->Size)
        return 0;
    PVOID NextResponse = (PUINT8)Response + FSP_FSCTL_ALIGN_UP(Response->Size, FSP_FSCTL_DEFAULT_ALIGNMENT);
    return NextResponse <= ResponseBufEnd ? NextResponse : 0;
}

#if !defined(WINFSP_SYS_INTERNAL)
FSP_API NTSTATUS FspFsctlCreateVolume(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *Params, PSECURITY_DESCRIPTOR SecurityDescriptor,
    PWCHAR VolumePathBuf, SIZE_T VolumePathSize);
FSP_API NTSTATUS FspFsctlOpenVolume(PWSTR VolumePath,
    PHANDLE PVolumeHandle);
FSP_API NTSTATUS FspFsctlDeleteVolume(HANDLE VolumeHandle);
FSP_API NTSTATUS FspFsctlTransact(HANDLE VolumeHandle,
    FSP_FSCTL_TRANSACT_RSP *ResponseBuf, SIZE_T ResponseBufSize,
    FSP_FSCTL_TRANSACT_REQ *RequestBuf, SIZE_T *PRequestBufSize);
#endif

#endif
