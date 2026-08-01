#pragma once

#include "game_engine.hpp"
#include "type_registry.hpp"
#include "serialization/serializer.hpp"
#include "utility.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "utility.hpp"
#include "analytics.hpp"
#include "interface/gizmo.hpp"
#include "gui/gui_manager.hpp"
#include "experimental.hpp"
#include "iapplication.hpp"
#include "game_objects/player_character.hpp"
#include "entity_component_system/material_system.hpp"
#include "renderable/material_factory.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "renderable/mesh_factory.hpp"
#include "serialization/resource_provenance.hpp"
#include "save_file_store.hpp"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <quill/LogMacros.h>
#include <fmt/core.h>
#include <fmt/color.h>

#include <iostream>
#include <ranges>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>

GameEngine::GameEngine(std::unique_ptr<IApplication> app) :
	window(std::make_unique<App::Window>()),
	mouse(std::make_unique<Mouse>(*window)),
	graphics_engine(std::make_unique<GraphicsEngine>(*window)),
	application(std::move(app))
{
	init();
}

GameEngine::GameEngine(std::unique_ptr<App::Window> win, 
					   std::unique_ptr<IApplication> app, 
					   std::unique_ptr<GraphicsEngineBase> gfx_engine) :
	window(std::move(win)),
	mouse(std::make_unique<Mouse>(*window)),
	graphics_engine(std::move(gfx_engine)),
	application(std::move(app))
{
	init();
}

void GameEngine::init()
{
	graphics_engine->set_application_ui_manager(&application_ui_manager);
	graphics_engine->set_ui_layers_active(true, false);
	auto camera_focus_object = std::make_shared<Object>();
	camera_focus_object->set_transient(true);
	auto& camera_focus = spawn_object(std::move(camera_focus_object));
	attach_renderable(camera_focus.get_id(),
		Renderable::make_default(MeshSystem::add(MeshFactory::sphere())));
	auto camera_upvector_object = std::make_shared<Arrow>();
	camera_upvector_object->set_transient(true);
	auto& camera_upvector = static_cast<Arrow&>(spawn_object(
		std::shared_ptr<Object>(std::move(camera_upvector_object))));
	attach_renderable(camera_upvector.get_id(), Arrow::make_renderable());
	camera = std::make_unique<Camera>(
		ecs,
		Listener(audio_engine),
		static_cast<float>(window->get_width()) / static_cast<float>(window->get_height()),
		camera_focus,
		camera_upvector);
	gizmo = std::make_unique<Gizmo>(*this);
	window->setup_callbacks(*this);
	camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 3.0f, -3.0f));
	experimental = std::make_unique<Experimental>(*this);
	
	get_gui_manager().template spawn_gui<GuiMusic>(audio_engine.create_source());
	TPS_counter = std::make_unique<Analytics>([this](float tps) {
		set_tps(float(1e6) / tps);
	}, 1);
	TPS_counter->text = "TPS Counter";

	configure_ecs();
	application->create_ui(*this, application_ui_manager);
	application_ui_manager.seal();
	publish_completed_render_frame();
}

void GameEngine::configure_ecs()
{
	ecs.set_tile_spawner([this]() -> Object&
	{
		MaterialHandle new_tile_material;
		if (!tile_renderable || !MaterialSystem::contains(tile_renderable->get_material_id(0)))
		{
			ColorMaterial mat{};
			mat.data.ambient = glm::vec3(0.45f) / SDS::AMBIENT_STRENGTH;
			mat.data.diffuse = Maths::zero_vec;
			mat.data.specular = Maths::zero_vec;
			mat.data.emissive = Maths::zero_vec;
			mat.data.shininess = 0.0f;
			
			new_tile_material = MaterialSystem::add(std::make_unique<ColorMaterial>(mat));
			auto tile_mesh = MeshSystem::add(MeshFactory::cube());
			tile_renderable = Renderable{
				.pipeline_render_type = ERenderType::COLOR,
				.mesh_owner = std::move(tile_mesh),
				.material_owners = { new_tile_material },
			};
		}

		auto& object = spawn_object<Object>();
		attach_renderable(object.get_id(), *tile_renderable);
		return object;
	});
}

void GameEngine::run()
{
	graphics_engine_thread = std::thread(&GraphicsEngineBase::run, graphics_engine.get());
	Utility::sleep(std::chrono::milliseconds(100));

	try 
	{
		application->on_begin(*this);
		gizmo->init();

		Analytics analytics(60);
		analytics.text = "GameEngine: avg loop processing period (excluding sleep)";

		std::chrono::time_point<std::chrono::system_clock> time = std::chrono::system_clock::now();

		TPS_counter->start();
		Utility::LoopSleeper loop_sleeper(std::chrono::milliseconds(17));
		while (!should_shutdown && !window->should_close())
		{
	#ifndef DISABLE_SLEEP
			loop_sleeper();
	#endif
			const std::chrono::time_point<std::chrono::system_clock> new_time = std::chrono::system_clock::now();
			std::chrono::duration<float, std::milli> chrono_time_delta = new_time - time;
			const float time_delta = chrono_time_delta.count() * 0.001; // in seconds
			time = new_time;
			analytics.start();

			// for ticks per second
			TPS_counter->stop();
			TPS_counter->start();
			main_loop(time_delta);

			analytics.stop();
		}
    } catch (const std::exception& e) { // if an exception occurs in the game engine we need to cleanly shutdown graphics_engine first
		fmt::print(fg(fmt::color::red), "Exception Thrown!: {}\n", e.what());
	} catch (...) {
		fmt::print(fg(fmt::color::red), "Exception Thrown!: UNKNOWN\n");
	}

	shutdown();
	graphics_engine_thread.join();
}

void GameEngine::main_loop(const float time_delta)
{
	window->poll_events();

	process_objs_to_delete();

	get_gui_manager().process_persistent(*this);
	if (game_mode == EGameMode::EDITOR)
		get_gui_manager().process(*this);
	else
		get_gui_manager().process_application(application_ui_manager, *this);

	if (game_mode == EGameMode::NORMAL
		|| mouse->mmb_down || (camera_orbit_with_right_mouse && mouse->rmb_down))
	{
		// if (window->is_shift_down())
		if (false) // TODO: implement shift key tracking
		{
			// panning
			const float min_threshold = 0.01f;
			mouse->update_pos();
			const glm::vec2 offset_vec = mouse->get_orig_offset();
			const float magnitude = glm::length(offset_vec);
			if (magnitude > min_threshold)
			{
				camera->pan(offset_vec, magnitude);
			}
		} else
		{
			// orbiting
			const float min_threshold = 0.001f;
			mouse->update_pos();
			glm::vec2 offset = mouse->get_prev_offset();
			float magnitude = glm::length(offset);
			if (magnitude > min_threshold)
			{
				constexpr float NORMAL_LOOK_SENSITIVITY = 0.25f;
				const glm::vec2 rotation_offset = game_mode == EGameMode::NORMAL
					? -offset * NORMAL_LOOK_SENSITIVITY : offset;
				camera->rotate_camera(rotation_offset, time_delta);
			}
		}
	} else if (mouse->lmb_down)
	{
		const float sensitivity = 2.0f;
		const float min_threshold = 0.01f;

		// check w/ IMMEDIATE prev pos if offset is big enough
		if (mouse->update_pos_on_significant_offset(min_threshold))
		{
			const auto offset = mouse->get_orig_offset();
			glm::vec2 screen_axis(offset.x, offset.y);
			float magnitude = glm::length(screen_axis);

			const Maths::Ray r1 = camera->get_ray(mouse->orig_pos);
			const Maths::Ray r2 = camera->get_ray(mouse->curr_pos);
			if (game_mode == EGameMode::EDITOR)
				gizmo->process(r1, r2);
		}
	}

	if (game_mode == EGameMode::EDITOR)
		camera->process_keyboard_movement(keyboard, time_delta);
	// I just realised there is a MUCH more efficient method of doing this
	// all we need to do is find intersection point of ray with plane of tileset
	// and check if that point is within bounds of tileset, then we can calculate hovered tile coord from that point
	// and take into account gaps between tiles as well, this is way more efficient than checking ray intersection with every single tile's collider
	const auto hover_result = ecs.process_hover(get_mouse_ray());
	if (hover_result.prev_hovered)
	{
		unhighlight_object(*get_object(*hover_result.prev_hovered));
	}

	if (hover_result.new_hovered)
	{
		highlight_object(*get_object(*hover_result.new_hovered));
	}

	if (!paused)
	{
		if (game_mode == EGameMode::NORMAL && active_player)
		{
			active_player->pre_update(keyboard, *camera, ecs, time_delta);
			camera->update_follow();
		}
		application->on_pre_tick(*this, time_delta);
		ecs.process(time_delta);
		experimental->process(time_delta);
		application->on_tick(*this, time_delta);
		application->on_post_tick(*this, time_delta);
	}
	camera->sync_audio_listener();
	publish_completed_render_frame();
}

void GameEngine::set_game_mode(const EGameMode mode)
{
	if (game_mode == mode)
		return;
	if (mode == EGameMode::NORMAL)
	{
		std::vector<PlayerCharacter*> players;
		for (const auto& [_, object] : objects)
			if (auto* player = dynamic_cast<PlayerCharacter*>(object.get()))
				players.push_back(player);
		std::ranges::sort(players, {}, [](const PlayerCharacter* player)
		{
			return player->get_id().get_underlying();
		});
		if (players.empty())
		{
			if (!application->allows_playerless_normal_mode())
			{
				LOG_WARNING(Utility::get_logger(),
					"Cannot enter normal game mode without a PlayerCharacter");
				return;
			}
			active_player = nullptr;
		}
		else
		{
			const auto current = std::ranges::find(players, active_player);
			active_player = current == players.end() || std::next(current) == players.end()
				? players.front() : *std::next(current);
		}
	}
	game_mode = mode;
	graphics_engine->set_ui_layers_active(game_mode == EGameMode::EDITOR, game_mode == EGameMode::NORMAL);
	if (game_mode == EGameMode::EDITOR)
	{
		camera->stop_follow();
		window->set_cursor_captured(false);
		return;
	}
	// Gizmo children are ordinary overlay renderables, so hiding the selected
	// gizmo also removes it from the graphics pass outside editor mode.
	gizmo->deselect();
	if (active_player)
	{
		camera->follow(
			*active_player,
			active_player->get_definition().camera_focus_offset,
			active_player->get_definition().camera_horizontal_offset);
	}
	window->set_cursor_captured(true);
	mouse->update_pos();
}

void GameEngine::toggle_game_mode()
{
	set_game_mode(game_mode == EGameMode::EDITOR
		? EGameMode::NORMAL : EGameMode::EDITOR);
}

void GameEngine::set_render_mode(const ERenderMode mode)
{
	render_view_state.render_mode = mode == ERenderMode::RAYTRACING
		? ERenderMode::RASTERIZED
		: mode;
}

void GameEngine::shutdown_impl()
{
	if (should_shutdown)
	{
		return;
	}

	should_shutdown = true;
	graphics_engine->request_shutdown();
}

GameEngine::~GameEngine() = default;

Object& GameEngine::spawn_object(std::shared_ptr<Object>&& object)
{
	if (objects.contains(object->get_id()))
		throw std::runtime_error("GameEngine::spawn_object: duplicate object id");
	auto it = objects.emplace(object->get_id(), std::move(object));
	Object& new_obj = *(it.first->second);
	ecs.add_object(new_obj);

	// TODO: restore bone visualizers using skeletal renderable attachments.

	return new_obj;
}

RenderableID GameEngine::attach_renderable(
	const ObjectID object_id,
	Renderable renderable,
	const std::optional<SkeletonID> skeleton_id)
{
	validate_renderable_resources(renderable);
	return ecs.add_renderable(std::move(renderable), object_id, skeleton_id);
}

std::vector<RenderableID> GameEngine::attach_renderables(
	const ObjectID object_id,
	std::vector<Renderable> renderables,
	const std::optional<SkeletonID> skeleton_id)
{
	for (const auto& renderable : renderables)
		validate_renderable_resources(renderable);
	return ecs.add_renderables(std::move(renderables), object_id, skeleton_id);
}

void GameEngine::spawn_cubemap()
{
	Renderable renderable;
	renderable.pipeline_render_type = ERenderType::CUBEMAP;
	renderable.casts_shadow = false;
	auto mesh_owner = MeshSystem::add(MeshFactory::cube(MeshFactory::EVertexType::COLOR));
	renderable.mesh_owner = std::move(mesh_owner);
	for (const auto texture_name : { "right", "left", "top", "bottom", "front", "back" })
	{
		renderable.material_owners.push_back(ResourceLoader::fetch_texture(
			fmt::format("skybox/{}.jpg", texture_name)));
	}
	auto& object = spawn_object<Object>();
	attach_renderable(object.get_id(), std::move(renderable));
}

void GameEngine::validate_renderable_resources(const Renderable& renderable)
{
	if (!renderable.mesh_owner || !MeshSystem::contains(renderable.get_mesh_id()))
		throw std::runtime_error("GameEngine::attach_renderable: mesh owner is missing or invalid");
	for (const auto& material_owner : renderable.material_owners)
	{
		if (!material_owner || !MaterialSystem::contains(MaterialSystem::get_id(material_owner)))
			throw std::runtime_error("GameEngine::attach_renderable: material owner is missing or invalid");
	}
}

Object & GameEngine::spawn_particle_emitter(const ParticleEmitterConfig & config)
{
	auto& obj = spawn_object<Object>();
	attach_renderable(obj.get_id(), Renderable::make_default());
	ecs.get_transformation(obj.get_id()).set_scale(glm::vec3(0.2f));
	obj.set_visibility(false);
	ecs.spawn_particle_emitter(obj.get_id(), config);
	return obj;
}

void GameEngine::delete_object(ObjectID id)
{
	if (!objects.contains(id) || !pending_deletions.insert(id).second)
		return;
	entities_to_delete.push(id);
}

void GameEngine::process_objs_to_delete()
{
	while (!entities_to_delete.empty())
	{
		const ObjectID id = entities_to_delete.front();
		entities_to_delete.pop();
		pending_deletions.erase(id);
		if (!objects.contains(id))
			continue;

		// TODO: fix visualisers		
		// if (ecs.has_skeletal_component(id))
		// {
		// 	const auto& bone_visualisers = ecs.get_skeletal_component(id).get_visualisers();
		// 	for (const auto bone_visualiser : bone_visualisers)
		// 	{
		// 		delete_object(bone_visualiser);
		// 	}
		// }
		
		ecs.remove_clickable_entity(id);
		const Object& object = *get_object(id);
		if (&object == active_player)
		{
			set_game_mode(EGameMode::EDITOR);
			active_player = nullptr;
		}
		ecs.remove_object(id);
		objects.erase(id);
		render_view_state.stenciled_objects.erase(id);
	}
}

void GameEngine::reset_scene()
{
	reset_scene_state();
}

void GameEngine::reset_scene_state()
{
	gizmo->deselect();

	camera->stop_follow();
	ecs.reset_preserving_transient_transformations();
	std::erase_if(objects, [](const auto& entry) {
		return !entry.second->is_transient();
	});
	active_player = nullptr;
	game_mode = EGameMode::EDITOR;
	graphics_engine->set_ui_layers_active(true, false);
	entities_to_delete = {};
	pending_deletions.clear();
	render_view_state.stenciled_objects.clear();
	ResourceProvenance::clear();
	tile_renderable.reset();
	configure_ecs();
	for (const auto& [_, object] : objects)
		if (!ecs.has_object(object->get_id()))
			ecs.add_object(*object);
	gizmo->register_colliders();

	camera->set_mode(Camera::Mode::ORBIT);
	camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 3.0f, -3.0f));
}

void GameEngine::highlight_object(const Object& object)
{
	render_view_state.stenciled_objects.insert(object.get_id());
}

void GameEngine::unhighlight_object(const Object& object)
{
	render_view_state.stenciled_objects.erase(object.get_id());
}

RenderableID GameEngine::replace_renderable_texture(
	const RenderableID renderable_id,
	const ETextureSemantic semantic,
	std::optional<std::string> texture_filename)
{
	if (!ecs.has_renderable(renderable_id))
		throw std::runtime_error("GameEngine::replace_renderable_texture: renderable not found");
	auto renderable = ecs.get_renderable(renderable_id).renderable;
	if (renderable.pipeline_render_type != ERenderType::STANDARD
		&& renderable.pipeline_render_type != ERenderType::SKINNED)
		throw std::runtime_error("GameEngine::replace_renderable_texture: renderable does not support textures");
	if (semantic == ETextureSemantic::COUNT)
		throw std::runtime_error("GameEngine::replace_renderable_texture: invalid texture semantic");

	const TexturedMatGroup current(renderable.material_owners);
	MaterialID diffuse = current.base_color_mat;
	std::optional<MaterialID> normal = current.normal_mat;
	std::optional<MaterialID> specular = current.specular_mat;
	auto replacement_owner = texture_filename
		? ResourceLoader::fetch_texture(*texture_filename, semantic)
		: semantic == ETextureSemantic::BASE_COLOR
			? MaterialSystem::add(MaterialFactory::fetch_white_texture())
			: MaterialHandle{};
	const MaterialID replacement = replacement_owner
		? MaterialSystem::get_id(replacement_owner)
		: MaterialID{};
	const std::optional<MaterialID> old = [&]() -> std::optional<MaterialID>
	{
		switch (semantic)
		{
		case ETextureSemantic::BASE_COLOR: return diffuse;
		case ETextureSemantic::NORMAL: return normal;
		case ETextureSemantic::SPECULAR: return specular;
		case ETextureSemantic::COUNT: break;
		}
		throw std::runtime_error("GameEngine::replace_renderable_texture: invalid texture semantic");
	}();

	if (old && *old == replacement)
		return renderable_id;

	if (semantic == ETextureSemantic::BASE_COLOR)
		diffuse = replacement;
	else if (semantic == ETextureSemantic::NORMAL)
		normal = texture_filename ? std::optional<MaterialID>(replacement) : std::nullopt;
	else if (semantic == ETextureSemantic::SPECULAR)
		specular = texture_filename ? std::optional<MaterialID>(replacement) : std::nullopt;

	auto old_owners = std::move(renderable.material_owners);
	const auto take_old_owner = [&old_owners](const MaterialID id)
	{
		const auto found = std::ranges::find_if(old_owners, [id](const MaterialHandle& owner) {
			return owner && MaterialSystem::get_id(owner) == id;
		});
		if (found == old_owners.end())
			throw std::runtime_error(
				"GameEngine::replace_renderable_texture: material owner not found");
		return std::move(*found);
	};
	std::vector<MaterialHandle> updated_owners;
	updated_owners.reserve(3);
	updated_owners.push_back(semantic == ETextureSemantic::BASE_COLOR
		? std::move(replacement_owner)
		: take_old_owner(current.base_color_mat));
	if (normal)
		updated_owners.push_back(semantic == ETextureSemantic::NORMAL
			? std::move(replacement_owner)
			: take_old_owner(*current.normal_mat));
	if (specular)
		updated_owners.push_back(semantic == ETextureSemantic::SPECULAR
			? std::move(replacement_owner)
			: take_old_owner(*current.specular_mat));
	renderable.material_owners = std::move(updated_owners);
	old_owners.clear();
	return ecs.replace_renderable(renderable_id, std::move(renderable));
}

RenderableID GameEngine::set_renderable_specular_matte(
	const RenderableID renderable_id)
{
	if (!ecs.has_renderable(renderable_id))
		throw std::runtime_error("GameEngine::set_renderable_specular_matte: renderable not found");
	auto renderable = ecs.get_renderable(renderable_id).renderable;
	if (renderable.pipeline_render_type != ERenderType::STANDARD
		&& renderable.pipeline_render_type != ERenderType::SKINNED)
		throw std::runtime_error("GameEngine::set_renderable_specular_matte: renderable does not support textures");

	const TexturedMatGroup current(renderable.material_owners);
	auto matte_owner = MaterialSystem::add(MaterialFactory::fetch_black_texture());
	const MaterialID matte = MaterialSystem::get_id(matte_owner);
	if (current.specular_mat && *current.specular_mat == matte)
		return renderable_id;

	auto old_owners = std::move(renderable.material_owners);
	const auto take_old_owner = [&old_owners](const MaterialID id)
	{
		const auto found = std::ranges::find_if(old_owners, [id](const MaterialHandle& owner) {
			return owner && MaterialSystem::get_id(owner) == id;
		});
		if (found == old_owners.end())
			throw std::runtime_error(
				"GameEngine::set_renderable_specular_matte: material owner not found");
		return std::move(*found);
	};
	std::vector<MaterialHandle> updated_owners;
	updated_owners.reserve(3);
	updated_owners.push_back(take_old_owner(current.base_color_mat));
	if (current.normal_mat)
		updated_owners.push_back(take_old_owner(*current.normal_mat));
	updated_owners.push_back(std::move(matte_owner));
	renderable.material_owners = std::move(updated_owners);
	old_owners.clear();
	return ecs.replace_renderable(renderable_id, std::move(renderable));
}

EngineUiManager& GameEngine::get_gui_manager()
{
	return graphics_engine->get_gui_manager();
}

uint32_t GameEngine::get_window_width()
{
	return window->get_width();
}

uint32_t GameEngine::get_window_height()
{
	return window->get_height();
}

Maths::Ray GameEngine::get_mouse_ray() const
{
	return camera->get_ray(mouse->get_curr_pos());
}

Gizmo& GameEngine::get_gizmo() { return *gizmo; }

void GameEngine::save_scene(const std::string_view save_name) const
{
	const auto path = SaveFileStore(Utility::get_saves_path()).path_for_overwrite(save_name);
	gizmo->deselect();
	Serializer document;
	auto engine_state = document.map("engine");
	engine_state.write("paused", paused);
	engine_state.write("game_mode", static_cast<int>(game_mode));
	engine_state.write("camera_orbit_with_right_mouse", camera_orbit_with_right_mouse);
	auto saved_camera = document.map("camera");
	camera->serialize(saved_camera);
	auto saved_objects = document.sequence("objects");
	for (const auto& [_, object] : objects)
	{
		if (object->is_transient())
			continue;
		if (!TypeRegistry::contains(object->serialization_type()))
			throw SerializationError("Unsupported object type at $.objects: " + std::string(object->serialization_type()));
		auto saved = saved_objects.append_map();
		object->serialize(saved);
	}
	auto saved_ecs = document.map("ecs");
	ecs.serialize(saved_ecs);

	std::filesystem::create_directories(path.parent_path());
	const auto temporary = path.string() + ".tmp";
	{
		std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
		if (!stream)
			throw SerializationError("Unable to open scene temporary file: " + temporary);
		stream << document.emit();
		if (!stream)
			throw SerializationError("Unable to write scene temporary file: " + temporary);
	}
	std::filesystem::rename(temporary, path);
}

void GameEngine::load_scene(const std::string_view save_name)
{
	const auto path = SaveFileStore(Utility::get_saves_path()).path_for_overwrite(save_name);
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		throw SerializationError("Unable to open scene: " + path.string());
	std::ostringstream contents;
	contents << stream.rdbuf();
	const auto document = Deserializer::parse(contents.str());

	std::vector<Deserializer> saved_objects;
	std::unordered_map<ObjectID, bool> ids;
	for (const auto& saved : document.child("objects").elements())
	{
		if (!TypeRegistry::contains(saved.read<std::string>("type")))
			throw SerializationError("Unsupported object type at " + saved.path());
		const ObjectID id(saved.read<uint64_t>("id"));
		if (!ids.emplace(id, true).second)
			throw SerializationError("Duplicate object id at " + saved.path());
		saved_objects.push_back(saved);
	}
	reset_scene_state();
	std::unordered_map<std::string, ResourceLoader::LoadedModel> imported_models;
	const auto load_imported_model = [&](const Deserializer& source) -> const ResourceLoader::LoadedModel&
	{
		const auto path = source.read<std::string>("path");
		const auto scene = source.read<int>("scene");
		const auto cache_key = path + "#" + std::to_string(scene);
		if (const auto it = imported_models.find(cache_key); it != imported_models.end())
			return it->second;
		ResourceLoader::LoadOptions options;
		if (scene >= 0)
			options.scene_index = scene;
		options.generate_missing_tangents = true;
		return imported_models.emplace(
			cache_key, ResourceLoader::load_model(ecs, path, options)).first->second;
	};
	for (const auto& saved : document.child("ecs").child("skeletal_system").elements())
	{
		const auto fields = saved.keys();
		if (std::ranges::find(fields, "imported_source") != fields.end())
			load_imported_model(saved.child("imported_source"));
	}
	for (const auto& saved : document.child("ecs").child("renderable_system").elements())
		load_imported_model(saved.child("mesh_source"));
	for (auto& saved : saved_objects)
	{
		auto object = TypeRegistry::create(saved.read<std::string>("type"));
		object->deserialize(saved);
		const auto id = object->get_id();
		objects.emplace(id, object);
		ecs.add_object(*object);
	}
	ecs.deserialize(document.child("ecs"));
	const auto engine_state = document.child("engine");
	paused = engine_state.read<bool>("paused");
	const auto saved_game_mode = static_cast<EGameMode>(engine_state.read<int>("game_mode"));
	camera_orbit_with_right_mouse = engine_state.read<bool>("camera_orbit_with_right_mouse");
	camera->deserialize(document.child("camera"));
	set_game_mode(saved_game_mode);
	application->on_scene_loaded(*this);
}
