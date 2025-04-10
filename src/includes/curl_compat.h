#pragma once

#include <curl/curl.h>

// Check if curl_multi_wait is available
#if LIBCURL_VERSION_NUM < 0x071C00 // libcurl < 7.28.0
// Provide a fallback implementation
CURLMcode curl_multi_wait(CURLM *multi_handle,
                          struct curl_waitfd extra_fds[],
                          unsigned int extra_nfds,
                          int timeout_ms,
                          int *numfds);
#endif