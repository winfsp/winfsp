/**
 * @file dll/fuse/fuse.c
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
#include <fuse/fuse.h>

#define STR(x)                          STR_(x)
#define STR_(x)                         #x

FSP_FUSE_API int fsp_fuse_version(void)
{
    return FUSE_VERSION;
}

FSP_FUSE_API const char *fsp_fuse_pkgversion(void)
{
    return STR(FUSE_VERSION);
}

FSP_FUSE_API struct fuse_chan *fsp_fuse_mount(const char *mountpoint, struct fuse_args *args)
{
    return 0;
}

FSP_FUSE_API void fsp_fuse_unmount(const char *mountpoint, struct fuse_chan *ch)
{
}

FSP_FUSE_API int fsp_fuse_parse_cmdline(struct fuse_args *args, char **mountpoint,
    int *multithreaded, int *foreground)
{
    return 0;
}

FSP_FUSE_API int fsp_fuse_main_real(int argc, char *argv[],
    const struct fuse_operations *ops, size_t opsize, void *data,
    int environment)
{
    return 0;
}

FSP_FUSE_API struct fuse *fsp_fuse_new(struct fuse_chan *ch, struct fuse_args *args,
    const struct fuse_operations *ops, size_t opsize, void *data,
    int environment)
{
    return 0;
}

FSP_FUSE_API void fsp_fuse_destroy(struct fuse *f)
{
}

FSP_FUSE_API int fsp_fuse_loop(struct fuse *f)
{
    return 0;
}

FSP_FUSE_API int fsp_fuse_loop_mt(struct fuse *f)
{
    return 0;
}

FSP_FUSE_API void fsp_fuse_exit(struct fuse *f)
{
}

FSP_FUSE_API struct fuse_context *fsp_fuse_get_context(void)
{
    return 0;
}
