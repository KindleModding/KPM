#include "includes/curl_compat.h"
#include <sys/select.h>
#include <sys/time.h>

#if LIBCURL_VERSION_NUM < 0x071C00 // libcurl < 7.28.0
CURLMcode curl_multi_wait(CURLM *multi_handle,
                          struct curl_waitfd extra_fds[],
                          unsigned int extra_nfds,
                          int timeout_ms,
                          int *numfds)
{
  fd_set fdread;
  fd_set fdwrite;
  fd_set fdexcep;
  int maxfd = -1;

  FD_ZERO(&fdread);
  FD_ZERO(&fdwrite);
  FD_ZERO(&fdexcep);

  CURLMcode mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

  if (mc != CURLM_OK)
  {
    return mc;
  }

  if (maxfd == -1)
  {
    // No file descriptors to wait for, sleep a bit
    struct timeval wait = {0, timeout_ms * 1000};
    select(0, NULL, NULL, NULL, &wait);
    *numfds = 0;
    return CURLM_OK;
  }

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int ret = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &tv);
  *numfds = ret;

  return CURLM_OK;
}
#endif