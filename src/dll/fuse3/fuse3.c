/**
 * @file dll/fuse3/fuse3.c
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

#include <dll/fuse3/library.h>

FSP_FUSE_API int fsp_fuse3_main_real(struct fsp_fuse_env *env,
    int argc, char *argv[],
    const struct fuse3_operations *ops, size_t opsize, void *data)
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    char *mountpoint = 0;
    int multithreaded = 0;
    int foreground = 0;
    struct fuse3 *f3 = 0;
    int mounted = 0;
    int signal_handlers = 0;
    int result;

    result = fsp_fuse_parse_cmdline(env, &args, &mountpoint, &multithreaded, &foreground);
    if (-1 == result)
        goto exit;

    f3 = fsp_fuse3_new_30(env, &args, ops, opsize, data);
    if (0 == f3)
    {
        result = -1;
        goto exit;
    }

    result = fsp_fuse3_mount(env, f3, mountpoint);
    if (-1 == result)
        goto exit;
    mounted = 1;

    result = env->daemonize(foreground);
    if (-1 == result)
        goto exit;

    result = env->set_signal_handlers(f3);
    if (-1 == result)
        goto exit;
    signal_handlers = 1;

    result = multithreaded ? fsp_fuse3_loop_mt(env, f3, 0) : fsp_fuse3_loop(env, f3);

exit:
    if (signal_handlers)
        env->set_signal_handlers(0);

    if (mounted)
        fsp_fuse3_unmount(env, f3);

    if (0 != f3)
        fsp_fuse3_destroy(env, f3);

    env->memfree(mountpoint);

    fsp_fuse_opt_free_args(env, &args);

    /* main() style return: 0 success, 1 error */
    return !!result;
}

FSP_FUSE_API void fsp_fuse3_lib_help(struct fsp_fuse_env *env,
    struct fuse_args *args)
{
    char *helpargv[] =
    {
        "UNKNOWN",
        "-h",
        0
    };
    struct fuse_args helpargs = FUSE_ARGS_INIT(2, helpargv);
    struct fsp_fuse_core_opt_data opt_data;

    memset(&opt_data, 0, sizeof opt_data);
    fsp_fuse_core_opt_parse(env, &helpargs, &opt_data, /*help=*/1);
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

FSP_FUSE_API struct fuse3_conn_info_opts *fsp_fuse3_parse_conn_info_opts(
    struct fsp_fuse_env *env,
    struct fuse_args *args)
{
    static int dummy;
    return (struct fuse3_conn_info_opts *)&dummy;
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
#define STR(x)                          STR_(x)
#define STR_(x)                         #x
    return STR(FUSE_MAJOR_VERSION) "." STR(FUSE_MINOR_VERSION);
#undef STR_
#undef STR
}
