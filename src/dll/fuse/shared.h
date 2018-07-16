/**
 * @file dll/fuse/shared.h
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

#ifndef WINFSP_DLL_FUSE_SHARED_H_INCLUDED
#define WINFSP_DLL_FUSE_SHARED_H_INCLUDED

#define enosys(env)                     ('C' == (env)->environment ? 88 : 40)

struct fsp_fuse_obj_hdr
{
    void (*dtor)(void *);
    __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) UINT8 ObjectBuf[];
};

static inline void *fsp_fuse_obj_alloc(struct fsp_fuse_env *env, size_t size)
{
    struct fsp_fuse_obj_hdr *hdr;

    hdr = env->memalloc(sizeof(struct fsp_fuse_obj_hdr) + size);
    if (0 == hdr)
        return 0;

    hdr->dtor = env->memfree;
    memset(hdr->ObjectBuf, 0, size);
    return hdr->ObjectBuf;
}

static inline void fsp_fuse_obj_free(void *obj)
{
    if (0 == obj)
        return;

    struct fsp_fuse_obj_hdr *hdr = (PVOID)((PUINT8)obj - sizeof(struct fsp_fuse_obj_hdr));

    hdr->dtor(hdr);
}

#endif
