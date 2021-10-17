#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_frame.hpp"

#include <vulkan/vulkan.hpp>


class GraphicsEngine;
class GraphicsEngineObject;

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class GraphicsEngineSwapChain : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineSwapChain(GraphicsEngine& engine);
	~GraphicsEngineSwapChain();

	void reset();

	const int MAX_FRAMES_IN_FLIGHT = 2;
	static SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice& device, VkSurfaceKHR& surface);
	void spawn_object(GraphicsEngineObject& object);
	void draw();

public: // getters
	VkFormat get_image_format() { return swap_chain_image_format; }
	const VkExtent2D& get_extent() const { return swap_chain_extent; }
	uint32_t get_num_images() const { return frames.size(); }
	VkSwapchainKHR& get_swap_chain() { return swap_chain; }
	VkRenderPass& get_render_pass() { return render_pass; }

private:
	VkSwapchainKHR swap_chain;
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	VkRenderPass render_pass;
	std::vector<GraphicsEngineFrame> frames;

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
	// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

	// void recreate_swap_chain(); // useful for when size of window is changing

	void create_render_pass();

private: // synchronisation
	bool frame_buffer_resized = false;
	int current_frame = 0;
};