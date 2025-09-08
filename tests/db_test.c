#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "kpm.h"
#include "sqlite3.h"

int main()
{
    struct KPM kpm;
    KPM_Initialise(&kpm, "./repo_test.db");
    KPM_Cleanup(&kpm);

    // Check this db
    fprintf(stderr, "Opening database\n");
    sqlite3* db;
    sqlite3_open("./repo_test.db", &db);

    fprintf(stderr, "Checking tables\n");
    const char* statementString = "SELECT name FROM sqlite_schema WHERE type='table';";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(db, statementString, strlen(statementString), &statement, NULL);
    
    char* tables[] = {
        "repositories",
        "packages",
        "artifacts",
        "artifact_dependencies",
        "installed_packages",
        "dependencies"
    };
    const size_t tableCount = 6;

    size_t found = 0;
    while (sqlite3_step(statement) != SQLITE_DONE)
    {
        for (int i=0; i < tableCount; i++)
        {
            if (strcmp(tables[i], (const char*) sqlite3_column_text(statement, 0)) == 0)
            {
                fprintf(stderr, "Found table: %s\n", tables[i]);
                found++;
                break;
            }
        }
    }

    assert(found == tableCount);    

    sqlite3_finalize(statement);
    sqlite3_close(db);

    return 0;
}