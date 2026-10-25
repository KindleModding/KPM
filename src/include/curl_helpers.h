#ifndef CURL_HELPERS_H
#define CURL_HELPERS_H

#define KPM_SET_CURL_OPTIONS(curl) \
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); \
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "kpm/1.0.0"); \
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS); \
    curl_easy_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, 1L); \
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

#endif