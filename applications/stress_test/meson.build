stress_python = find_program('python3')
stress_provenance = custom_target('stress-test-provenance',
    output: 'stress_test-build.json',
    command: [stress_python, files('build_provenance.py'),
        '--source-root', meson.project_source_root(),
        '--build-root', meson.project_build_root(),
        '--build-type', get_option('buildtype'),
        '--compiler', cc.cmd_array()[0],
        '--output', '@OUTPUT@'],
    build_always_stale: true,
    install: true,
    install_dir: install_dir)

executable(app_name, app_sources,
    dependencies: krisp_core,
    link_depends: [krisp_runtime, stress_provenance],
    install: true,
    install_dir: install_dir,
    cpp_args: internal_app_args + ['-DKRISP_PYTHON="@0@"'.format(stress_python.full_path())])

install_data('benchmark_runner.py', 'benchmark_statistics.py', 'build_provenance.py',
    'benchmark_assets.py', 'assets.lock.json',
    install_dir: install_dir / 'stress_test-tools')

if get_option('build_tests')
    stress_case_checks = executable('stress_test_checks',
        ['tests/main.cpp', 'tests/scene_tests.cpp', 'tests/measurement_tests.cpp'],
        dependencies: [krisp_core, dependency('gtest')])
    test('stress_test_cases', stress_case_checks, args: ['--gtest_brief=1'])
endif
