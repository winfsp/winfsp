/**
 * @file sys/cleanup.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolCleanupComplete;
FSP_DRIVER_DISPATCH FspCleanup;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCleanup)
#pragma alloc_text(PAGE, FspFsvrtCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanupComplete)
#pragma alloc_text(PAGE, FspCleanup)
#endif

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result;
    FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
    PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;

    if (!FspDeviceRetain(FsvrtDeviceObject))
        return STATUS_CANCELLED;
    try
    {
        FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =
            FspFsvrtDeviceExtension(FsvrtDeviceObject);
        PFILE_OBJECT FileObject = IrpSp->FileObject;
        FSP_FILE_CONTEXT *FsContext = FileObject->FsContext;
        UINT64 UserContext = FsContext->UserContext;
        UINT64 UserContext2 = (UINT_PTR)FileObject->FsContext2;
        BOOLEAN FileNameRequired = 0 != FsvrtDeviceExtension->VolumeParams.FileNameRequired;
        BOOLEAN DeletePending;
        LONG OpenCount;
        FSP_FSCTL_TRANSACT_REQ *Request;

        /* lock the FsContext */
        ExAcquireResourceExclusiveLite(FsContext->Header.Resource, TRUE);

        /* propagate the FsContext DeleteOnClose to DeletePending */
        if (FsContext->DeleteOnClose)
            FsContext->DeletePending = TRUE;
        DeletePending = FsContext->DeletePending;

        /* all handles on this FileObject are gone; decrement its FsContext->OpenCount */
        OpenCount = FspFileContextClose(FsContext);

        /* unlock the FsContext */
        ExReleaseResourceLite(FsContext->Header.Resource);

        /* is the FsContext going away as well? */
        if (0 == OpenCount)
        {
            /*
             * The following must be done under the file system volume device Resource,
             * because we are manipulating its GenericTable.
             */
            ExAcquireResourceExclusiveLite(&FsvolDeviceExtension->Base.Resource, TRUE);
            try
            {
                /* remove the FsContext from the file system volume device generic table */
                FspFsvolDeviceDeleteContext(DeviceObject, FsContext->UserContext, 0);
            }
            finally
            {
                ExReleaseResourceLite(&FsvolDeviceExtension->Base.Resource);
            }
        }

        /* create the user-mode file system request */
        Result = FspIopCreateRequest(Irp, FileNameRequired ? &FsContext->FileName : 0, 0, &Request);
        if (!NT_SUCCESS(Result))
            goto leak_exit;

        /*
         * The new request is associated with our IRP and will be deleted during its completion.
         */

        /* populate the Cleanup request */
        Request->Kind = FspFsctlTransactCleanupKind;
        Request->Req.Cleanup.UserContext = UserContext;
        Request->Req.Cleanup.UserContext2 = UserContext2;
        Request->Req.Cleanup.Delete = DeletePending && 0 == OpenCount;

        Result = STATUS_PENDING;

        goto exit;

    leak_exit:;
#if DBG
        DEBUGLOG("FileObject=%p[%p:\"%wZ\"], UserContext=%llx, UserContext2=%llx: "
            "error: the user-mode file system handle will be leaked!",
            IrpSp->FileObject, IrpSp->FileObject->RelatedFileObject, IrpSp->FileObject->FileName,
            UserContext, UserContext2);
#endif

        /* IRP_MJ_CLEANUP cannot really fail :-\ */
        Result = STATUS_SUCCESS;

    exit:;
    }
    finally
    {
        FspDeviceRelease(FsvrtDeviceObject);
    }

    return Result;
}

VOID FspFsvolCleanupComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p", IrpSp->FileObject);
}

NTSTATUS FspCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CLEANUP == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCleanup(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCleanup(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCleanup(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}
