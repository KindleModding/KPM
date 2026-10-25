#pragma once

#include <curl/curl.h>

typedef struct
{
    CURL* curl; /**< The internal CURL object backing this request */
    long response_code; /**< The received response code */
    unsigned char* buffer; /**< The buffer of data from this request */
    size_t size; /**< The current size of the buffer */
    size_t max_size; /**< The maximum size of the response before the request is terminated */
    size_t (*callback)(void* ptr, size_t size, unsigned char* buffer); /**< Callback function! */
    void* ptr; /**< Additional data to provide in the callback */
} SimpleGETRequest;

size_t SimpleGET_Callback(char* ptr, size_t size, size_t nmemb, void* userdata);

/**
 * @brief Create a SimpleGETRequest
 * 
 * @param url The URL to request
 * @return SimpleGETRequest* A pointer to the created SimpleGETRequest object
 */
SimpleGETRequest* SimpleGET_Initialise(const char* url);

void SimpleGET_SetMaxSize(SimpleGETRequest* request, size_t max_size);
void SimpleGET_SetCallback(SimpleGETRequest* request, size_t (*callback)(void* ptr, size_t size, unsigned char* buffer));

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