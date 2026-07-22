#pragma once

#include "game_engine.hpp"
#include "type_registry.hpp"
#include "serialization/serializer.hpp"
#include "utility.hpp"

#include "camera.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "graphics_engine/graphics_engine_commands.hpp"
#include "utility.hpp"
#include "analytics.hpp"
#include "interface/gizmo.hpp"
#include "gui/gui_manager.hpp"
#include "experimental.hpp"
#include "iapplication.hpp"
#include "entity_component_system/material_system.hpp"
#include "renderable/material_factory.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "renderable/mesh_factory.hpp"
#include "serialization/serialization_helpers.hpp"
#include "serialization/resource_provenance.hpp"

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
	graphics_engine(std::make_unique<GraphicsEngine>(*this)),
	camera(std::make_unique<Camera>(Listener(),
		   static_cast<float>(window->get_width())/static_cast<float>(window->get_height()))),
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
	camera(std::make_unique<Camera>(Listener(),
		   static_cast<float>(window->get_width())/static_cast<float>(window->get_height()))),
	application(std::move(app))
{
	init();
}

void GameEngine::init()
{
	gizmo = std::make_unique<Gizmo>(*this);
	window->setup_callbacks(*this);
	camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 3.0f, -3.0f));
	draw_object(camera->focus_obj);
	draw_object(camera->upvector_obj);
	experimental = std::make_unique<Experimental>(*this);
	
	get_gui_manager().template spawn_gui<GuiMusic>(audio_engine.create_source());
	TPS_counter = std::make_unique<Analytics>([this](float tps) {
		set_tps(float(1e6) / tps);
	}, 1);
	TPS_counter->text = "TPS Counter";

	configure_ecs();
}

void GameEngine::configure_ecs()
{
	ecs.set_tile_spawner([this]() -> Object&
	{
		if (!tile_renderable || !MaterialSystem::contains(tile_renderable->material_ids.front()))
		{
			ColorMaterial mat{};
			mat.data.ambient = glm::vec3(0.45f) / SDS::AMBIENT_STRENGTH;
			mat.data.diffuse = Maths::zero_vec;
			mat.data.specular = Maths::zero_vec;
			mat.data.emissive = Maths::zero_vec;
			mat.data.shininess = 0.0f;
			
			tile_renderable = Renderable{
				.mesh_id = MeshFactory::cube_id(),
				.material_ids = { MaterialSystem::add(std::make_unique<ColorMaterial>(mat)) },
				.pipeline_render_type = ERenderType::COLOR
			};
		}

		return spawn_object<Object>(*tile_renderable);
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

	// poll gui stuff, we should take advantage of polymorphism later on, but for now this is relatively simple
	get_gui_manager().process(*this);

	if (mouse->mmb_down || (camera_orbit_with_right_mouse && mouse->rmb_down))
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
				camera->rotate_camera(offset, time_delta);
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
			gizmo->process(r1, r2);
		}
	}

	if (camera_keyboard_navigation_enabled)
		process_camera_movement(time_delta);
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
		application->on_pre_tick(*this, time_delta);
		ecs.process(time_delta);
		experimental->process(time_delta);
		application->on_tick(*this, time_delta);
		application->on_post_tick(*this, time_delta);
	}
}

void GameEngine::process_camera_movement(float time_delta)
{
	const float move_speed = 1.5f * camera->get_focal_length();
	glm::vec3 offset = Maths::zero_vec;
	
	// Get camera's forward direction (from rotation)
	const glm::vec3 up = camera->get_rotation() * Maths::up_vec;
	const glm::vec3 right = camera->get_rotation() * Maths::right_vec;
	
	// W/S: move up/down along camera's up direction
	if (keyboard.w_pressed())
		offset += up * move_speed * time_delta;
	if (keyboard.s_pressed())
		offset -= up * move_speed * time_delta;
	
	// A/D: move left/right along camera's right direction
	if (keyboard.a_pressed())
		offset -= right * move_speed * time_delta;
	if (keyboard.d_pressed())
		offset += right * move_speed * time_delta;
	
	if (offset != Maths::zero_vec)
		camera->pan(offset, 1.0f);
}

void GameEngine::shutdown_impl()
{
	if (should_shutdown)
	{
		return;
	}

	should_shutdown = true;
	graphics_engine->enqueue_cmd(std::make_unique<ShutdownCmd>());
}

GameEngine::~GameEngine() = default;

Object& GameEngine::spawn_object(std::shared_ptr<Object>&& object)
{
	auto it = objects.emplace(object->get_id(), std::move(object));
	if (!it.second)
		throw std::runtime_error("GameEngine::spawn_object: duplicate object id");
	try
	{
		retain_renderable_resources(*it.first->second);
	}
	catch (...)
	{
		objects.erase(it.first);
		throw;
	}
	Object& new_obj = *(it.first->second);
	ecs.add_object(new_obj);

	// TODO: fix visualisers if skinned object
	// std::vector<ObjectID> visualisers;
	// const glm::quat bone_rotator = glm::angleAxis(-Maths::PI/2.0f, Maths::right_vec);
	// for (const auto& bone : bones)
	// {
	// 	Object& obj = spawn_object<Object>(ShapeFactory::arrow());
	// 	for (auto& shape : obj.get_shapes())
	// 	{
	// 		// gltf models have bones pointing upwards by default
	// 		shape->transform_vertices(bone_rotator);
	// 	}
	// 	obj.set_visibility(false);
	// 	visualisers.push_back(obj.get_id());
	// }
	// ecs.add_bone_visualisers(object->get_id(), visualisers);

	send_graphics_cmd(std::make_unique<SpawnObjectCmd>(it.first->second));

	return new_obj;
}

void GameEngine::spawn_cubemap()
{
	Renderable renderable;
	renderable.pipeline_render_type = ERenderType::CUBEMAP;
	renderable.casts_shadow = false;
	renderable.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::COLOR);
	for (const auto texture_name : { "right", "left", "top", "bottom", "front", "back" })
		renderable.material_ids.push_back(
			ResourceLoader::fetch_texture(fmt::format("skybox/{}.jpg", texture_name)));
	spawn_object<Object>(renderable);
}

void GameEngine::retain_renderable_resources(const Object& object)
{
	for (const auto& renderable : object.renderables)
	{
		if (!MeshSystem::contains(renderable.mesh_id))
			throw std::runtime_error("GameEngine::spawn_object: mesh not found");
		for (const auto material_id : renderable.material_ids)
		{
			if (!MaterialSystem::contains(material_id))
				throw std::runtime_error("GameEngine::spawn_object: material not found");
		}
	}

	for (const auto& renderable : object.renderables)
	{
		const size_t mesh_references = ++mesh_resource_references[renderable.mesh_id];
		if (MeshSystem::get_num_owners(renderable.mesh_id) < mesh_references)
			MeshSystem::register_owner(renderable.mesh_id);

		for (const auto material_id : renderable.material_ids)
		{
			const size_t material_references = ++material_resource_references[material_id];
			if (MaterialSystem::get_num_owners(material_id) < material_references)
				MaterialSystem::register_owner(material_id);
		}
	}
}

void GameEngine::release_renderable_resources(
	const Object& object,
	DestroyResourcesCmd& destroy_resources_cmd)
{
	for (const auto& renderable : object.renderables)
	{
		for (const auto material_id : renderable.material_ids)
		{
			auto reference = material_resource_references.find(material_id);
			if (reference == material_resource_references.end() || reference->second == 0)
				throw std::runtime_error("GameEngine::delete_object: untracked material reference");
			if (--reference->second == 0)
				material_resource_references.erase(reference);
			if (MaterialSystem::unregister_owner(material_id) == 0)
				destroy_resources_cmd.material_ids.push_back(material_id);
		}

		auto reference = mesh_resource_references.find(renderable.mesh_id);
		if (reference == mesh_resource_references.end() || reference->second == 0)
			throw std::runtime_error("GameEngine::delete_object: untracked mesh reference");
		if (--reference->second == 0)
			mesh_resource_references.erase(reference);
		if (MeshSystem::unregister_owner(renderable.mesh_id) == 0)
			destroy_resources_cmd.mesh_ids.push_back(renderable.mesh_id);
	}
}

Object & GameEngine::spawn_particle_emitter(const ParticleEmitterConfig & config)
{
	auto& obj = spawn_object<Object>(Renderable::make_default());
	obj.set_scale(glm::vec3(0.2f));
	obj.set_visibility(false);
	ecs.spawn_particle_emitter(obj.get_id(), config);
	return obj;
}

void GameEngine::delete_object(ObjectID id)
{
	if (!entity_deletion_queue.push(id))
	{
		return;
	}

	graphics_engine->enqueue_cmd(std::make_unique<DeleteObjectCmd>(id));
}

void GameEngine::process_objs_to_delete()
{
	const auto curr_deleted_objs_count_in_graphics_engine = graphics_engine->get_num_objs_deleted();
	DestroyResourcesCmd destroy_resources_cmd;
	while (!entity_deletion_queue.empty())
	{
		const auto& obj = entity_deletion_queue.front();
		const ObjectID id = obj.first;

		if (obj.second >= curr_deleted_objs_count_in_graphics_engine)
		{
			break;
		}

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
		release_renderable_resources(object, destroy_resources_cmd);

		ecs.remove_object(id);
		objects.erase(id);
		entity_deletion_queue.pop();
	}
	if (!destroy_resources_cmd.material_ids.empty() || !destroy_resources_cmd.mesh_ids.empty())
		send_graphics_cmd(std::make_unique<DestroyResourcesCmd>(std::move(destroy_resources_cmd)));
}

void GameEngine::reset_scene()
{
	gizmo->deselect();

	std::vector<ObjectID> object_ids;
	object_ids.reserve(objects.size());
	for (const auto& [id, _] : objects)
		object_ids.push_back(id);

	auto reset_command = std::make_unique<ResetSceneCmd>(std::move(object_ids));
	auto graphics_paused = reset_command->complete.get_future();
	auto resume_graphics = reset_command->resume;
	send_graphics_cmd(std::move(reset_command));
	if (graphics_engine_thread.joinable())
		graphics_paused.get();

	bool graphics_resumed = false;
	try
	{
		DestroyResourcesCmd destroyed_resources;
		for (const auto& [_, object] : objects)
			release_renderable_resources(*object, destroyed_resources);
		for (const auto& [_, object] : objects)
		{
			object->detach_all_children();
			object->detach_from();
		}

		ecs = ECS{};
		objects.clear();
		entity_deletion_queue.clear();
		mesh_resource_references.clear();
		material_resource_references.clear();
		ResourceProvenance::clear();
		tile_renderable.reset();
		configure_ecs();

		camera->set_mode(Camera::Mode::ORBIT);
		camera->look_at(Maths::zero_vec, glm::vec3(0.0f, 3.0f, -3.0f));

		resume_graphics->set_value();
		graphics_resumed = true;
		if (!destroyed_resources.mesh_ids.empty() || !destroyed_resources.material_ids.empty())
			send_graphics_cmd(std::make_unique<DestroyResourcesCmd>(std::move(destroyed_resources)));
	}
	catch (...)
	{
		if (!graphics_resumed)
			resume_graphics->set_value();
		throw;
	}
}

void GameEngine::highlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<StencilObjectCmd>(object));
}

void GameEngine::unhighlight_object(const Object& object)
{
	graphics_engine->enqueue_cmd(std::make_unique<UnStencilObjectCmd>(object));
}

void GameEngine::replace_renderable_texture(
	const ObjectID object_id,
	const size_t renderable_index,
	const ETextureSemantic semantic,
	std::optional<std::string> texture_filename)
{
	Object* object = get_object(object_id);
	if (!object)
		throw std::runtime_error("GameEngine::replace_renderable_texture: object not found");
	if (renderable_index >= object->renderables.size())
		throw std::runtime_error("GameEngine::replace_renderable_texture: renderable index is out of range");
	auto& renderable = object->renderables[renderable_index];
	if (renderable.pipeline_render_type != ERenderType::STANDARD
		&& renderable.pipeline_render_type != ERenderType::SKINNED)
		throw std::runtime_error("GameEngine::replace_renderable_texture: renderable does not support textures");
	if (semantic == ETextureSemantic::COUNT)
		throw std::runtime_error("GameEngine::replace_renderable_texture: invalid texture semantic");

	const TexturedMatGroup current(renderable.material_ids);
	MaterialID diffuse = current.base_color_mat;
	std::optional<MaterialID> normal = current.normal_mat;
	std::optional<MaterialID> specular = current.specular_mat;
	const MaterialID replacement = texture_filename
		? ResourceLoader::fetch_texture(*texture_filename, semantic)
		: semantic == ETextureSemantic::BASE_COLOR
			? MaterialFactory::fetch_white_texture()
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
	{
		if (texture_filename)
			MaterialSystem::unregister_owner(replacement);
		return;
	}

	if (semantic == ETextureSemantic::BASE_COLOR)
		diffuse = replacement;
	else if (semantic == ETextureSemantic::NORMAL)
		normal = texture_filename ? std::optional<MaterialID>(replacement) : std::nullopt;
	else if (semantic == ETextureSemantic::SPECULAR)
		specular = texture_filename ? std::optional<MaterialID>(replacement) : std::nullopt;

	std::vector<MaterialID> retired_materials;
	const auto release_material = [&](const MaterialID material_id)
	{
		auto reference = material_resource_references.find(material_id);
		if (reference == material_resource_references.end() || reference->second == 0)
			throw std::runtime_error("GameEngine::replace_renderable_texture: untracked material reference");
		if (--reference->second == 0)
			material_resource_references.erase(reference);
		if (MaterialSystem::unregister_owner(material_id) == 0)
			retired_materials.push_back(material_id);
	};
	const auto retain_material = [&](const MaterialID material_id)
	{
		const size_t references = ++material_resource_references[material_id];
		if (MaterialSystem::get_num_owners(material_id) < references)
			MaterialSystem::register_owner(material_id);
	};
	if (old)
		release_material(*old);
	if (semantic == ETextureSemantic::BASE_COLOR || texture_filename)
		retain_material(replacement);
	renderable.material_ids = { diffuse };
	if (normal)
		renderable.material_ids.push_back(*normal);
	if (specular)
		renderable.material_ids.push_back(*specular);

	send_graphics_cmd(std::make_unique<UpdateRenderableMaterialsCmd>(
		object_id,
		renderable_index,
		diffuse,
		normal,
		specular,
		std::move(retired_materials)));
}

void GameEngine::set_renderable_specular_matte(const ObjectID object_id, const size_t renderable_index)
{
	Object* object = get_object(object_id);
	if (!object || renderable_index >= object->renderables.size())
		throw std::runtime_error("GameEngine::set_renderable_specular_matte: object or renderable not found");
	auto& renderable = object->renderables[renderable_index];
	if (renderable.pipeline_render_type != ERenderType::STANDARD
		&& renderable.pipeline_render_type != ERenderType::SKINNED)
		throw std::runtime_error("GameEngine::set_renderable_specular_matte: renderable does not support textures");

	const TexturedMatGroup current(renderable.material_ids);
	const MaterialID matte = MaterialFactory::fetch_black_texture();
	if (current.specular_mat && *current.specular_mat == matte)
		return;

	std::vector<MaterialID> retired_materials;
	if (current.specular_mat)
	{
		auto reference = material_resource_references.find(*current.specular_mat);
		if (reference == material_resource_references.end() || reference->second == 0)
			throw std::runtime_error("GameEngine::set_renderable_specular_matte: untracked material reference");
		if (--reference->second == 0)
			material_resource_references.erase(reference);
		if (MaterialSystem::unregister_owner(*current.specular_mat) == 0)
			retired_materials.push_back(*current.specular_mat);
	}
	const size_t matte_references = ++material_resource_references[matte];
	if (MaterialSystem::get_num_owners(matte) < matte_references)
		MaterialSystem::register_owner(matte);
	renderable.material_ids = { current.base_color_mat, matte };
	if (current.normal_mat)
		renderable.material_ids.push_back(*current.normal_mat);

	send_graphics_cmd(std::make_unique<UpdateRenderableMaterialsCmd>(
		object_id,
		renderable_index,
		current.base_color_mat,
		current.normal_mat,
		matte,
		std::move(retired_materials)));
}

GuiManager& GameEngine::get_gui_manager()
{
	return graphics_engine->get_gui_manager();
}

void GameEngine::send_graphics_cmd(std::unique_ptr<GraphicsEngineCommand>&& cmd)
{
	graphics_engine->enqueue_cmd(std::move(cmd));
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

void GameEngine::preview_objs_in_gui(
	const std::vector<ObjectID>& objs,
	GuiPhotoBase& gui_window)
{
	send_graphics_cmd(std::make_unique<PreviewObjectsCmd>(objs, gui_window));
}

Gizmo& GameEngine::get_gizmo() { return *gizmo; }

void GameEngine::save_scene(const std::filesystem::path& path) const
{
	gizmo->deselect();
	Serializer document;
	auto engine_state = document.map("engine");
	engine_state.write("paused", paused);
	engine_state.write("camera_keyboard_navigation", camera_keyboard_navigation_enabled);
	engine_state.write("camera_orbit_with_right_mouse", camera_orbit_with_right_mouse);
	auto saved_camera = document.map("camera");
	camera->serialize(saved_camera);
	std::unordered_map<MaterialID, bool> saved_material_ids;
	for (const auto& [_, object] : objects)
		for (const auto& renderable : object->renderables)
			for (const auto material_id : renderable.material_ids)
				saved_material_ids.emplace(material_id, true);
	auto saved_materials = document.sequence("materials");
	for (const auto& [material_id, _] : saved_material_ids)
	{
		// Imported glTF materials are restored together with the mesh primitive
		// that owns them.  Writing TextureMaterial::source here loses embedded
		// GLB image provenance, so deliberately omit them from this legacy table.
		if (ResourceProvenance::material(material_id))
			continue;
		const auto& material = MaterialSystem::get(material_id);
		auto saved = saved_materials.append_map();
		saved.write("id", material_id.get_underlying());
		if (const auto* color = dynamic_cast<const ColorMaterial*>(&material))
		{
			saved.write("type", "color");
			auto data = saved.map("data");
			Serialization::write_vec3(data, "ambient", color->data.ambient);
			Serialization::write_vec3(data, "diffuse", color->data.diffuse);
			Serialization::write_vec3(data, "specular", color->data.specular);
			Serialization::write_vec3(data, "emissive", color->data.emissive);
			data.write("shininess", color->data.shininess);
		}
		else if (const auto* texture = dynamic_cast<const TextureMaterial*>(&material))
		{
			saved.write("type", "texture");
			saved.write("source", texture->source);
			saved.write("semantic", static_cast<int>(texture->semantic));
		}
		else
			throw SerializationError("Unsupported material at $.materials");
	}
	auto saved_objects = document.sequence("objects");
	for (const auto& [_, object] : objects)
	{
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

void GameEngine::load_scene(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		throw SerializationError("Unable to open scene: " + path.string());
	std::ostringstream contents;
	contents << stream.rdbuf();
	const auto document = Deserializer::parse(contents.str());

	struct SavedObject { Deserializer data; std::optional<ObjectID> parent; };
	std::vector<SavedObject> saved_objects;
	std::unordered_map<ObjectID, bool> ids;
	for (const auto& saved : document.child("objects").elements())
	{
		if (!TypeRegistry::contains(saved.read<std::string>("type")))
			throw SerializationError("Unsupported object type at " + saved.path());
		const ObjectID id(saved.read<uint64_t>("id"));
		if (!ids.emplace(id, true).second)
			throw SerializationError("Duplicate object id at " + saved.path());
		const auto parent = saved.child("parent_id");
		saved_objects.push_back({ saved, parent.kind() == SerializationKind::Null
			? std::nullopt : std::optional<ObjectID>(ObjectID(parent.as<uint64_t>())) });
	}
	reset_scene();
	std::unordered_map<MaterialID, MaterialID> material_remap;
	for (const auto& saved : document.child("materials").elements())
	{
		const MaterialID saved_id(saved.read<uint64_t>("id"));
		if (MaterialSystem::contains(saved_id))
		{
			material_remap.emplace(saved_id, saved_id);
			continue;
		}
		const auto type = saved.read<std::string>("type");
		if (type == "color")
		{
			ColorMaterial material;
			const auto data = saved.child("data");
			material.data.ambient = Serialization::read_vec3(data, "ambient");
			material.data.diffuse = Serialization::read_vec3(data, "diffuse");
			material.data.specular = Serialization::read_vec3(data, "specular");
			material.data.emissive = Serialization::read_vec3(data, "emissive");
			material.data.shininess = data.read<float>("shininess");
			material_remap.emplace(saved_id, MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(material)), false));
		}
		else if (type == "texture")
		{
			const auto loaded_id = ResourceLoader::fetch_texture(
				saved.read<std::string>("source"),
				static_cast<ETextureSemantic>(saved.read<int>("semantic")));
			material_remap.emplace(saved_id, loaded_id);
		}
		else
			throw SerializationError("Unsupported material type at " + saved.path());
	}
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
		return imported_models.emplace(cache_key, ResourceLoader::load_model(ecs, path, options)).first->second;
	};
	for (auto& saved : saved_objects)
	{
		auto object = TypeRegistry::create(saved.data.read<std::string>("type"));
		object->deserialize(saved.data);
		const auto saved_renderables = saved.data.child("renderables").elements();
		for (size_t renderable_index = 0; renderable_index < object->renderables.size(); ++renderable_index)
		{
			auto& renderable = object->renderables[renderable_index];
			const auto& saved_renderable = saved_renderables.at(renderable_index);
			const auto fields = saved_renderable.keys();
			if (std::ranges::find(fields, "mesh_source") != fields.end())
			{
				const auto source = saved_renderable.child("mesh_source");
				const auto& model = load_imported_model(source);
				const int node = source.read<int>("node");
				const int primitive = source.read<int>("primitive");
				const auto loaded_mesh = std::ranges::find_if(model.meshes, [node](const auto& mesh) {
					return mesh.source_node == node;
				});
				if (loaded_mesh == model.meshes.end() || primitive < 0
					|| primitive >= static_cast<int>(loaded_mesh->renderables.size()))
					throw SerializationError("Invalid imported mesh reference at " + saved_renderable.path());
				renderable = loaded_mesh->renderables[primitive];
				if (loaded_mesh->skeleton_id)
					ecs.attach_skeleton(object->get_id(), *loaded_mesh->skeleton_id);
				continue;
			}
			if (!MeshSystem::contains(renderable.mesh_id))
				throw SerializationError("Missing mesh resource at " + saved.data.path());
			for (auto& material_id : renderable.material_ids)
			{
				const auto remapped = material_remap.find(material_id);
				if (remapped == material_remap.end())
					throw SerializationError("Missing material resource at " + saved.data.path());
				material_id = remapped->second;
			}
		}
		const auto id = object->get_id();
		objects.emplace(id, object);
		retain_renderable_resources(*object);
		ecs.add_object(*object);
		send_graphics_cmd(std::make_unique<SpawnObjectCmd>(object));
	}
	for (const auto& saved : saved_objects)
		if (saved.parent)
		{
			Object* child = get_object(ObjectID(saved.data.read<uint64_t>("id")));
			Object* parent = get_object(*saved.parent);
			if (!parent)
				throw SerializationError("Missing parent object at " + saved.data.path());
			child->attach_to(parent);
		}
	ecs.deserialize(document.child("ecs"));
	const auto engine_state = document.child("engine");
	paused = engine_state.read<bool>("paused");
	camera_keyboard_navigation_enabled = engine_state.read<bool>("camera_keyboard_navigation");
	camera_orbit_with_right_mouse = engine_state.read<bool>("camera_orbit_with_right_mouse");
	camera->deserialize(document.child("camera"));
	application->on_scene_loaded(*this);
}

void GameEngine::quick_save() const
{
	save_scene(Utility::get_top_level_path() / ".saves" / "quicksave.yaml");
}

void GameEngine::quick_load()
{
	load_scene(Utility::get_top_level_path() / ".saves" / "quicksave.yaml");
}
