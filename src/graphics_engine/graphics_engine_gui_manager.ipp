#pragma once

#include "graphics_engine_gui_manager.hpp"
#include "graphics_engine.hpp"
#include "renderers/renderers.hpp"
#include "utility.hpp"
#include "resource_paths.hpp"
#include "entity_component_system/material_system.hpp"

#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <fmt/core.h>

#include <cstdio>
#include <utility>
#include <algorithm>
#include <iterator>


namespace
{
void panel_settings_read_init(ImGuiContext*, ImGuiSettingsHandler* handler)
{
	static_cast<EngineUiManager*>(handler->UserData)->clear_saved_panel_visibility();
}

void* panel_settings_read_open(
	ImGuiContext*, ImGuiSettingsHandler* handler, const char* name)
{
	return &static_cast<EngineUiManager*>(handler->UserData)->get_or_create_saved_panel_visibility(name);
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
	static_cast<EngineUiManager*>(handler->UserData)->apply_saved_panel_visibility();
}

void panel_settings_write_all(
	ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* output)
{
	const auto& windows = static_cast<EngineUiManager*>(handler->UserData)->get_gui_windows();
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
	if (imgui_vulkan_initialized)
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
	const std::lock_guard lock(engine_ui_manager.state_mutex);
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	const bool performance_overlay = get_graphics_engine().get_runtime_options().performance_overlay;
	if (engine_ui_manager.is_loading())
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.060f, 0.075f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		constexpr ImGuiWindowFlags loading_flags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (ImGui::Begin("Loading###loading", nullptr, loading_flags))
		{
			constexpr const char* message = "Loading...";
			const ImVec2 text_size = ImGui::CalcTextSize(message);
			const ImVec2 window_size = ImGui::GetWindowSize();
			ImGui::SetCursorPos(ImVec2(
				(window_size.x - text_size.x) * 0.5f,
				(window_size.y - text_size.y) * 0.5f));
			ImGui::TextUnformatted(message);
		}
		ImGui::End();
		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor();
	}
	else if (get_graphics_engine().get_runtime_options().ui)
	{
		const bool exclusive = application_ui_manager && application_ui_manager->get_exclusive_window();
		if (!exclusive && engine_ui_active.load(std::memory_order_acquire))
		{
			draw_workspace();
			for (auto& gui_window : engine_ui_manager.get_gui_windows())
			{
				if (gui_window->is_visible())
					gui_window->draw();
			}
		}
		if (application_ui_manager && (exclusive || application_ui_active.load(std::memory_order_acquire)))
			draw_application_ui();
		for (auto& gui_window : engine_ui_manager.get_persistent_windows())
		{
			if (performance_overlay && gui_window.get() == &engine_ui_manager.fps_counter)
				continue;
			if (gui_window->is_visible() && (!exclusive || gui_window.get() == &engine_ui_manager.command_prompt))
				gui_window->draw();
		}
	}
	if (performance_overlay)
		engine_ui_manager.fps_counter.draw();

	ImGui::Render();
}

void GraphicsEngineGuiManager::draw_application_ui()
{
	const auto& theme = application_ui_manager->get_theme();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(theme.text[0], theme.text[1], theme.text[2], theme.text[3]));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(
		theme.window_background[0], theme.window_background[1], theme.window_background[2], theme.window_background[3]));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], theme.accent[3]));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme.window_rounding);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, theme.window_border_size);

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const auto& windows = application_ui_manager->get_windows();
	for (const bool overlay : { false, true })
	{
		for (size_t index = 0; index < windows.size(); ++index)
		{
			auto& window = windows[index];
			if (!application_ui_manager->is_active(*window) || application_ui_manager->is_overlay(index) != overlay)
				continue;
			const auto& layout = application_ui_manager->get_layout(index);
			ImVec2 position = viewport->WorkPos;
			switch (layout.anchor)
			{
				case ApplicationUiAnchor::TOP_RIGHT: position.x += viewport->WorkSize.x; break;
				case ApplicationUiAnchor::BOTTOM_LEFT: position.y += viewport->WorkSize.y; break;
				case ApplicationUiAnchor::BOTTOM_RIGHT: position.x += viewport->WorkSize.x; position.y += viewport->WorkSize.y; break;
				case ApplicationUiAnchor::CENTER: position.x += viewport->WorkSize.x * .5f; position.y += viewport->WorkSize.y * .5f; break;
				case ApplicationUiAnchor::TOP_LEFT: break;
			}
			position.x += layout.offset[0]; position.y += layout.offset[1];
			ImVec2 pivot(0.0f, 0.0f);
			if (layout.anchor == ApplicationUiAnchor::TOP_RIGHT || layout.anchor == ApplicationUiAnchor::BOTTOM_RIGHT)
				pivot.x = 1.0f;
			if (layout.anchor == ApplicationUiAnchor::BOTTOM_LEFT || layout.anchor == ApplicationUiAnchor::BOTTOM_RIGHT)
				pivot.y = 1.0f;
			if (layout.anchor == ApplicationUiAnchor::CENTER)
				pivot = ImVec2(.5f, .5f);
			ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
			if (layout.size[0] > 0.0f || layout.size[1] > 0.0f)
				ImGui::SetNextWindowSize(ImVec2(layout.size[0], layout.size[1]), ImGuiCond_Always);
			window->draw();
		}
	}
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
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
			for (const auto& gui_window : engine_ui_manager.get_gui_windows())
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
				engine_ui_manager.reset_panel_visibility();
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

	for (const auto& gui_window : engine_ui_manager.get_gui_windows())
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

void GraphicsEngineGuiManager::setup_imgui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Engine/editor chrome: graphite surfaces with a restrained blue accent.
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.080f, 0.095f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.095f, 0.105f, 0.125f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.12f, 0.25f, 0.42f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.36f, 0.62f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.11f, 0.28f, 0.50f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.40f, 0.70f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.65f, 1.0f, 1.0f);
	ImGuiSettingsHandler panel_settings_handler;
	panel_settings_handler.TypeName = "KrispPanel";
	panel_settings_handler.TypeHash = ImHashStr("KrispPanel");
	panel_settings_handler.ReadInitFn = panel_settings_read_init;
	panel_settings_handler.ReadOpenFn = panel_settings_read_open;
	panel_settings_handler.ReadLineFn = panel_settings_read_line;
	panel_settings_handler.ApplyAllFn = panel_settings_apply_all;
	panel_settings_handler.WriteAllFn = panel_settings_write_all;
	panel_settings_handler.UserData = &engine_ui_manager;
	ImGui::AddSettingsHandler(&panel_settings_handler);
	const auto config_path = ResourcePaths::get_user_config_path("imgui.ini");
	std::filesystem::create_directories(config_path.parent_path());
	imgui_ini_path = config_path.string();
	io.IniFilename = imgui_ini_path.c_str();
	ImGui::LoadIniSettingsFromDisk(io.IniFilename);
	ImGui_ImplGlfw_InitForVulkan(get_graphics_engine().get_window().get_glfw_window(), true);

	initialize_imgui_vulkan();
}

void GraphicsEngineGuiManager::initialize_imgui_vulkan()
{
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
	init_info.MinImageCount = std::max(2u, get_graphics_engine().get_num_swapchain_images());
	init_info.ImageCount = init_info.MinImageCount;
	// GUI uses a dedicated single-sample pipeline over the presentation image.
	const VkFormat presentation_format = get_graphics_engine().get_swap_chain().get_image_format();
	VkPipelineRenderingCreateInfo rendering_info{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachmentFormats = &presentation_format;
	init_info.UseDynamicRendering = true;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo = rendering_info;
	init_info.PipelineInfoMain.MSAASamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);
	imgui_vulkan_initialized = true;
}

void GraphicsEngineGuiManager::release_per_frame_resources(const bool image_count_changed)
{
	const std::lock_guard lock(engine_ui_manager.state_mutex);
	if (!image_count_changed || !imgui_vulkan_initialized || render_slicer_descriptor == VK_NULL_HANDLE)
		return;
	ImGui_ImplVulkan_RemoveTexture(render_slicer_descriptor);
	render_slicer_descriptor = VK_NULL_HANDLE;
}

void GraphicsEngineGuiManager::rebuild_per_frame_resources(const bool image_count_changed)
{
	if (!image_count_changed)
		return;
	const std::lock_guard lock(engine_ui_manager.state_mutex);
	const uint32_t image_count = get_graphics_engine().get_num_swapchain_images();
	if (image_count > 0)
		ImGui_ImplVulkan_SetImageCount(std::max(2u, image_count));
	refresh_render_slicer_preview();
}

void GraphicsEngineGuiManager::setup_gui_windows()
{
	engine_ui_manager.photo.init([&](MaterialHandle texture_owner)
	{
		compose_texture_for_gui_window(std::move(texture_owner), engine_ui_manager.photo);
	});

	// RenderSlicer will always read from output of QuadRenderer.
	refresh_render_slicer_preview();
	engine_ui_manager.render_slicer.init([&](const std::string& slice)
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
	MaterialHandle texture_owner,
	GuiPhotoBase& gui_photo)
{
	auto binding = std::find_if(gui_texture_bindings.begin(), gui_texture_bindings.end(),
		[&](const GuiTextureBinding& current) { return current.target == &gui_photo; });
	if (binding == gui_texture_bindings.end())
	{
		gui_texture_bindings.push_back({ std::move(texture_owner), &gui_photo });
		binding = std::prev(gui_texture_bindings.end());
	}
	else
	{
		if (imgui_vulkan_initialized && binding->descriptor != VK_NULL_HANDLE)
			ImGui_ImplVulkan_RemoveTexture(binding->descriptor);
		binding->owner = std::move(texture_owner);
		binding->descriptor = VK_NULL_HANDLE;
	}
	refresh_gui_texture_binding(*binding);
}

void GraphicsEngineGuiManager::refresh_render_slicer_preview()
{
	if (get_graphics_engine().get_num_swapchain_images() == 0)
		return;
	auto& quad_renderer = get_graphics_engine().get_renderer_mgr().get_renderer(ERendererType::QUAD);
	render_slicer_descriptor = ImGui_ImplVulkan_AddTexture(
		get_graphics_engine().get_texture_mgr().fetch_sampler(
			PbrMaterial::TextureSampler::clamp_to_edge()),
		quad_renderer.get_output_image_view(0),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	engine_ui_manager.render_slicer.update(
		render_slicer_descriptor, { quad_renderer.get_extent().width, quad_renderer.get_extent().height });
}

void GraphicsEngineGuiManager::refresh_gui_texture_binding(GuiTextureBinding& binding)
{
	if (!imgui_vulkan_initialized || binding.descriptor != VK_NULL_HANDLE)
		return;
	GraphicsEngineTexture& texture = get_graphics_engine().get_texture_mgr().fetch_texture(
		binding.owner, PbrMaterial::TextureSampler::clamp_to_edge());
	binding.descriptor = ImGui_ImplVulkan_AddTexture(
		texture.get_texture_sampler(), texture.get_texture_image_view(),
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	binding.target->update(binding.descriptor, { texture.get_width(), texture.get_height() });
}
