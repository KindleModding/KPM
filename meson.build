project('kpm', 'cpp', version : '1.0.0', license : 'GPL', default_options: ['cpp_std=c++17' ])
add_global_arguments('-DKPM_ARCH="' + meson.get_external_property('kpm_arch', host_machine.cpu_family()) + '"', language: 'cpp')

curl_dep = dependency('libcurl')
nlohmann_json_dep = dependency('nlohmann_json')
sqlitecpp_dep = dependency('sqlitecpp', static: true)

subdir('./stubs')
subdir('./src')