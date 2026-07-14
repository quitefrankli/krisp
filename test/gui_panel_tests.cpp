#include "gui/gui_windows.hpp"

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
