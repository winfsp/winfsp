/**
 * @file dll/fuse3/fuse3.c
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

#include <dll/fuse3/library.h>

FSP_FUSE_API int fsp_fuse3_main_real(struct fsp_fuse_env *env,
    int argc, char *argv[],
    const struct fuse3_operations *ops, size_t opsize, void *data)
{
    return 0;
}

FSP_FUSE_API void fsp_fuse3_lib_help(struct fsp_fuse_env *env,
    struct fuse_args *args)
{
}

FSP_FUSE_API int fsp_fuse3_loop(struct fsp_fuse_env *env,
    struct fuse3 *f3)
{
    return 0 == fsp_fuse_loop(env, f3->fuse) ? 0 : -EINVAL/* same on MSVC and Cygwin */;
}

FSP_FUSE_API int fsp_fuse3_loop_mt_31(struct fsp_fuse_env *env,
    struct fuse3 *f3, int clone_fd)
{
    return 0 == fsp_fuse_loop_mt(env, f3->fuse) ? 0 : -EINVAL/* same on MSVC and Cygwin */;
}

FSP_FUSE_API int fsp_fuse3_loop_mt(struct fsp_fuse_env *env,
    struct fuse3 *f3, struct fuse3_loop_config *config)
{
    return 0 == fsp_fuse_loop_mt(env, f3->fuse) ? 0 : -EINVAL/* same on MSVC and Cygwin */;
}

FSP_FUSE_API void fsp_fuse3_exit(struct fsp_fuse_env *env,
    struct fuse3 *f3)
{
    fsp_fuse_exit(env, f3->fuse);
}

FSP_FUSE_API struct fuse3_context *fsp_fuse3_get_context(struct fsp_fuse_env *env)
{
    FSP_FSCTL_STATIC_ASSERT(
        sizeof(struct fuse_context) == sizeof(struct fuse3_context),
        "incompatible structs fuse_context and fuse3_context");
    FSP_FSCTL_STATIC_ASSERT(FIELD_OFFSET(
        struct fuse_context, private_data) == FIELD_OFFSET(struct fuse3_context, private_data),
        "incompatible structs fuse_context and fuse3_context");
    return (struct fuse3_context *)fsp_fuse_get_context(env);
}

FSP_FUSE_API struct fuse3_conn_info_opts* fsp_fuse3_parse_conn_info_opts(
    struct fsp_fuse_env *env,
    struct fuse_args *args)
{
    return 0;
}

FSP_FUSE_API void fsp_fuse3_apply_conn_info_opts(struct fsp_fuse_env *env,
    struct fuse3_conn_info_opts *opts, struct fuse3_conn_info *conn)
{
}

FSP_FUSE_API int fsp_fuse3_version(struct fsp_fuse_env *env)
{
    return FUSE_VERSION;
}

FSP_FUSE_API const char *fsp_fuse3_pkgversion(struct fsp_fuse_env *env)
{
#define STR(x)                          #x
    return STR(FUSE_MAJOR_VERSION) "." STR(FUSE_MINOR_VERSION);
#undef STR
}
