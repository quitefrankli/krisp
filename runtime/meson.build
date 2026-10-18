# Development reads source assets and generated files directly. This target
# builds generated runtime data without staging a second directory tree.
krisp_runtime = [shader_artifacts, default_environment_lighting]
alias_target('krisp_runtime', krisp_runtime)

# Only installation assembles a self-contained runtime for packaged consumers.
install_subdir('../resources/default',
    install_dir: get_option('datadir') / 'krisp' / 'resources',
    strip_directory: true,
    install_tag: 'engine')
install_data('../configs/default.yaml',
    install_dir: get_option('datadir') / 'krisp' / 'configs',
    install_tag: 'engine')
install_subdir(meson.project_build_root() / 'shaders',
    install_dir: get_option('datadir') / 'krisp' / 'shaders',
    strip_directory: true,
    exclude_files: ['shader_artifacts.stamp'],
    install_tag: 'engine')
