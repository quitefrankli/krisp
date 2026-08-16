default_skybox_faces = files(
  '../resources/default/textures/skybox/right.jpg',
  '../resources/default/textures/skybox/left.jpg',
  '../resources/default/textures/skybox/top.jpg',
  '../resources/default/textures/skybox/bottom.jpg',
  '../resources/default/textures/skybox/front.jpg',
  '../resources/default/textures/skybox/back.jpg')

default_environment_lighting = custom_target(
  'default_environment_lighting',
  input: default_skybox_faces,
  output: 'default_environment.krisp-ibl',
  command: [environment_map_precompute, '@OUTPUT@', '@INPUT@'],
  build_by_default: true)

subdir('krisp')
subdir('bird')
subdir('tetris')
subdir('chess')
subdir('campfire')
subdir('billiards')
