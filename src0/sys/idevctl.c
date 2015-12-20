/**
 * @file sys/idevctl.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolInternalDeviceControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_IOCMPL_DISPATCH FspFsvolInternalDeviceControlComplete;
FSP_DRIVER_DISPATCH FspInternalDeviceControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolInternalDeviceControl)
#pragma alloc_text(PAGE, FspFsvolInternalDeviceControlComplete)
#pragma alloc_text(PAGE, FspInternalDeviceControl)
#endif

static NTSTATUS FspFsvolInternalDeviceControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_INVALID_DEVICE_REQUEST;
    switch (IrpSp->Parameters.DeviceIoControl.IoControlCode)
    {
    case FSP_FSCTL_WORK:
        {
            FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension = FspFsvolDeviceExtension(DeviceObject);
            PDEVICE_OBJECT FsvrtDeviceObject = FsvolDeviceExtension->FsvrtDeviceObject;

            if (!FspDeviceRetain(FsvrtDeviceObject))
                return STATUS_CANCELLED;
            try
            {
                FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =
                    FspFsvrtDeviceExtension(FsvrtDeviceObject);
                FSP_FSCTL_TRANSACT_REQ *Request = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;

                ASSERT(0 == Request->Hint);

                /* associate the passed Request with our Irp; acquire ownership of the Request */
                Request->Hint = (UINT_PTR)Irp;
                FspIrpRequest(Irp) = Request;

                /*
                 * Post the IRP to our Ioq; we do this here instead of at IRP_LEAVE_MJ time,
                 * so that we can disassociate the Request on failure and release ownership
                 * back to the caller.
                 */
                if (!FspIoqPostIrp(&FsvrtDeviceExtension->Ioq, Irp))
                {
                    /* this can only happen if the Ioq was stopped */
                    ASSERT(FspIoqStopped(&FsvrtDeviceExtension->Ioq));

                    Request->Hint = 0;
                    FspIrpRequest(Irp) = 0;

                    Result = STATUS_CANCELLED;
                    goto exit;
                }

                Result = STATUS_PENDING;

            exit:;
            }
            finally
            {
                FspDeviceRelease(FsvrtDeviceObject);
            }
        }
        break;
    }
    return Result;
}

VOID FspFsvolInternalDeviceControlComplete(
    PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s", IoctlCodeSym(IrpSp->Parameters.DeviceIoControl.IoControlCode));
}

NTSTATUS FspInternalDeviceControl(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_INTERNAL_DEVICE_CONTROL == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolInternalDeviceControl(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s", IoctlCodeSym(IrpSp->Parameters.DeviceIoControl.IoControlCode));
}
