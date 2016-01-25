/**
 * @file dll/debug.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <dll/library.h>
#include <sddl.h>
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

FSP_API VOID FspDebugLogSD(const char *format, PSECURITY_DESCRIPTOR SecurityDescriptor)
{
    char *Sddl;

    if (0 == SecurityDescriptor)
        FspDebugLog(format, "null security descriptor");
    else if (ConvertSecurityDescriptorToStringSecurityDescriptorA(SecurityDescriptor, SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
        DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
        &Sddl, 0))
    {
        FspDebugLog(format, Sddl);
        LocalFree(Sddl);
    }
    else
        FspDebugLog(format, "invalid security descriptor");
}

FSP_API VOID FspDebugLogFT(const char *format, PFILETIME FileTime)
{
    SYSTEMTIME SystemTime;
    char buf[32];

    if (FileTimeToSystemTime(FileTime, &SystemTime))
    {
        wsprintfA(buf, "%04hu-%02hu-%02huT%02hu:%02hu:%02hu.%03huZ",
            SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
            SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
            SystemTime.wMilliseconds);
        FspDebugLog(format, buf);
    }
    else
        FspDebugLog(format, "invalid file time");
}
