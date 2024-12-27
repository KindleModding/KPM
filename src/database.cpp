#include "database.h"
#include "sqlite3.h"
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>

Database::Database(std::string path) {
    const int returnCode = sqlite3_open(path.c_str(), &this->database);
    if (returnCode != SQLITE_OK) {
        printf("Failed to open local database.\n");
        sqlite3_close(this->database);
        throw std::runtime_error("Failed to open local database.");
    }

    // Check what tables the database has
    bool shouldCreateReposTable = true;
    bool shouldCreatePackageIndexTable = true;
    bool shouldCreateInstalledPackagesTable = true;

    const std::string checkTableQuery = "SELECT name FROM sqlite_schema WHERE type='table';";
    sqlite3_stmt* pstmt;
    sqlite3_prepare_v2(this->database, checkTableQuery.c_str(), checkTableQuery.length(), &pstmt, NULL);
    while (true) {
        const int result = sqlite3_step(pstmt);
        if (result == SQLITE_DONE) {
            break;
        } else if (result == SQLITE_ROW) {
            const unsigned char* tableName = sqlite3_column_text(pstmt, 0);
            if (memcmp(tableName, repoTableName.c_str(), repoTableName.length()) == 0) {
                shouldCreateReposTable = false;
            } else if (memcmp(tableName, packageIndexTableName.c_str(), packageIndexTableName.length()) == 0) {
                shouldCreatePackageIndexTable = false;
            } else if (memcmp(tableName, installedPackageTableName.c_str(), installedPackageTableName.length()) == 0) {
                shouldCreateInstalledPackagesTable = false;
            }
        }
    }
    sqlite3_finalize(pstmt);

    if (shouldCreateReposTable) {
        printf("Repos table not found - creating\n");
        mustRunQuery("CREATE TABLE " + repoTableName + " ("
                                                "id TEXT NOT NULL PRIMARY KEY,"
                                                "uri TEXT NOT NULL"
                                                ") STRICT;");
    }

    if (shouldCreatePackageIndexTable) {
        printf("Package index not found - creating\n");
        mustRunQuery("CREATE TABLE " + packageIndexTableName + " ("
                                                "id TEXT NOT NULL,"
                                                "repo_id TEXT NOT NULL,"
                                                "architecture TEXT NOT NULL,"
                                                "version_name TEXT NOT NULL,"
                                                "version_number INTEGER NOT NULL,"
                                                "min_firmware INTEGER NOT NULL,"
                                                "max_firmware INTEGER NOT NULL,"
                                                "screenshots TEXT NOT NULL,"
                                                "PRIMARY KEY(id, repo_id, architecture)"
                                                ") STRICT;");
    }

    if (shouldCreateInstalledPackagesTable) {
        printf("Installed package index not found - creating\n");
        mustRunQuery("CREATE TABLE " + installedPackageTableName + " ("
                                                "id TEXT NOT NULL,"
                                                "repo_id TEXT NOT NULL,"
                                                "architecture TEXT NOT NULL,"
                                                "version_name TEXT NOT NULL,"
                                                "version_number INTEGER NOT NULL,"
                                                "min_firmware INTEGER NOT NULL,"
                                                "max_firmware INTEGER NOT NULL,"
                                                "screenshots TEXT NOT NULL,"
                                                "PRIMARY KEY(id, repo_id, architecture)"
                                                ") STRICT;");
    }
}

void Database::mustRunQuery(const std::string& query) {
    sqlite3_stmt* pstmt;
    sqlite3_prepare_v2(this->database, query.c_str(), query.length(), &pstmt, NULL);
    const int result = sqlite3_step(pstmt);
    if (result != SQLITE_DONE) {
        printf("Error in query - [%s] (%d)\n", sqlite3_errmsg(this->database), result);
        throw std::runtime_error("Query error.");
    }
    sqlite3_finalize(pstmt);
}

std::string* Database::GetRepositories() {
    sqlite3_stmt* pstmt;
    std::vector<Repository> repos;
    const std::string query = "SELECT * FROM " + repoTableName + ";";
    sqlite3_prepare_v2(this->database, query.c_str(), query.length(), &pstmt, NULL);

    return nullptr;
}

Database::~Database() {
    sqlite3_close(this->database);
}