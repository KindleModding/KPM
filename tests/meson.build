# tests/meson.build
catch2_dep = dependency('catch2', fallback: ['catch2', 'catch2_with_main_dep'])

# Include the project sources
kpm_includes = include_directories('../src/includes', '../stubs/lab126utils')

# Common dependencies for all tests
test_deps = [
  catch2_dep,
  sqlitecpp_dep,
  sqlite3_dep,
  nlohmann_json_dep,
  curl_dep,
  libarchive_dep,
  lab126utils_dep
]

# Source files needed from main project (excluding main.cpp)
common_sources = [
  '../src/database.cpp',
  '../src/utils.cpp',
  '../src/repositories.cpp',
  '../src/flags.cpp',
  '../src/simpleGET.cpp'
]

# Test executables
database_test = executable('database_test',
  ['database_test.cpp'] + common_sources,
  include_directories: kpm_includes,
  dependencies: test_deps)

utils_test = executable('utils_test',
  ['utils_test.cpp', '../src/utils.cpp', '../src/flags.cpp', '../src/database.cpp', '../src/repositories.cpp', '../src/simpleGET.cpp', '../src/multiDownload.cpp'],
  include_directories: kpm_includes,
  dependencies: test_deps)

multidownload_test = executable('multidownload_test',
  ['multiDownload_test.cpp', '../src/multiDownload.cpp', '../src/simpleGET.cpp', '../src/flags.cpp'],
  include_directories: kpm_includes,
  dependencies: test_deps)

# Register with Meson's test system
test('database_tests', database_test)
test('utils_tests', utils_test)
test('multidownload_tests', multidownload_test)
