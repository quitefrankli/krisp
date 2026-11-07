#include "graphics_engine/graphics_engine_swap_chain.hpp"
#include <gtest/gtest.h>
#include <limits>

TEST(SwapchainSelection, UsesRequiredSurfaceExtent)
{
	VkSurfaceCapabilitiesKHR caps{};
	caps.currentExtent = {800, 600};
	const auto extent = GraphicsEngineSwapChain::choose_swap_extent(caps, {1280, 720});
	EXPECT_EQ(extent.width, 800u);
	EXPECT_EQ(extent.height, 600u);
}

TEST(SwapchainSelection, ClampsFramebufferPixelsWhenSurfaceAllowsSelection)
{
	VkSurfaceCapabilitiesKHR caps{};
	caps.currentExtent = {UINT32_MAX, UINT32_MAX};
	caps.minImageExtent = {64, 64};
	caps.maxImageExtent = {1920, 1080};
	const auto extent = GraphicsEngineSwapChain::choose_swap_extent(caps, {2560, 32});
	EXPECT_EQ(extent.width, 1920u);
	EXPECT_EQ(extent.height, 64u);
}
