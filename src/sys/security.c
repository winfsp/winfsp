/**
 * @file sys/security.c
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

#include <sys/driver.h>

static NTSTATUS FspFsvolQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolQuerySecurityComplete;
static FSP_IOP_REQUEST_FINI FspFsvolQuerySecurityRequestFini;
static NTSTATUS FspFsvolSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolSetSecurityComplete;
static FSP_IOP_REQUEST_FINI FspFsvolSetSecurityRequestFini;
FSP_DRIVER_DISPATCH FspQuerySecurity;
FSP_DRIVER_DISPATCH FspSetSecurity;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQuerySecurity)
#pragma alloc_text(PAGE, FspFsvolQuerySecurityComplete)
#pragma alloc_text(PAGE, FspFsvolQuerySecurityRequestFini)
#pragma alloc_text(PAGE, FspFsvolSetSecurity)
#pragma alloc_text(PAGE, FspFsvolSetSecurityComplete)
#pragma alloc_text(PAGE, FspFsvolSetSecurityRequestFini)
#pragma alloc_text(PAGE, FspQuerySecurity)
#pragma alloc_text(PAGE, FspSetSecurity)
#endif

enum
{
    /* QuerySecurity */
    RequestFileNode                     = 0,
    RequestSecurityChangeNumber         = 1,

    /* SetSecurity */
    //RequestFileNode                   = 0,
};

static NTSTATUS FspFsvolQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    SECURITY_INFORMATION SecurityInformation = IrpSp->Parameters.QuerySecurity.SecurityInformation;
    PVOID Buffer = Irp->UserBuffer;
    ULONG Length = IrpSp->Parameters.QuerySecurity.Length;
    PVOID SecurityBuffer;
    FSP_FSCTL_TRANSACT_REQ *Request;

    ASSERT(FileNode == FileDesc->FileNode);

    FspFileNodeAcquireShared(FileNode, Main);
    if (FspFileNodeReferenceSecurity(FileNode, &SecurityBuffer, 0))
    {
        FspFileNodeRelease(FileNode, Main);

        Result = FspQuerySecurityDescriptorInfo(SecurityInformation, Buffer, &Length, SecurityBuffer);
        FspFileNodeDereferenceSecurity(SecurityBuffer);

        Irp->IoStatus.Information = Length;
        return Result;
    }

    FspFileNodeAcquireShared(FileNode, Pgio);

    Result = FspBufferUserBuffer(Irp, Length, IoWriteAccess);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Result = FspIopCreateRequestEx(Irp, 0, 0, FspFsvolQuerySecurityRequestFini, &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactQuerySecurityKind;
    Request->Req.QuerySecurity.UserContext = FileNode->UserContext;
    Request->Req.QuerySecurity.UserContext2 = FileDesc->UserContext2;

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolQuerySecurityComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    SECURITY_INFORMATION SecurityInformation = IrpSp->Parameters.QuerySecurity.SecurityInformation;
    PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
    ULONG Length = IrpSp->Parameters.QuerySecurity.Length;
    PVOID SecurityBuffer = 0;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);
    BOOLEAN Success;

    if (0 != FspIopRequestContext(Request, RequestFileNode))
    {
        /* check that the security descriptor we got back is valid */
        if (Response->Buffer + Response->Rsp.QuerySecurity.SecurityDescriptor.Size >
                (PUINT8)Response + Response->Size ||
            !RtlValidRelativeSecurityDescriptor((PVOID)Response->Buffer,
                Response->Rsp.QuerySecurity.SecurityDescriptor.Size, 0))
        {
            Irp->IoStatus.Information = 0;
            Result = STATUS_INVALID_SECURITY_DESCR;
            FSP_RETURN();
        }

        FspIopRequestContext(Request, RequestSecurityChangeNumber) = (PVOID)
            FspFileNodeSecurityChangeNumber(FileNode);
        FspIopRequestContext(Request, RequestFileNode) = 0;

        FspFileNodeReleaseOwner(FileNode, Full, Request);
    }

    Success = DEBUGTEST(90) && FspFileNodeTryAcquireExclusive(FileNode, Main);
    if (!Success)
    {
        FspIopRetryCompleteIrp(Irp, Response, &Result);
        FSP_RETURN();
    }

    Success = !FspFileNodeTrySetSecurity(FileNode,
        Response->Buffer, Response->Rsp.QuerySecurity.SecurityDescriptor.Size,
        (ULONG)(UINT_PTR)FspIopRequestContext(Request, RequestSecurityChangeNumber));
    Success = Success && FspFileNodeReferenceSecurity(FileNode, &SecurityBuffer, 0);
    FspFileNodeRelease(FileNode, Main);
    if (Success)
    {
        Result = FspQuerySecurityDescriptorInfo(SecurityInformation, Buffer, &Length, SecurityBuffer);
        FspFileNodeDereferenceSecurity(SecurityBuffer);
    }
    else
    {
        SecurityBuffer = (PVOID)Response->Buffer;
        Result = FspQuerySecurityDescriptorInfo(SecurityInformation, Buffer, &Length, SecurityBuffer);
    }

    Irp->IoStatus.Information = Length;

    FSP_LEAVE_IOC("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.QuerySecurity.SecurityInformation);
}

static VOID FspFsvolQuerySecurityRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

static NTSTATUS FspFsvolSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    /* is this a valid FileObject? */
    if (!FspFileNodeIsValid(IrpSp->FileObject->FsContext))
        return STATUS_INVALID_DEVICE_REQUEST;

    NTSTATUS Result;
    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    SECURITY_INFORMATION SecurityInformation = IrpSp->Parameters.SetSecurity.SecurityInformation;
    PSECURITY_DESCRIPTOR SecurityDescriptor = IrpSp->Parameters.SetSecurity.SecurityDescriptor;
    ULONG SecurityDescriptorSize = 0;

    ASSERT(FileNode == FileDesc->FileNode);

#if 0
    /* captured security descriptor is always valid */
    if (0 == SecurityDescriptor || !RtlValidSecurityDescriptor(SecurityDescriptor))
        return STATUS_INVALID_PARAMETER;
#endif
    SecurityDescriptorSize = RtlLengthSecurityDescriptor(SecurityDescriptor);

    FspFileNodeAcquireExclusive(FileNode, Full);

    FSP_FSCTL_TRANSACT_REQ *Request;

    Result = FspIopCreateRequestEx(Irp, 0, SecurityDescriptorSize, FspFsvolSetSecurityRequestFini,
        &Request);
    if (!NT_SUCCESS(Result))
    {
        FspFileNodeRelease(FileNode, Full);
        return Result;
    }

    Request->Kind = FspFsctlTransactSetSecurityKind;
    Request->Req.SetSecurity.UserContext = FileNode->UserContext;
    Request->Req.SetSecurity.UserContext2 = FileDesc->UserContext2;
    Request->Req.SetSecurity.SecurityInformation = SecurityInformation;
    Request->Req.SetSecurity.SecurityDescriptor.Offset = 0;
    Request->Req.SetSecurity.SecurityDescriptor.Size = (UINT16)SecurityDescriptorSize;
    RtlCopyMemory(Request->Buffer, SecurityDescriptor, SecurityDescriptorSize);

    FspFileNodeSetOwner(FileNode, Full, Request);
    FspIopRequestContext(Request, RequestFileNode) = FileNode;

    return FSP_STATUS_IOQ_POST;
}

NTSTATUS FspFsvolSetSecurityComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    if (!NT_SUCCESS(Response->IoStatus.Status))
    {
        Irp->IoStatus.Information = 0;
        Result = Response->IoStatus.Status;
        FSP_RETURN();
    }

    PFILE_OBJECT FileObject = IrpSp->FileObject;
    FSP_FILE_NODE *FileNode = FileObject->FsContext;
    FSP_FILE_DESC *FileDesc = FileObject->FsContext2;
    FSP_FSCTL_TRANSACT_REQ *Request = FspIrpRequest(Irp);

    ASSERT(FileNode == FileDesc->FileNode);

    /* if the security descriptor that we got back is valid */
    if (0 < Response->Rsp.SetSecurity.SecurityDescriptor.Size &&
        Response->Buffer + Response->Rsp.SetSecurity.SecurityDescriptor.Size <=
            (PUINT8)Response + Response->Size &&
        RtlValidRelativeSecurityDescriptor((PVOID)Response->Buffer,
            Response->Rsp.SetSecurity.SecurityDescriptor.Size, 0))
    {
        /* update the cached security */
        FspFileNodeSetSecurity(FileNode,
            Response->Buffer, Response->Rsp.SetSecurity.SecurityDescriptor.Size);
    }
    else
    {
        /* invalidate the cached security */
        FspFileNodeSetSecurity(FileNode, 0, 0);
    }

    FileDesc->DidSetSecurity = TRUE;
    FileDesc->DidSetMetadata = TRUE;

    FspFileNodeNotifyChange(FileNode, FILE_NOTIFY_CHANGE_SECURITY, FILE_ACTION_MODIFIED, FALSE);

    FspIopRequestContext(Request, RequestFileNode) = 0;
    FspFileNodeReleaseOwner(FileNode, Full, Request);

    Irp->IoStatus.Information = 0;
    Result = STATUS_SUCCESS;

    FSP_LEAVE_IOC("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.SetSecurity.SecurityInformation);
}

static VOID FspFsvolSetSecurityRequestFini(FSP_FSCTL_TRANSACT_REQ *Request, PVOID Context[4])
{
    PAGED_CODE();

    FSP_FILE_NODE *FileNode = Context[RequestFileNode];

    if (0 != FileNode)
        FspFileNodeReleaseOwner(FileNode, Full, Request);
}

NTSTATUS FspQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQuerySecurity(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.QuerySecurity.SecurityInformation);
}

NTSTATUS FspSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetSecurity(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p, SecurityInformation=%x",
        IrpSp->FileObject, IrpSp->Parameters.SetSecurity.SecurityInformation);
}
