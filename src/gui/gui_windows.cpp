#pragma once

#include "gui/gui_manager.hpp"
#include "game_engine.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "audio_engine/audio_source.hpp"
#include "utility.hpp"
#include "camera.hpp"
#include "graphics_engine/constants.hpp"
#include "entity_component_system/material_system.hpp"
#include "renderable/mesh_factory.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <quill/Quill.h>

#include <iostream>
#include <algorithm>
#include "gui_windows.hpp"


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
				const auto mat_id = ResourceLoader::fetch_texture(Utility::get_texture("texture.jpg").data());
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
	for (const auto& entry : std::filesystem::directory_iterator(Utility::get().get_audio_path()))
	{
		if (!entry.is_directory() && entry.path().extension() == ".wav")
		{
			LOG_INFO(Utility::get().get_logger(), "GuiMusic::GuiMusic: adding {}", entry.path());
			songs_paths.push_back(entry.path());
			songs_.push_back(entry.path().filename().string());
		}
	}

	for (const auto& song : songs_)
	{
		songs.push_back(song.c_str());
	}
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
	ImGui::Combo("Song", &selected_song, songs.data(), songs.size());
	if (ImGui::Button("Play"))
	{
		std::cout<<"Playing\n";
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

	ImGui::ProgressBar(
		float(vertex_buffer_capacity.filled_capacity) / float(vertex_buffer_capacity.total_capacity), 
		ImVec2(0.0f, 0.0f), 
		"vertex buffer");
	ImGui::SameLine(); ImGui::Text("%ukB", vertex_buffer_capacity.total_capacity / 1024);
	ImGui::ProgressBar(
		float(index_buffer_capacity.filled_capacity) / float(index_buffer_capacity.total_capacity), 
		ImVec2(0.0f, 0.0f), 
		"index buffer");
	ImGui::SameLine(); ImGui::Text("%ukB", index_buffer_capacity.total_capacity / 1024);
	ImGui::ProgressBar(
		float(uniform_buffer_capacity.filled_capacity) / float(uniform_buffer_capacity.total_capacity), 
		ImVec2(0.0f, 0.0f), 
		"uniform buffer");
	ImGui::SameLine(); ImGui::Text("%ukB", uniform_buffer_capacity.total_capacity / 1024);
	ImGui::ProgressBar(
		float(materials_buffer_capacity.filled_capacity) / float(materials_buffer_capacity.total_capacity), 
		ImVec2(0.0f, 0.0f), 
		"materials buffer");
	ImGui::SameLine(); ImGui::Text("%ukB", materials_buffer_capacity.total_capacity / 1024);
	ImGui::ProgressBar(
		float(mapping_buffer_capacity.filled_capacity) / float(mapping_buffer_capacity.total_capacity), 
		ImVec2(0.0f, 0.0f), 
		"mapping buffer");
	ImGui::SameLine(); ImGui::Text("%ukB", mapping_buffer_capacity.total_capacity / 1024);
	ImGui::ProgressBar(
		float(bone_buffer_capacity.filled_capacity) / float(bone_buffer_capacity.total_capacity), 
		ImVec2(0.0f, 0.0f), 
		"bone buffer");
	ImGui::SameLine(); ImGui::Text("%ukB", bone_buffer_capacity.total_capacity / 1024);

	ImGui::End();
}

void GuiStatistics::update_buffer_capacities(
	const std::vector<std::pair<size_t, size_t>>& buffer_capacities)
{
	assert(buffer_capacities.size() == 6);
	vertex_buffer_capacity.filled_capacity = buffer_capacities[0].first;
	vertex_buffer_capacity.total_capacity = buffer_capacities[0].second;
	index_buffer_capacity.filled_capacity = buffer_capacities[1].first;
	index_buffer_capacity.total_capacity = buffer_capacities[1].second;
	uniform_buffer_capacity.filled_capacity = buffer_capacities[2].first;
	uniform_buffer_capacity.total_capacity = buffer_capacities[2].second;
	materials_buffer_capacity.filled_capacity = buffer_capacities[3].first;
	materials_buffer_capacity.total_capacity = buffer_capacities[3].second;
	mapping_buffer_capacity.filled_capacity = buffer_capacities[4].first;
	mapping_buffer_capacity.total_capacity = buffer_capacities[4].second;
	bone_buffer_capacity.filled_capacity = buffer_capacities[5].first;
	bone_buffer_capacity.total_capacity = buffer_capacities[5].second;
}

void GuiDebug::process(GameEngine& engine)
{
	if (show_bone_visualisers.changed)
	{
		for (const auto entity : engine.get_ecs().get_all_skinned_entities())
		{
			engine.get_object(entity)->set_visibility(!show_bone_visualisers);
			for (const auto visualiser : engine.get_ecs().get_skeletal_component(entity).get_visualisers())
			{
				engine.get_object(visualiser)->set_visibility(show_bone_visualisers);
			}
		}

		show_bone_visualisers.changed = false;
	}

	if (selected_object.changed)
	{
		const auto pos = engine.get_object(selected_object)->get_position();
		engine.get_camera().look_at(pos);
		selected_object.changed = false;
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

	if (ImGui::Checkbox("Show Bone Visualisers", &show_bone_visualisers.value))
	{
		show_bone_visualisers.changed = true;
	}

	if (ImGui::Button("Refresh Objects List"))
	{
		should_refresh_objects_list = true;
	}

	if (ImGui::BeginCombo("Objects", "Select Object"))
	{
		for (int i = 0; i < object_ids.size(); i++)
		{
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
	static const std::vector<std::string_view> recognised_extensions =
	{
		".jpg", ".JPG", ".PNG", ".png"
	};
	for (const auto& entry : std::filesystem::directory_iterator(Utility::get().get_textures_path()))
	{
		if (!entry.is_directory() && 
			std::find_if(
				recognised_extensions.begin(), 
				recognised_extensions.end(), 
				[&entry](const std::string_view ext)
				{
					return entry.path().extension() == ext;
				}
			) != recognised_extensions.end())
		{
			LOG_INFO(Utility::get().get_logger(), "GuiPhoto::GuiPhoto: adding {}", entry.path());
			photo_paths.push_back(entry.path());
			photos.push_back(entry.path().filename().string());
		}
	}

	selected_image = 0;
	selected_image.changed = true;
}

void GuiPhoto::init(std::function<void(const std::string_view)>&& texture_requester)
{
	this->texture_requester = std::move(texture_requester);
}

void GuiPhoto::process(GameEngine& engine)
{
}

void GuiPhoto::draw()
{
	ImGui::Begin("Texture Viewer");

	if (ImGui::BeginCombo("Texture", photos[selected_image].c_str()))
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
			texture_requester(photo_paths[selected_image].string().c_str());
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
				cycles_before_draw = CSTS::NUM_EXPECTED_SWAPCHAIN_IMAGES * 2; // 2 here is extra delay for safety
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