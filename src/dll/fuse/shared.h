/**
 * @file dll/fuse/shared.h
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

#ifndef WINFSP_DLL_FUSE_SHARED_H_INCLUDED
#define WINFSP_DLL_FUSE_SHARED_H_INCLUDED

#define enosys(env)                     ('C' == (env)->environment ? 88 : 40)

#endif
