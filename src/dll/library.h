/**
 * @file dll/library.h
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

#ifndef WINFSP_DLL_LIBRARY_H_INCLUDED
#define WINFSP_DLL_LIBRARY_H_INCLUDED

#define WINFSP_DLL_INTERNAL
#include <winfsp/winfsp.h>
#include <shared/minimal.h>
#include <strsafe.h>

#define LIBRARY_NAME                    "WinFsp"

/* DEBUGLOG */
#if !defined(NDEBUG)
#define DEBUGLOG(fmt, ...)              \
    FspDebugLog("[U] " LIBRARY_NAME "!" __FUNCTION__ ": " fmt "\n", __VA_ARGS__)
#define DEBUGLOGSD(fmt, SD)             \
    FspDebugLogSD("[U] " LIBRARY_NAME "!" __FUNCTION__ ": " fmt "\n", SD)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#define DEBUGLOGSD(fmt, SD)             ((void)0)
#endif

VOID FspPosixFinalize(BOOLEAN Dynamic);
VOID FspEventLogFinalize(BOOLEAN Dynamic);
VOID FspServiceFinalize(BOOLEAN Dynamic);
VOID fsp_fuse_finalize(BOOLEAN Dynamic);
VOID fsp_fuse_finalize_thread(VOID);

NTSTATUS FspFsctlRegister(VOID);
NTSTATUS FspFsctlUnregister(VOID);
NTSTATUS FspNpRegister(VOID);
NTSTATUS FspNpUnregister(VOID);
NTSTATUS FspEventLogRegister(VOID);
NTSTATUS FspEventLogUnregister(VOID);

PWSTR FspDiagIdent(VOID);

BOOL WINAPI FspServiceConsoleCtrlHandler(DWORD CtrlType);

#endif
