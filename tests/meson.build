run_command(
    'bash', '-c', 'cp -ar $MESON_SOURCE_ROOT/tests/resources $MESON_BUILD_ROOT/tests/',
    check: true
)

run_command(
    'bash', '-c', 'cp -ar $MESON_SOURCE_ROOT/kpm_helper.py $MESON_BUILD_ROOT/tests/resources/kpm_helper.py',
    check: true
)

run_command(
    'bash', '-c', 'python $MESON_BUILD_ROOT/tests/resources/gen_resources.py',
    check: true
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
    'KPM Utils Test',
    executable('kpm_utils_test', files('kpm_utils_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'SemVer Test',
    executable('semver_test', files('semver_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)

test(
    'KPM Migration Test',
    executable('kpm_migration_test', files('kpm_migration_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
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

test(
    'KPM Package Test',
    executable('kpm_package_test', files('kpm_package_test.c') + all_test_files, dependencies: [kpm_dep], include_directories: all_test_includes),
    is_parallel: false
)