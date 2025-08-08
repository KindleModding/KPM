#include "kpm.h"

/**
 * @brief List the repositories that KPM is using
 * 
 * @return std::vector<Repository> 
 */
std::vector<Repository> KPM::ListRepositories()
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