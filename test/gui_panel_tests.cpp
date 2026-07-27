#include "gui/gui_windows.hpp"
#include "gui/gui_manager.hpp"
#include "audio_engine/audio_engine_pimpl.hpp"

#include <gtest/gtest.h>

namespace
{
class TestGuiPanel : public GuiWindow
{
public:
	explicit TestGuiPanel(GuiPanelInfo info) : GuiWindow(std::move(info)) {}

	void draw() override {}
};
}

TEST(GuiPanel, stores_stable_docking_metadata)
{
	TestGuiPanel panel({ "asset_preview", "Asset Preview", GuiPanelDock::BOTTOM, false });

	EXPECT_EQ(panel.get_panel_info().id, "asset_preview");
	EXPECT_EQ(panel.get_panel_info().title, "Asset Preview");
	EXPECT_EQ(panel.get_panel_info().default_dock, GuiPanelDock::BOTTOM);
	EXPECT_STREQ(panel.get_imgui_name(), "Asset Preview###asset_preview");
	EXPECT_FALSE(panel.is_visible());
}

TEST(GuiPanel, save_manager_is_registered_and_visible_by_default)
{
	GuiManager manager;
	EXPECT_EQ(manager.save_manager.get_panel_info().id, "save_manager");
	EXPECT_EQ(manager.save_manager.get_panel_info().title, "Save Manager");
	EXPECT_TRUE(manager.save_manager.is_visible());
}

TEST(GuiPanel, visibility_can_be_restored_after_closing)
{
	TestGuiPanel panel({ "debug", "Debug", GuiPanelDock::RIGHT });

	EXPECT_TRUE(panel.is_visible());
	panel.set_visible(false);
	EXPECT_FALSE(panel.is_visible());
	panel.set_visible(true);
	EXPECT_TRUE(panel.is_visible());
}

TEST(GuiPanel, manager_applies_saved_visibility_and_resets_defaults)
{
	GuiManager manager;
	manager.get_or_create_saved_panel_visibility("debug") = false;
	manager.get_or_create_saved_panel_visibility("texture_viewer") = true;
	manager.apply_saved_panel_visibility();

	EXPECT_FALSE(manager.debug.is_visible());
	EXPECT_TRUE(manager.photo.is_visible());
	EXPECT_FALSE(manager.material_editor.is_visible());

	manager.reset_panel_visibility();
	EXPECT_TRUE(manager.debug.is_visible());
	EXPECT_FALSE(manager.photo.is_visible());
	EXPECT_FALSE(manager.material_editor.is_visible());
}

TEST(GuiPanel, dynamically_spawned_panels_restore_saved_visibility)
{
	GuiManager manager;
	manager.get_or_create_saved_panel_visibility("dynamic_panel") = false;
	auto& panel = manager.spawn_gui<TestGuiPanel>(
		GuiPanelInfo{ "dynamic_panel", "Dynamic Panel", GuiPanelDock::RIGHT, true });

	EXPECT_FALSE(panel.is_visible());
}

TEST(GuiMusic, safely_selects_only_existing_songs)
{
	const std::vector<std::string> songs{ "first.wav", "second.ogg" };
	EXPECT_FALSE(GuiMusic::selected_path({}, 0));
	EXPECT_FALSE(GuiMusic::selected_path(songs, -1));
	EXPECT_FALSE(GuiMusic::selected_path(songs, 2));
	EXPECT_EQ(GuiMusic::selected_path(songs, 1), songs[1]);
}

TEST(GuiMusic, sorts_audio_paths_alphabetically)
{
	const auto paths = GuiMusic::sort_paths({ "Zulu.wav", "alpha.wav", "middle.wav" });
	EXPECT_EQ(paths, (std::vector<std::string>{ "alpha.wav", "middle.wav", "Zulu.wav" }));
}

TEST(GuiAnimationSelector, sorts_and_keeps_all_animation_choices)
{
	const std::vector<GuiAnimationSelector::AnimationChoice> choices{
		{ AnimationID(3), "walk.glb: Walk" },
		{ AnimationID(2), "idle.glb: Idle" },
		{ AnimationID(1), "walk.glb: Walk" },
	};
	const auto sorted = GuiAnimationSelector::sort_animation_choices(choices);
	ASSERT_EQ(sorted.size(), 3u);
	EXPECT_EQ(sorted[0].second, "idle.glb: Idle");
	EXPECT_EQ(sorted[1].first, AnimationID(1));
	EXPECT_EQ(sorted[2].first, AnimationID(3));
}

TEST(GuiAnimationSelector, discovers_only_animations_for_the_selected_rig)
{
	const SkeletalRigSignature selected_rig{ { .name = "root" } };
	const SkeletalRigSignature other_rig{ { .name = "other" } };
	const std::unordered_map<AnimationID, SkeletalAnimation> animations{
		{ AnimationID(1), { .name = "Walk", .source = "movement.glb",
			.rig_signature = selected_rig } },
		{ AnimationID(2), { .name = "Idle", .source = "movement.glb",
			.rig_signature = selected_rig } },
		{ AnimationID(3), { .name = "Walk", .source = "movement.glb",
			.rig_signature = other_rig } },
	};

	const auto choices =
		GuiAnimationSelector::animation_choices_for_rig(animations, selected_rig);

	ASSERT_EQ(choices.size(), 2u);
	EXPECT_EQ(choices[0], GuiAnimationSelector::AnimationChoice(
		AnimationID(2), "movement.glb: Idle"));
	EXPECT_EQ(choices[1], GuiAnimationSelector::AnimationChoice(
		AnimationID(1), "movement.glb: Walk"));
	EXPECT_TRUE(GuiAnimationSelector::animation_source_is_loaded(
		animations, selected_rig, "movement.glb"));
	EXPECT_FALSE(GuiAnimationSelector::animation_source_is_loaded(
		animations, other_rig, "missing.glb"));
}

TEST(GuiAnimationSelector, cycles_animation_choices_with_wraparound)
{
	const std::vector<GuiAnimationSelector::AnimationChoice> choices{
		{ AnimationID(1), "Idle" },
		{ AnimationID(2), "Walk" },
		{ AnimationID(3), "Run" },
	};

	EXPECT_EQ(
		GuiAnimationSelector::cycle_animation_choice(choices, AnimationID(2), 1), AnimationID(3));
	EXPECT_EQ(
		GuiAnimationSelector::cycle_animation_choice(choices, AnimationID(3), 1), AnimationID(1));
	EXPECT_EQ(
		GuiAnimationSelector::cycle_animation_choice(choices, AnimationID(1), -1), AnimationID(3));
	EXPECT_EQ(
		GuiAnimationSelector::cycle_animation_choice(choices, std::nullopt, 1), AnimationID(1));
	EXPECT_EQ(
		GuiAnimationSelector::cycle_animation_choice(choices, std::nullopt, -1), AnimationID(3));
	EXPECT_FALSE(GuiAnimationSelector::cycle_animation_choice({}, std::nullopt, 1));
}
