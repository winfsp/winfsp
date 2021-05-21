/**
 * @file dll/fuse/fuse_compat.c
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

#include <dll/library.h>

/*
 * This file provides an implementation of the `fuse_*` symbols. This
 * implementation is a simple shim that forwards `fuse_*` calls to the
 * equivalent `fsp_fuse_*` ones using a default `fsp_fuse_env`.
 *
 * These symbols should *not* be used by C/C++ programs. For this reason
 * the `fuse.h` headers only expose the `fsp_fuse_*` symbols, wrapped
 * with macros. These symbols are for use only from programs using FFI
 * technology to access FUSE symbols (e.g. fusepy, jnr-fuse).
 */

#define FSP_FUSE_API
#define FSP_FUSE_SYM(proto, ...)        __declspec(dllexport) proto { __VA_ARGS__ }
#include <fuse/fuse_common.h>
#include <fuse/fuse.h>
#include <fuse/fuse_opt.h>
