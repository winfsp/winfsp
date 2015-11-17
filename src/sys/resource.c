/**
 * @file sys/resource.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspAcquireFileForNtCreateSection)
#pragma alloc_text(PAGE, FspReleaseFileForNtCreateSection)
#pragma alloc_text(PAGE, FspAcquireForModWrite)
#pragma alloc_text(PAGE, FspReleaseForModWrite)
#pragma alloc_text(PAGE, FspAcquireForCcFlush)
#pragma alloc_text(PAGE, FspReleaseForCcFlush)
#endif

VOID
FspAcquireFileForNtCreateSection(
    _In_ PFILE_OBJECT FileObject)
{
    UNREFERENCED_PARAMETER(FileObject);

    PAGED_CODE();
}

VOID
FspReleaseFileForNtCreateSection(
    _In_ PFILE_OBJECT FileObject)
{
    UNREFERENCED_PARAMETER(FileObject);

    PAGED_CODE();
}

NTSTATUS
FspAcquireForModWrite(
    _In_ PFILE_OBJECT FileObject,
    _In_ PLARGE_INTEGER EndingOffset,
    _Out_ PERESOURCE *ResourceToRelease,
    _In_ PDEVICE_OBJECT DeviceObject)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(EndingOffset);
    UNREFERENCED_PARAMETER(ResourceToRelease);
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
FspReleaseForModWrite(
    _In_ PFILE_OBJECT FileObject,
    _In_ PERESOURCE ResourceToRelease,
    _In_ PDEVICE_OBJECT DeviceObject)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(ResourceToRelease);
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
FspAcquireForCcFlush(
    _In_ PFILE_OBJECT FileObject,
    _In_ PDEVICE_OBJECT DeviceObject)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
FspReleaseForCcFlush(
    _In_ PFILE_OBJECT FileObject,
    _In_ PDEVICE_OBJECT DeviceObject)
{
    UNREFERENCED_PARAMETER(FileObject);
    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    return STATUS_NOT_IMPLEMENTED;
}
