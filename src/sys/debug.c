/**
 * @file sys/debug.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

#if DBG
const char *NtStatusSym(NTSTATUS Status)
{
    switch (Status)
    {
#define SYM(x)                          case x: return #x;
/* cygwin: sed -n '/_WAIT_0/!s/^#define[ \t]*\(STATUS_[^ \t]*\).*NTSTATUS.*$/SYM(\1)/p' ntstatus.h >ntstatus.i */
#include "ntstatus.i"
#undef SYM
    default:
        return "UNKNOWN";
    }
}
#endif
