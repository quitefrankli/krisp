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


GuiGraphicsSettings::GuiGraphicsSettings() :
	light_ray(0.0f, 3.0f, 0.0f, 0.0f, -1.0f, 0.0f)
{
}

void GuiGraphicsSettings::draw()
{
	ImGui::Begin("Graphics Settings");

	ImGui::SliderFloat("lighting", &light_strength, 0.0f, 1.0f);
	ImGui::SliderFloat3("light position", glm::value_ptr(light_ray.origin), -50.0f, 50.0f);
	ImGui::SliderFloat3("light direction", glm::value_ptr(light_ray.direction), -1.0f, 1.0f);
	
	ImGui::End();
}

GuiObjectSpawner::GuiObjectSpawner()
{
	mapping = {
		{"cube", spawning_function_type([this](GameEngine& engine, bool textured)
			{
				auto& obj = engine.spawn_object<Cube>();
				if (textured)
				{
					ResourceLoader::assign_object_texture(obj, Utility::get_texture("texture.jpg").data());
				} 
			})},
		{"sphere", spawning_function_type([this](GameEngine& engine, bool textured){ 
			textured ? engine.spawn_object<Sphere>() : engine.spawn_object<Sphere>(); })}
	};
}

void GuiObjectSpawner::draw()
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

void GuiObjectSpawner::process(GameEngine& engine)
{
	if (spawning_function)
	{
		(*spawning_function)(engine, use_texture);
		spawning_function = nullptr;
	}	
}

GuiFPSCounter::GuiFPSCounter(unsigned initial_window_width)
{
	window_width = initial_window_width;
}

void GuiFPSCounter::draw()
{
	ImGui::Begin("FPS Counter", nullptr, 
		ImGuiWindowFlags_::ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_::ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_::ImGuiWindowFlags_NoScrollbar
	);
	const int TEXT_RIGHT_PADDING = 50;
	ImGui::SetWindowPos(ImVec2{ float(window_width - TEXT_RIGHT_PADDING), 0.0f });
	ImGui::Text("%.1f", fps);
	ImGui::End();
}

GuiMusic::GuiMusic(AudioSource&& audio_source) : 
	audio_source(std::make_unique<AudioSource>(std::move(audio_source)))
{
	for (const auto& entry : std::filesystem::directory_iterator(Utility::get().get_audio_path()))
	{
		if (!entry.is_directory() && entry.path().extension() == ".wav")
		{
			std::cout << "GuiMusic::GuiMusic: adding " << entry.path() << '\n';
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



