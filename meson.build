project('kpm', 'cpp', version : '1.0.0', license : 'GPL', default_options: ['cpp_std=c++17' ])
add_global_arguments('-DKPM_ARCH="' + meson.get_external_property('kpm_arch', host_machine.cpu_family()) + '"', language: 'cpp')

subproject('libarchive')
subproject('nlohmann_json')
subproject('sqlitecpp', default_options: ['SQLITE_OMIT_LOAD_EXTENSION=true'])

curl_dep = dependency('libcurl')
nlohmann_json_dep = dependency('nlohmann_json', static: true)
sqlitecpp_dep = dependency('sqlitecpp', static: false)
libarchive_dep = dependency('libarchive', static: true)

subdir('./stubs')
subdir('./src')