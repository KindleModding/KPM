#include <nlohmann/json.hpp>

#include "kpm.h"
#include "simpleGET.h"

/**
 * @brief List the repositories that KPM is using
 * 
 * @return std::vector<Repository> 
 */
std::vector<KPM::Repository> KPM::KPM::ListRepositories()
{
    std::vector<Repository> result;

    const std::string zSQL = "SELCT * FROM repositories;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(db, zSQL.c_str(), zSQL.size(), &statement, NULL);
    
    while (true)
    {
        if (sqlite3_step(statement) != SQLITE_ROW)
        {
            break;
        }

        result.push_back({
            .id = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 0))),
            .url = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 1))),
            .name = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 2))),
            .description = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 3)))
        });
    }

    sqlite3_finalize(statement);
    return result;
}

KPM::Repository KPM::KPM::GetRepository(const std::string& repositoryID)
{
    Repository result;

    const std::string zSQL = "SELCT * FROM repositories WHERE id=? LIMIT=1;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(db, zSQL.c_str(), zSQL.size(), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryID.c_str(), repositoryID.size(), SQLITE_STATIC);

    while (true)
    {
        if (sqlite3_step(statement) != SQLITE_ROW)
        {
            break;
        }

        result = {
            .id = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 0))),
            .url = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 1))),
            .name = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 2))),
            .description = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 3)))
        };
    }

    sqlite3_finalize(statement);
    return result;
}

KPM::Repository KPM::KPM::AddRepository(const std::string& url)
{
    SimpleGET request(url.c_str());
    request.Perform();
    request.GetBuffer();
    if (request.GetResponseCode() != 200)
    {
        throw Exceptions::HTTPError();
    }

    nlohmann::json json = nlohmann::json::parse(request);
    if (
        !json.contains("id") ||
        !json.contains("name") ||
        !json.contains("description") ||
        !json.contains("packages") ||
        json.at("id").type() != nlohmann::json::value_t::string ||
        json.at("name").type() != nlohmann::json::value_t::string ||
        json.at("description").type() != nlohmann::json::value_t::string ||
        json.at("packages").type() != nlohmann::json::value_t::array
    )
    {
        throw Exceptions::ManifestError();
    }

    const std::string zSQL = "INSERT INTO repositories (id, url, name, description) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(db, zSQL.c_str(), zSQL.size(), &statement, NULL);
    sqlite3_bind_text(statement, 1, json.at("id").get<std::string>().c_str(), json.at("id").size(), SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, url.c_str(), url.size(), SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, json.at("name").get<std::string>().c_str(), json.at("id").size(), SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, json.at("description").get<std::string>().c_str(), json.at("id").size(), SQLITE_STATIC);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        throw Exceptions::SQLiteError();
    }

    sqlite3_finalize(statement);
    return {
        .id = json.at("id").get<std::string>(),
        .url = url,
        .name = json.at("name").get<std::string>(),
        .description = json.at("description").get<std::string>()
    };
}


void KPM::KPM::RemoveRepository(Repository repository)
{
    const std::string zSQL = "DELETE FROM repositories WHERE id=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(db, zSQL.c_str(), zSQL.size(), &statement, NULL);
    sqlite3_bind_text(statement, 1, repository.id.c_str(), repository.id.size(), SQLITE_STATIC);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        throw Exceptions::SQLiteError();
    }

    sqlite3_finalize(statement);
}

std::vector<KPM::IndexedPackage> KPM::KPM::ListRepositoryPackages(Repository repository)
{
    std::vector<IndexedPackage> result;

    const std::string zSQL = "SELCT * FROM packages WHERE repository=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(db, zSQL.c_str(), zSQL.size(), &statement, NULL);
    sqlite3_bind_text(statement, 1, repository.id.c_str(), repository.id.size(), SQLITE_STATIC);

    while (true)
    {
        if (sqlite3_step(statement) != SQLITE_ROW)
        {
            break;
        }

        result.push_back({
            .repository = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 0))),
            .id = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 1))),
            .name = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 2))),
            .description = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 3))),
            .author = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 4))),
            .icon = std::string(reinterpret_cast<const char*> (sqlite3_column_text(statement, 5)))
        });
    }

    sqlite3_finalize(statement);
    return result;
}