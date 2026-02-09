graphics_sources = files('graphics_engine/pipeline/pipeline.cpp',
						 'graphics_engine/renderers/renderer.cpp',
						 'graphics_engine/raytracing.cpp',
						 'graphics_engine/resource_manager/graphics_resource_manager.cpp',
						 'graphics_engine/resource_manager/graphics_buffer.cpp',
						 'graphics_engine/graphics_engine.cpp',
						 'graphics_engine/graphics_engine_commands.cpp',
						 'graphics_engine/engine_commands_handler.cpp',
						 'graphics_engine/submodules.cpp')

ecs_sources = files('entity_component_system/animation.cpp',
					'entity_component_system/light_source.cpp',
					'entity_component_system/hoverable.cpp',
					'entity_component_system/collider_ecs.cpp',
					'entity_component_system/ecs.cpp',
					'entity_component_system/clickable.cpp',
					'entity_component_system/skeletal.cpp',
					'entity_component_system/physics/gravity.cpp',
					'entity_component_system/physics/physics.cpp',
					'entity_component_system/particle_system.cpp')

audio_sources = files('audio_engine/audio_engine.cpp',
					  'audio_engine/audio_engine_pimpl.cpp',
					  'audio_engine/SoundBuffer.cpp',
					  'audio_engine/audio_source.cpp',
					  'audio_engine/listener.cpp')

sources = files('collision/collider.cpp',
				# 'hot_reload.cpp',
				'collision/collision_detector.cpp',
				'collision/bounding_box.cpp',
				'utility.cpp',
				'window.cpp',
				'gui/gui_windows.cpp',
				'input.cpp',
				'game_engine_callbacks.cpp',
				'analytics.cpp',
				'objects/object.cpp',
				'audio_engine/audio_engine_pimpl.cpp',
				'objects/objects.cpp',
				'objects/cubemap.cpp',
				'config.cpp',
				'maths.cpp',
				'camera.cpp',
				'interface/objects.cpp',
				'interface/gizmo.cpp',
				'game_engine.cpp',
				'renderable/mesh_maths.cpp',
				'renderable/material_factory.cpp',
				'renderable/renderable.cpp',
				'renderable/mesh_factory.cpp',
				'vertex.cpp',
				'experimental.cpp',
				'resource_loader/resource_loader.cpp')

all_sources = sources + graphics_sources + ecs_sources #+ audio_sources

krisp_libs = static_library(
	'krisp_libs', 
	all_sources,
	include_directories: additional_include_directories,
	dependencies: conan_dependencies + third_party_libs_dep)

krisp_core = declare_dependency(
	include_directories: additional_include_directories + include_directories('.'),
	dependencies: conan_dependencies,
	link_with: krisp_libs)