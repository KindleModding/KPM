project('kpm', 'c', version : '1.0.0', license : 'GPL')
add_global_arguments('-DKPM_ARCH="' + meson.get_external_property('kpm_arch', host_machine.cpu_family()) + '"', language: 'c')

subproject('sqlite3', default_options: ['default_library=static']) # PW2 sqlite3 is unusable it straight up doesn't compile I need to fix this
subproject('cjson', default_options: ['tests=false', 'default_library=static'])
subproject('libarchive', default_options: ['default_library=static'])
curl = subproject('curl', default_options: ['tool=disabled', 'tests=disabled', 'unittests=disabled', 'default_library=static'])

# I know it seems ugly but we statically link EVERYTHING (trust)
# It builds on my trash laptop, it'll build on yours
sqlite3_dep = dependency('sqlite3', static: true)
curl_dep = curl.get_variable('curl_dep') # Get static libcurl
libcjson_dep = dependency('libcjson', static: true)
libarchive_dep = dependency('libarchive', static: true)

subdir('./stubs')
subdir('./src')
subdir('./tests')

if get_option('cli').enabled()
    subdir('./cli')
endif