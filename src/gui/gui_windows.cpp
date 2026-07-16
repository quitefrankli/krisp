#pragma once

#include "gui/gui_manager.hpp"
#include "game_engine.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "audio_engine/audio_source.hpp"
#include "utility.hpp"
#include "camera.hpp"
#include "constants.hpp"
#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"
#include "interface/gizmo.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <quill/LogMacros.h>
#include <fmt/core.h>

#include <algorithm>
#include <unordered_set>
#include "gui_windows.hpp"

namespace
{
template<typename MeshType>
MeshID bake_mesh_transform(const MeshType& source, const glm::mat4& transform)
{
	auto vertices = source.get_vertices();
	const glm::mat3 normal_transform = glm::transpose(glm::inverse(glm::mat3(transform)));
	for (auto& vertex : vertices)
	{
		vertex.pos = glm::vec3(transform * glm::vec4(vertex.pos, 1.0f));
		vertex.normal = glm::normalize(normal_transform * vertex.normal);
	}
	return MeshSystem::add(std::make_unique<MeshType>(std::move(vertices), source.get_indices()));
}

MeshID bake_mesh_transform(const MeshID mesh_id, const glm::mat4& transform)
{
	const Mesh& source = MeshSystem::get(mesh_id);
	MeshID baked_id;
	if (const auto* mesh = dynamic_cast<const ColorMesh*>(&source))
		baked_id = bake_mesh_transform(*mesh, transform);
	else if (const auto* mesh = dynamic_cast<const TexMesh*>(&source))
		baked_id = bake_mesh_transform(*mesh, transform);
	else if (const auto* mesh = dynamic_cast<const SkinnedMesh*>(&source))
		baked_id = bake_mesh_transform(*mesh, transform);
	else
		throw std::runtime_error("GuiModelSpawner: unsupported mesh type");

	MeshSystem::unregister_owner(mesh_id);
	return baked_id;
}

std::string report_resource_load_error(std::string_view context, const ResourceLoadError& error)
{
	std::string message = fmt::format("{}: {}", context, error.what());
	LOG_ERROR(Utility::get_logger(), "{}", message);
	return message;
}

void draw_resource_load_error(const std::optional<std::string>& error)
{
	if (!error)
		return;

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
	ImGui::TextWrapped("%s", error->c_str());
	ImGui::PopStyleColor();
}

bool begin_italic_combo(const char* label, const char* preview)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	const int first_vertex = draw_list->VtxBuffer.Size;
	const bool open = ImGui::BeginCombo(label, preview);
	const ImVec2 white_pixel = ImGui::GetIO().Fonts->TexUvWhitePixel;

	float baseline = 0.0f;
	for (int i = first_vertex; i < draw_list->VtxBuffer.Size; ++i)
	{
		const auto& vertex = draw_list->VtxBuffer[i];
		if (vertex.uv.x != white_pixel.x || vertex.uv.y != white_pixel.y)
			baseline = std::max(baseline, vertex.pos.y);
	}
	for (int i = first_vertex; i < draw_list->VtxBuffer.Size; ++i)
	{
		auto& vertex = draw_list->VtxBuffer[i];
		if (vertex.uv.x != white_pixel.x || vertex.uv.y != white_pixel.y)
			vertex.pos.x += (baseline - vertex.pos.y) * 0.18f;
	}

	return open;
}

Object& get_or_spawn_collider_visual(GameEngine& engine,
									 std::unordered_map<EntityID, ObjectID>& collider_visualiser_ids,
									 EntityID collider_entity,
									 const Collider& collider)
{
	auto it = collider_visualiser_ids.find(collider_entity);
	if (it != collider_visualiser_ids.end())
	{
		if (auto* existing = engine.get_object(it->second))
		{
			return *existing;
		}

		collider_visualiser_ids.erase(it);
	}

	Object& visual = collider.spawn_debug_object(engine);
	collider_visualiser_ids.emplace(collider_entity, visual.get_id());
	return visual;
}
}

bool GuiWindow::begin(int flags)
{
	const bool was_visible = visible;
	const bool expanded = ImGui::Begin(get_imgui_name(), get_visible_ptr(), flags);
	if (visible != was_visible)
		ImGui::MarkIniSettingsDirty();
	return expanded;
}

void GuiWindow::end()
{
	ImGui::End();
}

void GuiWindow::set_visible(const bool value)
{
	if (visible == value)
		return;
	visible = value;
	if (ImGui::GetCurrentContext())
		ImGui::MarkIniSettingsDirty();
}


GuiGraphicsSettings::GuiGraphicsSettings() :
	GuiWindow({ "graphics_settings", "Graphics Settings", GuiPanelDock::RIGHT })
{
}

void GuiGraphicsSettings::draw()
{
	static const float combo_width = [&]() 
	{
		float width = 0.0f;
		for (auto& projection : camera_projections)
		{
			width = std::max(ImGui::CalcTextSize(projection).x, width);
		}

		const float drop_down_icon_width = 18.5f;

		return width + ImGui::GetStyle().FramePadding.x * 2.0f + drop_down_icon_width;
	}();

	if (begin())
	{

	ImGui::SliderFloat("lighting", &light_strength, 0.0f, 1.0f);
	rtx_on.changed = ImGui::Checkbox("RTX", &rtx_on.value);
	wireframe_mode.changed |= ImGui::Checkbox("wireframe", &wireframe_mode.value);
	ImGui::SetNextItemWidth(combo_width);
	selected_camera_projection.changed |= ImGui::Combo(
		"projection", 
		&selected_camera_projection.value, 
		camera_projections.data(), 
		camera_projections.size());

	}
	end();
}
void GuiGraphicsSettings::process(GameEngine& engine)
{
	if (selected_camera_projection.changed)
	{
		engine.get_camera().toggle_projection();
		selected_camera_projection.changed = false;
	}

	if (wireframe_mode.changed)
	{
		engine.get_graphics_engine().enqueue_cmd(std::make_unique<ToggleWireFrameModeCmd>());
		wireframe_mode.changed = false;
	}
}

GuiObjectSpawner::GuiObjectSpawner() :
	GuiWindow({ "object_spawner", "Object Spawner", GuiPanelDock::LEFT })
{
	mapping = {
		{"cube", spawning_function_type([this](GameEngine& engine)
			{
				auto& obj = engine.template spawn_object<Object>(
					Renderable::make_default(MeshFactory::cube_id(MeshFactory::EVertexType::COLOR)));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<BoxCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
			})
		},
		{"textured_cube", spawning_function_type([this](GameEngine& engine)
			{
				const auto mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
				const auto mat_id = ResourceLoader::fetch_texture("texture.jpg");
				Renderable renderable;
				renderable.mesh_id = mesh_id;
				renderable.material_ids = { mat_id };
				renderable.pipeline_render_type = ERenderType::STANDARD;

				auto obj = std::make_shared<Object>(renderable);
				engine.get_ecs().add_object(*obj);
				engine.get_ecs().add_collider(obj->get_id(), std::make_unique<BoxCollider>());
				engine.get_ecs().add_clickable_entity(obj->get_id());
				engine.spawn_object(std::move(obj));
			})
		},
		{"diffuse_cube", spawning_function_type([this](GameEngine& engine)
			{
				ColorMaterial material;
				material.data.shininess = 1.0f;
				Renderable renderable;
				renderable.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::COLOR);
				renderable.material_ids = { MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(material))) };
				auto obj = std::make_shared<Object>(renderable);
				engine.get_ecs().add_object(*obj);
				engine.get_ecs().add_collider(obj->get_id(), std::make_unique<BoxCollider>());
				engine.get_ecs().add_clickable_entity(obj->get_id());
				engine.spawn_object(std::move(obj));
			})
		},
		{"sphere", spawning_function_type([this](GameEngine& engine)
			{
				auto& obj = engine.template spawn_object<Object>(
					Renderable::make_default(MeshFactory::sphere_id(
						MeshFactory::EVertexType::COLOR, 
						MeshFactory::GenerationMethod::ICO_SPHERE, 
						100)));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<SphereCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
			})
		},
		{"physics sphere", spawning_function_type([this](GameEngine& engine)
			{
				auto& obj = engine.template spawn_object<Object>(
					Renderable::make_default(MeshFactory::sphere_id(
						MeshFactory::EVertexType::COLOR,
						MeshFactory::GenerationMethod::ICO_SPHERE)));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<SphereCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
				engine.get_ecs().add_physics_entity(obj.get_id(), PhysicsComponent{});
			})
		}
	};
}

void GuiObjectSpawner::draw()
{
	if (begin())
	{

	ImVec2 button_dim(button_width, button_height);
	if (spawning_function)
	{
		std::for_each(mapping.begin(), mapping.end(), [&](auto& pair){ ImGui::Button(pair.first.c_str(), button_dim); });
	} else {
		for (auto& [key, value] : mapping)
		{
			if (ImGui::Button(key.c_str(), button_dim))
			{
				spawning_function = &value;
			}
		}
	}
	draw_resource_load_error(load_error);
	
	}
	end();
}

void GuiObjectSpawner::process(GameEngine& engine)
{
	if (spawning_function)
	{
		try
		{
			(*spawning_function)(engine);
			load_error.reset();
		}
		catch (const ResourceLoadError& error)
		{
			load_error = report_resource_load_error("Object Spawner failed to load a resource", error);
		}
		spawning_function = nullptr;
	}	
}

GuiModelSpawner::GuiModelSpawner() :
	GuiWindow({ "model_spawner", "Model Spawner", GuiPanelDock::LEFT })
{
	refresh_models();
}

void GuiModelSpawner::refresh_models()
{
	std::optional<std::string> selected_path;
	if (selected_model.value >= 0 && selected_model.value < static_cast<int>(model_paths.size()))
		selected_path = model_paths[selected_model.value].lexically_normal().generic_string();

	model_paths = Utility::get_all_models();
	std::ranges::sort(model_paths, [](const auto& lhs, const auto& rhs)
	{
		return lhs.lexically_normal().generic_string() < rhs.lexically_normal().generic_string();
	});

	models.clear();
	models.reserve(model_paths.size());
	std::ranges::transform(
		model_paths,
		std::back_inserter(models),
		[](const auto& path) { return path.filename().string(); });

	selected_model = 0;
	if (selected_path)
	{
		const auto selected = std::ranges::find_if(model_paths, [&](const auto& path)
		{
			return path.lexically_normal().generic_string() == *selected_path;
		});
		if (selected != model_paths.end())
			selected_model = static_cast<int>(std::distance(model_paths.begin(), selected));
	}
	selected_model.changed = true;
	load_error.reset();
}

void GuiModelSpawner::process(GameEngine& engine)
{
	if (should_refresh_models)
	{
		should_refresh_models = false;
		refresh_models();
	}

	if (model_to_spawn)
	{
		const auto model_path = std::move(*model_to_spawn);
		model_to_spawn.reset();
		ResourceLoader::LoadedModel loaded_model;
		try
		{
			loaded_model = ResourceLoader::load_model(model_path);
			load_error.reset();
		}
		catch (const ResourceLoadError& error)
		{
			load_error = report_resource_load_error(
				fmt::format("Model Spawner failed to load '{}'", model_path.string()), error);
			return;
		}

		const auto model_name = model_path.stem().string();
		const bool contains_skinned_mesh = std::ranges::any_of(loaded_model.meshes, [](const auto& loaded_mesh)
		{
			return loaded_mesh.skeleton_id.has_value();
		});
		const bool merge_meshes = merge_imported_meshes.value && !contains_skinned_mesh;
		if (merge_imported_meshes.value && contains_skinned_mesh)
			LOG_WARNING(Utility::get_logger(), "GuiModelSpawner: cannot merge skinned model '{}' into one object", model_name);

		if (merge_meshes)
		{
			auto object = std::make_shared<Object>();
			object->set_name(model_name);
			object->set_transform(loaded_model.onload_transform.get_mat4());
			for (auto& loaded_mesh : loaded_model.meshes)
			{
				const glm::mat4 transform = loaded_mesh.transform.get_mat4();
				for (auto renderable : loaded_mesh.renderables)
				{
					renderable.mesh_id = bake_mesh_transform(renderable.mesh_id, transform);
					object->renderables.push_back(std::move(renderable));
				}
			}
			Object& spawned_object = engine.spawn_object(std::move(object));
			engine.get_ecs().add_mesh_collider(spawned_object.get_id());
			engine.get_ecs().add_clickable_entity(spawned_object.get_id());
			return;
		}

		for (const auto& loaded_mesh : loaded_model.meshes)
		{
			auto mesh = std::make_shared<Object>(loaded_mesh.renderables, loaded_mesh.skeleton_id);
			mesh->set_transform(loaded_model.onload_transform.get_mat4() * loaded_mesh.transform.get_mat4());
			mesh->set_name(loaded_mesh.name.empty() ? model_name : loaded_mesh.name);
			Object& object = engine.spawn_object(std::move(mesh));
			engine.get_ecs().add_mesh_collider(object.get_id());
			engine.get_ecs().add_clickable_entity(object.get_id());
		}
	}
}

void GuiModelSpawner::draw() 
{
	if (begin())
	{

	const bool dropdown_open = begin_italic_combo("##model", "(select model)");
	if (dropdown_open && !model_dropdown_open)
		should_refresh_models = true;
	model_dropdown_open = dropdown_open;

	if (dropdown_open)
	{
		for (int i = 0; i < static_cast<int>(models.size()); i++)
		{
			if (ImGui::Selectable(models[i].c_str(), i == selected_model.value))
			{
				selected_model = i;
				selected_model.changed = true;
				model_to_spawn = model_paths[i];
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox("Merge imported meshes into one object", &merge_imported_meshes.value);
	draw_resource_load_error(load_error);

	}
	end();
}


GuiFPSCounter::GuiFPSCounter() :
	GuiWindow({ "fps_counter", "FPS Counter", GuiPanelDock::NONE, true, false })
{
}

void GuiFPSCounter::process(GameEngine& engine)
{
	tps = engine.get_tps();
	fps = engine.get_graphics_engine().get_fps();
	window_width = engine.get_window_width();
}

void GuiFPSCounter::draw()
{
	const auto get_num_digits = [](float f) {
		int digits = 0;
		while (f >= 10.0f)
		{
			f *= 0.1f;
			digits++;
		}
		return digits + 1;
	};
	const int PAD_PER_DIGIT = 10;
	const int TEXT_RIGHT_PADDING = 50 + PAD_PER_DIGIT * std::max(get_num_digits(fps), get_num_digits(tps));
	const uint32_t width = window_width.has_value() ? *window_width : 0;
	ImGui::SetNextWindowPos(ImVec2{ float(width - TEXT_RIGHT_PADDING), 0.0f }, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2{ float(TEXT_RIGHT_PADDING), 80.0f }, ImGuiCond_Always);
	ImGui::Begin(get_imgui_name(), nullptr,
		ImGuiWindowFlags_::ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_::ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_::ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoSavedSettings
	);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.0f, 0.0f, 0.0f, 1.0f});
	ImGui::Text("fps %.1f", fps);
	ImGui::Text("tps %.1f", tps);
	ImGui::PopStyleColor();
	ImGui::End();
}

GuiMusic::GuiMusic(AudioSource&& audio_source) :
	GuiWindow({ "audio", "Audio", GuiPanelDock::RIGHT, false }),
	audio_source(std::make_unique<AudioSource>(std::move(audio_source)))
{
	songs_paths = Utility::get_all_audio();
	std::ranges::transform(songs_paths, std::back_inserter(songs), [](const auto& path) { return path.filename().string(); });
}

GuiMusic::~GuiMusic() = default;

void GuiMusic::process(GameEngine& engine)
{
	audio_source->set_gain(gain);
	audio_source->set_pitch(pitch);
	audio_source->set_position(position);
}

void GuiMusic::draw()
{
	if (begin())
	{

	ImGui::SliderFloat("Gain", &gain, 0.0f, 1.0f);
	ImGui::SliderFloat("Pitch", &pitch, 0.0f, 2.0f);
	ImGui::SliderFloat3("Position", glm::value_ptr(position), -40.0f, 40.0f);
	if (!songs.empty() && ImGui::BeginCombo("Model", songs[selected_song].c_str()))
	{
		for (int i = 0; i < songs.size(); i++)
		{
			if (ImGui::Selectable(songs[i].c_str(), i == selected_song))
			{
				selected_song = i;
				selected_song.changed = true;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::Button("Play"))
	{
		audio_source->set_audio(songs_paths[selected_song].string());
		audio_source->play();
	}

	}
	end();
}

GuiStatistics::GuiStatistics() :
	GuiWindow({ "statistics", "Statistics", GuiPanelDock::BOTTOM })
{
}

void GuiStatistics::process(GameEngine& engine)
{
}

void GuiStatistics::draw()
{
	if (begin())
	{

	for (const auto& [label, capacity] : buffer_capacities)
	{
		static constexpr float bytes_to_mb = 1.0f / (1024.0f * 1024.0f);
		ImGui::ProgressBar(
			float(capacity.filled_capacity) / float(capacity.total_capacity), 
			ImVec2(0.0f, 0.0f), 
			label.data());
		ImGui::SameLine(); ImGui::Text("%.2f Mb", float(capacity.total_capacity) * bytes_to_mb);
	}

	}
	end();
}

void GuiStatistics::update_buffer_capacities(
	const std::vector<std::pair<size_t, size_t>>& buffer_capacities)
{
	assert(buffer_capacities.size() == 6);

	for (size_t i = 0; i < buffer_capacities.size(); ++i)
	{
		auto& [label, capacity] = this->buffer_capacities[i];
		const auto& capacity_pair = buffer_capacities[i];
		capacity.filled_capacity = capacity_pair.first;
		capacity.total_capacity = capacity_pair.second;
	}
}

GuiDebug::GuiDebug() :
	GuiWindow({ "debug", "Debug", GuiPanelDock::RIGHT })
{
}

void GuiDebug::process(GameEngine& engine)
{
	is_paused = engine.is_paused();
	if (should_toggle_pause)
	{
		engine.toggle_paused();
		is_paused = engine.is_paused();
		should_toggle_pause = false;
	}

	// TODO: fix bone visualisers
	// if (show_bone_visualisers.changed)
	// {
	// 	for (const auto entity : engine.get_ecs().get_all_skinned_entities())
	// 	{
	// 		engine.get_object(entity)->set_visibility(!show_bone_visualisers);
	// 		for (const auto visualiser : engine.get_ecs().get_skeletal_component(entity).get_visualisers())
	// 		{
	// 			engine.get_object(visualiser)->set_visibility(show_bone_visualisers);
	// 		}
	// 	}

	// 	show_bone_visualisers.changed = false;
	// }

	if (selected_object.changed)
	{
		const auto pos = engine.get_object(selected_object)->get_position();
		engine.get_camera().look_at(pos);
		engine.get_gizmo().select_object(engine.get_object(selected_object.value));
		selected_object.changed = false;
	}

	if (show_collider_visualisers.changed)
	{
		if (!show_collider_visualisers.value)
		{
			clear_collider_visualisers(engine);
		}

		show_collider_visualisers.changed = false;
	}

	if (show_collider_visualisers.value)
	{
		sync_collider_visualisers(engine);
	}

	if (should_refresh_objects_list)
	{
		object_ids.clear();
		object_ids_strs.clear();
		for (auto& [id, object] : engine.get_objects())
		{
			object_ids.push_back(id);
			const auto& name = object->get_name();
			object_ids_strs.push_back(name.empty() ? std::to_string(id.get_underlying()) : name);
		}

		should_refresh_objects_list = false;
	}

}

void GuiDebug::draw()
{
	if (begin())
	{

	if (ImGui::Button(is_paused ? "Resume" : "Pause"))
	{
		should_toggle_pause = true;
	}

	if (ImGui::Button("screenshot"))
	{
		should_take_screenshot = true;
	}

	if (ImGui::Button(is_recording ? "Stop Recording" : "Start Recording"))
	{
		if (is_recording)
			should_stop_recording = true;
		else
			should_start_recording = true;
	}
	if (is_recording)
	{
		ImGui::SameLine();
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "REC");
	}

	if (ImGui::Checkbox("Show Bone Visualisers", &show_bone_visualisers.value))
	{
		show_bone_visualisers.changed = true;
	}

	if (ImGui::Checkbox("Show Collider Visualisers", &show_collider_visualisers.value))
	{
		show_collider_visualisers.changed = true;
	}

	if (ImGui::Button("Refresh Objects List"))
	{
		should_refresh_objects_list = true;
	}

	ImGui::InputText("Filter", filter_text.data(), filter_text.size());

	if (ImGui::BeginCombo("Objects", "Select Object"))
	{
		const std::string search_text = filter_text.data();
		for (int i = 0; i < object_ids.size(); i++)
		{
			if (!search_text.empty() && object_ids_strs[i].find(search_text) == std::string::npos)
			{
				continue;
			}

			// bool is_selected = (current_item == items[n]); // You can store your selection however you want, outside or inside your objects
			if (ImGui::Selectable(object_ids_strs[i].data(), selected_object == object_ids[i]))
			{
				selected_object = object_ids[i];
				selected_object.changed = true;
			}
				// current_item = items[n];

			// if (is_selected)
			// {
			// 	ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
			// }
		}
		ImGui::EndCombo();
	}

	}
	end();
}

void GuiDebug::sync_collider_visualisers(GameEngine& engine)
{
	std::unordered_set<EntityID> seen;

	for (const auto& [entity_id, _] : engine.get_ecs().get_all_colliders())
	{
		seen.insert(entity_id);

		const Collider* collider = engine.get_ecs().get_collider(entity_id);
		if (!collider)
		{
			continue;
		}

		Object& visual = get_or_spawn_collider_visual(engine, collider_visualiser_ids, entity_id, *collider);
		collider->update_debug_object(visual);
	}

	for (auto it = collider_visualiser_ids.begin(); it != collider_visualiser_ids.end(); )
	{
		if (seen.contains(it->first))
		{
			++it;
			continue;
		}

		engine.delete_object(it->second);
		it = collider_visualiser_ids.erase(it);
	}
}

void GuiDebug::clear_collider_visualisers(GameEngine& engine)
{
	for (const auto& [_, visual_id] : collider_visualiser_ids)
	{
		engine.delete_object(visual_id);
	}

	collider_visualiser_ids.clear();
}

void GuiPhotoBase::update(void* img_rsrc, const glm::uvec2& true_img_size, uint32_t requested_width)
{
	this->img_rsrc = img_rsrc;
	true_dims = true_img_size;
}

void GuiPhotoBase::draw()
{
	if (img_rsrc)
	{
		ImGui::Image((ImTextureID)img_rsrc, ImVec2(get_requested_width(), get_requested_height()));
	}
}

GuiPhoto::GuiPhoto() :
	GuiWindow({ "texture_viewer", "Texture Viewer", GuiPanelDock::BOTTOM, false })
{
	refresh_textures();
}

void GuiPhoto::refresh_textures()
{
	std::optional<std::string> selected_path;
	if (selected_image.value >= 0 && selected_image.value < static_cast<int>(photo_paths.size()))
		selected_path = photo_paths[selected_image.value].lexically_normal().generic_string();

	photo_paths = Utility::get_all_textures();
	std::ranges::sort(photo_paths, [](const auto& lhs, const auto& rhs)
	{
		return lhs.lexically_normal().generic_string() < rhs.lexically_normal().generic_string();
	});

	photos.clear();
	photos.reserve(photo_paths.size());
	std::ranges::transform(photo_paths, std::back_inserter(photos), [](const auto& path) { return path.filename().string(); });

	selected_image = 0;
	if (selected_path)
	{
		const auto selected = std::ranges::find_if(photo_paths, [&](const auto& path)
		{
			return path.lexically_normal().generic_string() == *selected_path;
		});
		if (selected != photo_paths.end())
			selected_image = static_cast<int>(std::distance(photo_paths.begin(), selected));
	}
	selected_image.changed = true;
	load_error.reset();
}

void GuiPhoto::init(std::function<void(const std::filesystem::path&)>&& texture_requester)
{
	this->texture_requester = std::move(texture_requester);
}

void GuiPhoto::process(GameEngine& engine)
{
	if (should_refresh_textures)
	{
		should_refresh_textures = false;
		refresh_textures();
	}
}

void GuiPhoto::draw()
{
	if (begin())
	{

	const bool dropdown_open = begin_italic_combo("##texture", "(select texture)");
	if (dropdown_open && !texture_dropdown_open)
		should_refresh_textures = true;
	texture_dropdown_open = dropdown_open;

	if (dropdown_open)
	{
		for (int i = 0; i < static_cast<int>(photos.size()); i++)
		{
			if (ImGui::Selectable(photos[i].c_str(), i == selected_image.value))
			{
				selected_image = i;
				selected_image.changed = true;
				texture_to_show = photo_paths[i];
			}
		}
		ImGui::EndCombo();
	}

	ImGui::InputInt("width", &requested_width);

	if (texture_to_show)
	{
		const auto texture_path = std::move(*texture_to_show);
		texture_to_show.reset();
		if (texture_requester)
		{
			try
			{
				texture_requester(texture_path);
				selected_image.changed = false;
				load_error.reset();
			}
			catch (const ResourceLoadError& error)
			{
				load_error = report_resource_load_error(
					fmt::format("Texture Viewer failed to load '{}'", texture_path.string()), error);
				should_show = false;
			}
		}
		should_show = !load_error;
	}

	if (ImGui::Button("Hide"))
	{
		should_show = false;
	}

	if (should_show && img_rsrc && requested_width > 5)
	{
		ImGui::Image((ImTextureID)img_rsrc, ImVec2(get_requested_width(), get_requested_height()));
	}
	draw_resource_load_error(load_error);

	}
	end();
}

GuiRenderSlicer::GuiRenderSlicer() :
	GuiWindow({ "render_slicer", "RenderSlicer", GuiPanelDock::BOTTOM, false })
{
}

void GuiRenderSlicer::draw()
{
	if (begin())
	{

	if (ImGui::BeginCombo("Slices", render_slices[selected_slice].c_str()))
	{
		for (int i = 0; i < render_slices.size(); i++)
		{
			if (ImGui::Selectable(render_slices[i].c_str(), i == selected_slice))
			{
				selected_slice = i;
				selected_slice.changed = true;
				slice_requester(render_slices[i]);
				cycles_before_draw = CSTS::UPPERBOUND_SWAPCHAIN_IMAGES + 2; // 2 here is extra delay for safety
			}
		}
		ImGui::EndCombo();
	}

	if (selected_slice != 0 && cycles_before_draw == 0)
	{
		GuiPhotoBase::draw();
	}

	}
	end();

	if (cycles_before_draw > 0)
	{
		--cycles_before_draw;
	}
}

GuiAnimationSelector::GuiAnimationSelector() :
	GuiWindow({ "animation_selector", "Animation Selector", GuiPanelDock::LEFT, false })
{
	refresh_animation_files();
}

void GuiAnimationSelector::refresh_animation_files()
{
	animation_paths = Utility::get_all_animations();
	std::ranges::sort(animation_paths, [](const auto& lhs, const auto& rhs)
	{
		return lhs.lexically_normal().generic_string() < rhs.lexically_normal().generic_string();
	});
	animation_files.clear();
	animation_files.reserve(animation_paths.size());
	std::ranges::transform(animation_paths, std::back_inserter(animation_files),
		[](const auto& path){ return path.filename().string(); });
}

void GuiAnimationSelector::process(GameEngine& engine)
{
	if (should_refresh_animation_files)
	{
		should_refresh_animation_files = false;
		refresh_animation_files();
	}

	selected_skeleton.reset();
	target_status = "Select a skinned object";
	if (const auto* selected_object = engine.get_gizmo().get_selected_object())
	{
		selected_skeleton = selected_object->get_skeleton_id();
		if (selected_skeleton)
			target_status = "Target: " + selected_object->get_name();
		else
			target_status = "Selected object is not skinned";
	}

	compatible_animations.clear();
	if (selected_skeleton)
	{
		for (const auto& [id, _] : ECS::get().get_skeletal_animations())
		{
			if (ECS::get().is_animation_compatible(*selected_skeleton, id))
				compatible_animations.insert(id);
		}
	}
	if (selected_animation && !compatible_animations.contains(*selected_animation))
	{
		selected_animation.reset();
		selected_animation_name = "(select clip)";
	}

	if (animation_to_load)
	{
		const auto path = std::move(*animation_to_load);
		animation_to_load.reset();
		if (!selected_skeleton)
		{
			load_error = report_resource_load_error(
				fmt::format("Animation Selector failed to load '{}'", path.string()),
				ResourceLoadError(target_status));
		}
		else
		{
			try
			{
				auto loaded = ResourceLoader::load_animations(path, *selected_skeleton);
				for (const auto& warning : loaded.warnings)
					LOG_WARNING(Utility::get_logger(), "Animation loader warning for '{}': {}", path.string(), warning.message);
				load_error.reset();
				if (!loaded.animations.empty())
				{
					selected_animation = loaded.animations.front();
					const auto& animation = ECS::get().get_skeletal_animations().at(*selected_animation);
					selected_animation_name = animation.source + ": " + animation.name;
					compatible_animations.insert(*selected_animation);
				}
			}
			catch (const ResourceLoadError& error)
			{
				load_error = report_resource_load_error(
					fmt::format("Animation Selector failed to load '{}'", path.string()), error);
			}
		}
	}

	if (should_play)
	{
		should_play = false;
		if (!selected_skeleton || !selected_animation ||
			!compatible_animations.contains(*selected_animation))
			return;
		try
		{
			engine.get_ecs().play_animation(*selected_skeleton, *selected_animation, loop);
			load_error.reset();
		}
		catch (const std::runtime_error& error)
		{
			load_error = report_resource_load_error(
				"Animation Selector failed to play animation", ResourceLoadError(error.what()));
		}
	}
}

void GuiAnimationSelector::draw() 
{
	if (begin())
	{
	const bool file_dropdown_open = begin_italic_combo("##animation_file", "(select animation file)");
	if (file_dropdown_open && !animation_file_dropdown_open)
		should_refresh_animation_files = true;
	animation_file_dropdown_open = file_dropdown_open;
	if (file_dropdown_open)
	{
		for (size_t index = 0; index < animation_files.size(); ++index)
		{
			if (ImGui::Selectable(animation_files[index].c_str()))
				animation_to_load = animation_paths[index];
		}
		ImGui::EndCombo();
	}

	ImGui::TextWrapped("%s", target_status.c_str());

	if (ImGui::BeginCombo("Animations", selected_animation_name.c_str()))
	{
		const auto& animations = ECS::get().get_skeletal_animations();
		std::vector<AnimationID> animation_ids;
		animation_ids.reserve(animations.size());
		for (const auto& [id, _] : animations)
			animation_ids.push_back(id);
		std::ranges::sort(animation_ids, [&](const auto lhs, const auto rhs)
		{
			const auto& left = animations.at(lhs);
			const auto& right = animations.at(rhs);
			return std::tie(left.source, left.name, lhs) < std::tie(right.source, right.name, rhs);
		});
		for (const auto id : animation_ids)
		{
			const auto& animation = animations.at(id);
			const std::string label = animation.source + ": " + animation.name;
			const bool compatible = compatible_animations.contains(id);
			ImGui::PushID(static_cast<int>(id.get_underlying()));
			ImGui::BeginDisabled(!compatible);
			if (ImGui::Selectable(label.c_str(), selected_animation == id) && compatible)
			{
				selected_animation_name = label;
				selected_animation = id;
			}
			ImGui::EndDisabled();
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox("Loop", &loop);

	ImGui::SameLine();

	const bool can_play = selected_skeleton && selected_animation && compatible_animations.contains(*selected_animation);
	ImGui::BeginDisabled(!can_play);
	if (ImGui::Button("Play"))
	{
		should_play = true;
	}
	ImGui::EndDisabled();

	draw_resource_load_error(load_error);

	}
	end();
}

GuiMaterialEditor::GuiMaterialEditor() :
	GuiWindow({ "material_editor", "Material Editor", GuiPanelDock::RIGHT, false })
{
	refresh_textures();
}

void GuiMaterialEditor::refresh_textures()
{
	texture_paths = Utility::get_all_textures();
	std::ranges::sort(texture_paths, [](const auto& lhs, const auto& rhs)
	{
		return lhs.lexically_normal().generic_string() < rhs.lexically_normal().generic_string();
	});
	texture_names.clear();
	texture_names.reserve(texture_paths.size());
	std::ranges::transform(texture_paths, std::back_inserter(texture_names),
		[](const auto& path){ return path.filename().string(); });
}

void GuiMaterialEditor::process(GameEngine& engine)
{
	if (should_refresh_textures)
	{
		should_refresh_textures = false;
		refresh_textures();
	}

	if (pending_change)
	{
		auto change = std::move(*pending_change);
		pending_change.reset();
		try
		{
			engine.replace_renderable_texture(
				change.object_id, change.renderable_index, change.semantic, std::move(change.path));
			load_error.reset();
		}
		catch (const ResourceLoadError& error)
		{
			load_error = report_resource_load_error("Material Editor failed to load texture", error);
		}
		catch (const std::runtime_error& error)
		{
			load_error = report_resource_load_error(
				"Material Editor failed to update material", ResourceLoadError(error.what()));
		}
	}

	const auto previous_target = target_object;
	target_object.reset();
	renderable_labels.clear();
	compatible = false;
	diffuse_label = "(none)";
	normal_label = "(none)";
	const Object* object = engine.get_gizmo().get_selected_object();
	if (!object)
	{
		target_status = "Select an object with the gizmo";
		return;
	}

	target_object = object->get_id();
	if (previous_target != target_object)
		selected_renderable = 0;
	target_status = object->get_name().empty()
		? fmt::format("Object {}", object->get_id().get_underlying())
		: object->get_name();
	for (size_t index = 0; index < object->renderables.size(); ++index)
	{
		const auto& renderable = object->renderables[index];
		renderable_labels.push_back(fmt::format(
			"Mesh {} (ID {})", index + 1, renderable.mesh_id.get_underlying()));
	}
	if (renderable_labels.empty())
	{
		target_status += " has no renderables";
		return;
	}
	selected_renderable.value = std::clamp(
		selected_renderable.value, 0, static_cast<int>(renderable_labels.size()) - 1);
	const auto& renderable = object->renderables[selected_renderable.value];
	compatible = renderable.pipeline_render_type == ERenderType::STANDARD
		|| renderable.pipeline_render_type == ERenderType::SKINNED;
	if (!compatible)
	{
		target_status += " — selected mesh does not support textures";
		return;
	}
	if (renderable.material_ids.empty())
	{
		compatible = false;
		target_status += " — selected mesh has no material";
		return;
	}

	const auto material_label = [](const MaterialID id)
	{
		if (id == MaterialFactory::fetch_white_texture())
			return std::string("(none)");
		if (!MaterialSystem::contains(id))
			return std::string("(missing material)");
		const auto* texture = dynamic_cast<const TextureMaterial*>(&MaterialSystem::get(id));
		if (!texture || texture->source.empty())
			return fmt::format("Material {}", id.get_underlying());
		const std::filesystem::path source(texture->source);
		return source.filename().empty() ? texture->source : source.filename().string();
	};
	diffuse_label = material_label(renderable.material_ids[0]);
	if (renderable.material_ids.size() > 1)
		normal_label = material_label(renderable.material_ids[1]);
}

void GuiMaterialEditor::draw_texture_section(
	const char* title,
	const ETextureSemantic semantic,
	const std::string& current_label,
	bool& dropdown_was_open)
{
	if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
		return;
	ImGui::PushID(title);
	const bool dropdown_open = ImGui::BeginCombo("##texture", current_label.c_str());
	if (dropdown_open && !dropdown_was_open)
		should_refresh_textures = true;
	dropdown_was_open = dropdown_open;
	if (dropdown_open)
	{
		if (ImGui::Selectable("(none)", current_label == "(none)"))
			pending_change = TextureChange{
				*target_object, static_cast<size_t>(selected_renderable.value), semantic, std::nullopt };
		for (size_t index = 0; index < texture_paths.size(); ++index)
		{
			if (ImGui::Selectable(texture_names[index].c_str(), current_label == texture_names[index]))
				pending_change = TextureChange{
					*target_object, static_cast<size_t>(selected_renderable.value), semantic, texture_paths[index] };
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
}

void GuiMaterialEditor::draw()
{
	if (begin())
	{
		ImGui::TextWrapped("%s", target_status.c_str());
		if (!renderable_labels.empty() && ImGui::BeginCombo(
			"Mesh", renderable_labels[selected_renderable.value].c_str()))
		{
			for (size_t index = 0; index < renderable_labels.size(); ++index)
			{
				if (ImGui::Selectable(
					renderable_labels[index].c_str(), selected_renderable.value == static_cast<int>(index)))
					selected_renderable = static_cast<int>(index);
			}
			ImGui::EndCombo();
		}

		ImGui::BeginDisabled(!compatible || !target_object);
		draw_texture_section(
			"Diffuse Texture", ETextureSemantic::BASE_COLOR, diffuse_label, diffuse_dropdown_open);
		draw_texture_section(
			"Normal Map", ETextureSemantic::NORMAL, normal_label, normal_dropdown_open);
		ImGui::EndDisabled();
		draw_resource_load_error(load_error);
	}
	end();
}
