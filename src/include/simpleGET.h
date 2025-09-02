#pragma once

#include <curl/curl.h>

struct SimpleGETRequest
{
    CURL* curl;
    long response_code;
    char* buffer;
    size_t size;
};

size_t SimpleGET_Callback(char* ptr, size_t size, size_t nmemb, void* userdata);
void SimpleGET_Initialise(struct SimpleGETRequest* request, const char* url);
void SimpleGET_Cleanup(struct SimpleGETRequest* request);
CURLcode SimpleGET_Perform(struct SimpleGETRequest* request);
long SimpleGET_GetResponseCode(struct SimpleGETRequest* request);