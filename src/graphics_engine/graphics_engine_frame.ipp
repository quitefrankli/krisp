#pragma once

#include "graphics_engine_frame.hpp"

#include "graphics_engine.hpp"
#include "graphics_engine_swap_chain.hpp"
#include "shared_data_structures.hpp"
#include "pipeline/pipeline.hpp"
#include "renderers/renderers.hpp"
#include "renderable/render_types.hpp"

#include <glm/gtx/string_cast.hpp>

#include <array>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <utility>


GraphicsEngineFrame::GraphicsEngineFrame(
	GraphicsEngine& engine, 
	GraphicsEngineSwapChain& parent_swapchain, 
	VkImage presentation_image, uint32_t image_index) :
	GraphicsEngineBaseModule(engine),
	presentation_image(presentation_image),
	swap_chain(parent_swapchain),
	image_index(image_index),
	analytics(std::string("Frame ") + std::to_string(image_index), 60)
{
	try
	{
		presentation_image_view = get_graphics_engine().create_image_view(
			presentation_image, swap_chain.get_image_format(), VK_IMAGE_ASPECT_COLOR_BIT);
		for (auto& [_, renderer] : engine.get_renderer_mgr().get_renderers())
			renderer->allocate_per_frame_resources(presentation_image, presentation_image_view);

		create_synchronisation_objects();
		command_buffer = get_rsrc_mgr().create_command_buffer();
		prepare_gpu_observation_queries();
	}
	catch (...)
	{
		if (command_buffer != VK_NULL_HANDLE)
			vkFreeCommandBuffers(get_logical_device(), get_graphics_engine().get_command_pool(), 1, &command_buffer);
		if (observation_query_pool != VK_NULL_HANDLE)
			vkDestroyQueryPool(get_logical_device(), observation_query_pool, nullptr);
		if (render_finished_semaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(get_logical_device(), render_finished_semaphore, nullptr);
		if (fence_frame_inflight != VK_NULL_HANDLE)
			vkDestroyFence(get_logical_device(), fence_frame_inflight, nullptr);
		if (present_fence != VK_NULL_HANDLE)
			vkDestroyFence(get_logical_device(), present_fence, nullptr);
		if (presentation_image_view != VK_NULL_HANDLE)
			vkDestroyImageView(get_logical_device(), presentation_image_view, nullptr);
		throw;
	}
}

GraphicsEngineFrame::GraphicsEngineFrame(GraphicsEngineFrame&& frame) noexcept :
	GraphicsEngineBaseModule(frame.get_graphics_engine()),
	presentation_image(std::move(frame.presentation_image)),
	presentation_image_view(std::move(frame.presentation_image_view)),
	command_buffer(std::move(frame.command_buffer)),
	image_index(std::move(frame.image_index)),
	swap_chain(frame.swap_chain),
	render_finished_semaphore(std::move(frame.render_finished_semaphore)),
	fence_frame_inflight(std::move(frame.fence_frame_inflight)),
	present_fence(std::move(frame.present_fence)),
	present_pending(frame.present_pending),
	submission_pending(frame.submission_pending),
	submission_serial(std::move(frame.submission_serial)),
	observation_query_pool(std::exchange(frame.observation_query_pool, VK_NULL_HANDLE)),
	current_gpu_query_recorded(frame.current_gpu_query_recorded),
	gpu_query_pending(frame.gpu_query_pending),
	pending_gpu_sample(std::move(frame.pending_gpu_sample)),
	timestamp_valid_bits(frame.timestamp_valid_bits),
	timestamp_period_ns(frame.timestamp_period_ns),
	analytics(std::move(frame.analytics)),
	screenshot_staging_buffer(std::move(frame.screenshot_staging_buffer)),
	screenshot_path(std::move(frame.screenshot_path)),
	screenshot_extent(std::move(frame.screenshot_extent)),
	frame_capture_staging_buffer(std::move(frame.frame_capture_staging_buffer)),
	frame_capture_extent(std::move(frame.frame_capture_extent)),
	frame_capture_target(std::move(frame.frame_capture_target)),
	recording_staging_buffer(std::move(frame.recording_staging_buffer)),
	recording_extent(std::move(frame.recording_extent)),
	recording_has_pending_frame(frame.recording_has_pending_frame),
	recording_capture_target(std::move(frame.recording_capture_target))
{
	frame.should_destroy = false;
	frame.presentation_image_view = VK_NULL_HANDLE;
	frame.command_buffer = VK_NULL_HANDLE;
	frame.render_finished_semaphore = VK_NULL_HANDLE;
	frame.fence_frame_inflight = VK_NULL_HANDLE;
	frame.present_fence = VK_NULL_HANDLE;
	frame.current_gpu_query_recorded = false;
	frame.gpu_query_pending = false;
	frame.present_pending = false;
	frame.submission_pending = false;
}

GraphicsEngineFrame::~GraphicsEngineFrame()
{
	if (!this->should_destroy)
	{
		return;
	}

	if (command_buffer != VK_NULL_HANDLE)
		vkFreeCommandBuffers(get_logical_device(), get_graphics_engine().get_command_pool(), 1, &command_buffer);
	if (observation_query_pool != VK_NULL_HANDLE)
		vkDestroyQueryPool(get_logical_device(), observation_query_pool, nullptr);

	// cleanup synchronisation objects
	if (render_finished_semaphore != VK_NULL_HANDLE)
		vkDestroySemaphore(get_logical_device(), render_finished_semaphore, nullptr);
	if (fence_frame_inflight != VK_NULL_HANDLE)
		vkDestroyFence(get_logical_device(), fence_frame_inflight, nullptr);
	if (present_fence != VK_NULL_HANDLE)
		vkDestroyFence(get_logical_device(), present_fence, nullptr);
	if (presentation_image_view != VK_NULL_HANDLE)
		vkDestroyImageView(get_logical_device(), presentation_image_view, nullptr);
}

void GraphicsEngineFrame::wait_for_completion()
{
	if (submission_pending && vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight,
		VK_TRUE, std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS)
		throw std::runtime_error("failed to wait for frame submission");
	if (present_pending && vkWaitForFences(get_logical_device(), 1, &present_fence,
		VK_TRUE, std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS)
		throw std::runtime_error("failed to wait for presentation completion");
	present_pending = false;
}

void GraphicsEngineFrame::drain()
{
	if (submission_pending)
	{
		if (vkWaitForFences(get_logical_device(), 1, &fence_frame_inflight, VK_TRUE,
			std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS)
			throw std::runtime_error("failed to drain frame submission");
		submission_pending = false;
		collect_gpu_observation();
		if (submission_serial)
		{
			get_graphics_engine().complete_graphics_submission(*submission_serial);
			submission_serial.reset();
		}
		flush_screenshot_capture();
		flush_frame_capture();
		flush_recording_capture();
	}
	if (present_pending)
	{
		if (vkWaitForFences(get_logical_device(), 1, &present_fence, VK_TRUE,
			std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS)
			throw std::runtime_error("failed to drain swap chain presentation");
		present_pending = false;
	}
}

void GraphicsEngineFrame::prepare_gpu_observation_queries()
{
	if (!get_graphics_engine().get_runtime_options().observation)
		return;

	timestamp_valid_bits = get_graphics_engine().get_timestamp_valid_bits();
	timestamp_period_ns = get_graphics_engine().get_timestamp_period();
	if (timestamp_valid_bits == 0 || !std::isfinite(timestamp_period_ns) || timestamp_period_ns <= 0.0)
		return;

	VkQueryPoolCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	create_info.queryCount = observation_query_count;
	if (vkCreateQueryPool(get_logical_device(), &create_info, nullptr, &observation_query_pool) != VK_SUCCESS)
		observation_query_pool = VK_NULL_HANDLE;
}

void GraphicsEngineFrame::collect_gpu_observation()
{
	if (!gpu_query_pending || observation_query_pool == VK_NULL_HANDLE)
		return;
	const auto fence_status = vkGetFenceStatus(get_logical_device(), fence_frame_inflight);
	if (fence_status == VK_NOT_READY)
		return;

	std::array<std::uint64_t, observation_query_count * 2> results{};
	const auto result = fence_status == VK_SUCCESS
		? vkGetQueryPoolResults(
			get_logical_device(), observation_query_pool, 0, observation_query_count, sizeof(results), results.data(),
			sizeof(std::uint64_t) * 2,
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT)
		: fence_status;

	const auto collected_ns = runtime_now_ns();
	const auto record = [&](RuntimeSampleKind kind, uint32_t first_query)
	{
		auto sample = pending_gpu_sample;
		sample.kind = kind;
		sample.end_ns = collected_ns;
		const auto first = first_query * 2;
		if (result == VK_SUCCESS && results[first + 1] != 0 && results[first + 3] != 0)
		{
			auto duration = timestamp_duration_ns(
				results[first], results[first + 2], timestamp_valid_bits, timestamp_period_ns);
			if (duration && timestamp_valid_bits < 64)
			{
				const long double wrap_period_ns = std::ldexp(
					static_cast<long double>(timestamp_period_ns), timestamp_valid_bits);
				const long double enclosing_cpu_ns = sample.end_ns >= sample.begin_ns
					? static_cast<long double>(sample.end_ns - sample.begin_ns)
					: wrap_period_ns;
				if (enclosing_cpu_ns >= wrap_period_ns)
					duration.reset();
			}
			sample.success = duration.has_value();
			if (duration)
				sample.duration_ns = *duration;
		}
		else
			sample.success = false;

		if (const auto& observation = get_graphics_engine().get_runtime_options().observation)
			observation->record_graphics(sample);
	};
	record(RuntimeSampleKind::GpuShadow, 2);
	record(RuntimeSampleKind::GpuMain, 4);
	record(RuntimeSampleKind::GpuUi, 6);
	// The total closes the submission after all pass samples have been published.
	record(RuntimeSampleKind::Gpu, 0);
	gpu_query_pending = false;
}

void GraphicsEngineFrame::update_command_buffer()
{
	current_gpu_query_recorded = false;
	VkCommandBufferResetFlags reset_flags = 0;
	vkResetCommandBuffer(command_buffer, reset_flags);

	// starting command buffer recording
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = nullptr;

	if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to begin recording command buffer!");
	}
	const auto& observation = get_graphics_engine().get_runtime_options().observation;
	const bool measure_gpu = observation && observation->active()
		&& observation_query_pool != VK_NULL_HANDLE;
	if (measure_gpu)
	{
		vkCmdResetQueryPool(command_buffer, observation_query_pool, 0, observation_query_count);
		// Timestamp endpoints retain their existing measurement stages.
		vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, observation_query_pool, 0);
	}

	auto& renderer_mgr = get_graphics_engine().get_renderer_mgr();
	const auto submit_draw_commands = [&](const ERendererType renderer_type,
		const std::optional<uint32_t> first_query = std::nullopt)
	{
		if (measure_gpu && first_query)
			vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
				observation_query_pool, *first_query);
		renderer_mgr.get_renderer(renderer_type).submit_draw_commands(command_buffer, presentation_image_view, image_index);
		if (measure_gpu && first_query)
			vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
				observation_query_pool, *first_query + 1);
	};
	get_graphics_engine().get_texture_compositor().record_pending(command_buffer);
	// Ray tracing is unsupported:
	// if (get_graphics_engine().get_render_mode() == ERenderMode::RAYTRACING)
	//     submit_draw_commands(ERendererType::RAYTRACING);
	submit_draw_commands(ERendererType::SHADOW_MAP, 2);
	submit_draw_commands(ERendererType::RASTERIZATION, 4);
	submit_draw_commands(ERendererType::PRESENTATION);
	maybe_prepare_frame_capture();
	// Note: Particle rendering is now done within the RasterizationRenderer
	submit_draw_commands(ERendererType::QUAD);
	maybe_prepare_screenshot_capture();

	submit_draw_commands(ERendererType::GUI, 6); // ImGui is composed after the SDR screenshot copy.

	maybe_prepare_recording_capture();
	if (measure_gpu)
	{
		vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, observation_query_pool, 1);
		current_gpu_query_recorded = true;
	}
	
	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

VkResult GraphicsEngineFrame::draw(VkSemaphore image_available_semaphore, std::uint64_t observation_submission)
{
	// The swapchain has already acquired this image. Wait before updating its resources.

	const auto& observation = get_graphics_engine().get_runtime_options().observation;
	const bool collect_cpu = observation && observation->active();
	const auto& render_frame = get_graphics_engine().get_render_frame();
	const auto record_cpu_sample = [&](const RuntimeSampleKind kind, const std::uint64_t begin_ns,
		const std::uint64_t end_ns, const bool success = true)
	{
		if (!collect_cpu)
			return;
		observation->record_graphics(RuntimeSample{
			.kind = kind,
			.begin_ns = begin_ns,
			.end_ns = end_ns,
			.submission_id = observation_submission,
			.frame_number = render_frame.frame_number,
			.tick_index = render_frame.simulation_tick,
			.publication_ns = render_frame.publication_ns,
			.success = success });
	};

	analytics.start();
	// CPU-GPU synchronisation for in flight images
	const auto frame_fence_begin_ns = collect_cpu ? runtime_now_ns() : 0;
	const auto frame_fence_result = vkWaitForFences(
			get_logical_device(), 1, &fence_frame_inflight, VK_TRUE,
			std::numeric_limits<uint64_t>::max());
	const auto frame_fence_end_ns = collect_cpu ? runtime_now_ns() : 0;
	analytics.stop();
	record_cpu_sample(RuntimeSampleKind::ImageFenceWait,
		frame_fence_begin_ns, frame_fence_end_ns, frame_fence_result == VK_SUCCESS);
	if (frame_fence_result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to wait for in-flight frame!");
	}
	submission_pending = false;
	collect_gpu_observation();
	if (submission_serial)
	{
		get_graphics_engine().complete_graphics_submission(*submission_serial);
		submission_serial.reset();
	}

	auto& shadows = static_cast<ShadowMapRenderer&>(
		get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::SHADOW_MAP));
	shadows.prepare_frame(image_index, prepare_point_lighting(
		render_frame.lights, render_frame.view.shadows_enabled));
	const auto recording_begin_ns = collect_cpu ? runtime_now_ns() : 0;
	update_command_buffer();
	record_cpu_sample(RuntimeSampleKind::CommandRecording, recording_begin_ns, collect_cpu ? runtime_now_ns() : 0);

	const auto uniform_begin_ns = collect_cpu ? runtime_now_ns() : 0;
	update_uniform_buffer();
	record_cpu_sample(RuntimeSampleKind::UniformUpload, uniform_begin_ns, collect_cpu ? runtime_now_ns() : 0);

	VkSemaphoreSubmitInfo image_available{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
	image_available.semaphore = image_available_semaphore;
	// Match the swap-chain image's first attachment transition, allowing earlier
	// offscreen work to execute without waiting for image acquisition.
	image_available.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo render_finished{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
	render_finished.semaphore = render_finished_semaphore;
	// Presentation must wait for every rendering, capture, and query command.
	render_finished.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkCommandBufferSubmitInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
	commands.commandBuffer = command_buffer;
	VkSubmitInfo2 submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
	submit_info.waitSemaphoreInfoCount = 1;
	submit_info.pWaitSemaphoreInfos = &image_available;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &commands;
	submit_info.signalSemaphoreInfoCount = 1;
	submit_info.pSignalSemaphoreInfos = &render_finished;

	vkResetFences(get_logical_device(), 1, &fence_frame_inflight);
	const auto submit_begin_ns = collect_cpu ? runtime_now_ns() : 0;
	const auto submit_result = vkQueueSubmit2(
		get_graphics_engine().get_graphics_queue(), 1, &submit_info, fence_frame_inflight);
	const auto submit_end_ns = collect_cpu ? runtime_now_ns() : 0;
	record_cpu_sample(RuntimeSampleKind::Submit, submit_begin_ns, submit_end_ns,
		submit_result == VK_SUCCESS);
	if (submit_result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}
	submission_pending = true;
	if (current_gpu_query_recorded)
	{
		pending_gpu_sample = RuntimeSample{
			.kind = RuntimeSampleKind::Gpu,
			.begin_ns = submit_begin_ns,
			.submission_id = observation_submission,
			.frame_number = render_frame.frame_number,
			.tick_index = render_frame.simulation_tick,
			.publication_ns = render_frame.publication_ns };
		gpu_query_pending = true;
	}
	submission_serial = get_graphics_engine().register_graphics_submission();

	//
	// Presentation
	// this step submits the result of the swapchain to have it eventually show up on the screen
	//

	if (present_pending)
	{
		if (vkWaitForFences(get_logical_device(), 1, &present_fence, VK_TRUE,
			std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS)
			throw std::runtime_error("failed to wait for prior presentation");
		present_pending = false;
	}
	if (vkResetFences(get_logical_device(), 1, &present_fence) != VK_SUCCESS)
		throw std::runtime_error("failed to reset presentation fence");
	VkSwapchainPresentFenceInfoEXT present_fence_info{VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT};
	present_fence_info.swapchainCount = 1;
	present_fence_info.pFences = &present_fence;
	VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	present_info.pNext = &present_fence_info;
	present_info.waitSemaphoreCount = 1;
	// the semaphore to wait on before presentation
	present_info.pWaitSemaphores = &render_finished_semaphore;

	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swap_chain.get_swap_chain();
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr; // allows you to specify array of VkResult values to check for every individual swap chain if presentation was successful

	const auto present_begin_ns = collect_cpu ? runtime_now_ns() : 0;
	const auto result = vkQueuePresentKHR(get_graphics_engine().get_present_queue(), &present_info);
	const auto present_end_ns = collect_cpu ? runtime_now_ns() : 0;
	record_cpu_sample(RuntimeSampleKind::Present, present_begin_ns, present_end_ns,
		result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
	if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
		present_pending = true;

	flush_screenshot_capture();
	flush_frame_capture();
	flush_recording_capture();
	return result;
}

void GraphicsEngineFrame::update_uniform_buffer()
{
	// update global uniform buffer
	const RenderFrame& render_frame = get_graphics_engine().get_render_frame();
	SDS::GlobalData gubo{};
	gubo.view = render_frame.camera.view;
	gubo.proj = render_frame.camera.projection;
	gubo.view_pos = render_frame.camera.position;

	const auto& lighting = static_cast<ShadowMapRenderer&>(
		get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::SHADOW_MAP))
		.get_lighting_data(image_index);
	gubo.light_count = lighting.light_count;
	gubo.shadow_far_plane = POINT_SHADOW_FAR_PLANE;
	std::ranges::copy(lighting.lights, gubo.lights);

	get_rsrc_mgr().write_to_global_uniform_buffer(image_index, gubo);

	// Update the producer-composed transform for each renderable.
	SDS::ObjectData object_data{};
	for (const auto& [_, graphics_renderable] : get_graphics_engine().get_renderables())
	{
		object_data.model = graphics_renderable->get_model_transform();
		object_data.mvp = gubo.proj * gubo.view * object_data.model;
		object_data.rot_mat = glm::mat3(object_data.model);
		get_rsrc_mgr().write_to_buffer(
			RenderableFrameID{graphics_renderable->get_id(), image_index},
			object_data);
	}

	// Skeleton pose resources are shared by every bound renderable and updated once.
	for (const auto& pose : render_frame.skeletons)
	{
		std::vector<SDS::Bone> bones =
			compose_bone_transforms(pose.local_transforms, *pose.definition);
		get_rsrc_mgr().write_to_buffer(
			SkeletonFrameID(pose.definition->id, image_index),
			bones);
	}
}

void GraphicsEngineFrame::create_synchronisation_objects()
{
	VkSemaphoreCreateInfo semaphore_create_info{};
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_create_info{};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(get_logical_device(), &semaphore_create_info, nullptr, &render_finished_semaphore) != VK_SUCCESS)
		throw std::runtime_error("failed to create render-finished semaphore");
	if (vkCreateFence(get_logical_device(), &fence_create_info, nullptr, &fence_frame_inflight) != VK_SUCCESS)
	{
		vkDestroySemaphore(get_logical_device(), render_finished_semaphore, nullptr);
		render_finished_semaphore = VK_NULL_HANDLE;
		throw std::runtime_error("failed to create frame submission fence");
	}
	if (vkCreateFence(get_logical_device(), &fence_create_info, nullptr, &present_fence) != VK_SUCCESS)
	{
		vkDestroyFence(get_logical_device(), fence_frame_inflight, nullptr);
		vkDestroySemaphore(get_logical_device(), render_finished_semaphore, nullptr);
		fence_frame_inflight = VK_NULL_HANDLE;
		render_finished_semaphore = VK_NULL_HANDLE;
		throw std::runtime_error("failed to create presentation fence");
	}
}
