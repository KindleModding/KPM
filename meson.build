project('kpm', 'cpp', version : '1.0.0', license : 'GPL', default_options: ['cpp_std=c++17' ])

curl_dep = dependency('libcurl')
nlohmann_json_dep = dependency('nlohmann_json')
sqlitecpp_dep = dependency('sqlitecpp')

sources = files(
    './src/kpm.cpp',
    './src/database.cpp'
)
include_dir = include_directories('./src/includes')

executable('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, nlohmann_json_dep, sqlitecpp_dep], cpp_args: ['-static-libstdc++'], link_args: ['-static-libstdc++'])