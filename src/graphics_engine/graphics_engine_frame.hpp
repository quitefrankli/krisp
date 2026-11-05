#pragma once

#include "graphics_engine_base_module.hpp"
#include "identifications.hpp"
#include "analytics.hpp"
#include "recording_session.hpp"
#include "frame_capture_session.hpp"
#include "submission_retirement_queue.hpp"
#include "runtime_performance.hpp"

#include <vulkan/vulkan.hpp>

#include <optional>
#include <filesystem>


class GraphicsEngineSwapChain;

// Note that frame refers to swap_chain frame and not actual frames
class GraphicsEngineFrame : public GraphicsEngineBaseModule
{
public:
	GraphicsEngineFrame(GraphicsEngine& engine, GraphicsEngineSwapChain& parent_swapchain, VkImage image, uint32_t image_index);
	GraphicsEngineFrame(const GraphicsEngineFrame&) = delete;
	GraphicsEngineFrame(GraphicsEngineFrame&& frame) noexcept;
	~GraphicsEngineFrame();

public:
	void update_command_buffer();
	void draw(VkSemaphore image_available_semaphore, std::uint64_t observation_submission);
	VkFence get_submission_fence() const { return fence_frame_inflight; }
	void collect_gpu_observation();

private:
	void update_uniform_buffer();
	void create_synchronisation_objects();
	void maybe_prepare_screenshot_capture();
	void flush_screenshot_capture();
	void maybe_prepare_frame_capture();
	void flush_frame_capture();
	void maybe_prepare_recording_capture();
	void flush_recording_capture();
	void prepare_gpu_observation_queries();
	
public:
	// Interprets a VkImage and describes how to access it
	VkImage presentation_image;
	VkImageView presentation_image_view;
	VkCommandBuffer command_buffer;

	const uint32_t image_index;
	
private:
	
	GraphicsEngineSwapChain& swap_chain;

	// Each frame owns the resources and presentation semaphore for one swapchain image.
	VkSemaphore render_finished_semaphore;
	VkFence fence_frame_inflight = VK_NULL_HANDLE;
	std::optional<SubmissionSerial> submission_serial;
	static constexpr uint32_t observation_query_count = 8;
	VkQueryPool observation_query_pool = VK_NULL_HANDLE;
	bool current_gpu_query_recorded = false;
	bool gpu_query_pending = false;
	RuntimeSample pending_gpu_sample{};
	std::uint32_t timestamp_valid_bits = 0;
	double timestamp_period_ns = 0.0;

	Analytics analytics;

	std::optional<GraphicsBuffer> screenshot_staging_buffer;
	std::filesystem::path screenshot_path;
	VkExtent2D screenshot_extent{};
	std::optional<GraphicsBuffer> frame_capture_staging_buffer;
	VkExtent2D frame_capture_extent{};
	std::optional<FrameCaptureSession::Target> frame_capture_target;

	std::optional<GraphicsBuffer> recording_staging_buffer;
	VkExtent2D recording_extent{};
	bool recording_has_pending_frame = false;
	std::optional<RecordingSession::CaptureTarget> recording_capture_target;
};
