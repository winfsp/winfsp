/**
 * @file tlib/callstack.c
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#include <tlib/callstack.h>
#include <stddef.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#elif defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#include <execinfo.h>
#else
#error tlib/callstack.c not implemented for this platform
#endif

void tlib_callstack(size_t skip, size_t count, struct tlib_callstack_s *stack)
{
    if (count > TLIB_MAX_SYMRET)
        count = TLIB_MAX_SYMRET;
    size_t naddrs = skip + count;
    if (naddrs > TLIB_MAX_SYMCAP)
        naddrs = TLIB_MAX_SYMCAP;
    memset((void *)stack->syms, 0, sizeof stack->syms);
#if defined(_WIN32) || defined(_WIN64)
    /* SymInitialize()/SymFromAddr() are not thread-safe. Furthermore SymInitialize() should
     * not be called more than once per process.
     */
    HANDLE hproc = GetCurrentProcess();
    static SYMBOL_INFO *syminfo = 0;
    void *addrs[TLIB_MAX_SYMCAP];
    size_t i = 0;
    if (0 == syminfo)
    {
        syminfo = (SYMBOL_INFO *)malloc(sizeof(SYMBOL_INFO) + TLIB_MAX_SYMLEN);
        if (0 == syminfo)
            return;
         SymInitialize(hproc, 0, TRUE);
    }
    memset(syminfo, 0, sizeof(SYMBOL_INFO));
    syminfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    syminfo->MaxNameLen = TLIB_MAX_SYMLEN;
    naddrs = CaptureStackBackTrace(0, naddrs, addrs, 0);
    for (void **p = addrs + skip, **endp = addrs + naddrs; endp > p; p++)
    {
        DWORD64 displacement = 0;
        if (SymFromAddr(hproc, (DWORD64)*p, &displacement, syminfo))
        {
            stack->syms[i] = stack->symbuf[i];
            strncpy(stack->symbuf[i], syminfo->Name, TLIB_MAX_SYMLEN);
            stack->symbuf[i][TLIB_MAX_SYMLEN] = '\0';
            i++;
        }
    }
#elif defined(__APPLE__) || defined(__linux__)
    void *addrs[TLIB_MAX_SYMCAP];
    naddrs = backtrace(addrs, naddrs);
    size_t i = 0;
    Dl_info syminfo;
    for (void **p = addrs + skip, **endp = addrs + naddrs; endp > p; p++)
    {
        if (dladdr(*p, &syminfo) && 0 != syminfo.dli_sname)
        {
            stack->syms[i] = stack->symbuf[i];
            strncpy(stack->symbuf[i], syminfo.dli_sname, TLIB_MAX_SYMLEN);
            stack->symbuf[i][TLIB_MAX_SYMLEN] = '\0';
            i++;
        }
    }
#endif
}
