/**
 * @file dll/fuse/fuse_main.c
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

#include <dll/fuse/library.h>

#define FSP_FUSE_MAIN_OPT(n, f, v)      { n, offsetof(struct fsp_fuse_main_opt_data, f), v }

struct fsp_fuse_main_opt_data
{
    struct fsp_fuse_env *env;
    char *mountpoint;
    int singlethread;
    int foreground;
};

static struct fuse_opt fsp_fuse_main_opts[] =
{
    FUSE_OPT_KEY("-h", 'h'),
    FUSE_OPT_KEY("--help", 'h'),
    FUSE_OPT_KEY("-ho", 'H'),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("debug", FUSE_OPT_KEY_KEEP),
    FSP_FUSE_MAIN_OPT("-d", foreground, 1),
    FSP_FUSE_MAIN_OPT("debug", foreground, 1),

    FSP_FUSE_MAIN_OPT("-f", foreground, 1),
    FSP_FUSE_MAIN_OPT("-s", singlethread, 1),

    FUSE_OPT_END,
};

static int fsp_fuse_main_opt_proc(void *opt_data0, const char *arg, int key,
    struct fuse_args *outargs)
{
    static PWSTR HeaderHelp = L""
        "\n"
        "    -o opt,[opt...]        mount options\n"
        "    -h   --help            print help\n"
        "    -V   --version         print version\n";
    static PWSTR MainHelp = L""
        "FUSE options:\n"
        "    -d   -o debug          enable debug output (implies -f)\n"
        "    -f                     foreground operation\n"
        "    -s                     disable multi-threaded operation\n";
    struct fsp_fuse_main_opt_data *opt_data = opt_data0;

    switch (key)
    {
    default:
        return 1;
    case 'h':
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            "usage: %s mountpoint [options]\n"
            "%s"
            "\n"
            "%s",
            FspDiagIdent(), HeaderHelp, MainHelp);
        return 1;
    case 'H':
        FspServiceLog(EVENTLOG_ERROR_TYPE, L""
            "%s",
            MainHelp);
        fsp_fuse_opt_add_arg(opt_data->env, outargs, "-h");
        return 0;
    case FUSE_OPT_KEY_NONOPT:
        if (0 == opt_data->mountpoint)
        {
            size_t size = lstrlenA(arg) + 1;
            opt_data->mountpoint = opt_data->env->memalloc(size);
            if (0 == opt_data->mountpoint)
                return -1;
            memcpy(opt_data->mountpoint, arg, size);
        }
        else
            FspServiceLog(EVENTLOG_ERROR_TYPE,
                L"Invalid argument \"%S\"", arg);
        return 1;
    }
}

FSP_FUSE_API int fsp_fuse_parse_cmdline(struct fsp_fuse_env *env,
    struct fuse_args *args,
    char **mountpoint, int *multithreaded, int *foreground)
{
    struct fsp_fuse_main_opt_data opt_data;

    memset(&opt_data, 0, sizeof opt_data);
    opt_data.env = env;

    if (-1 == fsp_fuse_opt_parse(env, args, &opt_data, fsp_fuse_main_opts, fsp_fuse_main_opt_proc))
        return -1;

    if (0 != mountpoint)
        *mountpoint = opt_data.mountpoint;
    else
        env->memfree(mountpoint);

    if (0 != multithreaded)
        *multithreaded = !opt_data.singlethread;

    if (0 != foreground)
        *foreground = opt_data.foreground;

    return 0;
}

FSP_FUSE_API int fsp_fuse_main_real(struct fsp_fuse_env *env,
    int argc, char *argv[],
    const struct fuse_operations *ops, size_t opsize, void *data)
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    char *mountpoint = 0;
    int multithreaded = 0;
    int foreground = 0;
    struct fuse_chan *ch = 0;
    struct fuse *f = 0;
    int signal_handlers = 0;
    int result;

    result = fsp_fuse_parse_cmdline(env, &args, &mountpoint, &multithreaded, &foreground);
    if (-1 == result)
        goto exit;

    ch = fsp_fuse_mount(env, mountpoint, &args);
    if (0 == ch)
    {
        result = -1;
        goto exit;
    }

    f = fsp_fuse_new(env, ch, &args, ops, opsize, data);
    if (0 == f)
    {
        result = -1;
        goto exit;
    }

    result = env->daemonize(foreground);
    if (-1 == result)
        goto exit;

    result = env->set_signal_handlers(f);
    if (-1 == result)
        goto exit;
    signal_handlers = 1;

    result = multithreaded ? fsp_fuse_loop_mt(env, f) : fsp_fuse_loop(env, f);

exit:
    if (signal_handlers)
        env->set_signal_handlers(0);

    if (0 != f)
        fsp_fuse_destroy(env, f);

    if (0 != ch)
        fsp_fuse_unmount(env, mountpoint, ch);

    env->memfree(mountpoint);

    fsp_fuse_opt_free_args(env, &args);

    /* main() style return: 0 success, 1 error */
    return !!result;
}
