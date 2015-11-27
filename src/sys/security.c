/**
 * @file sys/security.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsvolQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
DRIVER_DISPATCH FspQuerySecurity;
DRIVER_DISPATCH FspSetSecurity;
FSP_IOCOMPLETION_DISPATCH FspQuerySecurityComplete;
FSP_IOCOMPLETION_DISPATCH FspSetSecurityComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsvolQuerySecurity)
#pragma alloc_text(PAGE, FspFsvolSetSecurity)
#pragma alloc_text(PAGE, FspQuerySecurity)
#pragma alloc_text(PAGE, FspSetSecurity)
#pragma alloc_text(PAGE, FspQuerySecurityComplete)
#pragma alloc_text(PAGE, FspSetSecurityComplete)
#endif

static NTSTATUS FspFsvolQuerySecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

static NTSTATUS FspFsvolSetSecurity(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
FspQuerySecurity(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_QUERY_SECURITY == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolQuerySecurity(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}

NTSTATUS
FspSetSecurity(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_SET_SECURITY == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolSetSecurity(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("", 0);
}

VOID FspQuerySecurityComplete(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    FspCompleteRequest(Irp, STATUS_SUCCESS);
}

VOID FspSetSecurityComplete(PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    PAGED_CODE();

    FspCompleteRequest(Irp, STATUS_SUCCESS);
}
