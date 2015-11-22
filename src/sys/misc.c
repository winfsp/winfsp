/**
 * @file sys/misc.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

NTSTATUS CreateGuid(GUID *Guid);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CreateGuid)
#endif

NTSTATUS CreateGuid(GUID *Guid)
{
    NTSTATUS Result;
    int Retries = 3;
    do
    {
        Result = ExUuidCreate(Guid);
    } while (!NT_SUCCESS(Result) && 0 < --Retries);
    return Result;
}
