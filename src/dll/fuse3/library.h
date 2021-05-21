/**
 * @file dll/fuse3/library.h
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
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
