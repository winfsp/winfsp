/**
 * @file dll/debug.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>
#include <stdarg.h>

FSP_API VOID FspDebugLog(const char *format, ...)
{
    char buf[1024];
        /* DbgPrint has a 512 byte limit, but wvsprintf is only safe with a 1024 byte buffer */
    va_list ap;
    va_start(ap, format);
    wvsprintfA(buf, format, ap);
    va_end(ap);
    buf[sizeof buf - 1] = '\0';
    OutputDebugStringA(buf);
}
