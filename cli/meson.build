sources = files(
    'internal_utils.c',
    'io.c',
    'main.c'
)
include_dir = include_directories('./include')

subproject('fbink', default_options: ['bitmap=enabled', 'button-scan=disabled', 'fonts=disabled', 'image=disabled', 'inputlib=disabled', 'opentype=disabled', 'fbdepth=disabled'])
fbink_dep = dependency('fbink')
executable('kpm', sources, include_directories: include_dir, dependencies: [kpm_dep, fbink_dep], build_rpath: kmc_lib_dir, install_rpath: kmc_lib_dir)