#include "gui_animation_selector.hpp"

#include "gui_window_helpers.hpp"

#include "game_engine.hpp"
#include "input.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"
#include "utility.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <quill/LogMacros.h>

#include <algorithm>

namespace
{
constexpr float ANIMATION_FRAME_STEP_SECS = 1.0f / 30.0f;
}

using GuiWindowDetail::draw_resource_load_error;
using GuiWindowDetail::report_resource_load_error;

GuiAnimationSelector::GuiAnimationSelector() :
	EngineUiWindow({ "animation_selector", "Animation Selector", GuiPanelDock::LEFT, false })
{
	refresh_animation_files();
}

void GuiAnimationSelector::refresh_animation_files()
{
	animation_paths = Utility::get_all_animations();
	std::ranges::sort(animation_paths);
}

std::vector<GuiAnimationSelector::AnimationChoice> GuiAnimationSelector::sort_animation_choices(
	std::vector<AnimationChoice> choices)
{
	std::ranges::sort(choices, [](const auto& lhs, const auto& rhs)
	{
		return std::tie(lhs.second, lhs.first) < std::tie(rhs.second, rhs.first);
	});
	return choices;
}

std::vector<GuiAnimationSelector::AnimationChoice>
GuiAnimationSelector::animation_choices_for_rig(
	const std::unordered_map<AnimationID, SkeletalAnimation>& animations,
	const SkeletalRigSignature& rig_signature)
{
	std::vector<AnimationChoice> choices;
	for (const auto& [id, animation] : animations)
		if (animation.rig_signature == rig_signature)
			choices.emplace_back(id, animation.source + ": " + animation.name);
	return sort_animation_choices(std::move(choices));
}

bool GuiAnimationSelector::animation_source_is_loaded(
	const std::unordered_map<AnimationID, SkeletalAnimation>& animations,
	const SkeletalRigSignature& rig_signature,
	const std::string_view source)
{
	return std::ranges::any_of(animations, [&](const auto& entry)
	{
		const auto& animation = entry.second;
		return animation.rig_signature == rig_signature && animation.source == source;
	});
}

std::optional<AnimationID> GuiAnimationSelector::cycle_animation_choice(
	const std::vector<AnimationChoice>& choices,
	const std::optional<AnimationID> current,
	const int direction)
{
	if (choices.empty())
		return std::nullopt;
	const auto current_choice = current
		? std::ranges::find(choices, *current, &AnimationChoice::first) : choices.end();
	if (current_choice == choices.end())
		return direction < 0 ? choices.back().first : choices.front().first;
	const auto current_index = std::distance(choices.begin(), current_choice);
	const auto next_index = direction < 0
		? (current_index + choices.size() - 1) % choices.size()
		: (current_index + 1) % choices.size();
	return choices[next_index].first;
}

bool GuiAnimationSelector::handle_key_input(const KeyInput& input)
{
	using enum EInputAction;
	using enum EKeyModifier;

	if (!is_visible() || !selected_skeleton || animation_choices.empty()
		|| input.modifier != NONE
		|| (input.key != GLFW_KEY_LEFT && input.key != GLFW_KEY_RIGHT))
		return false;
	if (input.action == PRESS || input.action == REPEAT)
	{
		const int direction = input.key == GLFW_KEY_LEFT ? -1 : 1;
		selected_animation = cycle_animation_choice(
			animation_choices, selected_animation, direction);
		for (const auto& [id, label] : animation_choices)
			if (selected_animation == id)
			{
				selected_animation_name = label;
				break;
			}
		should_play = true;
		paused = false;
	}
	return true;
}

void GuiAnimationSelector::process(GameEngine& engine)
{
	if (should_refresh_animation_files)
	{
		should_refresh_animation_files = false;
		refresh_animation_files();
		if (selected_animation_path >= static_cast<int>(animation_paths.size()))
			selected_animation_path = -1;
	}

	const auto previous_skeleton = selected_skeleton;
	selected_skeleton.reset();
	target_status = "Select a skinned object";
	if (const auto* selected_object = engine.get_gizmo().get_selected_object())
	{
		std::vector<SkeletonID> skeletons;
		for (const RenderableID id : engine.get_ecs().get_renderable_ids(selected_object->get_id()))
			if (const auto skeleton = engine.get_ecs().get_renderable(id).skeleton_id)
				skeletons.push_back(*skeleton);
		std::ranges::sort(skeletons);
		skeletons.erase(std::ranges::unique(skeletons).begin(), skeletons.end());
		if (skeletons.size() == 1)
			selected_skeleton = skeletons.front();
		if (selected_skeleton)
			target_status = "Target: " + selected_object->get_name();
		else if (skeletons.size() > 1)
			target_status = "Selected object has multiple skeletons";
		else
			target_status = "Selected object is not skinned";
	}
	if (selected_skeleton != previous_skeleton)
	{
		playback_active = false;
		paused = false;
		if (selected_skeleton)
		{
			const auto rig_signature = make_skeletal_rig_signature(
				engine.get_ecs().get_skeletal_component(*selected_skeleton).get_bones());
			animation_choices = animation_choices_for_rig(
				engine.get_ecs().get_skeletal_animations(), rig_signature);
		}
		else
			animation_choices.clear();
		if (selected_animation
			&& std::ranges::find(animation_choices, *selected_animation, &AnimationChoice::first)
				== animation_choices.end())
		{
			selected_animation.reset();
			selected_animation_name = "(select clip)";
		}
	}

	if (pending_animation_file)
	{
		auto request = std::move(*pending_animation_file);
		pending_animation_file.reset();
		bool animation_choices_changed = false;
		if (!engine.get_ecs().has_skeleton(request.skeleton))
		{
			load_error = report_resource_load_error(
				"Animation Selector failed to load animation file",
				ResourceLoadError("Selected skeleton no longer exists"));
		}
		else
		{
			const auto rig_signature = make_skeletal_rig_signature(
				engine.get_ecs().get_skeletal_component(request.skeleton).get_bones());
			const auto& animations = engine.get_ecs().get_skeletal_animations();
			if (animation_source_is_loaded(animations, rig_signature, request.path))
			{
				load_error.reset();
				animation_choices_changed = true;
			}
			else try
			{
				auto loaded = ResourceLoader::load_animations(
					engine.get_ecs(), request.path, request.skeleton);
				for (const auto& warning : loaded.warnings)
					LOG_WARNING(Utility::get_logger(), "Animation loader warning for '{}': {}",
						request.path, warning.message);
				animation_choices_changed = true;
				load_error.reset();
			}
			catch (const ResourceLoadError& error)
			{
				load_error = report_resource_load_error(
					"Animation Selector failed to load animation file", error);
			}
		}
		if (animation_choices_changed && selected_skeleton == request.skeleton)
		{
			const auto rig_signature = make_skeletal_rig_signature(
				engine.get_ecs().get_skeletal_component(request.skeleton).get_bones());
			animation_choices = animation_choices_for_rig(
				engine.get_ecs().get_skeletal_animations(), rig_signature);
		}
	}

	if (selected_animation)
	{
		for (const auto& [id, label] : animation_choices)
		{
			if (id == *selected_animation)
			{
				selected_animation_name = label;
				break;
			}
		}
	}

	if (should_play)
	{
		should_play = false;
		if (!selected_skeleton || !selected_animation)
			return;
		if (engine.get_ecs().play_animation(*selected_skeleton, *selected_animation, loop))
		{
			engine.get_ecs().set_animation_speed(*selected_skeleton, playback_speed);
			load_error.reset();
			playback_active = true;
		}
		else
		{
			load_error = report_resource_load_error(
				"Animation Selector failed to play animation",
				ResourceLoadError("Animation is missing or incompatible with the selected skeleton"));
			playback_active = false;
		}
	}

	if (should_stop)
	{
		should_stop = false;
		if (selected_skeleton)
			engine.get_ecs().stop_animation(*selected_skeleton);
	}

	if (pause_request)
	{
		if (selected_skeleton)
			engine.get_ecs().set_animation_paused(*selected_skeleton, *pause_request);
		pause_request.reset();
	}

	if (loop_request)
	{
		if (selected_skeleton)
			engine.get_ecs().set_animation_looping(*selected_skeleton, *loop_request);
		loop_request.reset();
	}

	if (speed_request)
	{
		if (selected_skeleton)
			engine.get_ecs().set_animation_speed(*selected_skeleton, *speed_request);
		speed_request.reset();
	}

	if (pending_step_secs != 0.0f)
	{
		if (selected_skeleton)
			engine.get_ecs().step_animation(*selected_skeleton, pending_step_secs);
		pending_step_secs = 0.0f;
	}

	if (seek_request_secs)
	{
		if (selected_skeleton)
			engine.get_ecs().seek_animation(*selected_skeleton, *seek_request_secs);
		seek_request_secs.reset();
	}

	const auto playback = selected_skeleton
		? engine.get_ecs().get_animation_playback(*selected_skeleton)
		: std::nullopt;
	playback_active = playback.has_value();
	if (playback)
	{
		selected_animation = playback->animation_id;
		loop = playback->looping;
		paused = playback->paused;
		elapsed_secs = playback->elapsed_secs;
		duration_secs = playback->duration_secs;
		playback_speed = playback->speed;
		for (const auto& [id, label] : animation_choices)
		{
				if (id == playback->animation_id)
				{
					selected_animation_name = label;
					break;
				}
		}
	}
	else
	{
		paused = false;
		elapsed_secs = 0.0f;
		duration_secs = selected_animation
			&& engine.get_ecs().get_skeletal_animations().contains(*selected_animation)
			? engine.get_ecs().get_animation_duration(*selected_animation)
			: 0.0f;
	}
}

void GuiAnimationSelector::draw()
{
	if (begin())
	{
	ImGui::TextWrapped("%s", target_status.c_str());

	if (ImGui::Button("Refresh animation files"))
		should_refresh_animation_files = true;
	const char* selected_file_name = selected_animation_path >= 0
		&& selected_animation_path < static_cast<int>(animation_paths.size())
		? animation_paths[selected_animation_path].c_str() : "(select file)";
	if (ImGui::BeginCombo("Animation files", selected_file_name))
	{
		for (int index = 0; index < static_cast<int>(animation_paths.size()); ++index)
		{
			const bool selected = selected_animation_path == index;
			if (ImGui::Selectable(animation_paths[index].c_str(), selected))
			{
				selected_animation_path = index;
				if (selected_skeleton)
					pending_animation_file = AnimationFileLoadRequest{
						.skeleton = *selected_skeleton, .path = animation_paths[index] };
			}
		}
		ImGui::EndCombo();
	}

	const bool combo_open = ImGui::BeginCombo("Animations", selected_animation_name.c_str());
	if (combo_open)
	{
		for (const auto& [id, label] : animation_choices)
		{
			const std::string id_string = std::to_string(id.get_underlying());
			ImGui::PushID(id_string.c_str());
			if (ImGui::Selectable(label.c_str(), selected_animation == id))
			{
				selected_animation_name = label;
				selected_animation = id;
				should_play = true;
				paused = false;
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	ImGui::Text("%.2f / %.2f s", elapsed_secs, duration_secs);
	const bool can_seek = selected_skeleton && selected_animation
		&& duration_secs > 0.0f;
	ImGui::BeginDisabled(!can_seek);
	if (ImGui::SliderFloat(
		"##AnimationTimeline", &elapsed_secs, 0.0f, duration_secs, "%.2f s"))
	{
		if (!playback_active)
			should_play = true;
		paused = true;
		pause_request = true;
		seek_request_secs = elapsed_secs;
	}
	ImGui::EndDisabled();

	if (ImGui::Checkbox("Loop", &loop) && playback_active)
		loop_request = loop;

	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::InputFloat("Speed", &playback_speed, 0.0f, 0.0f, "%.2f"))
	{
		if (playback_active)
			speed_request = playback_speed;
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!playback_active);
	if (ImGui::Button("Stop"))
	{
		should_stop = true;
		playback_active = false;
		paused = false;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	const bool can_play = selected_skeleton && selected_animation;
	ImGui::BeginDisabled(!playback_active && !can_play);
	if (ImGui::Button(playback_active && !paused ? "Pause" : "Play"))
	{
		if (playback_active)
		{
			paused = !paused;
			pause_request = paused;
		}
		else
		{
			should_play = true;
			paused = false;
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!playback_active);
	if (ImGui::ArrowButton("##PreviousAnimationFrame", ImGuiDir_Left))
	{
		paused = true;
		pause_request = true;
		pending_step_secs -= ANIMATION_FRAME_STEP_SECS * playback_speed;
	}
	ImGui::SameLine();
	if (ImGui::ArrowButton("##NextAnimationFrame", ImGuiDir_Right))
	{
		paused = true;
		pause_request = true;
		pending_step_secs += ANIMATION_FRAME_STEP_SECS * playback_speed;
	}
	ImGui::EndDisabled();

	draw_resource_load_error(load_error);

	}
	end();
}
