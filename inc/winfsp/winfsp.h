/**
 * @file winfsp/winfsp.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_WINFSP_H_INCLUDED
#define WINFSP_WINFSP_H_INCLUDED

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <winternl.h>
#include <ntstatus.h>

#if defined(WINFSP_DLL_INTERNAL)
#define FSP_API                         __declspec(dllexport)
#else
#define FSP_API                         __declspec(dllimport)
#endif

FSP_API NTSTATUS FspNtStatusFromWin32(DWORD Error);
FSP_API VOID FspDebugLog(const char *format, ...);

#endif
