#include "repositories.h"
#include "request.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "log.h"

std::string Repositories::add(Database& db, const std::string& url) {
    Log::I("* Adding repository [%s]", url.c_str());

    const std::string repoID = updateRepository(db, url);
    if (repoID != "") {
        Log::D("* Registering repository with DB");
        try {
            db.AddRepository({
                .id = repoID,
                .url = url
            });
        } catch (std::exception& e) {
            Log::E("SQLite error: %s", e.what());
            return "";
        }
    }

    return repoID;
}

std::string Repositories::updateRepository(Database& db, const std::string& url) {
    Log::I("* Updating repository [%s]", url.c_str());
    Log::D("* Fetching repository manifest...");
    SimpleGET manifestRequest(url);
    const CURLcode error = manifestRequest.execute();
    if (error != 0) {
        Log::E("* CURL error: %s", curl_easy_strerror(error));
        return "";
    }

    nlohmann::json jsonData = nlohmann::json::parse(manifestRequest.get_buffer());
    // Add packages to DB
    Log::D("* Adding packages to DB");
    for (nlohmann::json package : jsonData["packages"]) {
        Log::D("Package found: %s", package["package_name"].get<std::string>().c_str());
    }


    return jsonData["id"].get<std::string>().c_str();
}