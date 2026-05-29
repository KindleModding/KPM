sources = files(
    'artifacts.c',
    'callback.c',
    'dependencies.c',
    'device.c',
    'index.c',
    'install.c',
    'installed_packages.c',
    'kpm.c',
    'package.c',
    'repository.c',
    'simpleGET.c',
    'utils.c',
    'internal_utils.c'
)
include_dir = include_directories('./include')

kpm_lib = library('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, sqlite3_dep, libcjson_dep, libarchive_dep, lab126utils_dep])

kpm_dep = declare_dependency(link_with : kpm_lib, include_directories : './include/kpm')
meson.override_dependency('kpm', kpm_dep)