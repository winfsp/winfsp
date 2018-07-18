/**
 * @file dll/fuse3/library.h
 *
 * @copyright 2015-2018 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#ifndef WINFSP_DLL_FUSE3_LIBRARY_H_INCLUDED
#define WINFSP_DLL_FUSE3_LIBRARY_H_INCLUDED

#include <dll/fuse/library.h>
#undef FUSE_H_
#undef FUSE_COMMON_H_
#undef FUSE_MAJOR_VERSION
#undef FUSE_MINOR_VERSION
#undef fuse_main
#include <fuse3/fuse.h>

struct fuse3
{
    struct fuse_args args;
    struct fuse3_operations ops;
    void *data;
    struct fuse *fuse;
};

#endif
