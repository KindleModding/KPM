#include "repositories.h"
#include "request.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "log.h"

std::string Repositories::add(Database& db, const std::string& url) {
    Log::I("* Adding repository [%s]", url.c_str());

    // Get the repository ID
    SimpleGET manifestRequest(url + "/manifest.json");
    const CURLcode error = manifestRequest.execute();
    if (error != 0) {
        Log::E("* CURL error: %s", curl_easy_strerror(error));
        return "";
    }

    nlohmann::json jsonData = nlohmann::json::parse(manifestRequest.get_buffer());

    if (jsonData["id"].get<std::string>() != "") {
        Log::D("* Registering repository with DB");
        try {
            db.AddRepository({
                .id = jsonData["id"].get<std::string>(),
                .url = url
            });
        } catch (std::exception& e) {
            Log::E("SQLite error: %s", e.what());
            return "";
        }
    }

    return jsonData["id"].get<std::string>();
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
        Log::D("Package found: %s", package["id"].get<std::string>().c_str());
        for (nlohmann::json version : package["versions"]) {
            db.AddPackage({
                .id = package["id"].get<std::string>(),
            });
        }
    }


    return jsonData["id"].get<std::string>().c_str();
}