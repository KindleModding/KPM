#pragma once

#include <curl/curl.h>

class SimpleGET
{
public:
    SimpleGET(const char* url);
    ~SimpleGET();

    CURLcode Perform();
    char* GetBuffer();
    size_t GetSize();
    long GetResponseCode();
private:
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    CURL* curl;
    char* buffer = NULL;
    size_t size;
};