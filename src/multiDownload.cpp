#include "multiDownload.hpp"
#include "log.hpp"
#include <cstddef>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <fstream>
#include <string>

MultiDownload::MultiDownload(std::vector<DownloadTarget>& targets): targets(targets) {
    this->curlMultiHandle = curl_multi_init();
    curl_multi_setopt(curlMultiHandle, CURLMOPT_MAXCONNECTS, MAX_DOWNLOADS); // Download 10 at a time!

    // Add transfers
    for (transfer=0; transfer < MAX_DOWNLOADS && transfer < targets.size(); transfer++) {
        addTransfer(targets[transfer]);
    }
};

void MultiDownload::addTransfer(DownloadTarget& downloadTarget) {
    CURL *eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, (void*)&(this->handleData));
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
    curl_multi_add_handle(this->curlMultiHandle, eh);
}

bool MultiDownload::execute() {
    int stillAlive = 1;

    while (completed < targets.size()) {
        Log::I("Performing curl...");
        curl_multi_perform(this->curlMultiHandle, &stillAlive);

        Log::I("Alr now im reading");
        int msgsLeft = -1;
        CURLMsg* msg;
        while ((msg = curl_multi_info_read(curlMultiHandle, &msgsLeft)) != NULL) {
            if(msg->msg == CURLMSG_DONE) {
                CURL *eh = msg->easy_handle;
                curl_multi_remove_handle(curlMultiHandle, eh);
                curl_easy_cleanup(eh);

                if (msg->data.result != 0) {
                    Log::E("%d - %s\n", msg->data.result, curl_easy_strerror(msg->data.result));
                    return false;
                }

                DownloadTarget* downloadTargetPointer;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &downloadTargetPointer);
                Log::D("Downloaded to %s (%s)", downloadTargetPointer->dest.c_str(), downloadTargetPointer->url.c_str());
                completed++;
            } else {
                Log::E("libCURL error: %d", msg->msg);
                return false;
            }
            
            if (transfer < targets.size()) {
                addTransfer(targets[transfer++]);
            }
        }
    }

    return true;
}

long MultiDownload::get_response_code() {
    long response_code;
    curl_easy_getinfo(curlMultiHandle, CURLINFO_RESPONSE_CODE, &response_code);
    return response_code;
}

size_t MultiDownload::handleData(char* data, size_t size, size_t nmemb, void* clientp) {
    DownloadTarget* dt = (DownloadTarget*)clientp;
    std::ofstream file;
    file.open((dt->dest), std::ios::out | std::ios::ate | std::ios::binary);
    file.write(data, size * nmemb);
    file.close();

    return size * nmemb;
}

MultiDownload::~MultiDownload() {
    curl_multi_cleanup(this->curlMultiHandle);
}