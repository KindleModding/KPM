#pragma once
#include "database.hpp"
#include <string>

namespace Repositories {
    std::string add(Database& db, const std::string& url);
    int updateRepository(Database& db, const std::string& url);
    int updateRepositories(Database& db);
}