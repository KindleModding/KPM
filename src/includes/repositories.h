#pragma once
#include "database.h"
#include <string>

namespace Repositories {
    std::string add(Database& db, const std::string& url);
    std::string updateRepository(Database& db, const std::string& url);
}