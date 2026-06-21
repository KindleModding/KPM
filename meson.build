project('kpm', 'c', version : '1.0.0', license : 'GPL', default_options: ['b_ndebug=if-release', 'c_std=gnu23'])

KPM_PLATFORM = meson.get_external_property('kindle_platform', get_option('kindle_platform'))
if KPM_PLATFORM == ''
    KPM_PLATFORM = host_machine.cpu_family()
endif

add_global_arguments('-DKPM_PLATFORM="' + KPM_PLATFORM + '"', language: 'c')
add_global_arguments('-DKPM_DB_PATH="' + get_option('db_path') + '"', language: 'c')
add_global_arguments('-DKPM_PKG_PATH="' + get_option('pkg_path') + '"', language: 'c')

kmc_lib_dir = '/var/local/kmc/lib'

subproject('sqlite3', default_options: ['default_library=static']) # PW2 sqlite3 is unusable it straight up doesn't compile I need to fix this
subproject('cjson', default_options: ['tests=false', 'default_library=static'])
subproject('libarchive', default_options: [
  'default_library=static',
  'openssl=disabled',
  'zlib=disabled',
  'bz2lib=disabled',
  'libb2=disabled',
  'iconv=disabled',
  'lz4=disabled',
  'zstd=disabled',
  'lzma=enabled',
  'lzo2=disabled',
  'cng=disabled',
  'xml2=disabled',
  'expat=disabled',
  'regex=disabled',
  'xattr=disabled',
  'acl=disabled',

])
subproject('google-brotli', default_options: ['default_library=static']) # Needed for cURL brotli support
subproject('openssl', default_options: ['default_library=static', 'build_cli=false']) # Needed for cURL HTTPS support
subproject('nghttp2', default_options: ['default_library=static']) # Needed for cURL HTTP/2 support
curl = subproject('curl', default_options: [
  'tool=disabled',
  'tests=disabled',
  'unittests=disabled',
  'default_library=static',
  'cookies=disabled',
  'doh=disabled',
  'form-api=disabled',
  'gsasl=disabled',
  'progress-meter=disabled',
  'websockets=disabled',
  'aws=disabled',
  'basic-auth=disabled',
  'bearer-auth=disabled',
  'digest-auth=disabled',
  'kerberos-auth=disabled',
  'negotiate-auth=disabled',
  'dict=disabled',
  'gopher=disabled',
  'imap=disabled',
  'ipfs=disabled',
  'ldap=disabled',
  'ldaps=disabled',
  'mqtt=disabled',
  'pop3=disabled',
  'rtmp=disabled',
  'rtsp=disabled',
  'smb=disabled',
  'smtp=disabled',
  'telnet=disabled',
  'ntlm=disabled',
  'ssh=disabled'
])

# I know it seems ugly but we statically link EVERYTHING (trust)
# It builds on my trash laptop, it'll build on yours
sqlite3_dep = dependency('sqlite3', static: true, default_options: [
  'default_library=static',
  'fts34=disabled',
  'fts5=disabled',
  'geopoly=disabled',
  'rbu=disabled',
  'rtree=disabled',
  'session=disabled'
])
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
    'cli': get_option('cli').enabled(),
    'db_path': get_option('db_path'),
    'pkg_path': get_option('pkg_path'),
  },
  bool_yn: true,
)