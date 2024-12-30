#include "repositories.h"
#include "request.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

std::string Repositories::add(Database& db, const std::string& url) {
    printf("* Adding repository [%s]\n", url.c_str());

    const std::string repoID = updateRepository(db, url);
    if (repoID != "") {
        db.AddRepository(repoID, url);
    }

    return repoID;
}

std::string Repositories::updateRepository(Database& db, const std::string& url) {
    printf("* Updating repository [%s]\n", url.c_str());
    printf("* Fetching repository manifest...\n");
    SimpleGET manifestRequest(url);
    const CURLcode error = manifestRequest.execute();
    if (error != 0) {
        printf("* CURL error: %s\n", curl_easy_strerror(error));
        return "";
    }

    nlohmann::json jsonData = nlohmann::json::parse(manifestRequest.get_buffer());

    return jsonData["id"].get<std::string>().c_str();
}