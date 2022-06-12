#pragma once

#include "gui/gui_manager.hpp"
#include "game_engine.hpp"
#include "objects/objects.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "audio_engine/audio_source.hpp"
#include "utility.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <algorithm>


template<typename GameEngineT>
GuiGraphicsSettings<GameEngineT>::GuiGraphicsSettings() :
	light_ray(0.0f, 3.0f, 0.0f, 0.0f, -1.0f, 0.0f)
{
}

template<typename GameEngineT>
void GuiGraphicsSettings<GameEngineT>::draw()
{
	ImGui::Begin("Graphics Settings");

	ImGui::SliderFloat("lighting", &light_strength, 0.0f, 1.0f);
	ImGui::SliderFloat3("light position", glm::value_ptr(light_ray.origin), -50.0f, 50.0f);
	ImGui::SliderFloat3("light direction", glm::value_ptr(light_ray.direction), -1.0f, 1.0f);
	
	ImGui::End();
}

template<typename GameEngineT>
GuiObjectSpawner<GameEngineT>::GuiObjectSpawner()
{
	mapping = {
		{"cube", spawning_function_type([this](GameEngineT& engine, bool textured)
			{
				auto& obj = engine.template spawn_object<Cube>();
				if (textured)
				{
					ResourceLoader::assign_object_texture(obj, Utility::get_texture("texture.jpg").data());
				} 
			})},
		{"sphere", spawning_function_type([this](GameEngineT& engine, bool textured){ 
			textured ? engine.template spawn_object<Sphere>() : engine.template spawn_object<Sphere>(); })}
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
	ImGui::SetWindowPos(ImVec2{ float(*window_width - TEXT_RIGHT_PADDING), 0.0f });
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
			std::cout << "GuiMusic<GameEngineT>::GuiMusic: adding " << entry.path() << '\n';
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
}
