#pragma once

#include "graphics_engine_base_module.hpp"
#include "graphics_engine_frame.hpp"

#include <vulkan/vulkan.hpp>

#include <optional>
#include <vector>


struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct SwapChainDrawOutcome
{
	bool presented = false;
	bool recreate = false;
};

class GraphicsEngineSwapChain : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineSwapChain(GraphicsEngine& engine);
	~GraphicsEngineSwapChain();

	void recreate(VkExtent2D framebuffer_extent);
	void drain();
	SwapChainDrawOutcome draw();

	void destroy_and_recreate_frames(const std::vector<VkImage>& images);

public: // getters
	VkFormat get_image_format() const { return image_format; }
	VkColorSpaceKHR get_color_space() const { return color_space; }
	
	// gets extent NOT resolution
	VkExtent2D get_extent() const { return extent; }
	static VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D framebuffer_extent);
	static SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice physical_device, VkSurfaceKHR window_surface);

	uint32_t get_num_images() const { return frames.size(); }
	VkSwapchainKHR& get_swap_chain() { return swap_chain; }
	VkPresentModeKHR get_present_mode() const { return present_mode; }

private:
		
	VkSwapchainKHR swap_chain = VK_NULL_HANDLE;
	VkFormat image_format = VK_FORMAT_B8G8R8A8_SRGB;
	VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	VkExtent2D extent{};
	std::vector<GraphicsEngineFrame> frames;

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
	VkSwapchainKHR create_swap_chain(VkExtent2D framebuffer_extent, VkSwapchainKHR old_swap_chain);
	std::vector<VkImage> get_swap_chain_images(VkSwapchainKHR swap_chain);

private: // synchronisation
	struct AcquisitionSlot
	{
		VkSemaphore image_available = VK_NULL_HANDLE;
		// Borrowed from the image frame whose submission last consumed this semaphore.
		VkFence last_submission = VK_NULL_HANDLE;
	};
	std::vector<AcquisitionSlot> acquisition_slots;
	std::size_t current_acquisition_slot = 0;
};
