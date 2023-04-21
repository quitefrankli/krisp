#pragma once

#include "gui/gui_manager.hpp"
#include "game_engine.hpp"
#include "objects/objects.hpp"
#include "objects/generic_objects.hpp"
#include "shapes/shape_factory.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "audio_engine/audio_source.hpp"
#include "utility.hpp"
#include "camera.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <quill/Quill.h>

#include <iostream>
#include <algorithm>
#include "gui_windows.hpp"


template<typename GameEngineT>
GuiGraphicsSettings<GameEngineT>::GuiGraphicsSettings()
{
}

template<typename GameEngineT>
void GuiGraphicsSettings<GameEngineT>::draw()
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
template<typename GameEngineT>
void GuiGraphicsSettings<GameEngineT>::process(GameEngineT& engine)
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

template<typename GameEngineT>
GuiObjectSpawner<GameEngineT>::GuiObjectSpawner()
{
	mapping = {
		{"cube", spawning_function_type([this](GameEngineT& engine, bool textured)
			{
				auto& obj = engine.template spawn_object<GenericClickableObject>(ShapeFactory::generate_cube());
				engine.add_clickable(obj.get_id(), &obj);
				if (textured)
				{
					ResourceLoader::assign_object_texture(obj, Utility::get_texture("texture.jpg").data());
				}
			})},
		{"sphere", spawning_function_type([this](GameEngineT& engine, bool textured)
			{
				auto& obj = engine.template spawn_object<GenericClickableObject>(
					ShapeFactory::generate_sphere(ShapeFactory::GenerationMethod::ICO_SPHERE, 100));
				engine.add_clickable(obj.get_id(), &obj);
			})}
	};
}

template<typename GameEngineT>
void GuiObjectSpawner<GameEngineT>::draw()
{
	ImGui::Begin("Object Spawner");
	
	ImGui::Checkbox("textured", &use_texture);

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

template<typename GameEngineT>
void GuiObjectSpawner<GameEngineT>::process(GameEngineT& engine)
{
	if (spawning_function)
	{
		(*spawning_function)(engine, use_texture);
		spawning_function = nullptr;
	}	
}

template<typename GameEngineT>
void GuiFPSCounter<GameEngineT>::process(GameEngineT& engine)
{
	tps = engine.get_tps();
	fps = engine.get_graphics_engine().get_fps();
	if (!window_width.has_value())
	{
		window_width = engine.get_window_width();
	}
}

template<typename GameEngineT>
void GuiFPSCounter<GameEngineT>::draw()
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

template<typename GameEngineT>
GuiMusic<GameEngineT>::GuiMusic(AudioSource&& audio_source) : 
	audio_source(std::make_unique<AudioSource>(std::move(audio_source)))
{
	for (const auto& entry : std::filesystem::directory_iterator(Utility::get().get_audio_path()))
	{
		if (!entry.is_directory() && entry.path().extension() == ".wav")
		{
			LOG_INFO(Utility::get().get_logger(), "GuiMusic<GameEngineT>::GuiMusic: adding {}", entry.path());
			songs_paths.push_back(entry.path());
			songs_.push_back(entry.path().filename().string());
		}
	}

	for (const auto& song : songs_)
	{
		songs.push_back(song.c_str());
	}
}

template<typename GameEngineT>
GuiMusic<GameEngineT>::~GuiMusic() = default;

template<typename GameEngineT>
void GuiMusic<GameEngineT>::process(GameEngineT& engine)
{
	audio_source->set_gain(gain);
	audio_source->set_pitch(pitch);
	audio_source->set_position(position);
}

template<typename GameEngineT>
void GuiMusic<GameEngineT>::draw()
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

template<typename GameEngineT>
void GuiStatistics<GameEngineT>::process(GameEngineT& engine)
{
}

template<typename GameEngineT>
void GuiStatistics<GameEngineT>::draw()
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
		float(staging_buffer_capacity.filled_capacity) / float(staging_buffer_capacity.total_capacity), 
		ImVec2(0.0f, 0.0f), 
		"staging buffer");
	ImGui::SameLine(); ImGui::Text("%ukB", staging_buffer_capacity.total_capacity / 1024);

	ImGui::End();
}

template<typename GameEngineT>
void GuiStatistics<GameEngineT>::update_buffer_capacities(
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
	staging_buffer_capacity.filled_capacity = buffer_capacities[5].first;
	staging_buffer_capacity.total_capacity = buffer_capacities[5].second;
}
