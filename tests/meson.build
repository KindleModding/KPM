all_test_files = files(
    'common/kpm_test_io.c',
    'common/kpm_test_common.c'
)

all_test_includes = include_directories(
    'common',
    '../src/include/'
)

test(
    'KPM Init Test',
    executable('kpm_init_test', files('kpm_init_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'KPM Repo Test',
    executable('kpm_REPO_test', files('kpm_repo_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)