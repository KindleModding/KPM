sources = files(
    'internal_utils.c',
    'logging.c',
    'main.c'
)
include_dir = include_directories('./include')

subproject('fbink', default_options: ['default_library=static', 'bitmap=disabled', 'button-scan=disabled', 'fonts=disabled', 'image=disabled', 'inputlib=disabled', 'opentype=disabled'])
fbink_dep = dependency('fbink')
executable('kpm', sources, include_directories: include_dir, dependencies: [kpm_dep, fbink_dep])