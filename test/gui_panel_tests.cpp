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

TEST(GuiAnimationSelector, sorts_and_hides_duplicate_clip_labels)
{
	const std::vector<GuiAnimationSelector::AnimationChoice> choices{
		{ AnimationID(3), "walk.glb: Walk" },
		{ AnimationID(2), "idle.glb: Idle" },
		{ AnimationID(1), "walk.glb: Walk" },
	};
	const auto unique = GuiAnimationSelector::sort_unique_animation_choices(choices);
	ASSERT_EQ(unique.size(), 2u);
	EXPECT_EQ(unique[0].second, "idle.glb: Idle");
	EXPECT_EQ(unique[1].first, AnimationID(1));
}
