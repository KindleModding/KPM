#include "repositories.hpp"
#include "database.hpp"
#include "request.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "log.hpp"

#define CURRENT_MANIFEST_VERSION 1

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

    const uint manifest_version = jsonData["version"].get<uint>();
    if (manifest_version > CURRENT_MANIFEST_VERSION) {
        Log::E("Repository manifest (%i) is newer than expected (%i)! Please update KPM.", manifest_version, CURRENT_MANIFEST_VERSION);
        return "";
    }

    if (jsonData["id"].get<std::string>().length() != 0) {
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

int Repositories::updateRepository(Database& db, const std::string& id) {
    Log::I("* Updating repository [%s]", id.c_str());
    Log::D("* Fetching repository manifest...");

    const Repository repo = db.GetRepository(id);

    SimpleGET manifestRequest(repo.url + "/manifest.json");
    const CURLcode error = manifestRequest.execute();
    if (error != 0) {
        Log::E("* CURL error: %s", curl_easy_strerror(error));
        return 0;
    }

    nlohmann::json jsonData = nlohmann::json::parse(manifestRequest.get_buffer());

    const uint manifest_version = jsonData["version"].get<uint>();
    if (manifest_version > CURRENT_MANIFEST_VERSION) {
        Log::E("Repository manifest (%i) is newer than expected (%i)!\nPlease update KPM.", manifest_version, CURRENT_MANIFEST_VERSION);
        exit(1);
    }

    Log::D("Clearing package index for this repo");
    db.DeleteRepositoryPackages(id);

    // Add packages to DB
    Log::D("* Adding packages to DB");
    int packages = 0;
    for (nlohmann::json package : jsonData["packages"]) {
        packages++;
        Log::D("Package found: %s", package["id"].get<std::string>().c_str());
        db.AddPackage({
            .id = package["id"].get<std::string>(),
            .alias = package["alias"].is_null() ? "" : package["alias"].get<std::string>(),
            .repo_id = repo.id,
            .name = package["name"],
            .description = package["description"],
            .screenshots = package["screenshots"].dump()
        });
        for (nlohmann::json version : package["versions"]) {
            for (std::string architecture : version["supported_arch"]) {
                db.AddPackageVersion({
                    .package_id = package["id"].get<std::string>(),
                    .repo_id = repo.id,
                    .version_number = version["version_number"].get<uint>(),
                    .version_name = version["version_name"].get<std::string>(),
                    .architecture = architecture,
                    .min_firmware = version["min_firmware"].get<std::string>(),
                    .max_firmware = version["max_firmware"].get<std::string>()
                });

                for (std::string dependencyString : version["dependencies"]) {
                    // Get the version number from the dependency info
                    db.AddPackageVersionDependency({
                        .package_id = package["id"].get<std::string>(),
                        .repo_id = repo.id,
                        .version_number = version["version_number"].get<uint>(),
                        .architecture = architecture,
                        .dependency_install_string = dependencyString
                    });
                }
            }
        }
    }

    return packages;
}

int Repositories::updateRepositories(Database &db) {
    const std::vector<Repository> repos = db.GetRepositories();
    int packageCount = 0;
    for (Repository repo : repos) {
        packageCount += updateRepository(db, repo.id);
    }
    return packageCount;
}