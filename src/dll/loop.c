/**
 * @file dll/loop.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>

typedef struct _FSP_DISPATCHER_WORK_ITEM
{
    FSP_FILE_SYSTEM *FileSystem;
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 RequestBuf[];
} FSP_DISPATCHER_WORK_ITEM;

FSP_API NTSTATUS FspFileSystemCreate(PWSTR DevicePath,
    const FSP_FSCTL_VOLUME_PARAMS *VolumeParams,
    const FSP_FILE_SYSTEM_INTERFACE *Interface,
    FSP_FILE_SYSTEM **PFileSystem)
{
    NTSTATUS Result;
    FSP_FILE_SYSTEM *FileSystem;

    *PFileSystem = 0;

    if (0 == Interface)
        return STATUS_INVALID_PARAMETER;

    FileSystem = MemAlloc(sizeof *FileSystem);
    if (0 == FileSystem)
        return STATUS_INSUFFICIENT_RESOURCES;
    memset(FileSystem, 0, sizeof *FileSystem);

    Result = FspFsctlCreateVolume(DevicePath, VolumeParams,
        FileSystem->VolumePath, sizeof FileSystem->VolumePath,
        &FileSystem->VolumeHandle);
    if (!NT_SUCCESS(Result))
    {
        MemFree(FileSystem);
        return Result;
    }

    FileSystem->Dispatcher = FspFileSystemDirectDispatcher;
    FileSystem->Operations[FspFsctlTransactCreateKind] = FspFileSystemOpCreate;
    // !!!: ...
    FileSystem->Interface = Interface;

    *PFileSystem = FileSystem;

    return STATUS_SUCCESS;
}

FSP_API VOID FspFileSystemDelete(FSP_FILE_SYSTEM *FileSystem)
{
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

            FileSystem->Dispatcher(FileSystem, Request);

            FspFileSystemGetDispatcherResult(FileSystem, &Result);
            if (!NT_SUCCESS(Result))
                goto exit;

            Request = NextRequest;
        }
    }

exit:
    MemFree(RequestBuf);

    return Result;
}

FSP_API VOID FspFileSystemDirectDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS DispatcherResult;

    if (FspFsctlTransactKindCount <= Request->Kind || 0 == FileSystem->Operations[Request->Kind])
        DispatcherResult = FspFileSystemSendResponseWithStatus(FileSystem,
            Request, STATUS_INVALID_DEVICE_REQUEST);
    else
    {
        FspFileSystemEnterOperation(FileSystem, Request);
        DispatcherResult = FileSystem->Operations[Request->Kind](FileSystem, Request);
        FspFileSystemLeaveOperation(FileSystem, Request);
    }

    FspFileSystemSetDispatcherResult(FileSystem, DispatcherResult);
}

static DWORD WINAPI FspFileSystemPoolDispatcherWorker(PVOID Param)
{
    NTSTATUS DispatcherResult;
    FSP_DISPATCHER_WORK_ITEM *WorkItem = Param;
    FSP_FILE_SYSTEM *FileSystem = WorkItem->FileSystem;
    FSP_FSCTL_TRANSACT_REQ *Request = (PVOID)WorkItem->RequestBuf;

    FspFileSystemEnterOperation(FileSystem, Request);
    DispatcherResult = FileSystem->Operations[Request->Kind](FileSystem, Request);
    FspFileSystemLeaveOperation(FileSystem, Request);
    FspFileSystemSetDispatcherResult(FileSystem, DispatcherResult);

    MemFree(WorkItem);

    return 0;
}

FSP_API VOID FspFileSystemPoolDispatcher(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request)
{
    NTSTATUS DispatcherResult;

    if (FspFsctlTransactKindCount <= Request->Kind || 0 == FileSystem->Operations[Request->Kind])
        DispatcherResult = FspFileSystemSendResponseWithStatus(FileSystem,
            Request, STATUS_INVALID_DEVICE_REQUEST);
    else
    {
        FSP_DISPATCHER_WORK_ITEM *WorkItem = MemAlloc(sizeof *WorkItem + Request->Size);
        if (0 == WorkItem)
            DispatcherResult = STATUS_INSUFFICIENT_RESOURCES;
        else
        {
            WorkItem->FileSystem = FileSystem;
            memcpy(WorkItem->RequestBuf, Request, Request->Size);

            if (QueueUserWorkItem(FspFileSystemPoolDispatcherWorker, WorkItem, WT_EXECUTEDEFAULT))
                DispatcherResult = STATUS_SUCCESS;
            else
            {
                DispatcherResult = FspNtStatusFromWin32(GetLastError());
                MemFree(WorkItem);
            }
        }
    }

    FspFileSystemSetDispatcherResult(FileSystem, DispatcherResult);
}

FSP_API NTSTATUS FspFileSystemSendResponse(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_RSP *Response)
{
    return FspFsctlTransact(FileSystem->VolumeHandle, Response, Response->Size, 0, 0);
}

FSP_API NTSTATUS FspFileSystemSendResponseWithStatus(FSP_FILE_SYSTEM *FileSystem,
    FSP_FSCTL_TRANSACT_REQ *Request, NTSTATUS Result)
{
    FSP_FSCTL_TRANSACT_RSP Response;
    memset(&Response, 0, sizeof Response);
    Response.Size = sizeof Response;
    Response.Kind = Request->Kind;
    Response.Hint = Request->Hint;
    Response.IoStatus.Status = Result;
    return FspFileSystemSendResponse(FileSystem, &Response);
}
