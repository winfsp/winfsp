/**
 * @file dll/fuse/fuse_opt.c
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

/*
 * Define the following symbol to support escaped commas (',') during fuse_opt_parse.
 */
#define FSP_FUSE_OPT_PARSE_ESCAPED_COMMAS

#define fsp_fuse_opt_match_none         ((const char *)0)   /* no option match */
#define fsp_fuse_opt_match_exact        ((const char *)1)   /* exact option match */
#define fsp_fuse_opt_match_next         ((const char *)2)   /* option match, value is next arg */

static void fsp_fuse_opt_match_templ(
    const char *templ, const char **pspec,
    const char **parg)
{
    const char *p, *q;

    *pspec = 0;

    for (p = templ, q = *parg;; p++, q++)
        if ('\0' == *q)
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
            {
                p++, q++;
                if ('%' == *p || '\0' == *p)
                    *pspec = p, *parg = q;
                else
                    *parg = 0 == invariant_strcmp(q, p) ?
                        fsp_fuse_opt_match_exact : fsp_fuse_opt_match_none;
            }
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
    const char **parg)
{
    const struct fuse_opt *opt;
    const char *arg;

    for (opt = opts; 0 != opt->templ; opt++)
    {
        arg = *parg;
        fsp_fuse_opt_match_templ(opt->templ, pspec, &arg);
        if (fsp_fuse_opt_match_none != arg)
        {
            *parg = arg;
            return opt;
        }
    }

    return 0;
}

static int fsp_fuse_opt_call_proc(struct fsp_fuse_env *env,
    void *data, fuse_opt_proc_t proc,
    const char *arg, const char *argl,
    int key, int is_opt,
    struct fuse_args *outargs)
{
    int result, len0, len1;
    char *fullarg = 0;

    if (FUSE_OPT_KEY_DISCARD == key)
        return 0;

    len0 = lstrlenA(arg);

    if (0 != argl && !(arg <= argl && argl < arg + len0))
    {
        len1 = lstrlenA(argl);

        fullarg = env->memalloc(len0 + len1 + 1);
        if (0 == fullarg)
            return -1;

        memcpy(fullarg, arg, len0);
        memcpy(fullarg + len0, argl, len1);
        fullarg[len0 + len1] = '\0';

        arg = fullarg;
    }

    if (FUSE_OPT_KEY_KEEP != key && 0 != proc)
    {
        result = proc(data, arg, key, outargs);
        if (-1 == result || 0 == result)
            goto exit;
    }

    if (is_opt)
    {
        if (!(3 <= outargs->argc &&
            '-' == outargs->argv[1][0] && 'o' == outargs->argv[1][1] &&
            '\0' == outargs->argv[1][2]))
        {
            result = fsp_fuse_opt_insert_arg(env, outargs, 1, "-o");
            if (-1 == result)
                goto exit;
            result = fsp_fuse_opt_insert_arg(env, outargs, 2, "");
            if (-1 == result)
                goto exit;
        }

#if defined(FSP_FUSE_OPT_PARSE_ESCAPED_COMMAS)
        result = fsp_fuse_opt_add_opt_escaped(env, &outargs->argv[2], arg);
#else
        result = fsp_fuse_opt_add_opt(env, &outargs->argv[2], arg);
#endif
        if (-1 == result)
            goto exit;
    }
    else
    {
        result = fsp_fuse_opt_add_arg(env, outargs, arg);
        if (-1 == result)
            goto exit;
    }

    result = 0;

exit:
    if (0 != fullarg)
        env->memfree(fullarg);

    return result;
}

static int fsp_fuse_opt_process_arg(struct fsp_fuse_env *env,
    void *data, const struct fuse_opt *opt, fuse_opt_proc_t proc,
    const char *spec,
    const char *arg, const char *argl,
    int is_opt,
    struct fuse_args *outargs)
{
#define VAR(data, opt, type)            *(type *)((char *)(data) + (opt)->offset)

    if (-1L == opt->offset)
        return fsp_fuse_opt_call_proc(env,
            data, proc, arg, argl, opt->value, is_opt, outargs);
    else
    {
        int h, j, l, t, z;
        long long llv;
        char *s;
        int len;

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
                llv = strtollint(argl, 0, 10, 1);
                goto ivar;
            case 'i':
                llv = strtollint(argl, 0, 0, 1);
                goto ivar;
            case 'o':
                llv = strtollint(argl, 0, 8, 0);
                goto ivar;
            case 'u':
                llv = strtollint(argl, 0, 10, 0);
                goto ivar;
            case 'x': case 'X':
                llv = strtollint(argl, 0, 16, 0);
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
                {
#if defined(_WIN64)
                    /* long is 8 bytes long in Cygwin64 and 4 bytes long in Win64 */
                    if ('C' == env->environment)
                        VAR(data, opt, long long) = (long long)llv;
                    else
                        VAR(data, opt, long) = (long)llv;
#else
                    VAR(data, opt, long) = (long)llv;
#endif
                }
                else if (2 <= l)
                    VAR(data, opt, long long) = (long long)llv;
                else
                    VAR(data, opt, int) = (int)llv;
                return 0;
            case 's': case 'c':
                len = lstrlenA(argl);
                s = env->memalloc(len + 1);
                if (0 == s)
                    return -1;
                memcpy(s, argl, len);
                s[len] = '\0';
                VAR(data, opt, const char *) = (const char *)s;
                return 0;
            case 'a': case 'e': case 'E': case 'f': case 'g':
                return -1; /* no float support */
            }

        return -1; /* bad option template */
    }

#undef VAR
}

static int fsp_fuse_opt_parse_arg(struct fsp_fuse_env *env,
    void *data, const struct fuse_opt opts[], fuse_opt_proc_t proc,
    const char *arg, const char *nextarg, int *pconsumed_nextarg,
    int is_opt,
    struct fuse_args *outargs)
{
    const struct fuse_opt *opt;
    const char *spec, *argl;
    int processed = 0;

    argl = arg;
    opt = opts;
    while (0 != (opt = fsp_fuse_opt_find(opt, &spec, &argl)))
    {
        if (fsp_fuse_opt_match_exact == argl)
            argl = arg;
        else if (fsp_fuse_opt_match_next == argl)
        {
            if (0 == nextarg)
                return -1; /* missing argument for option */
            argl = nextarg;
            *pconsumed_nextarg = 1;
        }

        if (-1 == fsp_fuse_opt_process_arg(env,
            data, opt, proc, spec, arg, argl, is_opt, outargs))
            return -1;
        processed++;

        argl = arg;
        opt++;
    }

    if (0 != processed)
        return 0;

    return fsp_fuse_opt_call_proc(env,
        data, proc, arg, arg, FUSE_OPT_KEY_OPT, is_opt, outargs);
}

static int fsp_fuse_opt_proc0(void *data, const char *arg, int key,
    struct fuse_args *outargs)
{
    return 1;
}

FSP_FUSE_API int fsp_fuse_opt_parse(struct fsp_fuse_env *env,
    struct fuse_args *args,
    void *data, const struct fuse_opt opts[], fuse_opt_proc_t proc)
{
    static struct fuse_args args0 = FUSE_ARGS_INIT(0, 0);
    static struct fuse_opt opts0[1] = { FUSE_OPT_END };
    struct fuse_args outargs = FUSE_ARGS_INIT(0, 0);
    const char *arg;
    char *argcopy, *argend;
    int dashdash = 0, consumed_nextarg;

    if (0 == args)
        args = &args0;
    if (0 == opts)
        opts = opts0;
    if (0 == proc)
        proc = fsp_fuse_opt_proc0;

    if (-1 == fsp_fuse_opt_add_arg(env, &outargs, args->argv[0]))
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
                argcopy = env->memalloc(lstrlenA(arg) + 1);
                if (0 == argcopy)
                    goto fail;
                argend = argcopy;
                for (;;)
                {
#if defined(FSP_FUSE_OPT_PARSE_ESCAPED_COMMAS)
                    if ('\\' == *arg)
                    {
                        arg++;
                        *argend++ = *arg++;
                        continue;
                    }
#endif
                    if ('\0' == *arg || ',' == *arg)
                    {
                        *argend = '\0';
                        if (-1 == fsp_fuse_opt_parse_arg(env,
                            data, opts, proc, argcopy, 0, 0, 1, &outargs))
                        {
                            env->memfree(argcopy);
                            goto fail;
                        }

                        if ('\0' == *arg)
                            break;

                        arg++;
                        argend = argcopy;
                    }
                    else
                        *argend++ = *arg++;
                }
                env->memfree(argcopy);
                break;
            case '-':
                if ('\0' == arg[2])
                {
                    if (-1 == fsp_fuse_opt_add_arg(env, &outargs, arg))
                        return -1;
                    dashdash = 1;
                    break;
                }
                /* fall through */
            default:
                consumed_nextarg = 0;
                if (-1 == fsp_fuse_opt_parse_arg(env,
                    data, opts, proc, arg, args->argv[argi + 1], &consumed_nextarg, 0, &outargs))
                    goto fail;
                if (consumed_nextarg)
                    argi++;
                break;
            }
        }
        else
            if (-1 == fsp_fuse_opt_call_proc(env,
                data, proc, arg, arg, FUSE_OPT_KEY_NONOPT, 0, &outargs))
                goto fail;
    }

    /* if "--" is the last argument, remove it (fuse_opt compatibility) */
    if (0 < outargs.argc &&
        '-' == outargs.argv[outargs.argc - 1][0] &&
        '-' == outargs.argv[outargs.argc - 1][1] &&
        '\0' == outargs.argv[outargs.argc - 1][2])
    {
        env->memfree(outargs.argv[--outargs.argc]);
        outargs.argv[outargs.argc] = 0;
    }

    fsp_fuse_opt_free_args(env, args);
    memcpy(args, &outargs, sizeof outargs);

    return 0;

fail:
    fsp_fuse_opt_free_args(env, &outargs);

    return -1;
}

FSP_FUSE_API int fsp_fuse_opt_add_arg(struct fsp_fuse_env *env,
    struct fuse_args *args, const char *arg)
{
    return fsp_fuse_opt_insert_arg(env, args, args->argc, arg);
}

FSP_FUSE_API int fsp_fuse_opt_insert_arg(struct fsp_fuse_env *env,
    struct fuse_args *args, int pos, const char *arg)
{
    char **argv;
    int argsize;

    if (0 == args)
        return -1;
    if (0 != args->argv && !args->allocated)
        return -1;
    if (0 > pos || pos > args->argc)
        return -1;

    argv = env->memalloc((args->argc + 2) * sizeof(char *));
    if (0 == argv)
        return -1;
    argsize = lstrlenA(arg) + 1;
    argv[pos] = env->memalloc(argsize);
    if (0 == argv[pos])
    {
        env->memfree(argv);
        return -1;
    }

    memcpy(argv[pos], arg, argsize);
    memcpy(argv, args->argv, sizeof(char *) * pos);
    memcpy(argv + pos + 1, args->argv + pos, sizeof(char *) * (args->argc - pos));

    env->memfree(args->argv);

    args->argc++;
    args->argv = argv;
    argv[args->argc] = 0;
    args->allocated = 1;

    return 0;
}

FSP_FUSE_API void fsp_fuse_opt_free_args(struct fsp_fuse_env *env,
    struct fuse_args *args)
{
    if (0 == args)
        return;

    if (args->allocated && 0 != args->argv)
    {
        for (int argi = 0; args->argc > argi; argi++)
            env->memfree(args->argv[argi]);

        env->memfree(args->argv);
    }

    args->argc = 0;
    args->argv = 0;
    args->allocated = 0;
}

static int fsp_fuse_opt_add_opt_internal(struct fsp_fuse_env *env,
    char **opts, const char *opt, int escaped)
{
    size_t optsize, optlen;
    char *newopts;
    const char *p;

    optsize = 0 != *opts && '\0' != (*opts)[0] ? lstrlenA(*opts) + 1 : 0;
    for (p = opt, optlen = 0; *p; p++, optlen++)
        if (escaped && (',' == *p || '\\' == *p))
            optlen++;

    newopts = env->memalloc(optsize + optlen + 1);
    if (0 == newopts)
        return -1;

    if (0 != optsize)
    {
        memcpy(newopts, *opts, optsize - 1);
        newopts[optsize - 1] = ',';
    }

    env->memfree(*opts);
    *opts = newopts;
    newopts += optsize;

    for (p = opt; *p; p++, newopts++)
    {
        if (escaped && (',' == *p || '\\' == *p))
            *newopts++ = '\\';
        *newopts = *p;
    }
    *newopts = '\0';

    return 0;
}

FSP_FUSE_API int fsp_fuse_opt_add_opt(struct fsp_fuse_env *env,
    char **opts, const char *opt)
{
    return fsp_fuse_opt_add_opt_internal(env, opts, opt, 0);
}

FSP_FUSE_API int fsp_fuse_opt_add_opt_escaped(struct fsp_fuse_env *env,
    char **opts, const char *opt)
{
    return fsp_fuse_opt_add_opt_internal(env, opts, opt, 1);
}

FSP_FUSE_API int fsp_fuse_opt_match(struct fsp_fuse_env *env,
    const struct fuse_opt opts[], const char *arg)
{
    if (0 == opts)
        return 0;

    const char *spec;
    return !!fsp_fuse_opt_find(opts, &spec, &arg);
}
