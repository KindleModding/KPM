sources = files(
    'internal_utils.c',
    'logging.c',
    'main.c'
)
include_dir = include_directories('./include')

executable('kpm', sources, include_directories: include_dir, dependencies: [kpm_dep])