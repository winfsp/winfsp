/**
 * @file sys/misc.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS CreateGuid(GUID *Guid);
NTSTATUS SecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CreateGuid)
#pragma alloc_text(PAGE, SecuritySubjectContextAccessCheck)
#endif

NTSTATUS FspCreateGuid(GUID *Guid)
{
    PAGED_CODE();

    NTSTATUS Result;

    int Retries = 3;
    do
    {
        Result = ExUuidCreate(Guid);
    } while (!NT_SUCCESS(Result) && 0 < --Retries);

    return Result;
}

NTSTATUS FspSecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_ACCESS_DENIED;
    SECURITY_SUBJECT_CONTEXT SecuritySubjectContext;
    ACCESS_MASK GrantedAccess;

    SeCaptureSubjectContext(&SecuritySubjectContext);
    if (SeAccessCheck(SecurityDescriptor,
        &SecuritySubjectContext, FALSE,
        DesiredAccess, 0, 0, IoGetFileObjectGenericMapping(), AccessMode,
        &GrantedAccess, &Result))
        Result = STATUS_SUCCESS;
    SeReleaseSubjectContext(&SecuritySubjectContext);

    return Result;
}
