/**
 * @file compat.h
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

#ifndef COMPAT_H_INCLUDED
#define COMPAT_H_INCLUDED

#if defined(_WIN32) && defined(FSP_FUSE_SYM)
#include <winfsp/winfsp.h>
#undef fuse_main
#define fuse_main(argc, argv, ops, data)\
    (FspLoad(0), fuse_main_real(argc, argv, ops, sizeof *(ops), data))
#endif

#if !defined(_WIN32) && !defined(fuse_stat)

#define fuse_uid_t                      uid_t
#define fuse_gid_t                      gid_t
#define fuse_pid_t                      pid_t

#define fuse_dev_t                      dev_t
#define fuse_mode_t                     mode_t
#define fuse_nlink_t                    nlink_t
#define fuse_off_t                      off_t

#define fuse_fsblkcnt_t                 fsblkcnt_t
#define fuse_fsfilcnt_t                 fsfilcnt_t
#define fuse_blksize_t                  blksize_t
#define fuse_blkcnt_t                   blkcnt_t

#define fuse_timespec                   timespec

#define fuse_stat                       stat

#define fuse_statvfs                    statvfs

#define fuse_flock                      flock

#define fuse_iovec                      iovec

#endif

#if !defined(S_IFMT)
#define S_IFMT                          0170000
#endif
#if !defined(S_IFDIR)
#define S_IFDIR                         0040000
#endif
#if !defined(S_IFCHR)
#define S_IFCHR                         0020000
#endif
#if !defined(S_IFBLK)
#define S_IFBLK                         0060000
#endif
#if !defined(S_IFREG)
#define S_IFREG                         0100000
#endif
#if !defined(S_IFLNK)
#define S_IFLNK                         0120000
#endif
#if !defined(S_IFSOCK)
#define S_IFSOCK                        0140000
#endif
#if !defined(S_IFIFO)
#define S_IFIFO                         0010000
#endif

#if defined(__APPLE__)
#define st_atim                         st_atimespec
#define st_ctim                         st_ctimespec
#define st_mtim                         st_mtimespec
#endif

#if defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)
#include <sys/xattr.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(_WIN32)
#define XATTR_CREATE                    1
#define XATTR_REPLACE                   2
#endif

#if !defined(ENOATTR)
#define ENOATTR                         ENODATA
#elif !defined(ENODATA)
#define ENODATA                         ENOATTR
#endif

#endif
