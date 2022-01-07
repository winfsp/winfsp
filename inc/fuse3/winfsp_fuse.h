/**
 * @file fuse3/winfsp_fuse.h
 * WinFsp FUSE3 compatible API.
 *
 * @copyright 2015-2022 Bill Zissimopoulos
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

#ifndef FUSE3_WINFSP_FUSE_H_INCLUDED
#define FUSE3_WINFSP_FUSE_H_INCLUDED

#include "../fuse/winfsp_fuse.h"

#if defined(_WIN64) || defined(_WIN32)
typedef intptr_t ssize_t;
#endif

#if !defined(WINFSP_DLL_INTERNAL)
#define fuse3                           fuse
#define fuse3_apply_conn_info_opts      fuse_apply_conn_info_opts
#define fuse3_buf                       fuse_buf
#define fuse3_buf_copy                  fuse_buf_copy
#define fuse3_buf_copy_flags            fuse_buf_copy_flags
#define fuse3_buf_flags                 fuse_buf_flags
#define fuse3_buf_size                  fuse_buf_size
#define fuse3_bufvec                    fuse_bufvec
#define fuse3_clean_cache               fuse_clean_cache
#define fuse3_config                    fuse_config
#define fuse3_conn_info                 fuse_conn_info
#define fuse3_conn_info_opts            fuse_conn_info_opts
#define fuse3_context                   fuse_context
#define fuse3_daemonize                 fuse_daemonize
#define fuse3_destroy                   fuse_destroy
#define fuse3_exit                      fuse_exit
#define fuse3_file_info                 fuse_file_info
#define fuse3_fill_dir_flags            fuse_fill_dir_flags
#define fuse3_fill_dir_t                fuse_fill_dir_t
#define fuse3_get_context               fuse_get_context
#define fuse3_get_session               fuse_get_session
#define fuse3_getgroups                 fuse_getgroups
#define fuse3_interrupted               fuse_interrupted
#define fuse3_invalidate_path           fuse_invalidate_path
#define fuse3_lib_help                  fuse_lib_help
#define fuse3_loop                      fuse_loop
#define fuse3_loop_config               fuse_loop_config
#define fuse3_loop_mt                   fuse_loop_mt
#define fuse3_loop_mt_31                fuse_loop_mt_31
#define fuse3_main_real                 fuse_main_real
#define fuse3_mount                     fuse_mount
#define fuse3_new                       fuse_new
#define fuse3_new_30                    fuse_new_30
#define fuse3_notify_poll               fuse_notify_poll
#define fuse3_operations                fuse_operations
#define fuse3_parse_conn_info_opts      fuse_parse_conn_info_opts
#define fuse3_pkgversion                fuse_pkgversion
#define fuse3_pollhandle                fuse_pollhandle
#define fuse3_pollhandle_destroy        fuse_pollhandle_destroy
#define fuse3_readdir_flags             fuse_readdir_flags
#define fuse3_remove_signal_handlers    fuse_remove_signal_handlers
#define fuse3_session                   fuse_session
#define fuse3_set_signal_handlers       fuse_set_signal_handlers
#define fuse3_start_cleanup_thread      fuse_start_cleanup_thread
#define fuse3_stop_cleanup_thread       fuse_stop_cleanup_thread
#define fuse3_unmount                   fuse_unmount
#define fuse3_version                   fuse_version
#endif

#endif
