#include "repositories.hpp"
#include "database.hpp"
#include "simpleGET.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "log.hpp"
#include "utils.hpp"

#define CURRENT_MANIFEST_VERSION 1

std::string Repositories::add(Database& db, const std::string& url) {
    /**
     * @brief Add a repository to the index from its URL
     * 
     */
    db.Begin();
    Log::I("* Adding repository [%s]", url.c_str());

    // Get the repository ID
    SimpleGET manifestRequest(url + "/manifest.json");
    const CURLcode error = manifestRequest.execute();
    if (error != 0) {
        Log::E("* CURL error: %s", curl_easy_strerror(error));
        db.End(true);
        return "";
    }

    nlohmann::json jsonData = nlohmann::json::parse(manifestRequest.get_buffer());

    const uint manifest_version = jsonData["version"].get<uint>();
    if (manifest_version > CURRENT_MANIFEST_VERSION) {
        Log::E("Repository manifest (%i) is newer than expected (%i)! Please update KPM.", manifest_version, CURRENT_MANIFEST_VERSION);
        db.End(true);
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
            db.End(true);
            return "";
        }
    }

    db.End();
    return jsonData["id"].get<std::string>();
}

int Repositories::updateRepository(Database& db, const std::string& id) {
    Log::I("* Updating repository [%s]", id.c_str());
    Log::D("* Fetching repository manifest...");
    db.Begin();

    const Repository repo = db.GetRepository(id);

    SimpleGET manifestRequest(repo.url + "/manifest.json");
    const CURLcode error = manifestRequest.execute();
    if (error != 0) {
        Log::E("* CURL error: %s", curl_easy_strerror(error));
        db.End(true);
        return 0;
    }

    nlohmann::json jsonData = nlohmann::json::parse(manifestRequest.get_buffer());

    const uint manifest_version = jsonData["version"].get<uint>();
    if (manifest_version > CURRENT_MANIFEST_VERSION) {
        Log::E("Repository manifest (%i) is newer than expected (%i)!\nPlease update KPM.", manifest_version, CURRENT_MANIFEST_VERSION);
        db.End(true);
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
            .id = package["id"],
            .alias = package["alias"].is_null() ? "" : package["alias"].get<std::string>(),
            .repository_id = repo.id,
            .display_name = package["display_name"],
            .description = package["description"],
            .screenshots = package["screenshots"]
        });
        for (nlohmann::json version : package["versions"]) {
            db.AddPackageVersion({
                .package_id = package["id"],
                .repository_id = repo.id,
                .version_name = version["version_name"],
                .version_number = version["version_number"],
                .architecture = version["supported_arch"],
                .min_firmware = version["min_firmware"],
                .max_firmware = version["max_firmware"]
            });

            for (std::string dependencyString : version["dependencies"]) {
                ParsedPackageTarget parsedTarget = parsePackageTarget(dependencyString);
                // Get the version number from the dependency info
                db.AddPackageDependency({
                    .dependent_package_id = package["id"],
                    .dependent_repository_id = repo.id,
                    .dependent_version_number = version["version_number"],
                    .dependent_architecture = version["supported_arch"],
                    .repository_id = parsedTarget.repository_id,
                    .package_name = parsedTarget.package_name,
                    .version_name = parsedTarget.version_name,
                    .version_comparison_type = parsedTarget.version_comparison_type
                });
            }
        }
    }

    db.End();
    return packages;
}

int Repositories::updateRepositories(Database &db) {
    const std::vector<Repository> repositories = db.GetRepositories();
    int packageCount = 0;
    for (Repository repo : repositories) {
        packageCount += updateRepository(db, repo.id);
    }
    return packageCount;
}