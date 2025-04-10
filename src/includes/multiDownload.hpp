#pragma once
#include <curl/curl.h>
#include <filesystem>
#include <string>
#include <vector>

#define MAX_DOWNLOADS 10

struct DownloadError
{
    std::string url;
    std::string error;
    long http_code;
};

struct DownloadTarget
{
    std::string url;
    std::filesystem::path dest;
};

class MultiDownload
{
public:
    MultiDownload(std::vector<DownloadTarget> &targets);

    MultiDownload(MultiDownload &multiDownload) = delete;
    MultiDownload operator=(const MultiDownload &) = delete;

    bool execute(void);
    std::string &get_buffer() { return this->buffer; };
    long get_response_code();
    const std::vector<DownloadError> &get_errors() const { return errors; }
    ~MultiDownload();

private:
    CURL *curlMultiHandle;
    std::vector<DownloadTarget> &targets;
    std::vector<DownloadError> errors;
    std::string buffer;
    size_t completed = 0;
    int transfer;

    void addTransfer(DownloadTarget &downloadTarget);
    static size_t handleData(char *data, size_t size, size_t nmemb, void *clientp);
};

// Function to check if a package is already cached
bool isPackageCached(const DownloadTarget &target);