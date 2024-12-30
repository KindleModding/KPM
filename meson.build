project('kpm', 'cpp', version : '1.0.0', license : 'GPL', default_options: ['cpp_std=c++17' ])

curl_dep = dependency('libcurl')
nlohmann_json_dep = dependency('nlohmann_json')
sqlitecpp_dep = dependency('sqlitecpp', static: true)

subdir('./stubs')
subdir('./src')