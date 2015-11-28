/**
 * @file sys/volinfo.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQueryVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolSetVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_DRIVER_DISPATCH FspQueryVolumeInformation;
FSP_DRIVER_DISPATCH FspSetVolumeInformation;
FSP_IOPROC_DISPATCH FspQueryVolumeInformationComplete;
FSP_IOPROC_DISPATCH FspSetVolumeInformationComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQueryVolumeInformation)
#pragma alloc_text(PAGE, FspFsvolSetVolumeInformation)
#pragma alloc_text(PAGE, FspQueryVolumeInformation)
#pragma alloc_text(PAGE, FspSetVolumeInformation)
#pragma alloc_text(PAGE, FspQueryVolumeInformationComplete)
#pragma alloc_text(PAGE, FspSetVolumeInformationComplete)
#endif

static NTSTATUS FspFsvolQueryVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolSetVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspQueryVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_VOLUME_INFORMATION == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQueryVolumeInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s", "");
}

NTSTATUS FspSetVolumeInformation(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_SET_VOLUME_INFORMATION == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetVolumeInformation(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("%s", "");
}

VOID FspQueryVolumeInformationComplete(
    PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s", "");
}

VOID FspSetVolumeInformationComplete(
    PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("%s", "");
}
