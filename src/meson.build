sources = files(
    './flags.cpp',
    './simpleGET.cpp',
    './utils.cpp',
    './repositories.cpp',
    './kpm.cpp',
    './database.cpp',
    './multiDownload.cpp'
)

# Add curl compatibility code for older libcurl versions
if get_option('curl_compat')
    sources += files('./curl_compat.cpp')
endif

include_dir = include_directories('./includes')

# Add ABI compatibility warning suppression for GCC < 7.1
cpp = meson.get_compiler('cpp')
if cpp.get_id() == 'gcc' and cpp.version().version_compare('<7.1')
    add_project_arguments('-Wno-psabi', language: 'cpp')
endif

executable('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, nlohmann_json_dep, sqlitecpp_dep, lab126utils_dep, libarchive_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])