#pragma once

#include <curl/curl.h>

struct SimpleGETRequest
{
    CURL* curl;
    long response_code;
    char* buffer;
    size_t size;
};
typedef struct SimpleGETRequest SimpleGETRequest;

size_t SimpleGET_Callback(char* ptr, size_t size, size_t nmemb, void* userdata);
void SimpleGET_Initialise(SimpleGETRequest* request, const char* url);
void SimpleGET_Cleanup(SimpleGETRequest* request);
CURLcode SimpleGET_Perform(SimpleGETRequest* request);
long SimpleGET_GetResponseCode(SimpleGETRequest* request);