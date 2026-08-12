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
