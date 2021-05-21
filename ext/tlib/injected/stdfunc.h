/**
 * @file tlib/injected/stdfunc.h
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#ifndef TLIB_INJECTED_STDFUNC_H_INCLUDED
#define TLIB_INJECTED_STDFUNC_H_INCLUDED

#include <stdlib.h>
#include <errno.h>

#if defined(TLIB_INJECTIONS_ENABLED)
#define calloc(count, size)             tlib_calloc(count, size)
#define malloc(size)                    tlib_malloc(size)
#define realloc(ptr, size)              tlib_realloc(ptr, size)
#endif

void *tlib_calloc(size_t count, size_t size);
void *tlib_malloc(size_t size);
void *tlib_realloc(void *ptr, size_t size);

#endif
