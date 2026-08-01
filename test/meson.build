sources = [
	'test_main.cpp',
	'mesh_tests.cpp',
	'game_engine_tests.cpp',
	'audio_engine_tests.cpp',
	'ecs/clickable_ecs_tests.cpp',
	'ecs/transformation_system_tests.cpp',
	'common_tests.cpp',
	'gui_panel_tests.cpp',
	'scene_resources_tests.cpp',
	'save_file_store_tests.cpp',
	'graphics_settings_tests.cpp',
	'resource_loader_tests.cpp',
	'render_frame_tests.cpp',
	'render_draw_list_tests.cpp',
	'submission_retirement_queue_tests.cpp',
	'graphics_buffer_tests.cpp',
	'camera_tests.cpp',
	'game_objects_tests.cpp',
	'math_tests.cpp',
	'collider_tests.cpp',
	'mock_graphics_engine.hpp',
	'object_tests.cpp',
	'ecs/orbital_physics_tests.cpp',
	'ecs/tile_system_tests.cpp',
	'ecs/equipment_tests.cpp',
	'ecs/renderable_system_tests.cpp',
	'scale_gizmo_tests.cpp']

sources += ['serializer_tests.cpp']
sources += ['ecs/ecs_serialization_tests.cpp']
sources += ['ecs/skeletal_tests.cpp']

exec = executable(
	'krisp_tests',
	sources,
	dependencies: krisp_core,
	install_dir: install_dir)

test('krisp_tests', exec)
