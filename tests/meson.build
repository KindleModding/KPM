run_command(
  'bash', '-c', 'cp -ar $MESON_SOURCE_ROOT/tests/resources $MESON_BUILD_ROOT/tests/'
)

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
    'KPM Online Repo Test',
    executable('kpm_online_repo_test', files('kpm_online_repo_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'KPM Local Repo Test',
    executable('kpm_local_repo_test', files('kpm_local_repo_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'KPM Package Test',
    executable('kpm_package_test', files('kpm_package_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)