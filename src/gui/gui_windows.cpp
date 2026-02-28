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
#include "renderable/mesh_factory.hpp"
#include "interface/gizmo.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <quill/LogMacros.h>

#include <iostream>
#include <algorithm>
#include <unordered_set>
#include "gui_windows.hpp"

namespace
{
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


GuiGraphicsSettings::GuiGraphicsSettings() = default;

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

	ImGui::Begin("Graphics Settings");

	ImGui::SliderFloat("lighting", &light_strength, 0.0f, 1.0f);
	rtx_on.changed = ImGui::Checkbox("RTX", &rtx_on.value);
	wireframe_mode.changed |= ImGui::Checkbox("wireframe", &wireframe_mode.value);
	ImGui::SetNextItemWidth(combo_width);
	selected_camera_projection.changed |= ImGui::Combo(
		"projection", 
		&selected_camera_projection.value, 
		camera_projections.data(), 
		camera_projections.size());

	ImGui::End();
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

GuiObjectSpawner::GuiObjectSpawner()
{
	mapping = {
		{"cube", spawning_function_type([this](GameEngine& engine)
			{
				auto& obj = engine.template spawn_object<Object>(
					Renderable::make_default(MeshFactory::cube_id(MeshFactory::EVertexType::COLOR)));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<SphereCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
			})
		},
		{"textured_cube", spawning_function_type([this](GameEngine& engine)
			{
				const auto mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
				const auto mat_id = ResourceLoader::fetch_texture(Utility::get_texture("texture.jpg"));
				Renderable renderable;
				renderable.mesh_id = mesh_id;
				renderable.material_ids = { mat_id };
				renderable.pipeline_render_type = ERenderType::STANDARD;

				auto obj = std::make_shared<Object>(renderable);
				engine.get_ecs().add_object(*obj);
				engine.get_ecs().add_collider(obj->get_id(), std::make_unique<SphereCollider>());
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
				engine.get_ecs().add_collider(obj->get_id(), std::make_unique<SphereCollider>());
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
	ImGui::Begin("Object Spawner");

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
	
	ImGui::End();
}

void GuiObjectSpawner::process(GameEngine& engine)
{
	if (spawning_function)
	{
		(*spawning_function)(engine);
		spawning_function = nullptr;
	}	
}

GuiModelSpawner::GuiModelSpawner()
{
	model_paths = Utility::get_all_models();
	std::ranges::transform(
		model_paths, 
		std::back_inserter(models), 
		[](const auto& path) { return path.filename().string(); });
}

void GuiModelSpawner::process(GameEngine& engine)
{
	if (should_spawn)
	{
		should_spawn = false;
		auto loaded_model = ResourceLoader::load_model(model_paths[selected_model]);
		std::vector<Renderable> all_renderables;
		for (const auto& mesh : loaded_model.meshes)
		{
			all_renderables.insert(all_renderables.end(), mesh.renderables.begin(), mesh.renderables.end());
		}
		auto mesh = std::make_shared<Object>(all_renderables);
		mesh->set_transform(loaded_model.onload_transform.get_mat4());
		const auto name = model_paths[selected_model].stem().string();
		mesh->set_name(name);
		Object& object = engine.spawn_object(std::move(mesh));
		engine.get_ecs().add_collider(object.get_id(), std::make_unique<SphereCollider>());
		engine.get_ecs().add_clickable_entity(object.get_id());
	}
}

void GuiModelSpawner::draw() 
{
	ImGui::Begin("Model Spawner");

	if (!models.empty() && ImGui::BeginCombo("Model", models[selected_model].c_str()))
	{
		for (int i = 0; i < models.size(); i++)
		{
			if (ImGui::Selectable(models[i].c_str(), i == selected_model))
			{
				selected_model = i;
				selected_model.changed = true;
			}
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Spawn"))
	{
		should_spawn = true;
	}

	ImGui::End();
}


void GuiFPSCounter::process(GameEngine& engine)
{
	tps = engine.get_tps();
	fps = engine.get_graphics_engine().get_fps();
	if (!window_width.has_value())
	{
		window_width = engine.get_window_width();
	}
}

void GuiFPSCounter::draw()
{
	ImGui::Begin("FPS Counter", nullptr, 
		ImGuiWindowFlags_::ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_::ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_::ImGuiWindowFlags_NoScrollbar
	);
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
	ImGui::SetWindowPos(ImVec2{ float(width - TEXT_RIGHT_PADDING), 0.0f });
	ImGui::SetWindowSize(ImVec2{ float(TEXT_RIGHT_PADDING), 80.0f });
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.0f, 0.0f, 0.0f, 1.0f});
	ImGui::Text("fps %.1f", fps);
	ImGui::Text("tps %.1f", tps);
	ImGui::PopStyleColor();
	ImGui::End();
}

GuiMusic::GuiMusic(AudioSource&& audio_source) : 
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
	ImGui::Begin("Audio");

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

	ImGui::End();
}

void GuiStatistics::process(GameEngine& engine)
{
}

void GuiStatistics::draw()
{
	ImGui::Begin("Statistics");

	for (const auto& [label, capacity] : buffer_capacities)
	{
		static constexpr float bytes_to_mb = 1.0f / (1024.0f * 1024.0f);
		ImGui::ProgressBar(
			float(capacity.filled_capacity) / float(capacity.total_capacity), 
			ImVec2(0.0f, 0.0f), 
			label.data());
		ImGui::SameLine(); ImGui::Text("%.2f Mb", float(capacity.total_capacity) * bytes_to_mb);
	}

	ImGui::End();
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
	ImGui::Begin("Debug");

	if (ImGui::Button(is_paused ? "Resume" : "Pause"))
	{
		should_toggle_pause = true;
	}

	if (ImGui::Button("screenshot"))
	{
		should_take_screenshot = true;
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

	ImGui::End();
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

GuiPhoto::GuiPhoto()
{
	photo_paths = Utility::get_all_textures();
	std::ranges::transform(photo_paths, std::back_inserter(photos), [](const auto& path) { return path.filename().string(); });

	selected_image = 0;
	selected_image.changed = true;
}

void GuiPhoto::init(std::function<void(const std::filesystem::path&)>&& texture_requester)
{
	this->texture_requester = std::move(texture_requester);
}

void GuiPhoto::process(GameEngine& engine)
{
}

void GuiPhoto::draw()
{
	ImGui::Begin("Texture Viewer");

	const std::string_view current_texture = selected_image < photos.size() ? photos[selected_image] : "N/A";
	if (ImGui::BeginCombo("Texture", current_texture.data()))
	{
		for (int i = 0; i < photos.size(); i++)
		{
			if (ImGui::Selectable(photos[i].c_str(), i == selected_image))
			{
				selected_image = i;
				selected_image.changed = true;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::InputInt("width", &requested_width);

	if (ImGui::Button("Show"))
	{
		if (selected_image.changed)
		{
			texture_requester(photo_paths[selected_image]);
			selected_image.changed = false;
		}
		should_show = true;
	}

	ImGui::SameLine();
	if (ImGui::Button("Hide"))
	{
		should_show = false;
	}

	if (should_show && img_rsrc && requested_width > 5)
	{
		ImGui::Image((ImTextureID)img_rsrc, ImVec2(get_requested_width(), get_requested_height()));
	}

	ImGui::End();
}

void GuiRenderSlicer::draw()
{
	ImGui::Begin("RenderSlicer");

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

	ImGui::End();

	if (cycles_before_draw > 0)
	{
		--cycles_before_draw;
	}
}

void GuiAnimationSelector::process(GameEngine& engine) 
{
	if (should_play)
	{
		should_play = false;

		auto* selected_object = engine.get_gizmo().get_selected_object();
		if (!selected_object)
		{
			return;
		}

		const auto& renderables = selected_object->renderables;
		if (renderables.size() != 1 || renderables[0].pipeline_render_type != ERenderType::SKINNED)
		{
			return;
		}

		engine.get_ecs().play_animation(renderables[0].skeleton_id.value(), selected_animation.value(), loop);
	}
}

void GuiAnimationSelector::draw() 
{
	ImGui::Begin("Animation Selector");

	if (ImGui::BeginCombo("Animations", selected_animation_name.c_str()))
	{
		const auto& animations = ECS::get().get_skeletal_animations();
		for (const auto& [id, animation] : animations)
		{
			if (ImGui::Selectable(animation.name.c_str(), selected_animation == id))
			{
				selected_animation_name = animation.name;
				selected_animation = id;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox("Loop", &loop);

	ImGui::SameLine();

	if (ImGui::Button("Play"))
	{
		if (selected_animation)
		{
			should_play = true;
		}
	}

	ImGui::End();
}
