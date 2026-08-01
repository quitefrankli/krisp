#include "gui_windows.hpp"

#include "gui_window_helpers.hpp"

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

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cctype>

using GuiWindowDetail::begin_italic_combo;
using GuiWindowDetail::draw_resource_load_error;
using GuiWindowDetail::report_resource_load_error;

bool GuiWindow::begin(const int flags, const bool closable)
{
	const bool was_visible = visible;
	const bool expanded = ImGui::Begin(
		get_imgui_name(), closable ? get_visible_ptr() : nullptr, flags);
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

GuiSaveManager::GuiSaveManager() :
	EngineUiWindow({ "save_manager", "Save Manager", GuiPanelDock::LEFT }),
	store(Utility::get_saves_path())
{
}

void GuiSaveManager::queue(const Action action, const std::string& name)
{
	if (!pending)
		pending = Request{ action, name };
}

void GuiSaveManager::draw()
{
	if (begin())
	{
		if (ImGui::BeginTable("SaveFiles", 2,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
			ImVec2(0.0f, 220.0f)))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Modified");
			ImGui::TableHeadersRow();
			for (const auto& entry : entries)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (ImGui::Selectable(entry.name.c_str(), selected == entry.name,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
				{
					selected = entry.name;
					const auto length = std::min(entry.name.size(), name_buffer.size() - 1);
					std::ranges::copy_n(entry.name.begin(), length, name_buffer.begin());
					name_buffer[length] = '\0';
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						queue(Action::LOAD, entry.name);
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(entry.modified_label.c_str());
			}
			ImGui::EndTable();
		}

		ImGui::InputText("Save name", name_buffer.data(), name_buffer.size());
		ImGui::BeginDisabled(pending.has_value());
		if (ImGui::Button("Save"))
			queue(Action::SAVE, name_buffer.data());
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!selected || pending.has_value());
		if (ImGui::Button("Delete"))
			queue(Action::DELETE_SAVE, *selected);
		ImGui::EndDisabled();

		if (!status.empty())
		{
			if (status_is_error)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.25f, 0.25f, 1.0f));
			ImGui::TextWrapped("%s", status.c_str());
			if (status_is_error)
				ImGui::PopStyleColor();
		}
	}
	end();
}

void GuiSaveManager::process(GameEngine& engine)
{
	std::optional<Request> request;
	bool refresh = false;
	request = std::move(pending);
	pending.reset();
	refresh = std::exchange(refresh_requested, false);
	if (!request && !refresh)
		return;

	try
	{
		std::string result;
		if (request)
		{
			switch (request->action)
			{
				case Action::SAVE:
					engine.save_scene(request->name);
					result = "Saved '" + request->name + "'.";
					break;
				case Action::LOAD:
					engine.load_scene(request->name);
					result = "Loaded '" + request->name + "'.";
					break;
				case Action::DELETE_SAVE:
					if (!store.remove(request->name))
						throw std::runtime_error("Save no longer exists");
					result = "Deleted '" + request->name + "'.";
					break;
			}
		}
		const auto refreshed = store.list();
		entries = refreshed;
		status = std::move(result);
		status_is_error = false;
		if (request && request->action == Action::DELETE_SAVE)
			selected.reset();
		else if (request)
			selected = request->name;
	}
	catch (const std::exception& error)
	{
		status = error.what();
		status_is_error = true;
	}
}


GuiGraphicsSettings::GuiGraphicsSettings() :
	EngineUiWindow({ "graphics_settings", "Graphics Settings", GuiPanelDock::RIGHT })
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
	for (const auto& [label, mode] : std::array{
		std::pair{ "Rasterized", ERenderMode::RASTERIZED },
		// std::pair{ "RTX", ERenderMode::RAYTRACING },
		std::pair{ "Wireframe", ERenderMode::WIREFRAME },
		std::pair{ "Unlit Base Color", ERenderMode::UNLIT_BASE_COLOR },
	})
	{
		if (ImGui::RadioButton(label, render_mode.value == mode))
			select_render_mode(mode);
	}
	ImGui::SetNextItemWidth(combo_width);
	selected_camera_projection.changed |= ImGui::Combo(
		"projection",
		&selected_camera_projection.value,
		camera_projections.data(),
		camera_projections.size());

	}
	end();
}

bool GuiGraphicsSettings::select_render_mode(const ERenderMode mode)
{
	if (render_mode.value == mode)
		return false;
	render_mode.value = mode;
	render_mode.changed = true;
	return true;
}

void GuiGraphicsSettings::process(GameEngine& engine)
{
	if (selected_camera_projection.changed)
	{
		engine.get_camera().toggle_projection();
		selected_camera_projection.changed = false;
	}

	if (render_mode.changed)
	{
		engine.set_render_mode(render_mode.value);
		render_mode.changed = false;
	}
}

GuiObjectSpawner::GuiObjectSpawner() :
	EngineUiWindow({ "object_spawner", "Object Spawner", GuiPanelDock::LEFT })
{
	mapping = {
		{"cube", spawning_function_type([this](GameEngine& engine)
			{
				auto& obj = engine.template spawn_object<Object>();
				engine.attach_renderable(obj.get_id(), Renderable::make_default(
					MeshSystem::add(MeshFactory::cube(MeshFactory::EVertexType::COLOR))));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<BoxCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
			})
		},
		{"textured_cube", spawning_function_type([this](GameEngine& engine)
			{
				auto mesh_owner = MeshSystem::add(
					MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
				auto material_owner = ResourceLoader::fetch_texture("texture.jpg");
				Renderable renderable;
				renderable.pipeline_render_type = ERenderType::STANDARD;
				renderable.mesh_owner = std::move(mesh_owner);
				renderable.material_owners = { std::move(material_owner) };

				auto& obj = engine.template spawn_object<Object>();
				engine.attach_renderable(obj.get_id(), std::move(renderable));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<BoxCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
			})
		},
		{"diffuse_cube", spawning_function_type([this](GameEngine& engine)
			{
				ColorMaterial material;
				material.data.shininess = 1.0f;
				Renderable renderable;
				renderable.mesh_owner = MeshSystem::add(
					MeshFactory::cube(MeshFactory::EVertexType::COLOR));
				auto material_owner = MaterialSystem::add(
					std::make_unique<ColorMaterial>(std::move(material)));
				renderable.material_owners = { std::move(material_owner) };
				auto& obj = engine.template spawn_object<Object>();
				engine.attach_renderable(obj.get_id(), std::move(renderable));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<BoxCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
			})
		},
		{"sphere", spawning_function_type([this](GameEngine& engine)
			{
				auto& obj = engine.template spawn_object<Object>();
				engine.attach_renderable(obj.get_id(), Renderable::make_default(
					MeshSystem::add(MeshFactory::sphere(
						MeshFactory::EVertexType::COLOR,
						MeshFactory::GenerationMethod::ICO_SPHERE,
						100))));
				engine.get_ecs().add_collider(obj.get_id(), std::make_unique<SphereCollider>());
				engine.get_ecs().add_clickable_entity(obj.get_id());
			})
		},
		{"physics sphere", spawning_function_type([this](GameEngine& engine)
			{
				auto& obj = engine.template spawn_object<Object>();
				engine.attach_renderable(obj.get_id(), Renderable::make_default(
					MeshSystem::add(MeshFactory::sphere(
						MeshFactory::EVertexType::COLOR,
						MeshFactory::GenerationMethod::ICO_SPHERE))));
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

GuiMusic::GuiMusic(AudioSource&& audio_source) :
	EngineUiWindow({ "audio", "Audio", GuiPanelDock::RIGHT, false }),
	audio_source(std::make_unique<AudioSource>(std::move(audio_source)))
{
	songs_paths = sort_paths(Utility::get_all_audio());
	songs = songs_paths;
}

GuiMusic::~GuiMusic() = default;

std::optional<std::string> GuiMusic::selected_path(
	const std::vector<std::string>& paths,
	const int selected_index)
{
	if (selected_index < 0 || static_cast<size_t>(selected_index) >= paths.size())
		return std::nullopt;
	return paths[selected_index];
}

std::vector<std::string> GuiMusic::sort_paths(std::vector<std::string> paths)
{
	std::ranges::sort(paths, [](const std::string& lhs, const std::string& rhs) {
		const bool precedes = std::lexicographical_compare(
			lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](const unsigned char left, const unsigned char right) {
				return std::tolower(left) < std::tolower(right);
			});
		const bool follows = std::lexicographical_compare(
			rhs.begin(), rhs.end(), lhs.begin(), lhs.end(), [](const unsigned char left, const unsigned char right) {
				return std::tolower(left) < std::tolower(right);
			});
		return precedes || (!follows && lhs < rhs);
	});
	return paths;
}

void GuiMusic::process(GameEngine&)
{
	std::optional<std::string> requested_audio;
	float requested_gain;
	float requested_pitch;
	glm::vec3 requested_position;
	bool requested_loop;
	requested_audio = std::move(audio_to_play);
	audio_to_play.reset();
	requested_gain = gain;
	requested_pitch = pitch;
	requested_position = position;
	requested_loop = loop;

	try
	{
		audio_source->set_gain(requested_gain);
		audio_source->set_pitch(requested_pitch);
		audio_source->set_position(requested_position);
		audio_source->set_loop(requested_loop);
		if (requested_audio)
		{
			audio_source->set_audio(
				Utility::get_audio(*requested_audio).string(), AudioLoadMode::STREAM);
			audio_source->play();
			load_error.reset();
		}
	}
	catch (const std::exception& error)
	{
		load_error = error.what();
	}
}

void GuiMusic::draw()
{
	if (begin())
	{

	ImGui::SliderFloat("Gain", &gain, 0.0f, 3.0f);
	ImGui::SliderFloat("Pitch", &pitch, 0.0f, 2.0f);
	ImGui::SliderFloat3("Position", glm::value_ptr(position), -40.0f, 40.0f);
	ImGui::Checkbox("Loop", &loop);
	const auto selected = selected_path(songs_paths, selected_song);
	const std::string preview = selected.value_or("No audio files found");
	if (!songs.empty() && ImGui::BeginCombo("Audio", preview.c_str()))
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
	ImGui::BeginDisabled(!selected.has_value());
	if (ImGui::Button("Play"))
		audio_to_play = *selected;
	ImGui::EndDisabled();
	draw_resource_load_error(load_error);

	}
	end();
}

GuiStatistics::GuiStatistics() :
	EngineUiWindow({ "statistics", "Statistics", GuiPanelDock::BOTTOM })
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
	assert(buffer_capacities.size() == this->buffer_capacities.size());

	for (size_t i = 0; i < buffer_capacities.size(); ++i)
	{
		auto& [label, capacity] = this->buffer_capacities[i];
		const auto& capacity_pair = buffer_capacities[i];
		capacity.filled_capacity = capacity_pair.first;
		capacity.total_capacity = capacity_pair.second;
	}
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
	EngineUiWindow({ "texture_viewer", "Texture Viewer", GuiPanelDock::BOTTOM, false })
{
	refresh_textures();
}

void GuiPhoto::refresh_textures()
{
	std::optional<std::string> selected_path;
	if (selected_image.value >= 0 && selected_image.value < static_cast<int>(photo_paths.size()))
		selected_path = photo_paths[selected_image.value];

	photo_paths = Utility::get_all_textures();
	std::ranges::sort(photo_paths);

	photos = photo_paths;

	selected_image = 0;
	if (selected_path)
	{
		const auto selected = std::ranges::find(photo_paths, *selected_path);
		if (selected != photo_paths.end())
			selected_image = static_cast<int>(std::distance(photo_paths.begin(), selected));
	}
	selected_image.changed = true;
	load_error.reset();
}

void GuiPhoto::init(std::function<void(std::string_view)>&& texture_requester)
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
					fmt::format("Texture Viewer failed to load '{}'", texture_path), error);
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
	EngineUiWindow({ "render_slicer", "RenderSlicer", GuiPanelDock::BOTTOM, false })
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
