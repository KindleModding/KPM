#include "request.h"
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>
#include <string>


SimpleGET::SimpleGET(const std::string& url): url(url) {
    this->curlHandle = curl_easy_init();
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, (void*)&(this->handleData));
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, (void *)&this->buffer);
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "kpm/1.0.0");
    curl_easy_setopt(curlHandle, CURLOPT_BUFFERSIZE, 102400L);
    curl_easy_setopt(curlHandle, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curlHandle, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curlHandle, CURLOPT_FTP_SKIP_PASV_IP, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
};

CURLcode SimpleGET::execute() {
    return curl_easy_perform(this->curlHandle);
}

long SimpleGET::get_response_code() {
    long response_code;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &response_code);
    return response_code;
}

size_t SimpleGET::handleData(char* data, size_t size, size_t nmemb, void* clientp) {
    std::string* buffer = static_cast<std::string*>(clientp);
    const size_t realsize = size * nmemb;
    const size_t currentBufferLength = buffer->length();

    buffer->resize(currentBufferLength + realsize);
    std::memcpy(&(buffer->data()[currentBufferLength]), data, realsize);

    return buffer->length();
}

SimpleGET::~SimpleGET() {
    curl_easy_cleanup(this->curlHandle);
}