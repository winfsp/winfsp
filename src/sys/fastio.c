/**
 * @file sys/fastio.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFastIoCheckIfPossible)
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
