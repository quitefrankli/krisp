#pragma once

#include "graphics_engine_gui_manager.hpp"
#include "graphics_engine.hpp"
#include "renderers/renderers.hpp"
#include "utility.hpp"
#include "resource_loader/resource_loader.hpp"
#include "entity_component_system/material_system.hpp"

#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <fmt/core.h>

#include <cstdio>


namespace
{
void panel_settings_read_init(ImGuiContext*, ImGuiSettingsHandler* handler)
{
	static_cast<GuiManager*>(handler->UserData)->clear_saved_panel_visibility();
}

void* panel_settings_read_open(
	ImGuiContext*, ImGuiSettingsHandler* handler, const char* name)
{
	return &static_cast<GuiManager*>(handler->UserData)->get_or_create_saved_panel_visibility(name);
}

void panel_settings_read_line(
	ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
	int visible = 0;
	if (std::sscanf(line, "Visible=%d", &visible) == 1)
		*static_cast<bool*>(entry) = visible != 0;
}

void panel_settings_apply_all(ImGuiContext*, ImGuiSettingsHandler* handler)
{
	static_cast<GuiManager*>(handler->UserData)->apply_saved_panel_visibility();
}

void panel_settings_write_all(
	ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* output)
{
	const auto& windows = static_cast<GuiManager*>(handler->UserData)->get_gui_windows();
	for (const auto& window : windows)
	{
		output->appendf("[KrispPanel][%s]\n", window->get_panel_info().id.c_str());
		output->appendf("Visible=%d\n\n", window->is_visible() ? 1 : 0);
	}
}
}


GraphicsEngineGuiManager::GraphicsEngineGuiManager(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
	setup_imgui();

	// certain gui_windows require some setup
	setup_gui_windows();
}

GraphicsEngineGuiManager::~GraphicsEngineGuiManager()
{
	// Persist immediately on orderly shutdown, including sessions shorter than
	// ImGui's periodic settings-save interval.
	ImGui::SaveIniSettingsToDisk(imgui_ini_path.c_str());
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
	draw_workspace();

	for (auto& gui_window : gui_windows)
	{
		if (gui_window->is_visible())
		{
			gui_window->draw();
		}
	}
	
	ImGui::Render();
}

void GraphicsEngineGuiManager::draw_workspace()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	const ImGuiWindowFlags host_flags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::Begin("Krisp Workspace###workspace", nullptr, host_flags);
	ImGui::PopStyleVar(3);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("View"))
		{
			for (const auto& gui_window : gui_windows)
			{
				const auto& panel = gui_window->get_panel_info();
				if (!panel.dockable)
				{
					continue;
				}

				bool visible = gui_window->is_visible();
				if (ImGui::MenuItem(panel.title.c_str(), nullptr, &visible))
				{
					gui_window->set_visible(visible);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout"))
			{
				reset_layout_requested = true;
				reset_panel_visibility();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	const ImGuiID dockspace_id = ImGui::GetID("KrispWorkspaceDockSpace");
	if (reset_layout_requested || ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
	{
		build_default_layout(dockspace_id, viewport->WorkSize);
		reset_layout_requested = false;
	}

	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();
}

void GraphicsEngineGuiManager::build_default_layout(ImGuiID dockspace_id, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(
		dockspace_id,
		ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::DockBuilderSetNodeSize(dockspace_id, size);

	ImGuiID remaining = dockspace_id;
	ImGuiID left = 0;
	ImGuiID right = 0;
	ImGuiID bottom = 0;
	ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Left, 0.23f, &left, &remaining);
	ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Right, 0.28f, &right, &remaining);
	ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Down, 0.25f, &bottom, &remaining);

	for (const auto& gui_window : gui_windows)
	{
		const auto& panel = gui_window->get_panel_info();
		if (!panel.dockable)
		{
			continue;
		}

		ImGuiID target = remaining;
		switch (panel.default_dock)
		{
			case GuiPanelDock::LEFT: target = left; break;
			case GuiPanelDock::RIGHT: target = right; break;
			case GuiPanelDock::BOTTOM: target = bottom; break;
			case GuiPanelDock::NONE: break;
		}
		ImGui::DockBuilderDockWindow(gui_window->get_imgui_name(), target);
	}

	ImGui::DockBuilderFinish(dockspace_id);
}

void GraphicsEngineGuiManager::update_preview_window(
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

void GraphicsEngineGuiManager::setup_imgui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGuiSettingsHandler panel_settings_handler;
	panel_settings_handler.TypeName = "KrispPanel";
	panel_settings_handler.TypeHash = ImHashStr("KrispPanel");
	panel_settings_handler.ReadInitFn = panel_settings_read_init;
	panel_settings_handler.ReadOpenFn = panel_settings_read_open;
	panel_settings_handler.ReadLineFn = panel_settings_read_line;
	panel_settings_handler.ApplyAllFn = panel_settings_apply_all;
	panel_settings_handler.WriteAllFn = panel_settings_write_all;
	panel_settings_handler.UserData = static_cast<GuiManager*>(this);
	ImGui::AddSettingsHandler(&panel_settings_handler);
	const auto config_path = Utility::get_user_config_path("imgui.ini");
	std::filesystem::create_directories(config_path.parent_path());
	imgui_ini_path = config_path.string();
	io.IniFilename = imgui_ini_path.c_str();
	ImGui::LoadIniSettingsFromDisk(io.IniFilename);
	ImGui_ImplGlfw_InitForVulkan(get_graphics_engine().get_window().get_glfw_window(), true);
	
	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.ApiVersion = VK_API_VERSION_1_3;
	init_info.Instance = get_graphics_engine().get_instance();
	init_info.PhysicalDevice = get_graphics_engine().get_physical_device();
	init_info.Device = get_graphics_engine().get_logical_device();
	init_info.Queue = get_graphics_engine().get_graphics_queue();
	// Keep ImGui descriptors isolated from the engine's combined-image-sampler
	// pool: the current backend requires separate sampled-image and sampler
	// descriptor types for its dynamic font atlas.
	init_info.DescriptorPool = VK_NULL_HANDLE;
	init_info.DescriptorPoolSize = 64;
	init_info.MinImageCount = get_graphics_engine().get_num_swapchain_images();
	init_info.ImageCount = init_info.MinImageCount;
	// GUI uses a dedicated single-sample pipeline over the presentation image.
	init_info.PipelineInfoMain.RenderPass =
		get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::GUI).get_render_pass();
	init_info.PipelineInfoMain.MSAASamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);
}

void GraphicsEngineGuiManager::setup_gui_windows()
{
	this->photo.init([&](const std::filesystem::path& picture)
	{
		compose_texture_for_gui_window(picture, this->photo);
	});

	// RenderSlicer will always read from output of QuadRenderer
	auto& quad_renderer = get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::QUAD);
	VkDescriptorSet dset = ImGui_ImplVulkan_AddTexture(
		get_graphics_engine().get_texture_mgr().fetch_sampler(ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE), 
		quad_renderer.get_output_image_view(0),
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	this->render_slicer.update(dset, { quad_renderer.get_extent().width, quad_renderer.get_extent().height });
	this->render_slicer.init([&](const std::string& slice)
	{
		if (slice == "none")
		{
			get_graphics_engine().get_renderer_mgr().pipe_output_to_quad_renderer(ERendererType::NONE);
		} else if (slice == "shadow_map")
		{
			get_graphics_engine().get_renderer_mgr().pipe_output_to_quad_renderer(ERendererType::SHADOW_MAP);
		}
	});
}

void GraphicsEngineGuiManager::compose_texture_for_gui_window(
	const std::filesystem::path& texture_path,
	GuiPhotoBase& gui_photo)
{
	const auto texture_mat_id = ResourceLoader::fetch_texture(texture_path);
	GraphicsEngineTexture& texture = get_graphics_engine().get_texture_mgr().fetch_texture(
		texture_mat_id, 
		ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE);
	// TODO: release the acquired material owner when this ImGui texture is replaced.
	
	// TODO: figure out if we need to also do ImGui_ImplVulkan_RemoveTexture(tex_data->DS);
	// https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
	VkDescriptorSet dset = ImGui_ImplVulkan_AddTexture(
		texture.get_texture_sampler(),
		texture.get_texture_image_view(), 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	gui_photo.update(dset, { texture.get_width(), texture.get_height() });
}
