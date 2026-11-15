python_exe = find_program('python3', 'python')
run_command(
  'bash', '-c', 'cp -ar $MESON_SOURCE_ROOT/tests/resources $MESON_BUILD_ROOT/tests/',
  python_exe, '$MESON_BUILD_ROOT/tests/resources/gen_resources.py',
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
    'SemVer Test',
    executable('semver_test', files('semver_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'KPM Init Test',
    executable('kpm_init_test', files('kpm_init_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'KPM Add Repository Test',
    executable('kpm_add_repo_test', files('kpm_add_repo_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'KPM Indexing Test',
    executable('kpm_index_test', files('kpm_index_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)