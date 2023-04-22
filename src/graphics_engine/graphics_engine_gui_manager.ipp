#pragma once

#include "graphics_engine_gui_manager.hpp"
#include "graphics_engine.hpp"
#include "utility.hpp"

#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <iostream>


template<typename GraphicsEngineT, typename GameEngineT>
GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::GraphicsEngineGuiManager(GraphicsEngineT& engine) :
	GraphicsEngineBaseModule<GraphicsEngineT>(engine)
{
	setup_imgui();

	// certain gui_windows require some setup
	setup_gui_windows();
}

template<typename GraphicsEngineT, typename GameEngineT>
GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::~GraphicsEngineGuiManager()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

template<typename GraphicsEngineT, typename GameEngineT>
void GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::add_render_cmd(VkCommandBuffer& cmd_buffer)
{
	if (!ImGui::GetDrawData())
	{
		return;
	}
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buffer);
}

template<typename GraphicsEngineT, typename GameEngineT>
void GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::draw()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	// ImGui::ShowDemoWindow();

	for (auto& gui_window : gui_windows)
	{
		gui_window->draw();
	}
	
	ImGui::Render();
}

template<typename GraphicsEngineT, typename GameEngineT>
void GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::update_preview_window(
	GuiPhotoBase& gui,
	VkSampler sampler,
	VkImageView image_view,
	const glm::uvec2& dimensions)
{
	// for caching dsets
	static std::unordered_map<VkImageView, VkDescriptorSet> dsets;
	const auto it = dsets.find(image_view);
	if (it != dsets.end())
	{
		gui.update(it->second, dimensions);
		return;
	}

	VkDescriptorSet dset = dsets.emplace(image_view, ImGui_ImplVulkan_AddTexture(
		sampler, 
		image_view, 
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)).first->second;

	gui.update(dset, dimensions);
}

template<typename GraphicsEngineT, typename GameEngineT>
void GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::setup_imgui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	static const std::string configs_path = fmt::format("{}/imgui.ini", Utility::get().get_config_path().string());
	io.IniFilename = configs_path.c_str();
	ImGui_ImplGlfw_InitForVulkan(get_graphics_engine().get_window().get_glfw_window(), true);
	
	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = get_graphics_engine().get_instance();
	init_info.PhysicalDevice = get_graphics_engine().get_physical_device();
	init_info.Device = get_graphics_engine().get_logical_device();
	init_info.Queue = get_graphics_engine().get_graphics_queue();
	init_info.DescriptorPool = get_graphics_engine().get_rsrc_mgr().descriptor_pool;
	init_info.MinImageCount = get_graphics_engine().get_num_swapchain_images();
	init_info.ImageCount = init_info.MinImageCount;
	// temporary fix, Gui doesn't really need anti-aliasing it should use its own dedicated pipeline
	init_info.MSAASamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT; //get_graphics_engine().get_msaa_samples();

	ImGui_ImplVulkan_Init(&init_info, get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::GUI).get_render_pass());

	// execute a GPU command to upload ImGui font textures
	VkCommandBuffer buffer = get_graphics_engine().begin_single_time_commands();
	ImGui_ImplVulkan_CreateFontsTexture(buffer);
	get_graphics_engine().end_single_time_commands(buffer);

	// clear font textures from CPU data
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

template<typename GraphicsEngineT, typename GameEngineT>
void GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::setup_gui_windows()
{
	photo.init([&](const std::string_view picture)
	{
		compose_texture_for_gui_window(picture, photo);
	});
}

template<typename GraphicsEngineT, typename GameEngineT>
void GraphicsEngineGuiManager<GraphicsEngineT, GameEngineT>::compose_texture_for_gui_window(
	const std::string_view texture_path,
	GuiPhotoBase& gui_photo)
{
	GraphicsEngineTexture& texture = get_graphics_engine().get_texture_mgr().fetch_texture(
		texture_path, 
		ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE);
	
	// TODO: figure out if we need to also do ImGui_ImplVulkan_RemoveTexture(tex_data->DS);
	// https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
	VkDescriptorSet dset = ImGui_ImplVulkan_AddTexture(
		texture.get_texture_sampler(),
		texture.get_texture_image_view(), 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	gui_photo.update(dset, { texture.get_width(), texture.get_height() });
}
