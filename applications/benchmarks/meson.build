benchmark_python = find_program('python3')
benchmark_provenance = custom_target('benchmarks-provenance',
    output: 'benchmarks-build.json',
    command: [benchmark_python, files('performance/build_provenance.py'),
        '--source-root', meson.project_source_root(),
        '--build-root', meson.project_build_root(),
        '--build-type', get_option('buildtype'),
        '--compiler', cc.cmd_array()[0],
        '--output', '@OUTPUT@'],
    build_always_stale: true,
    install: true,
    install_dir: install_dir)

benchmarks_app = executable('benchmarks', 'benchmarks.cpp',
    'performance/benchmark_worker.cpp',
    '../rendering_fidelity/rendering_fidelity_worker.cpp',
    dependencies: krisp_core,
    link_depends: [krisp_runtime, benchmark_provenance],
    install: true,
    install_dir: install_dir,
    cpp_args: internal_app_args + ['-DKRISP_PYTHON="@0@"'.format(benchmark_python.full_path())])

install_data('performance/benchmark_runner.py', 'performance/benchmark_statistics.py',
    'performance/build_provenance.py', 'performance/benchmark_assets.py',
    'performance/assets.lock.json',
    install_dir: install_dir / 'benchmarks-tools' / 'performance')
install_data('../rendering_fidelity/__init__.py', '../rendering_fidelity/fidelity_runner.py',
    '../rendering_fidelity/fidelity_images.py', '../rendering_fidelity/fidelity_report.py',
    '../rendering_fidelity/fidelity_assets.py', '../rendering_fidelity/assets.lock.json',
    '../rendering_fidelity/fidelity_references.py', '../rendering_fidelity/reference_cycles.py',
    '../rendering_fidelity/reference_khronos.py',
    install_dir: install_dir / 'benchmarks-tools' / 'fidelity')
install_subdir('../rendering_fidelity/reference_web',
    install_dir: install_dir / 'benchmarks-tools' / 'fidelity', exclude_directories: ['node_modules'])

run_target('benchmarks_fidelity_gpu',
    command: [benchmarks_app, 'fidelity', 'run'],
    depends: krisp_runtime)

if get_option('build_tests')
    performance_case_checks = executable('benchmark_performance_checks',
        ['performance/tests/main.cpp', 'performance/tests/scene_tests.cpp',
         'performance/tests/measurement_tests.cpp'],
        dependencies: [krisp_core, dependency('gtest')])
    test('benchmark_performance_cases', performance_case_checks, args: ['--gtest_brief=1'])
endif
