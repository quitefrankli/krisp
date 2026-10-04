environment_map_precompute = executable(
	'environment_map_precompute',
	'environment_map_precompute.cpp',
	'../src/graphics_engine/environment_map_asset.cpp',
	'../src/graphics_engine/environment_map_processor.cpp',
	'../third_party/stb_image/stb_image.cpp',
	include_directories: [
		include_directories('../src/graphics_engine'),
		include_directories('../third_party/stb_image')],
	dependencies: [
		dependency('glm', required: true),
		dependency('stb', required: true)])

# The default lighting belongs to the engine, including library-only builds.
skybox_faces = files(
    '../resources/default/textures/skybox/right.jpg',
    '../resources/default/textures/skybox/left.jpg',
    '../resources/default/textures/skybox/top.jpg',
    '../resources/default/textures/skybox/bottom.jpg',
    '../resources/default/textures/skybox/front.jpg',
    '../resources/default/textures/skybox/back.jpg')
default_environment_lighting = custom_target(
    'default_environment_lighting',
    input: skybox_faces,
    output: 'default_environment.krisp-ibl',
    command: [environment_map_precompute, '@OUTPUT@', '@INPUT@'])
