/**
 * @file cygfuse/cygfuse.cpp
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

#include <windows.h>
#undef _WIN32
#undef _WIN64

static HANDLE cygfuse_init_winfsp();
static HANDLE cygfuse_init_fail(int err);
static inline void cygfuse_init()
{
    static HANDLE Handle = cygfuse_init_winfsp();
}

#define FSP_FUSE_API                    static
#define FSP_FUSE_API_NAME(api)          (* pfn_ ## api)
#define FSP_FUSE_API_CALL(api)          (cygfuse_init(), pfn_ ## api)
#define FSP_FUSE_SYM(proto, ...)        __attribute__ ((visibility("default"))) proto { __VA_ARGS__ }
#include <fuse_common.h>
#include <fuse.h>
#include <fuse_opt.h>

#if defined(__LP64__)
#define CYGFUSE_WINFSP_NAME             "winfsp-x64.dll"
#else
#define CYGFUSE_WINFSP_NAME             "winfsp-x86.dll"
#endif
#define CYGFUSE_WINFSP_PATH             "bin\\" CYGFUSE_WINFSP_NAME
#define CYGFUSE_API_GET(h, n)           \
    if (0 == (*(FARPROC *)&(pfn_ ## n) = GetProcAddress((HMODULE)h, #n)))\
        return cygfuse_init_fail(ERROR_PROC_NOT_FOUND);

static HANDLE cygfuse_init_fail(int err)
{
    //RaiseException(ERROR_SEVERITY_ERROR | (109/*FACILITY_VISUALCPP*/ << 16) | err, 0, 0, 0);
    abort();
    return 0;
}

static HANDLE cygfuse_init_winfsp()
{
    HANDLE Handle;

    Handle = LoadLibraryW(L"" CYGFUSE_WINFSP_NAME);
    if (0 == Handle)
    {
        HKEY RegKey;
        DWORD RegResult, RegType;
        WCHAR Path[MAX_PATH];
        DWORD Size;

        Size = sizeof(Path);
        if (ERROR_SUCCESS == (RegResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"Software\\WinFsp", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &RegKey)))
        {
            RegResult = RegQueryValueExW(RegKey, L"InstallDir", 0, &RegType, (PBYTE)Path, &Size);
            RegCloseKey(RegKey);
        }
        if (ERROR_SUCCESS != RegResult)
            return cygfuse_init_fail(ERROR_MOD_NOT_FOUND);

        Size /= sizeof(WCHAR);
        if (Size >= MAX_PATH)
            Size = MAX_PATH - 1;
        Path[Size] = L'\0';

        Size = lstrlenW(Path);
        if (Size * sizeof(WCHAR) + sizeof L"" CYGFUSE_WINFSP_PATH > MAX_PATH * sizeof(WCHAR))
            return cygfuse_init_fail(ERROR_MOD_NOT_FOUND);

        memcpy(Path + Size, L"" CYGFUSE_WINFSP_PATH, sizeof L"" CYGFUSE_WINFSP_PATH);

        Handle = LoadLibraryW(Path);
        if (0 == Handle)
            return cygfuse_init_fail(ERROR_MOD_NOT_FOUND);
    }

    /* winfsp_fuse.h */
    CYGFUSE_API_GET(Handle, fsp_fuse_signal_handler);

    /* fuse_common.h */
    CYGFUSE_API_GET(Handle, fsp_fuse_version);
    CYGFUSE_API_GET(Handle, fsp_fuse_mount);
    CYGFUSE_API_GET(Handle, fsp_fuse_unmount);
    CYGFUSE_API_GET(Handle, fsp_fuse_parse_cmdline);
    CYGFUSE_API_GET(Handle, fsp_fuse_ntstatus_from_errno);

    /* fuse.h */
    CYGFUSE_API_GET(Handle, fsp_fuse_main_real);
    CYGFUSE_API_GET(Handle, fsp_fuse_is_lib_option);
    CYGFUSE_API_GET(Handle, fsp_fuse_new);
    CYGFUSE_API_GET(Handle, fsp_fuse_destroy);
    CYGFUSE_API_GET(Handle, fsp_fuse_loop);
    CYGFUSE_API_GET(Handle, fsp_fuse_loop_mt);
    CYGFUSE_API_GET(Handle, fsp_fuse_exit);
    CYGFUSE_API_GET(Handle, fsp_fuse_get_context);

    /* fuse_opt.h */
    CYGFUSE_API_GET(Handle, fsp_fuse_opt_parse);
    CYGFUSE_API_GET(Handle, fsp_fuse_opt_add_arg);
    CYGFUSE_API_GET(Handle, fsp_fuse_opt_insert_arg);
    CYGFUSE_API_GET(Handle, fsp_fuse_opt_free_args);
    CYGFUSE_API_GET(Handle, fsp_fuse_opt_add_opt);
    CYGFUSE_API_GET(Handle, fsp_fuse_opt_add_opt_escaped);
    CYGFUSE_API_GET(Handle, fsp_fuse_opt_match);

    return Handle;
}
