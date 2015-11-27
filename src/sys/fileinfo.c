/**
 * @file sys/fileinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
DRIVER_DISPATCH FspQueryInformation;
DRIVER_DISPATCH FspSetInformation;
FSP_IOCOMPLETION_DISPATCH FspQueryInformationComplete;
FSP_IOCOMPLETION_DISPATCH FspSetInformationComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryInformation)
#pragma alloc_text(PAGE, FspFsvolSetInformation)
#pragma alloc_text(PAGE, FspQueryInformation)
#pragma alloc_text(PAGE, FspSetInformation)
#pragma alloc_text(PAGE, FspQueryInformationComplete)
#pragma alloc_text(PAGE, FspSetInformationComplete)
#endif

static NTSTATUS FspFsvolQueryInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolSetInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspQueryInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_INFORMATION == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQueryInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}

NTSTATUS
FspSetInformation(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_SET_INFORMATION == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}

VOID FspQueryInformationComplete(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    FspCompleteRequest(Irp, STATUS_SUCCESS);
}

VOID FspSetInformationComplete(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    FspCompleteRequest(Irp, STATUS_SUCCESS);
}
