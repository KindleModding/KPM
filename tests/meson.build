db_test = executable('db_test', 'db_test.c', dependencies: [kpm_dep, sqlite3_dep])
repository_test = executable('repository_test', 'repository_test.c', dependencies: [kpm_dep])


test('Database Test', db_test, is_parallel: false)
test('Repository Test', repository_test, is_parallel: false)