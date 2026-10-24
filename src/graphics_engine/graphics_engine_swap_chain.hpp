#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_frame.hpp"

#include <vulkan/vulkan.hpp>

#include <optional>


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

	void draw();

	// TODO: deprecate this infavor of something more useful, i.e. recreating the entire swapchain
	void destroy_and_recreate_frames();

public: // getters
	VkFormat get_image_format() const { return image_format; }
	VkColorSpaceKHR get_color_space() const { return color_space; }
	
	// gets extent NOT resolution
	VkExtent2D get_extent();
	static VkExtent2D get_extent(VkPhysicalDevice physical_device, VkSurfaceKHR window_surface);
	static SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice physical_device, VkSurfaceKHR window_surface);
	static VkExtent2D choose_swap_extent(VkSurfaceKHR window_surface, const VkSurfaceCapabilitiesKHR& capabilities);

	uint32_t get_num_images() const { return frames.size(); }
	VkSwapchainKHR& get_swap_chain() { return swap_chain; }
	VkPresentModeKHR get_present_mode() const { return present_mode; }
	// assuming swapchain draw call is last in the main graphics execution loop
	// this will reflect the frame TO BE drawn
	GraphicsEngineFrame& get_curr_frame() { return frames[current_frame]; }
	GraphicsEngineFrame& get_prev_frame() { return frames[(current_frame + frames.size() - 1) % frames.size()]; }

private:
		
	VkSwapchainKHR swap_chain;
	VkFormat image_format = VK_FORMAT_UNDEFINED;
	VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	static std::optional<VkExtent2D> swap_chain_extent;
	std::vector<GraphicsEngineFrame> frames;

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
	// void recreate_swap_chain(); // useful for when size of window is changing

private:
	VkImage presentation_image;
	VkDeviceMemory presentation_image_memory;

private: // synchronisation
	bool frame_buffer_resized = false;
	int current_frame = 0;
};
