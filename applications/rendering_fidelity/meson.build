fidelity_python = find_program('python3')
fidelity_app = executable(app_name, app_sources,
    dependencies: krisp_core,
    link_depends: krisp_runtime,
    install: true,
    install_dir: install_dir,
    cpp_args: internal_app_args + ['-DKRISP_PYTHON="@0@"'.format(fidelity_python.full_path())])

install_data('__init__.py', 'fidelity_runner.py', 'fidelity_images.py', 'fidelity_report.py',
    install_dir: install_dir / 'rendering_fidelity-tools')

# This target deliberately stays outside Meson's default CPU test run. Missing
# graphics, assets or approved baselines fail the explicit GPU invocation.
run_target('rendering_fidelity_gpu',
    command: [fidelity_app, 'run'],
    depends: krisp_runtime)
