/**
 * @file tlib/injected/curlfunc.c
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#include <tlib/injected/curlfunc.h>
#define TLIB_INJECTIONS_ENABLED
#include <tlib/injection.h>

#undef curl_easy_init
#undef curl_multi_init
#undef curl_multi_add_handle
#undef curl_multi_perform

CURL *tlib_curl_easy_init(void)
{
    TLIB_INJECT("curl_easy_init", return 0);
    return curl_easy_init();
}
CURLM *tlib_curl_multi_init(void)
{
    TLIB_INJECT("curl_multi_init", return 0);
    return curl_multi_init();
}
CURLMcode tlib_curl_multi_add_handle(CURLM *mh, CURL *eh)
{
    TLIB_INJECT("curl_multi_add_handle", return CURLM_INTERNAL_ERROR);
    return curl_multi_add_handle(mh, eh);
}
CURLMcode tlib_curl_multi_perform(CURLM *mh, int *running_handles)
{
    TLIB_INJECT("curl_multi_perform", return CURLM_INTERNAL_ERROR);
    return curl_multi_perform(mh, running_handles);
}
