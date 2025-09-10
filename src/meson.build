sources = files(
    'simpleGET.c',
    'kpm.c',
    'repository.c',
    'package.c',
    'index.c',
    'utils.c',
    'install.c',
    'callback.c',
    'artifacts.c'
)
include_dir = include_directories('./include')

kpm_lib = library('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, sqlite3_dep, libcjson_dep, libarchive_dep, lab126utils_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])

kpm_dep = declare_dependency(link_with : kpm_lib, include_directories : './include/kpm')
meson.override_dependency('kpm', kpm_dep)