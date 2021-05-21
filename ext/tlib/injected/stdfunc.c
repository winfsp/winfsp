/**
 * @file tlib/injected/stdfunc.c
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#include <tlib/injected/stdfunc.h>
#define TLIB_INJECTIONS_ENABLED
#include <tlib/injection.h>

#undef calloc
#undef malloc
#undef realloc

void *tlib_calloc(size_t count, size_t size)
{
    TLIB_INJECT("calloc", return 0);
    return calloc(count, size);
}
void *tlib_malloc(size_t size)
{
    TLIB_INJECT("malloc", return 0);
    return malloc(size);
}
void *tlib_realloc(void *ptr, size_t size)
{
    TLIB_INJECT("realloc", return 0);
    return realloc(ptr, size);
}
