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

BOOLEAN
FspFastIoCheckIfPossible(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER FileOffset,
    _In_ ULONG Length,
    _In_ BOOLEAN Wait,
    _In_ ULONG LockKey,
    _In_ BOOLEAN CheckForReadOperation,
    _Out_ PIO_STATUS_BLOCK IoStatus,
    _In_ PDEVICE_OBJECT DeviceObject
    )
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(FileOffset);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Wait);
    UNREFERENCED_PARAMETER(LockKey);
    UNREFERENCED_PARAMETER(CheckForReadOperation);
    UNREFERENCED_PARAMETER(IoStatus);
    UNREFERENCED_PARAMETER(DeviceObject);

    return FALSE;
}
