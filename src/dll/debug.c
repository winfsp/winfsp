/**
 * @file dll/debug.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>
#include <stdarg.h>

#if !defined(NDEBUG)
VOID FspDebugLog(const char *format, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, format);
    StringCbVPrintfA(buf, sizeof buf, format, ap);
    va_end(ap);
    OutputDebugStringA(buf);
}
#endif
