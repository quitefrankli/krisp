sources = [
	'test_main.cpp',
	'mesh_tests.cpp',
	'game_engine_tests.cpp',
	'clickable_ecs_tests.cpp',
	'common_tests.cpp',
	'resource_loader_tests.cpp',
	'graphics_buffer_tests.cpp',
	'camera_tests.cpp',
	'math_tests.cpp',
	'mock_graphics_engine.hpp',
	'object_tests.cpp',
	'orbital_physics_tests.cpp']

exec = executable(
	'krisp_tests',
	sources,
	dependencies: krisp_core,
	install_dir: install_dir)

test('krisp_tests', exec)

