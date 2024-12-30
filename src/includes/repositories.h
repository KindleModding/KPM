#pragma once
#include "database.h"
#include <string>

namespace Repositories {
    std::string add(Database& db, const std::string& url);
    int updateRepository(Database& db, const std::string& url);
    int updateRepositories(Database& db);
}