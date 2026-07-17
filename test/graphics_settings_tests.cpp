#include "gui/gui_windows.hpp"

#include <gtest/gtest.h>

TEST(GraphicsSettings, defaults_to_rasterized_rendering)
{
	GuiGraphicsSettings settings;

	EXPECT_EQ(settings.get_render_mode(), ERenderMode::RASTERIZED);
	EXPECT_FALSE(settings.render_mode.changed);
}

TEST(GraphicsSettings, selecting_a_render_mode_replaces_the_active_mode)
{
	GuiGraphicsSettings settings;

	EXPECT_TRUE(settings.select_render_mode(ERenderMode::UNLIT_BASE_COLOR));
	EXPECT_EQ(settings.get_render_mode(), ERenderMode::UNLIT_BASE_COLOR);
	EXPECT_TRUE(settings.render_mode.changed);

	settings.render_mode.changed = false;
	EXPECT_FALSE(settings.select_render_mode(ERenderMode::UNLIT_BASE_COLOR));
	EXPECT_FALSE(settings.render_mode.changed);

	EXPECT_TRUE(settings.select_render_mode(ERenderMode::WIREFRAME));
	EXPECT_EQ(settings.get_render_mode(), ERenderMode::WIREFRAME);
}
