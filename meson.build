project('kpm', 'cpp', version : '1.0.0', license : 'GPL', default_options: ['cpp_std=c++17' ])
add_global_arguments('-DKPM_ARCH="' + meson.get_external_property('kpm_arch', host_machine.cpu_family()) + '"', language: 'cpp')

subproject('sqlite3') # PW2 sqlite3 is unusable it straight up doesn't compile I need to fix this
subproject('nlohmann_json')
subproject('libarchive')

# I know it seems ugly but we statically link EVERYTHING (trust)
# It builds on my trash laptop, it'll build on yours
sqlite3_dep = dependency('sqlite3', static: true)
curl_dep = dependency('libcurl', static: false) # Annoyingly libcurl cannot be static (we should look into this)
nlohmann_json_dep = dependency('nlohmann_json', static: true)
libarchive_dep = dependency('libarchive', static: true)

subdir('./stubs')
subdir('./src')
#subdir('./tests')