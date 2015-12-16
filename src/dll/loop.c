/**
 * @file dll/loop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

typedef struct _FSP_WORK_ITEM
{
    FSP_FILE_SYSTEM *FileSystem;
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 RequestBuf[];
} FSP_WORK_ITEM;

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *Params, FSP_FILE_SYSTEM_PROCESSREQ *ProcessRequest,
    FSP_FILE_SYSTEM **PFileSystem)
{
    NTSTATUS Result;
    WCHAR VolumePathBuf[MAX_PATH];
    HANDLE VolumeHandle = INVALID_HANDLE_VALUE;
    FSP_FILE_SYSTEM *FileSystem = 0;

    *PFileSystem = 0;

    if (0 == ProcessRequest)
        ProcessRequest = FspProcessRequestDirect;

    FileSystem = MemAlloc(sizeof *FileSystem);
    if (0 == FileSystem)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Result = FspFsctlCreateVolume(DevicePath, Params, 0, VolumePathBuf, sizeof VolumePathBuf);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = FspFsctlOpenVolume(VolumePathBuf, &VolumeHandle);
    if (!NT_SUCCESS(Result))
        goto exit;

    memset(FileSystem, 0, sizeof *FileSystem);
    FileSystem->VolumeHandle = VolumeHandle;
    FileSystem->ProcessRequest = ProcessRequest;
    *PFileSystem = FileSystem;

    Result = STATUS_SUCCESS;

exit:
    if (!NT_SUCCESS(Result))
    {
        if (INVALID_HANDLE_VALUE != VolumeHandle)
            CloseHandle(VolumeHandle);
        MemFree(FileSystem);
    }

    return Result;
}

FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem)
{
    if (INVALID_HANDLE_VALUE != FileSystem->VolumeHandle)
        CloseHandle(FileSystem->VolumeHandle);
    MemFree(FileSystem);
}

FSP_API NTSTATUS FspFileSystemLoop(FSP_FILE_SYSTEM *FileSystem)
{
    NTSTATUS Result;
    PUINT8 RequestBuf, RequestBufEnd;
    SIZE_T RequestBufSize;
    FSP_FSCTL_TRANSACT_REQ *Request, *NextRequest;

    RequestBuf = MemAlloc(FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMIN);
    if (0 == RequestBuf)
        return STATUS_INSUFFICIENT_RESOURCES;

    for (;;)
    {
        RequestBufSize = FSP_FSCTL_TRANSACT_REQ_BUFFER_SIZEMIN;
        Result = FspFsctlTransact(FileSystem->VolumeHandle, 0, 0, RequestBuf, &RequestBufSize);
        if (!NT_SUCCESS(Result))
            goto exit;
        RequestBufEnd = RequestBuf + RequestBufSize;

        Request = (PVOID)RequestBuf;
        for (;;)
        {
            NextRequest = FspFsctlTransactConsumeRequest(Request, RequestBufEnd);
            if (0 == NextRequest)
                break;

            Result = FileSystem->ProcessRequest(FileSystem, Request);
            if (!NT_SUCCESS(Result))
                goto exit;

            Request = NextRequest;
        }
    }

exit:
    MemFree(RequestBuf);

    return Result;
}

FSP_API NTSTATUS FspProcessRequestDirect(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (FspFsctlTransactKindCount <= Request->Kind || 0 == FileSystem->Operations[Request->Kind])
        return FspProduceResponseWithStatus(FileSystem, Request, STATUS_INVALID_DEVICE_REQUEST);

    FileSystem->Operations[Request->Kind](FileSystem, Request);
    return STATUS_SUCCESS;
}

static DWORD WINAPI FspProcessRequestInPoolWorker(PVOID Param)
{
    FSP_WORK_ITEM *WorkItem = Param;
    FSP_FSCTL_TRANSACT_REQ *Request = (PVOID)WorkItem->RequestBuf;

    WorkItem->FileSystem->Operations[Request->Kind](WorkItem->FileSystem, Request);
    MemFree(WorkItem);

    return 0;
}

FSP_API NTSTATUS FspProcessRequestInPool(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    if (FspFsctlTransactKindCount <= Request->Kind || 0 == FileSystem->Operations[Request->Kind])
        return FspProduceResponseWithStatus(FileSystem, Request, STATUS_INVALID_DEVICE_REQUEST);

    FSP_WORK_ITEM *WorkItem;
    BOOLEAN Success;

    WorkItem = MemAlloc(sizeof *WorkItem + Request->Size);
    if (0 == WorkItem)
        return STATUS_INSUFFICIENT_RESOURCES;

    WorkItem->FileSystem = FileSystem;
    memcpy(WorkItem->RequestBuf, Request, Request->Size);

    Success = QueueUserWorkItem(FspProcessRequestInPoolWorker, WorkItem, WT_EXECUTEDEFAULT);
    if (!Success)
    {
        NTSTATUS Result0 = FspNtStatusFromWin32(GetLastError());
        MemFree(WorkItem);
        return Result0;
    }

    return STATUS_SUCCESS;
}

FSP_API NTSTATUS FspProduceResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response)
{
    return FspFsctlTransact(FileSystem->VolumeHandle, Response, Response->Size, 0, 0);
}

FSP_API NTSTATUS FspProduceResponseWithStatus(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, NTSTATUS Result)
{
    FSP_FSCTL_TRANSACT_RSP Response;
    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = Request->Kind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = Result;
    return FspProduceResponse(FileSystem, &Response);
}
