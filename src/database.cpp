#include "database.h"
#include "sqlite3.h"
#include <stdexcept>

Database::Database(std::string path) {
    const int returnCode = sqlite3_open(path.c_str(), &this->database);
    if (returnCode != SQLITE_OK) {
        printf("Failed to open local database.");
        sqlite3_close(this->database);
        std::runtime_error("Failed to open local database.");
    }
}

std::string* Database::GetRepositories() {
    sqlite3_stmt* pstmt;
    sqlite3_prepare(this->database, "SELECT * FROM repositories;", strlen("SELECT * FROM repositories;"), &pstmt, );

    return nullptr;
}

Database::~Database() {
    sqlite3_close(this->database);
}