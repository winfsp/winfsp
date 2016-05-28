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
#include <stdint.h>

#define fsp_fuse_opt_match_none         ((const char *)0)   /* no option match */
#define fsp_fuse_opt_match_exact        ((const char *)1)   /* exact option match */
#define fsp_fuse_opt_match_next         ((const char *)2)   /* option match, value is next arg */

static long long strtoint(const char *p, const char *endp, int base, int is_signed)
{
    long long v;
    int maxdig, maxalp, sign = +1;

    if (is_signed)
    {
        if ('+' == *p)
            p++;
        else if ('-' == *p)
            p++, sign = -1;
    }

    if (0 == base)
    {
        if ('0' == *p)
        {
            p++;
            if ('x' == *p || 'X' == *p)
            {
                p++;
                base = 16;
            }
            else
                base = 8;
        }
        else
        {
            base = 10;
        }
    }

    maxdig = 10 < base ? '9' : (base - 1) + '0';
    maxalp = 10 < base ? (base - 1 - 10) + 'a' : 0;

    for (v = 0; endp > p; p++)
    {
        int c = *p;

        if ('0' <= c && c <= maxdig)
            v = 10 * v + (c - '0');
        else
        {
            c |= 0x20;
            if ('a' <= c && c <= maxalp)
                v = 10 * v + (c - 'a');
            else
                break;
        }
    }

    return sign * v;
}

static void fsp_fuse_opt_match_templ(
    const char *templ, const char **pspec,
    const char **parg, const char *argend)
{
    const char *p, *q;

    *pspec = 0;

    for (p = templ, q = *parg;; p++, q++)
        if ('\0' == *q || (0 != argend && q >= argend))
        {
            if ('\0' == *p)
                *parg = fsp_fuse_opt_match_exact;
            else if (' ' == *p)
                *pspec = p + 1, *parg = fsp_fuse_opt_match_next;
            else
                *parg = fsp_fuse_opt_match_none;
            break;
        }
        else if ('=' == *p)
        {
            if (*q == *p)
                *pspec = p + 1, *parg = q + 1;
            else
                *parg = fsp_fuse_opt_match_none;
            break;
        }
        else if (' ' == *p)
        {
            *pspec = p + 1, *parg = q;
            break;
        }
        else if (*q != *p)
        {
            *parg = fsp_fuse_opt_match_none;
            break;
        }
}

static const struct fuse_opt *fsp_fuse_opt_find(
    const struct fuse_opt opts[], const char **pspec,
    const char **parg, const char *argend)
{
    const struct fuse_opt *opt;
    const char *arg;

    for (opt = opts; 0 != opt->templ; opt++)
    {
        arg = *parg;
        fsp_fuse_opt_match_templ(opt->templ, pspec, &arg, argend);
        if (fsp_fuse_opt_match_none != arg)
        {
            *parg = arg;
            return opt;
        }
    }

    return 0;
}

static int fsp_fuse_opt_call_proc(void *data,
    fuse_opt_proc_t proc,
    const char *arg, int key,
    struct fuse_args *outargs,
    FSP_FUSE_MEMFN_P)
{
    // !!!: NEEDIMPL
    switch (key)
    {
    case FUSE_OPT_KEY_KEEP:
        return 1;
    case FUSE_OPT_KEY_DISCARD:
        return 0;
    default:
        return proc(data, arg, key, outargs);
    }
}

static int fsp_fuse_opt_process_arg(void *data,
    const struct fuse_opt *opt, fuse_opt_proc_t proc,
    const char *spec, const char *arg, const char *argend,
    struct fuse_args *outargs,
    FSP_FUSE_MEMFN_P)
{
#define VAR(data, opt, type)            *(type *)((char *)(data) + (opt)->offset)

    if (-1L == opt->offset)
        return fsp_fuse_opt_call_proc(data, proc, arg, opt->value, outargs, FSP_FUSE_MEMFN_A);
    else
    {
        int h, j, l, t, z;
        long long llv;
        char *s;

        if (0 == spec || '\0' == spec[0])
        {
            VAR(data, opt, int) = opt->value;
            return 0;
        }

        if ('%' != spec[0])
            return -1; /* bad option template */

        h = j = l = t = z = 0;
        for (spec++; *spec; spec++)
            switch (*spec)
            {
            default:
            case 0: case 1: case 2: case 3: case 4:
            case 5: case 6: case 7: case 8: case 9:
            case 'm':
                break;
            case 'h':
                h++;
                break;
            case 'j':
                j++;
                break;
            case 'l':
                l++;
                break;
            case 'L': case 'q':
                l += 2;
                break;
            case 't':
                t++;
                break;
            case 'z':
                z++;
                break;
            case 'd':
                llv = strtoint(arg, argend, 10, 1);
                goto ivar;
            case 'i':
                llv = strtoint(arg, argend, 0, 1);
                goto ivar;
            case 'o':
                llv = strtoint(arg, argend, 8, 0);
                goto ivar;
            case 'u':
                llv = strtoint(arg, argend, 10, 0);
                goto ivar;
            case 'x': case 'X':
                llv = strtoint(arg, argend, 16, 0);
            ivar:
                if (z)
                    VAR(data, opt, size_t) = (size_t)llv;
                else if (t)
                    VAR(data, opt, ptrdiff_t) = (ptrdiff_t)llv;
                else if (j)
                    VAR(data, opt, intmax_t) = (intmax_t)llv;
                else if (1 == h)
                    VAR(data, opt, short) = (short)llv;
                else if (2 <= h)
                    VAR(data, opt, char) = (char)llv;
                else if (1 == l)
                    VAR(data, opt, long) = (long)llv;
                else if (2 <= l)
                    VAR(data, opt, long long) = (long long)llv;
                else
                    VAR(data, opt, int) = (int)llv;
                return 0;
            case 's': case 'c':
                s = memalloc(argend - arg);
                if (0 == s)
                    return -1;
                memcpy(s, arg, argend - arg);
                VAR(data, opt, const char *) = (const char *)s;
                return 0;
            case 'a': case 'e': case 'E': case 'f': case 'g':
                return -1; /* no float support */
            }

        return -1; /* bad option template */
    }

#undef VAR
}

static int fsp_fuse_opt_parse_arg(void *data,
    const struct fuse_opt opts[], fuse_opt_proc_t proc,
    const char *arg0, const char *argend0, const char *arg1,
    struct fuse_args *outargs,
    FSP_FUSE_MEMFN_P)
{
    const struct fuse_opt *opt;
    const char *spec, *arg, *argend;
    int processed = 0;

    arg = arg0, argend = argend0;
    opt = opts;
    while (0 != (opt = fsp_fuse_opt_find(opt, &spec, &arg, argend)))
    {
        if (fsp_fuse_opt_match_exact == arg)
        {
            arg = 0, argend = 0;
        }
        else if (fsp_fuse_opt_match_next == arg)
        {
            if (0 == arg1)
                return -1; /* missing argument for option */
            arg = arg1, argend = 0;
        }

        if (-1 == fsp_fuse_opt_process_arg(data, opt, proc, spec, arg, argend, outargs,
            FSP_FUSE_MEMFN_A))
            return -1;
        processed++;

        arg = arg0, argend = argend0;
        opt++;
    }

    if (0 != processed)
        return 0;

    return fsp_fuse_opt_call_proc(data, proc, arg, FUSE_OPT_KEY_OPT, outargs, FSP_FUSE_MEMFN_A);
}

static int fsp_fuse_opt_proc0(void *data, const char *arg, int key,
    struct fuse_args *outargs)
{
    return 1;
}

FSP_FUSE_API int fsp_fuse_opt_parse(struct fuse_args *args, void *data,
    const struct fuse_opt opts[], fuse_opt_proc_t proc,
    FSP_FUSE_MEMFN_P)
{
    static struct fuse_args args0 = FUSE_ARGS_INIT(0, 0);
    static struct fuse_opt opts0[1] = { FUSE_OPT_END };
    struct fuse_args outargs = FUSE_ARGS_INIT(0, 0);
    const char *arg, *argend;
    int dashdash = 0;

    if (0 == args)
        args = &args0;
    if (0 == opts)
        opts = opts0;
    if (0 == proc)
        proc = fsp_fuse_opt_proc0;

    if (-1 == fsp_fuse_opt_add_arg(&outargs, args->argv[0], FSP_FUSE_MEMFN_A))
        return -1;

    for (int argi = 1; args->argc > argi; argi++)
    {
        arg = args->argv[argi];
        if ('-' == arg[0] && !dashdash)
        {
            switch (arg[1])
            {
            case 'o':
                if ('\0' == arg[2])
                {
                    if (args->argc <= argi + 1)
                        goto fail; /* missing argument for option "-o" */
                    arg = args->argv[++argi];
                }
                else
                    arg += 2;
                for (argend = arg;; argend++)
                {
                    if ('\0' == *argend || ',' == *argend)
                    {
                        if (-1 == fsp_fuse_opt_parse_arg(data, opts, proc,
                            arg, argend, 0, &outargs, FSP_FUSE_MEMFN_A))
                            goto fail;
                    }
                    else if ('\\' == *argend && '\0' != argend[1])
                        argend++;
                }
                break;
            case '-':
                if ('\0' == arg[2])
                {
                    if (-1 == fsp_fuse_opt_add_arg(&outargs, arg, FSP_FUSE_MEMFN_A))
                        return -1;
                    dashdash = 1;
                    break;
                }
                /* fall through */
            default:
                if (-1 == fsp_fuse_opt_parse_arg(data, opts, proc,
                    arg, 0, args->argv[argi + 1], &outargs, FSP_FUSE_MEMFN_A))
                    goto fail;
                break;
            }
        }
        else
            if (-1 == fsp_fuse_opt_call_proc(data, proc, arg, FUSE_OPT_KEY_NONOPT, &outargs,
                FSP_FUSE_MEMFN_A))
                goto fail;
    }

    return 0;

fail:
    fsp_fuse_opt_free_args(&outargs, FSP_FUSE_MEMFN_A);

    return -1;
}

FSP_FUSE_API int fsp_fuse_opt_add_arg(struct fuse_args *args, const char *arg,
    FSP_FUSE_MEMFN_P)
{
    return fsp_fuse_opt_insert_arg(args, args->argc, arg, FSP_FUSE_MEMFN_A);
}

FSP_FUSE_API int fsp_fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg,
    FSP_FUSE_MEMFN_P)
{
    char **argv;
    int argsize;

    if (0 == args)
        return -1;
    if (!args->allocated && 0 != args->argv)
        return -1;
    if (0 > pos || pos > args->argc)
        return -1;

    argv = memalloc((args->argc + 2) * sizeof(char *));
    if (0 == argv)
        return -1;
    argsize = lstrlenA(arg) + 1;
    argv[pos] = memalloc(argsize);
    if (0 == argv[pos])
    {
        memfree(argv);
        return -1;
    }

    memcpy(argv[pos], arg, argsize);
    memcpy(argv, args->argv, pos);
    memcpy(argv + pos + 1, args->argv + pos, args->argc - pos);

    memfree(args->argv);

    args->argc++;
    args->argv = argv;
    argv[args->argc] = 0;

    return 0;
}

FSP_FUSE_API void fsp_fuse_opt_free_args(struct fuse_args *args,
    FSP_FUSE_MEMFN_P)
{
    if (0 == args)
        return;

    if (args->allocated && 0 != args->argv)
    {
        for (int argi = 0; args->argc > argi; argi++)
            memfree(args->argv[argi]);

        memfree(args->argv);
    }

    args->argc = 0;
    args->argv = 0;
    args->allocated = 0;
}

static int fsp_fuse_opt_add_opt_internal(char **opts, const char *opt, int escaped,
    FSP_FUSE_MEMFN_P)
{
    size_t optsize, optlen;
    char *newopts;
    const char *p;

    optsize = 0 != *opts ? lstrlenA(*opts) + 1 : 0;
    for (p = opt, optlen = 0; *p; p++, optlen++)
        if (escaped && (',' == *p || '\\' == *p))
            optlen++;

    newopts = memalloc(optsize + optlen + 1);
    if (0 == newopts)
        return -1;
    memfree(*opts);
    *opts = newopts;

    if (0 != optsize)
    {
        memcpy(newopts, *opts, optsize - 1);
        newopts[optsize - 1] = ',';
        newopts += optsize;
    }

    for (p = opt; *p; p++, newopts++)
    {
        if (escaped && (',' == *p || '\\' == *p))
            *newopts++ = '\\';
        *newopts = *p;
    }

    return 0;
}

FSP_FUSE_API int fsp_fuse_opt_add_opt(char **opts, const char *opt,
    FSP_FUSE_MEMFN_P)
{
    return fsp_fuse_opt_add_opt_internal(opts, opt, 0,
        FSP_FUSE_MEMFN_A);
}

FSP_FUSE_API int fsp_fuse_opt_add_opt_escaped(char **opts, const char *opt,
    FSP_FUSE_MEMFN_P)
{
    return fsp_fuse_opt_add_opt_internal(opts, opt, 1,
        FSP_FUSE_MEMFN_A);
}

FSP_FUSE_API int fsp_fuse_opt_match(const struct fuse_opt opts[], const char *arg,
    FSP_FUSE_MEMFN_P)
{
    if (0 == opts)
        return 0;

    const char *spec;
    return fsp_fuse_opt_find(opts, &spec, &arg, 0) ? 1 : 0;
}
