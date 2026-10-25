#include <curl/curl.h>
#include <curl/easy.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "simpleGET.h"
#include "curl_helpers.h"

SimpleGETRequest* SimpleGET_Initialise(const char* url)
{
    SimpleGETRequest* request = malloc(sizeof(SimpleGETRequest));
    request->callback = NULL;
    request->ptr = NULL;
    request->buffer = NULL;
    request->size = 0;
    request->response_code = 0;
    request->curl = curl_easy_init();
    request->max_size = SIZE_MAX;

    curl_easy_setopt(request->curl, CURLOPT_URL, url);
    curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, SimpleGET_Callback);
    curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, (void *)request); // This _should_ work
    KPM_SET_CURL_OPTIONS(request->curl);

    return request;
}

void SimpleGET_SetMaxSize(SimpleGETRequest* request, size_t max_size)
{
    request->max_size = max_size;
}

void SimpleGET_SetCallback(SimpleGETRequest* request, size_t (*callback)(void* ptr, size_t size, unsigned char* buffer))
{
    request->callback = callback;
}

void SimpleGET_Cleanup(SimpleGETRequest *request)
{
    free(request->buffer);
    curl_easy_cleanup(request->curl);
    free(request);
}

CURLcode SimpleGET_Perform(SimpleGETRequest *request)
{
    CURLcode result = curl_easy_perform(request->curl);
    curl_easy_getinfo(request->curl, CURLINFO_RESPONSE_CODE, &request->response_code);
    return result;
}

size_t SimpleGET_Callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t realsize = size * nmemb;
    SimpleGETRequest* current = (SimpleGETRequest*) userdata;

    unsigned char* newBuffer = (unsigned char*) realloc(current->buffer, current->size + realsize + 1);
    if (!newBuffer)
        return CURL_WRITEFUNC_ERROR; // OOM

    current->buffer = newBuffer;
    memcpy(&(current->buffer[current->size]), ptr, realsize);
    current->size += realsize;
    current->buffer[current->size] = 0;

    if (current->size > current->max_size)
        return CURL_WRITEFUNC_ERROR;

    if (current->callback)
        return current->callback(current->ptr, current->size, current->buffer);

    return realsize;
}