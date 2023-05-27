#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_frame.hpp"

#include <vulkan/vulkan.hpp>

#include <optional>


template<typename GraphicsEngineT>
class GraphicsEngineObject;

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

template<typename GraphicsEngineT>
class GraphicsEngineSwapChain : public GraphicsEngineBaseModule<GraphicsEngineT>
{
public:
	GraphicsEngineSwapChain(GraphicsEngineT& engine);
	~GraphicsEngineSwapChain();

	void reset();

	static constexpr int EXPECTED_NUM_SWAPCHAIN_IMAGES = CSTS::NUM_EXPECTED_SWAPCHAIN_IMAGES;
	void update_command_buffer();
	void draw();

	// TODO: deprecate this infavor of something more useful, i.e. recreating the entire swapchain
	void destroy_and_recreate_frames();

public: // getters
	VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	
	// gets extent NOT resolution
	VkExtent2D get_extent();
	static VkExtent2D get_extent(VkPhysicalDevice physical_device, VkSurfaceKHR window_surface);
	static SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice physical_device, VkSurfaceKHR window_surface);
	static VkExtent2D choose_swap_extent(VkSurfaceKHR window_surface, const VkSurfaceCapabilitiesKHR& capabilities);

	uint32_t get_num_images() const { return frames.size(); }
	VkSwapchainKHR& get_swap_chain() { return swap_chain; }
	// assuming swapchain draw call is last in the main graphics execution loop
	// this will reflect the frame TO BE drawn
	GraphicsEngineFrame<GraphicsEngineT>& get_curr_frame() { return frames[current_frame]; }
	GraphicsEngineFrame<GraphicsEngineT>& get_prev_frame() { return frames[(current_frame + frames.size() - 1) % frames.size()]; }

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
		
	VkSwapchainKHR swap_chain;
	static std::optional<VkExtent2D> swap_chain_extent;
	std::vector<GraphicsEngineFrame<GraphicsEngineT>> frames;

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