sources = files(
    './flags.cpp',
    './simpleGET.cpp',
    './utils.cpp',
    './repositories.cpp',
    './kpm.cpp',
    './database.cpp',
    './multiDownload.cpp'
)
include_dir = include_directories('./includes')

executable('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, nlohmann_json_dep, sqlitecpp_dep, lab126utils_dep, libarchive_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])