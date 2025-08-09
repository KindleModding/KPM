sources = files(
    './kpm.cpp',
    './repository.cpp',
    './simpleGET.cpp',
)
include_dir = include_directories('./include')

#executable('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, nlohmann_json_dep, sqlitecpp_dep, lab126utils_dep, libarchive_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])
kpm_lib = library('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, sqlite3_dep, nlohmann_json_dep, libarchive_dep, lab126utils_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])

kpm_dep = declare_dependency(link_with : kpm_lib, include_directories : './include/kpm')
meson.override_dependency('kpm', kpm_dep)