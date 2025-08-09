#include "simpleGET.hpp"
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>

SimpleGET::SimpleGET(const char* url)
{
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, this->write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)this); // This _should_ work
}

SimpleGET::~SimpleGET()
{
    free(buffer);
    curl_easy_cleanup(curl);
}

CURLcode SimpleGET::Perform()
{
    return curl_easy_perform(curl);
}

char* SimpleGET::GetBuffer()
{
    return buffer;
}

size_t SimpleGET::GetSize()
{
    return size;
}

long SimpleGET::GetResponseCode()
{
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    return response_code;
}

size_t SimpleGET::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t realsize = size * nmemb;
    SimpleGET* current = static_cast<SimpleGET*>(userdata);

    char* newBuffer = (char*) realloc(current->buffer, current->size + realsize + 1);
    if (!newBuffer)
    {
        return 0; // OOM
    }

    current->buffer = newBuffer;
    memcpy(&(current->buffer[current->size]), ptr, realsize);
    current->size += realsize;
    current->buffer[current->size] = 0;

    return realsize;
}