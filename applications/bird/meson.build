executable(
  'bird',
  'bird.cpp',
  dependencies: krisp_core,
  link_depends: default_environment_lighting,
  install: true,
  install_dir: install_dir,
  cpp_args: [
    '-DPROJECT_NAME="bird"',
    '-DPROJECT_ENVIRONMENT_LIGHTING_ASSET="@0@"'.format(
      default_environment_lighting.full_path())])
