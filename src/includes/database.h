#pragma once
#include <string>
#include <sqlite3.h>

class Database {
    public:
        Database(std::string path);
        ~Database();

        std::string* GetRepositories();
    private:
        sqlite3* database;
};