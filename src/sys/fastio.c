/**
 * @file sys/fastio.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;
FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFastIoCheckIfPossible)
#pragma alloc_text(PAGE, FspAcquireFileForNtCreateSection)
#pragma alloc_text(PAGE, FspReleaseFileForNtCreateSection)
#pragma alloc_text(PAGE, FspAcquireForModWrite)
#pragma alloc_text(PAGE, FspReleaseForModWrite)
#pragma alloc_text(PAGE, FspAcquireForCcFlush)
#pragma alloc_text(PAGE, FspReleaseForCcFlush)
#endif

BOOLEAN FspFastIoCheckIfPossible(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER FileOffset,
    ULONG Length,
    BOOLEAN Wait,
    ULONG LockKey,
    BOOLEAN CheckForReadOperation,
    PIO_STATUS_BLOCK IoStatus,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER_BOOL(PAGED_CODE());

    Result = FALSE;

    FSP_LEAVE_BOOL("%s", "");
}

VOID FspAcquireFileForNtCreateSection(
    PFILE_OBJECT FileObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FSP_LEAVE_VOID("%s", "");
}

VOID FspReleaseFileForNtCreateSection(
    PFILE_OBJECT FileObject)
{
    FSP_ENTER_VOID(PAGED_CODE());

    FSP_LEAVE_VOID("%s", "");
}

NTSTATUS FspAcquireForModWrite(
    PFILE_OBJECT FileObject,
    PLARGE_INTEGER EndingOffset,
    PERESOURCE *ResourceToRelease,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    Result = STATUS_NOT_IMPLEMENTED;

    FSP_LEAVE("%s", "");
}

NTSTATUS FspReleaseForModWrite(
    PFILE_OBJECT FileObject,
    PERESOURCE ResourceToRelease,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    Result = STATUS_NOT_IMPLEMENTED;

    FSP_LEAVE("%s", "");
}

NTSTATUS FspAcquireForCcFlush(
    PFILE_OBJECT FileObject,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    Result = STATUS_NOT_IMPLEMENTED;

    FSP_LEAVE("%s", "");
}

NTSTATUS FspReleaseForCcFlush(
    PFILE_OBJECT FileObject,
    PDEVICE_OBJECT DeviceObject)
{
    FSP_ENTER(PAGED_CODE());

    Result = STATUS_NOT_IMPLEMENTED;

    FSP_LEAVE("%s", "");
}
