/**
 * @file tlib/injected/curlfunc.h
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#ifndef TLIB_INJECTED_CURLFUNC_H_INCLUDED
#define TLIB_INJECTED_CURLFUNC_H_INCLUDED

#include <curl/curl.h>

#if defined(TLIB_INJECTIONS_ENABLED)
#define curl_easy_init()                tlib_curl_easy_init()
#define curl_multi_init()               tlib_curl_multi_init()
#define curl_multi_add_handle(mh, eh)   tlib_curl_multi_add_handle(mh, eh)
#define curl_multi_perform(mh, rh)      tlib_curl_multi_perform(mh, rh)
#endif

CURL *tlib_curl_easy_init(void);
CURLM *tlib_curl_multi_init(void);
CURLMcode tlib_curl_multi_add_handle(CURLM *mh, CURL *eh);
CURLMcode tlib_curl_multi_perform(CURLM *mh, int *running_handles);

#endif
