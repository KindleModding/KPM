sources = files(
    './repository.cpp',
    './simpleGET.cpp',
)
include_dir = include_directories('./include')

#executable('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, nlohmann_json_dep, sqlitecpp_dep, lab126utils_dep, libarchive_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])
library('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, sqlite3_dep, nlohmann_json_dep, libarchive_dep, lab126utils_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])