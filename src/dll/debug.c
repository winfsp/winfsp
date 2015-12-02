/**
 * @file dll/debug.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>
#include <stdarg.h>

FSP_API VOID FspDebugLog(const char *format, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, format);
    StringCbVPrintfA(buf, sizeof buf, format, ap);
    va_end(ap);
    OutputDebugStringA(buf);
}
