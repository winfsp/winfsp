/**
 * @file tlib/callstack.h
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#ifndef TLIB_CALLSTACK_H_INCLUDED
#define TLIB_CALLSTACK_H_INCLUDED

#include <stddef.h>

enum
{
    TLIB_MAX_SYMLEN = 63,
    TLIB_MAX_SYMRET = 8,
    TLIB_MAX_SYMCAP = 62, /* max number of frames to capture (Windows restriction) */
};

struct tlib_callstack_s
{
    const char *syms[TLIB_MAX_SYMRET + 1];
    char symbuf[TLIB_MAX_SYMRET][TLIB_MAX_SYMLEN + 1];
};
void tlib_callstack(size_t skip, size_t count, struct tlib_callstack_s *stack);

#endif
