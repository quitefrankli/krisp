#pragma once

#include "graphics_engine.hpp"
#include "shared_data_structures.hpp"
#include "renderers/renderers.hpp"
#include "shared_data_structures.hpp"
#include "entity_component_system/ecs.hpp"


void GraphicsEngine::handle_command(SpawnObjectCmd& cmd)
{
	const auto spawn_object = [&](GraphicsEngineObject& graphics_object)
	{
		spawn_object_create_buffers(graphics_object);
		spawn_object_create_dsets(graphics_object);
	};

	if (cmd.object_ref)
	{
		auto graphics_object = objects.emplace(
			cmd.object_ref->get_id(),
			std::make_unique<GraphicsEngineObjectRef>(*this, *cmd.object_ref));
		spawn_object(*graphics_object.first->second);
	} else {
		auto id = cmd.object->get_id();
		auto graphics_object = objects.emplace(
			id,
			std::make_unique<GraphicsEngineObjectPtr>(*this, std::move(cmd.object)));
		spawn_object(*graphics_object.first->second);
	}
}

void GraphicsEngine::handle_command(DeleteObjectCmd& cmd)
{
	get_swap_chain().get_prev_frame().mark_obj_for_delete(cmd.object_id);
	get_objects()[cmd.object_id]->mark_for_delete();
}

void GraphicsEngine::handle_command(StencilObjectCmd& cmd)
{
	stenciled_objects.insert(cmd.object_id);
}

void GraphicsEngine::handle_command(UnStencilObjectCmd& cmd)
{
	stenciled_objects.erase(cmd.object_id);
}

void GraphicsEngine::handle_command(ShutdownCmd& cmd)
{
	shutdown();
}

void GraphicsEngine::handle_command(ToggleWireFrameModeCmd& cmd)
{
    is_wireframe_mode = !is_wireframe_mode;
	update_command_buffer();
}

void GraphicsEngine::handle_command(UpdateCommandBufferCmd& cmd)
{
	update_command_buffer();
}

void GraphicsEngine::handle_command(UpdateRayTracingCmd& cmd)
{
	raytracing_component.update_acceleration_structures();
	static_cast<RaytracingRenderer&>(
		renderer_mgr.get_renderer(ERendererType::RAYTRACING)).update_rt_dsets();
}

void GraphicsEngine::handle_command(PreviewObjectsCmd& cmd)
{
	offscreen_rendering_objects.clear();
	for (const auto& id : cmd.objects)
	{
		auto it = objects.find(id);
		if (it == objects.end())
		{
			LOG_WARNING(Utility::get_logger(), "PreviewObjectsCmd: ignoring obj with id:={}", id.get_underlying());
			continue;
		}

		offscreen_rendering_objects.emplace(id, it->second.get());
	}

	Renderer& renderer = get_renderer_mgr().get_renderer(ERendererType::OFFSCREEN_GUI_VIEWPORT);
	const auto extent = renderer.get_extent();
	get_graphics_gui_manager().update_preview_window(
		cmd.gui, 
		get_texture_mgr().fetch_sampler(ETextureSamplerType::ADDR_MODE_CLAMP_TO_EDGE),
		renderer.get_output_image_view(get_swap_chain().get_curr_frame().image_index),
		glm::uvec2(extent.width, extent.height));
}

void GraphicsEngine::handle_command(DestroyResourcesCmd& cmd)
{
	for (const auto mat_id : cmd.material_ids)
	{
		get_rsrc_mgr().free_buffer(mat_id);
		get_texture_mgr().free_texture(mat_id);
	}

	for (const auto mesh_id : cmd.mesh_ids)
	{
		get_rsrc_mgr().free_buffer(mesh_id);
	}
}

void GraphicsEngine::handle_command(UpdateRenderableMaterialsCmd& cmd)
{
	const auto free_retired_materials = [&]
	{
		for (const auto material_id : cmd.retired_materials)
		{
			get_rsrc_mgr().free_buffer(material_id);
			get_texture_mgr().free_texture(material_id);
		}
	};
	const auto object_it = objects.find(cmd.object_id);
	if (object_it == objects.end()
		|| cmd.renderable_index >= object_it->second->get_renderable_dsets().size())
	{
		LOG_WARNING(
			Utility::get_logger(),
			"UpdateRenderableMaterialsCmd: target object or renderable no longer exists");
		vkDeviceWaitIdle(get_logical_device());
		free_retired_materials();
		return;
	}

	// Renderable descriptor sets are shared by all frames, so they must not be
	// updated while an earlier frame can still be reading them.
	vkDeviceWaitIdle(get_logical_device());
	const VkDescriptorSet descriptor_set =
		object_it->second->get_renderable_dsets()[cmd.renderable_index];
	const auto sampler_type = ETextureSamplerType::ADDR_MODE_REPEAT;
	const GraphicsEngineTexture& diffuse =
		get_texture_mgr().fetch_texture(cmd.diffuse_material, sampler_type);
	const GraphicsEngineTexture& normal = cmd.normal_material
		? get_texture_mgr().fetch_texture(*cmd.normal_material, sampler_type)
		: get_texture_mgr().fetch_flat_normal_texture();

	VkDescriptorImageInfo diffuse_info{};
	diffuse_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	diffuse_info.imageView = diffuse.get_texture_image_view();
	diffuse_info.sampler = get_texture_mgr().fetch_sampler(sampler_type);
	VkDescriptorImageInfo normal_info{};
	normal_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normal_info.imageView = normal.get_texture_image_view();
	normal_info.sampler = get_texture_mgr().fetch_sampler(sampler_type);

	std::array<VkWriteDescriptorSet, 2> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = descriptor_set;
	writes[0].dstBinding = SDS::RASTERIZATION_ALBEDO_TEXTURE_DATA_BINDING;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].descriptorCount = 1;
	writes[0].pImageInfo = &diffuse_info;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = descriptor_set;
	writes[1].dstBinding = SDS::RASTERIZATION_NORMAL_TEXTURE_DATA_BINDING;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].descriptorCount = 1;
	writes[1].pImageInfo = &normal_info;
	vkUpdateDescriptorSets(
		get_logical_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	free_retired_materials();
}
