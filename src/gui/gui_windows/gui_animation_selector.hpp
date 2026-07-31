#pragma once

#include "gui_windows.hpp"

#include "entity_component_system/skeletal.hpp"

#include <unordered_map>

class GuiAnimationSelector : public EngineUiWindow
{
public:
	using AnimationChoice = std::pair<AnimationID, std::string>;

	GuiAnimationSelector();
	void process(GameEngine& engine) override;
	void draw() override;
	bool handle_key_input(const KeyInput& input);
	static std::vector<AnimationChoice> sort_animation_choices(
		std::vector<AnimationChoice> choices);
	static std::vector<AnimationChoice> animation_choices_for_rig(
		const std::unordered_map<AnimationID, SkeletalAnimation>& animations,
		const SkeletalRigSignature& rig_signature);
	static bool animation_source_is_loaded(
		const std::unordered_map<AnimationID, SkeletalAnimation>& animations,
		const SkeletalRigSignature& rig_signature,
		std::string_view source);
	static std::optional<AnimationID> cycle_animation_choice(
		const std::vector<AnimationChoice>& choices,
		std::optional<AnimationID> current,
		int direction);

private:
	struct AnimationFileLoadRequest
	{
		SkeletonID skeleton;
		std::string path;
	};

	void refresh_animation_files();

	std::vector<std::string> animation_paths;
	std::optional<SkeletonID> selected_skeleton;
	std::vector<AnimationChoice> animation_choices;
	std::optional<AnimationFileLoadRequest> pending_animation_file;
	int selected_animation_path = -1;
	bool should_refresh_animation_files = false;
	std::optional<AnimationID> selected_animation;
	std::string selected_animation_name = "(select clip)";
	std::string target_status = "Select a skinned object";
	std::optional<std::string> load_error;
	bool loop = false;
	bool should_play = false;
	bool should_stop = false;
	std::optional<bool> loop_request;
	std::optional<bool> pause_request;
	std::optional<float> speed_request;
	std::optional<float> seek_request_secs;
	float pending_step_secs = 0.0f;
	float elapsed_secs = 0.0f;
	float duration_secs = 0.0f;
	float playback_speed = SkeletalAnimationSystem::DEFAULT_PLAYBACK_SPEED;
	bool paused = false;
	bool playback_active = false;
	// process() runs on the game thread while draw() runs on the graphics
	// thread. All selector state, including the cached animation IDs, is shared.
	std::mutex state_mutex;
};
