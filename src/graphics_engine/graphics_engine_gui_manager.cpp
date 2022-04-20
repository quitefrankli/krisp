#include "graphics_engine_gui_manager.hpp"
#include "graphics_engine.hpp"

#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <iostream>


GraphicsEngineGuiManager::GraphicsEngineGuiManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine), GuiManager(engine.get_window_width())
{
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForVulkan(engine.get_window(), true);
	
	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = engine.get_instance();
	init_info.PhysicalDevice = engine.get_physical_device();
	init_info.Device = engine.get_logical_device();
	init_info.Queue = engine.get_graphics_queue();
	init_info.DescriptorPool = engine.get_graphics_resource_manager().descriptor_pool;
	init_info.MinImageCount = engine.get_num_swapchain_images();
	init_info.ImageCount = engine.get_num_swapchain_images();
	// temporary fix, Gui doesn't really need anti-aliasing it should use its own dedicated pipeline
	init_info.MSAASamples = engine.get_swap_chain().get_msaa_samples();

	ImGui_ImplVulkan_Init(&init_info, engine.get_swap_chain().get_render_pass());

	// execute a GPU command to upload ImGui font textures
	VkCommandBuffer buffer = engine.begin_single_time_commands();
	ImGui_ImplVulkan_CreateFontsTexture(buffer);
	engine.end_single_time_commands(buffer);

	// clear font textures from CPU data
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

GraphicsEngineGuiManager::~GraphicsEngineGuiManager()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void GraphicsEngineGuiManager::add_render_cmd(VkCommandBuffer& cmd_buffer)
{
	if (!ImGui::GetDrawData())
	{
		return;
	}
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buffer);
}

void GraphicsEngineGuiManager::draw()
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

void GraphicsEngineGuiManager::update_fps(const float fps)
{
	fps_counter.fps = fps;
}