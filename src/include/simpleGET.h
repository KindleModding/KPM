#pragma once

#include <curl/curl.h>

struct SimpleGETRequest
{
    CURL* curl; /**< The internal CURL object backing this request */
    long response_code; /**< The received response code */
    unsigned char* buffer; /**< The buffer of data from this request */
    size_t size; /**< The current size of the buffer */
    size_t max_size; /**< The maximum size of the response before the request is terminated */
};
typedef struct SimpleGETRequest SimpleGETRequest;

size_t SimpleGET_Callback(char* ptr, size_t size, size_t nmemb, void* userdata);

/**
 * @brief Create a SimpleGETRequest
 * 
 * @param url The URL to request
 * @return SimpleGETRequest* A pointer to the created SimpleGETRequest object
 */
SimpleGETRequest* SimpleGET_Initialise(const char* url);

/**
 * @brief Frees a SimpleGETRequest object
 * 
 * @param request The SimpleGetRequest object to free
 */
void SimpleGET_Cleanup(SimpleGETRequest* request);

/**
 * @brief Perform the request
 * 
 * @param request The SimpleGETRequest to perform
 * @return CURLcode The internal CURL result of the request, given by curl_easy_perform
 */
CURLcode SimpleGET_Perform(SimpleGETRequest* request);