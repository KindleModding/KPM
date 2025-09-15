#include "simpleGET.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdlib.h>
#include <string.h>

void SimpleGET_Initialise(struct SimpleGETRequest* request, const char* url)
{
    request->buffer = NULL;
    request->size = 0;
    request->response_code = 0;
    request->curl = curl_easy_init();

    curl_easy_setopt(request->curl, CURLOPT_URL, url);
    curl_easy_setopt(request->curl, CURLOPT_WRITEFUNCTION, SimpleGET_Callback);
    curl_easy_setopt(request->curl, CURLOPT_WRITEDATA, (void *)request); // This _should_ work
    curl_easy_setopt(request->curl, CURLOPT_FOLLOWLOCATION, 1L); //@TODO: Wth?
    curl_easy_setopt(request->curl, CURLOPT_USERAGENT, "kpm/1.0.0");
    curl_easy_setopt(request->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(request->curl, CURLOPT_FTP_SKIP_PASV_IP, 1L);
    curl_easy_setopt(request->curl, CURLOPT_TCP_KEEPALIVE, 1L);
}

void SimpleGET_Cleanup(struct SimpleGETRequest *request)
{
    free(request->buffer);
    curl_easy_cleanup(request->curl);
}

CURLcode SimpleGET_Perform(struct SimpleGETRequest *request)
{
    CURLcode result = curl_easy_perform(request->curl);
    curl_easy_getinfo(request->curl, CURLINFO_RESPONSE_CODE, &request->response_code);
    return result;
}

size_t SimpleGET_Callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t realsize = size * nmemb;
    struct SimpleGETRequest* current = (struct SimpleGETRequest*) userdata;

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