python = import('python').find_installation('python3')
krisp_runtime = custom_target(
    'krisp_runtime',
    input: [shader_artifacts, default_environment_lighting, skybox_faces, files('../configs/default.yaml')],
    output: 'runtime.stamp',
    command: [python, files('../scripts/stage_runtime.py'),
        meson.project_source_root(), meson.project_build_root(), '@OUTPUT@'],
    depend_files: files('../scripts/stage_runtime.py'),
    build_by_default: true)

# Install the assembled runtime without build bookkeeping files.
install_subdir(meson.current_build_dir(),
    install_dir: get_option('datadir') / 'krisp',
    strip_directory: true,
    exclude_files: ['runtime.stamp'],
    install_tag: 'engine')
