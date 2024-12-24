project('kpm', 'cpp', version : '1.0.0', license : 'GPL', default_options: [  ])

curl_dep = dependency('libcurl')
nlohmann_json_dep = dependency('nlohmann_json')

sources = files('./src/kpm.cpp')
include_dir = include_directories('./src/includes')

executable('kpm', sources, include_directories: include_dir, dependencies: [curl_dep, nlohmann_json_dep])