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
    int GetResponseCode();
private:
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    CURL* curl;
    char* buffer;
    size_t size;
};