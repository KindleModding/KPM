project('kpm', 'c', version : '1.0.0', license : 'GPL', default_options: ['b_ndebug=if-release'])

KPM_PLATFORM = meson.get_external_property('kindle_platform', get_option('kindle_platform'))
if KPM_PLATFORM == ''
    KPM_PLATFORM = host_machine.cpu_family()
endif

add_global_arguments('-DKPM_PLATFORM="' + KPM_PLATFORM + '"', language: 'c')

kmc_lib_dir = '/var/local/kmc/lib'

subproject('sqlite3', default_options: ['default_library=static']) # PW2 sqlite3 is unusable it straight up doesn't compile I need to fix this
subproject('cjson', default_options: ['tests=false', 'default_library=static'])
subproject('libarchive', default_options: ['default_library=static', 'openssl=disabled'])
curl = subproject('curl', default_options: ['tool=disabled', 'tests=disabled', 'unittests=disabled', 'default_library=static', 'brotli=disabled']) # @TODO: Check if brotli is needed or not

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

summary(
  {
    'arch': host_machine.cpu_family(),
    'kpm_platform': KPM_PLATFORM,
    'cli': get_option('cli').enabled()
  },
  bool_yn: true,
)