sources = files(
    'internal_utils.c',
    'io.c',
    'main.c'
)
include_dir = include_directories('./include')

KPM_PLATFORM = meson.get_external_property('kindle_platform', get_option('kindle_platform'))
if KPM_PLATFORM == ''
    KPM_PLATFORM = host_machine.cpu_family()
endif

subproject('fbink', default_options: ['bitmap=enabled', 'button-scan=disabled', 'fonts=disabled', 'image=disabled', 'inputlib=disabled', 'opentype=disabled', 'fbdepth=disabled', 'target=' + meson.get_external_property('target', 'Linux')])
fbink_dep = dependency('fbink')
executable('kpm', sources, include_directories: include_dir, dependencies: [kpm_dep, fbink_dep], build_rpath: kmc_lib_dir, install_rpath: kmc_lib_dir)