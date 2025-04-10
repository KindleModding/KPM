#include "multiDownload.hpp"
#include "log.hpp"
#include <cstddef>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

MultiDownload::MultiDownload(std::vector<DownloadTarget> &targets) : targets(targets)
{
    this->curlMultiHandle = curl_multi_init();
    curl_multi_setopt(curlMultiHandle, CURLMOPT_MAXCONNECTS, MAX_DOWNLOADS);

    // Add transfers
    for (transfer = 0; transfer < MAX_DOWNLOADS && transfer < targets.size(); transfer++)
    {
        addTransfer(targets[transfer]);
    }
}

void MultiDownload::addTransfer(DownloadTarget &downloadTarget)
{
    // Create parent directory if it doesn't exist
    try
    {
        std::filesystem::create_directories(downloadTarget.dest.parent_path());
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        Log::E("Failed to create directory for download: %s", e.what());
        errors.push_back({.url = downloadTarget.url,
                          .error = std::string("Failed to create directory: ") + e.what(),
                          .http_code = 0});
        return;
    }

    // Create a file for writing
    std::ofstream outFile(downloadTarget.dest, std::ios::binary | std::ios::trunc);
    if (!outFile)
    {
        Log::E("Failed to create output file: %s", downloadTarget.dest.c_str());
        errors.push_back({.url = downloadTarget.url,
                          .error = "Failed to create output file",
                          .http_code = 0});
        return;
    }
    outFile.close();

    CURL *eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, (void *)&(this->handleData));
    curl_easy_setopt(eh, CURLOPT_URL, downloadTarget.url.c_str());
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, &downloadTarget);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, &downloadTarget);
    curl_easy_setopt(eh, CURLOPT_USERAGENT, "kpm/1.0.0");
    curl_easy_setopt(eh, CURLOPT_BUFFERSIZE, 102400L);
    curl_easy_setopt(eh, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(eh, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(eh, CURLOPT_FTP_SKIP_PASV_IP, 1L);
    curl_easy_setopt(eh, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(eh, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(eh, CURLOPT_FAILONERROR, 1L);      // Consider HTTP errors as failures
    curl_easy_setopt(eh, CURLOPT_CONNECTTIMEOUT, 30L);  // 30 seconds connection timeout
    curl_easy_setopt(eh, CURLOPT_LOW_SPEED_TIME, 30L);  // If speed is below threshold for 30 seconds
    curl_easy_setopt(eh, CURLOPT_LOW_SPEED_LIMIT, 10L); // Below 10 bytes/sec is too slow
    curl_multi_add_handle(this->curlMultiHandle, eh);
}

bool MultiDownload::execute()
{
    int stillAlive = 1;

    // If we already have errors from addTransfer, don't proceed with the download
    if (!errors.empty())
    {
        return false;
    }

    errors.clear();

    while (completed < targets.size() && stillAlive)
    {
        curl_multi_perform(this->curlMultiHandle, &stillAlive);

        int msgsLeft = -1;
        CURLMsg *msg;
        while ((msg = curl_multi_info_read(curlMultiHandle, &msgsLeft)) != NULL)
        {
            if (msg->msg == CURLMSG_DONE)
            {
                CURL *eh = msg->easy_handle;
                CURLcode resultCode = msg->data.result;

                // Get the download target
                DownloadTarget *downloadTargetPointer;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &downloadTargetPointer);

                // Check if the transfer was successful
                if (resultCode != CURLE_OK)
                {
                    // Get the HTTP response code
                    long httpCode = 0;
                    curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &httpCode);

                    // Create an error record
                    errors.push_back({.url = downloadTargetPointer->url,
                                      .error = curl_easy_strerror(resultCode),
                                      .http_code = httpCode});

                    // Log the error
                    if (httpCode > 0)
                    {
                        Log::E("Download failed for %s: %s (HTTP %ld)",
                               downloadTargetPointer->url.c_str(),
                               curl_easy_strerror(resultCode),
                               httpCode);
                    }
                    else
                    {
                        Log::E("Download failed for %s: %s",
                               downloadTargetPointer->url.c_str(),
                               curl_easy_strerror(resultCode));
                    }

                    // Remove the partial download
                    try
                    {
                        if (std::filesystem::exists(downloadTargetPointer->dest))
                        {
                            std::filesystem::remove(downloadTargetPointer->dest);
                        }
                    }
                    catch (const std::filesystem::filesystem_error &e)
                    {
                        Log::W("Failed to remove partial download: %s", e.what());
                    }
                }
                else
                {
                    Log::D("Downloaded to %s (%s)", downloadTargetPointer->dest.c_str(), downloadTargetPointer->url.c_str());
                }

                curl_multi_remove_handle(curlMultiHandle, eh);
                curl_easy_cleanup(eh);
                completed++;
            }
            else
            {
                Log::E("libCURL error: %d", msg->msg);
                errors.push_back({.url = "unknown",
                                  .error = "Unknown libcurl error",
                                  .http_code = 0});
            }

            // Add the next transfer if any are left
            if (transfer < targets.size())
            {
                addTransfer(targets[transfer++]);
            }
        }

        // If we've got errors and all transfers are done, return failure
        if (!errors.empty() && completed >= targets.size())
        {
            return false;
        }

        // Add a timeout to prevent hanging when waiting for activity
        if (stillAlive)
        {
            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = -1;

            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);

            // Get file descriptors from the multi handle
            CURLMcode mc = curl_multi_fdset(curlMultiHandle, &fdread, &fdwrite, &fdexcep, &maxfd);
            if (mc != CURLM_OK)
            {
                Log::E("curl_multi_fdset() failed: %s", curl_multi_strerror(mc));
                return false;
            }

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; // 100ms timeout

            if (maxfd == -1)
            {
                // No file descriptors to wait for, just sleep a bit
                usleep(100000); // Sleep for 100ms
            }
            else
            {
                // Wait for activity on the file descriptors
                select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
            }
        }
    }

    return errors.empty();
}

long MultiDownload::get_response_code()
{
    long response_code;
    curl_easy_getinfo(curlMultiHandle, CURLINFO_RESPONSE_CODE, &response_code);
    return response_code;
}

size_t MultiDownload::handleData(char *data, size_t size, size_t nmemb, void *clientp)
{
    DownloadTarget *dt = (DownloadTarget *)clientp;
    std::ofstream file;
    file.open(dt->dest, std::ios::out | std::ios::app | std::ios::binary);
    if (!file)
    {
        // Return 0 to signal error to libcurl
        return 0;
    }
    file.write(data, size * nmemb);
    file.close();

    return size * nmemb;
}

bool isPackageCached(const DownloadTarget &target)
{
    // Check if the package file already exists
    if (!std::filesystem::exists(target.dest))
    {
        return false;
    }

    // Get file size to check if it's a valid file
    std::error_code ec;
    std::uintmax_t fileSize = std::filesystem::file_size(target.dest, ec);
    if (ec || fileSize == 0)
    {
        // File exists but is invalid (empty or can't determine size)
        return false;
    }

    // In the future we could do hash verification here
    // For now, we'll assume if the file exists and has content, it's valid

    return true;
}

MultiDownload::~MultiDownload()
{
    curl_multi_cleanup(this->curlMultiHandle);
}