#pragma once
#include <curl/curl.h>
#include <string>

class SimpleGET {
    public:
        SimpleGET(const std::string& url);
        CURLcode execute(void);
        std::string& get_buffer() { return this->buffer; };
        long get_response_code();
        ~SimpleGET();
    private:
        CURL* curlHandle;
        const std::string& url;
        std::string buffer;

        static size_t handleData(char* data, size_t size, size_t nmemb, void* clientp);
};