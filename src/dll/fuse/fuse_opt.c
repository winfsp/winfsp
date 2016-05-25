/**
 * @file dll/fuse/fuse_opt.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/library.h>
#include <fuse/fuse_opt.h>

FSP_FUSE_API int fsp_fuse_opt_parse(struct fuse_args *args, void *data,
    const struct fuse_opt opts[], fuse_opt_proc_t proc)
{
    // !!!: NEEDIMPL
    return 0;
}

FSP_FUSE_API int fsp_fuse_opt_add_opt(char **opts, const char *opt)
{
    // !!!: NEEDIMPL
    return 0;
}

FSP_FUSE_API int fsp_fuse_opt_add_opt_escaped(char **opts, const char *opt)
{
    // !!!: NEEDIMPL
    return 0;
}

FSP_FUSE_API int fsp_fuse_opt_add_arg(struct fuse_args *args, const char *arg)
{
    // !!!: NEEDIMPL
    return 0;
}

FSP_FUSE_API int fsp_fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg)
{
    // !!!: NEEDIMPL
    return 0;
}

FSP_FUSE_API void fsp_fuse_opt_free_args(struct fuse_args *args)
{
    // !!!: NEEDIMPL
}

FSP_FUSE_API int fsp_fuse_opt_match(const struct fuse_opt opts[], const char *opt)
{
    // !!!: NEEDIMPL
    return 0;
}
