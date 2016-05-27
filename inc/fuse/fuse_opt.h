/**
 * @file fuse/fuse_opt.h
 * WinFsp FUSE compatible API.
 *
 * This file is derived from libfuse/include/fuse_opt.h:
 *     FUSE: Filesystem in Userspace
 *     Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
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

#ifndef FUSE_OPT_H_
#define FUSE_OPT_H_

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(FSP_FUSE_API)
#if defined(WINFSP_DLL_INTERNAL)
#define FSP_FUSE_API                    __declspec(dllexport)
#else
#define FSP_FUSE_API                    __declspec(dllimport)
#endif
#endif

#define FSP_FUSE_MEMFN_P                void *(*memalloc)(size_t), void (*memfree)(void *)
#define FSP_FUSE_MEMFN_A                memalloc, memfree
#if defined(WINFSP_DLL_INTERNAL)
#define FSP_FUSE_MEMFN_V                MemAlloc, MemFree
#else
#define FSP_FUSE_MEMFN_V                malloc, free
#endif

#define FUSE_OPT_KEY(templ, key)        { templ, -1U, key }
#define FUSE_OPT_END                    { NULL, 0, 0 }

#define FUSE_OPT_KEY_OPT                -1
#define FUSE_OPT_KEY_NONOPT             -2
#define FUSE_OPT_KEY_KEEP               -3
#define FUSE_OPT_KEY_DISCARD            -4

#define FUSE_ARGS_INIT(argc, argv)      { argc, argv, 0 }

struct fuse_opt
{
	const char *templ;
	unsigned long offset;
	int value;
};

struct fuse_args
{
	int argc;
	char **argv;
	int allocated;
};

typedef int (*fuse_opt_proc_t)(void *data, const char *arg, int key,
    struct fuse_args *outargs);

FSP_FUSE_API int fsp_fuse_opt_parse(struct fuse_args *args, void *data,
    const struct fuse_opt opts[], fuse_opt_proc_t proc,
    FSP_FUSE_MEMFN_P);
FSP_FUSE_API int fsp_fuse_opt_add_arg(struct fuse_args *args, const char *arg,
    FSP_FUSE_MEMFN_P);
FSP_FUSE_API int fsp_fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg,
    FSP_FUSE_MEMFN_P);
FSP_FUSE_API void fsp_fuse_opt_free_args(struct fuse_args *args,
    FSP_FUSE_MEMFN_P);
FSP_FUSE_API int fsp_fuse_opt_add_opt(char **opts, const char *opt,
    FSP_FUSE_MEMFN_P);
FSP_FUSE_API int fsp_fuse_opt_add_opt_escaped(char **opts, const char *opt,
    FSP_FUSE_MEMFN_P);
FSP_FUSE_API int fsp_fuse_opt_match(const struct fuse_opt opts[], const char *opt,
    FSP_FUSE_MEMFN_P);

static inline int fuse_opt_parse(struct fuse_args *args, void *data,
    const struct fuse_opt opts[], fuse_opt_proc_t proc)
{
    return fsp_fuse_opt_parse(args, data, opts, proc,
        FSP_FUSE_MEMFN_V);
}

static inline int fuse_opt_add_arg(struct fuse_args *args, const char *arg)
{
    return fsp_fuse_opt_add_arg(args, arg,
        FSP_FUSE_MEMFN_V);
}

static inline int fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg)
{
    return fsp_fuse_opt_insert_arg(args, pos, arg,
        FSP_FUSE_MEMFN_V);
}

static inline void fuse_opt_free_args(struct fuse_args *args)
{
    fsp_fuse_opt_free_args(args,
        FSP_FUSE_MEMFN_V);
}

static inline int fuse_opt_add_opt(char **opts, const char *opt)
{
    return fsp_fuse_opt_add_opt(opts, opt,
        FSP_FUSE_MEMFN_V);
}

static inline int fuse_opt_add_opt_escaped(char **opts, const char *opt)
{
    return fsp_fuse_opt_add_opt_escaped(opts, opt,
        FSP_FUSE_MEMFN_V);
}

static inline int fuse_opt_match(const struct fuse_opt opts[], const char *opt)
{
    return fsp_fuse_opt_match(opts, opt,
        FSP_FUSE_MEMFN_V);
}

#ifdef __cplusplus
}
#endif

#endif
