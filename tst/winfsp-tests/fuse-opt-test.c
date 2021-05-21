/**
 * @file fuse-opt-test.c
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

#include <fuse/fuse_opt.h>
#include <tlib/testsuite.h>
#include <stddef.h>
#include <string.h>

#include "winfsp-tests.h"

struct data
{
    int fortytwo;
    int a, b, c, d, e, f, g, h;
    int i, j, k, l, m, n, o, p;
    char *q, *r, *s, *t;
    int u, v, V;
    char *w;
    short x;
    long y;
    long long z;
    int dec, neg, hex, oct;
    int sel;
    char *esc; int ESC;
    int arg_discard, arg_keep;
    int opt_discard, opt_keep;
    int nonopt_discard, nonopt_keep;
};

static int fuse_opt_parse_test_proc(void *data0, const char *arg, int key,
    struct fuse_args *outargs)
{
    struct data *data = data0;

    switch (key)
    {
    default:
        ASSERT(0);
        return -1;
    case FUSE_OPT_KEY_OPT:
        if (0 == strcmp("--arg-discard", arg))
        {
            data->arg_discard++;
            return 0;
        }
        if (0 == strcmp("--arg-keep", arg))
        {
            data->arg_keep++;
            return 1;
        }
        if (0 == strcmp("opt-discard", arg))
        {
            data->opt_discard++;
            return 0;
        }
        if (0 == strcmp("opt-keep", arg))
        {
            data->opt_keep++;
            return 1;
        }
        ASSERT(0);
        return -1;
    case FUSE_OPT_KEY_NONOPT:
        if (0 == strcmp("--discard", arg))
        {
            data->nonopt_discard++;
            return 1;
        }
        if (0 == strcmp("--keep", arg))
        {
            data->nonopt_keep++;
            return 0;
        }
        ASSERT(0);
        return -1;
    case 'a':
        ASSERT(0);
        return -1;
    case 'b':
        ASSERT(0 == strcmp("-b", arg));
        data->b = 'B';
        return 1;
    case 'c':
        ASSERT(0);
        return -1;
    case 'd':
        ASSERT(0 == strcmp("--dlong", arg));
        data->d = 'D';
        return 1;
    case 'e':
        ASSERT(0);
        return -1;
    case 'f':
        ASSERT(0 == strcmp("f", arg));
        data->f = 'F';
        return 1;
    case 'g':
        ASSERT(0);
        return -1;
    case 'h':
        ASSERT(0 == strcmp("hlong", arg));
        data->h = 'H';
        return 1;
    case 'i':
        ASSERT(0);
        return -1;
    case 'j':
        ASSERT(0 == strcmp("-j=74", arg));
        data->j = 'J';
        return 1;
    case 'k':
        ASSERT(0);
        return -1;
    case 'l':
        ASSERT(0 == strcmp("--llong=76", arg));
        data->l = 'L';
        return 1;
    case 'm':
        ASSERT(0);
        return -1;
    case 'n':
        ASSERT(0 == strcmp("n=78", arg));
        data->n = 'N';
        return 1;
    case 'o':
        ASSERT(0);
        return -1;
    case 'p':
        ASSERT(0 == strcmp("plong=80", arg));
        data->p = 'P';
        return 1;
    case 'q':
        ASSERT(0);
        return -1;
    case 'r':
        ASSERT(0);
        return -1;
    case 's':
        ASSERT(0);
        return -1;
    case 't':
        ASSERT(0);
        return -1;
    case 'u':
        ASSERT(0);
        return -1;
    case 'v':
        ASSERT(0 == strcmp("-v86", arg));
        data->v = 'V';
        return 1;
    case 'V':
        ASSERT(0 == strcmp("-V118", arg));
        data->V = 'v';
        return 1;
    case 'w':
        ASSERT(0);
        return -1;
    case 'x':
        ASSERT(0);
        return -1;
    case 'y':
        ASSERT(0);
        return -1;
    case 'z':
        ASSERT(0);
        return -1;
    case 'ESC':
        ASSERT(0 == strcmp("ESC=\\FOO,BAR\\", arg));
        data->ESC = 1;
        return 1;
    }
}

void fuse_opt_parse_test(void)
{
    static struct fuse_opt opts[] =
    {
        { "-a", offsetof(struct data, a), 'a' },
        FUSE_OPT_KEY("-b", 'b'),
        { "--clong", offsetof(struct data, c), 'c' },
        FUSE_OPT_KEY("--dlong", 'd'),
        { "e", offsetof(struct data, e), 'e' },
        FUSE_OPT_KEY("f", 'f'),
        { "glong", offsetof(struct data, g), 'g' },
        FUSE_OPT_KEY("hlong", 'h'),

        { "-i=", offsetof(struct data, i), 'i' },
        FUSE_OPT_KEY("-j=", 'j'),
        { "--klong=", offsetof(struct data, k), 'k' },
        FUSE_OPT_KEY("--llong=", 'l'),
        { "m=", offsetof(struct data, m), 'm' },
        FUSE_OPT_KEY("n=", 'n'),
        { "olong=", offsetof(struct data, o), 'o' },
        FUSE_OPT_KEY("plong=", 'p'),

        { "-q=%s", offsetof(struct data, q), 'q' },
        { "--rlong=%s", offsetof(struct data, r), 'r' },
        { "s=%s", offsetof(struct data, s), 's' },
        { "tlong=%s", offsetof(struct data, t), 't' },

        { "-u ", offsetof(struct data, u), 'u' },
        FUSE_OPT_KEY("-v ", 'v'),
        FUSE_OPT_KEY("-V ", 'V'),

        { "-w %s", offsetof(struct data, w), 'w' },

        { "-x=%hi", offsetof(struct data, x), 'x' },
        { "-y=%li", offsetof(struct data, y), 'y' },
        { "-z=%lli", offsetof(struct data, z), 'z' },

        { "--dec=%d", offsetof(struct data, dec), 'dec' },
        { "--neg=%d", offsetof(struct data, neg), 'neg' },
        { "--hex=%x", offsetof(struct data, hex), 'hex' },
        { "--oct=%o", offsetof(struct data, oct), 'oct' },

        { "--sel=fortyone", offsetof(struct data, sel), 141 },
        { "--sel=fortytwo", offsetof(struct data, sel), 142 },
        { "--sel=fortythree", offsetof(struct data, sel), 143 },

        { "esc=%s", offsetof(struct data, esc), 'esc' },
        FUSE_OPT_KEY("ESC=", 'ESC'),

        FUSE_OPT_KEY("--discard", FUSE_OPT_KEY_DISCARD),
        FUSE_OPT_KEY("--keep", FUSE_OPT_KEY_KEEP),

        FUSE_OPT_END,
    };
    static char *argv[] =
    {
        "exec",
        "-a",
        "-b",
        "--clong",
        "--dlong",
        "-o", "e,f",
        "-oglong,hlong",
        "-i=73",
        "-j=74",
        "--klong=75",
        "--llong=76",
        "-om=77",
        "-o", "n=78,olong=79,plong=80",
        "-q=QqQq",
        "--rlong=RrRrRrRr",
        "-os=SsSs",
        "-otlong=TtTtTtTt",
        "-u85",
        "-v", "86",
        "-V118",
        "-wWwWw",
        "-x=65537",
        "-y=040000000001",
        "-z=0x100000001",
        "--dec=+1234567890",
        "--neg=-1234567890",
        "--hex=ABCDEF",
        "--oct=12345670",
        "--sel=fortytwo",
        "-oesc=\\\\foo\\,bar\\\\,ESC=\\\\FOO\\,BAR\\\\",
        "--discard",
        "--keep",
        "--arg-discard",
        "--arg-keep",
        "-oopt-discard",
        "-oopt-keep",
        "--",
        "--discard",
        "--keep",
        0
    };
    static char *outargv[] =
    {
        "exec",
        "-o", "f,hlong,n=78,plong=80,ESC=\\\\FOO\\,BAR\\\\,opt-keep",
        "-b",
        "--dlong",
        "-j=74",
        "--llong=76",
        "-v86",
        "-V118",
        "--keep",
        "--arg-keep",
        "--",
        "--discard",
        0
    };
    struct fuse_args args = FUSE_ARGS_INIT(0, 0);
    struct data data;
    int result;

    args.argc = sizeof argv / sizeof argv[0] - 1;
    args.argv = argv;

    memset(&data, 0, sizeof data);
    data.fortytwo = 42;
    result = fuse_opt_parse(&args, &data, opts, fuse_opt_parse_test_proc);
    ASSERT(0 == result);

    ASSERT(42 == data.fortytwo);
    ASSERT('a' == data.a);
    ASSERT('B' == data.b);
    ASSERT('c' == data.c);
    ASSERT('D' == data.d);
    ASSERT('e' == data.e);
    ASSERT('F' == data.f);
    ASSERT('g' == data.g);
    ASSERT('H' == data.h);
    ASSERT('i' == data.i);
    ASSERT('J' == data.j);
    ASSERT('k' == data.k);
    ASSERT('L' == data.l);
    ASSERT('m' == data.m);
    ASSERT('N' == data.n);
    ASSERT('o' == data.o);
    ASSERT('P' == data.p);
    ASSERT(0 == strcmp("QqQq", data.q));
    ASSERT(0 == strcmp("RrRrRrRr", data.r));
    ASSERT(0 == strcmp("SsSs", data.s));
    ASSERT(0 == strcmp("TtTtTtTt", data.t));
    ASSERT('u' == data.u);
    ASSERT('V' == data.v);
    ASSERT('v' == data.V);
    ASSERT(0 == strcmp("WwWw", data.w));
    ASSERT(1 == data.x);
    ASSERT((long)040000000001 == data.y);
    ASSERT((long long)0x100000001 == data.z);
    ASSERT(+1234567890 == data.dec);
    ASSERT(-1234567890 == data.neg);
    ASSERT(0xABCDEF == data.hex);
    ASSERT(012345670 == data.oct);
    ASSERT(142 == data.sel);
    ASSERT(0 == strcmp("\\foo,bar\\", data.esc));
    ASSERT(1 == data.ESC);
    ASSERT(1 == data.arg_discard);
    ASSERT(1 == data.arg_keep);
    ASSERT(1 == data.opt_discard);
    ASSERT(1 == data.opt_keep);
    ASSERT(1 == data.nonopt_discard);
    ASSERT(1 == data.nonopt_keep);

    ASSERT(args.argc == sizeof outargv / sizeof outargv[0] - 1);
    for (int i = 0; args.argc > i; i++)
        ASSERT(0 == strcmp(args.argv[i], outargv[i]));
    ASSERT(0 == args.argv[args.argc]);
    ASSERT(args.allocated);

    fuse_opt_free_args(&args);

    free(data.q);
    free(data.r);
    free(data.s);
    free(data.t);
    free(data.w);
    free(data.esc);
}

void fuse_opt_tests(void)
{
    if (OptExternal)
        return;

    TEST(fuse_opt_parse_test);
}
