#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_frame.hpp"

#include <vulkan/vulkan.hpp>


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

	static const int EXPECTED_NUM_SWAPCHAIN_IMAGES = 3;
	static SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice& device, VkSurfaceKHR& surface);
	void update_command_buffer();
	void spawn_object(GraphicsEngineObject<GraphicsEngineT>& object);
	void draw();

public: // getters
	VkFormat get_image_format() { return VK_FORMAT_B8G8R8A8_SRGB; }
	const VkExtent2D& get_extent() const { return swap_chain_extent; }
	uint32_t get_num_images() const { return frames.size(); }
	VkSwapchainKHR& get_swap_chain() { return swap_chain; }
	static constexpr VkSampleCountFlagBits get_msaa_samples() { return msaa_samples; }
	VkImageView get_color_image_view() { return colorImageView; }
	// assuming swapchain draw call is last in the main graphics execution loop
	// this will reflect the frame TO BE drawn
	GraphicsEngineFrame<GraphicsEngineT>& get_curr_frame() { return frames[current_frame]; }
	GraphicsEngineFrame<GraphicsEngineT>& get_prev_frame() { return frames[(current_frame + frames.size() - 1) % frames.size()]; }

	virtual VkRenderPass get_render_pass() override { return render_pass; }

private:
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_graphics_engine;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_logical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_physical_device;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_instance;
	using GraphicsEngineBaseModule<GraphicsEngineT>::create_buffer;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_num_swapchain_frames;
	using GraphicsEngineBaseModule<GraphicsEngineT>::get_render_pass;
	
	VkSwapchainKHR swap_chain;
	VkExtent2D swap_chain_extent;
	std::vector<GraphicsEngineFrame<GraphicsEngineT>> frames;

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
	// extent = resolution of the swap chain images and ~ resolution of window we are drawing to
	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

	// void recreate_swap_chain(); // useful for when size of window is changing

	// for now the render_pass is simply shared between pipelines
	VkRenderPass render_pass;

	void create_render_pass();

private: // color resources for MSAA sampling where we have multiple samples per pixel
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;
	static constexpr VkSampleCountFlagBits msaa_samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_4_BIT;

private: // synchronisation
	bool frame_buffer_resized = false;
	int current_frame = 0;
};