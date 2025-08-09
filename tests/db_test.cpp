#include <cassert>
#include <string>

#include "kpm.hpp"
#include "sqlite3.h"

int main()
{
    const std::string dbPath = "./repo_test.db";
    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }
    
    // Trigger db creation
    KPM::KPM kpm(dbPath);

    // Check this db
    sqlite3* db;
    sqlite3_open(dbPath.c_str(), &db);

    const std::string statementString = "SELECT name FROM sqlite_schema WHERE type='table';";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(db, statementString.c_str(), statementString.size(), &statement, NULL);
    
    std::vector<std::string> tables = {
        "repositories",
        "packages",
        "artifacts",
        "artifact_dependencies",
        "installed_packages",
        "dependencies"
    };

    size_t found = 0;
    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        for (const std::string& table : tables)
        {
            if (table == std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0))))
            {
                found++;
                break;
            }
        }
    }

    assert(found == tables.size());

    return 0;
}