#include "renderer.hpp"
#include "rasterization_renderer.ipp"
#include "gui_renderer.ipp"
// #include "raytracing_renderer.ipp" // Ray tracing is unsupported.
#include "shadowmap_renderer.ipp"
#include "quad_renderer.ipp"
#include "particle_renderer.ipp"
#include "renderer_manager.ipp"
#include "graphics_engine/graphics_engine.hpp"
#include "renderable/mesh.hpp"


Renderer::Renderer(GraphicsEngine& engine) :
	GraphicsEngineBaseModule(engine)
{
}

Renderer::~Renderer()
{
	auto logical_device = this->get_graphics_engine().get_logical_device();
	if (render_pass)
	{
		vkDestroyRenderPass(logical_device, render_pass, nullptr);
	}
	for (auto frame_buffer : frame_buffers)
	{
		vkDestroyFramebuffer(logical_device, frame_buffer, nullptr);
	}
}

VkExtent2D Renderer::get_extent()
{
	return get_graphics_engine().get_extent();
}

void Renderer::reset_draw_state()
{
	bound_pipeline = VK_NULL_HANDLE;
	bound_mesh.reset();
}

void Renderer::draw_renderable(VkCommandBuffer command_buffer,
							   const RenderableDefinition& renderable,
							   const VkDescriptorSet& object_dset,
							   const VkDescriptorSet& renderable_dset,
							   EPipelineModifier pipeline_modifier,
							   ERenderType primary_pipeline_override)
{
	const ERenderType primary_pipeline_type = primary_pipeline_override == ERenderType::UNASSIGNED ?
		renderable.pipeline_render_type : primary_pipeline_override;
	const auto* pipeline = get_graphics_engine().get_pipeline_mgr().fetch_pipeline({ 
		primary_pipeline_type, pipeline_modifier, renderable.alpha_mode });
	if (!pipeline)
	{
		return;
	}

	if (bound_pipeline != pipeline->graphics_pipeline)
	{
		vkCmdBindPipeline(
			command_buffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline->graphics_pipeline);
		bound_pipeline = pipeline->graphics_pipeline;
	}

	// binds uniform buffer dset
	vkCmdBindDescriptorSets(command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							pipeline->pipeline_layout,
							SDS::RASTERIZATION_PER_RENDERABLE_FRAME_SET_OFFSET,	// see SDS for more info
							1,
							&object_dset,
							0,
							nullptr);

	const Mesh& mesh = renderable.get_mesh();
	if (bound_mesh != mesh.get_id())
	{
		const VkDeviceSize buffer_offset =
			get_rsrc_mgr().get_vertex_buffer_offset(mesh.get_id());
		const VkBuffer buffer = get_rsrc_mgr().get_vertex_buffer();
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer, &buffer_offset);

		const VkDeviceSize index_buffer_offset =
			get_rsrc_mgr().get_index_buffer_offset(mesh.get_id());
		vkCmdBindIndexBuffer(
			command_buffer,
			get_rsrc_mgr().get_index_buffer(),
			index_buffer_offset,
			VK_INDEX_TYPE_UINT32);
		bound_mesh = mesh.get_id();
	}

	// binds renderable specific dsets, i.e. material group
	vkCmdBindDescriptorSets(command_buffer, 
							VK_PIPELINE_BIND_POINT_GRAPHICS, 					// unlike vertex buffer, descriptor sets are not unique to the graphics pipeline, compute pipeline is also possible
							pipeline->pipeline_layout, 
							SDS::RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET,  // see SDS for more info
							1,
							&renderable_dset,
							0,
							nullptr);

	if (primary_pipeline_type == ERenderType::STANDARD || primary_pipeline_type == ERenderType::SKINNED)
	{
		const TexturedMatGroup materials(renderable.material_owners);
		const auto& base_material = materials.get_material_owner(materials.base_color_mat)->get();
		const auto& sampled_material = dynamic_cast<const SampledMaterial&>(base_material);
		const SDS::AlphaMaterialData alpha_data{
			.alpha_cutoff = renderable.alpha_mode == EAlphaMode::MASK ? renderable.alpha_cutoff : 0.0f,
			.opacity = renderable.opacity,
			.premultiplied_base_color = sampled_material.is_premultiplied() ? 1 : 0,
		};
		vkCmdPushConstants(command_buffer, pipeline->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(alpha_data), &alpha_data);
	}
	else if (primary_pipeline_type == ERenderType::COLOR || primary_pipeline_type == ERenderType::SKINNED_COLOR)
	{
		const SDS::AlphaMaterialData alpha_data{
			.alpha_cutoff = 0.0f,
			.opacity = renderable.opacity,
			.premultiplied_base_color = 0,
		};
		vkCmdPushConstants(command_buffer, pipeline->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(alpha_data), &alpha_data);
	}

	vkCmdDrawIndexed(command_buffer,
					 mesh.get_num_vertex_indices(),
					 1,		// instance count
					 0,		// first index
					 0,		// first vertex index (used for offsetting and defines the lowest value of gl_VertexIndex)
					 0);	// first instance, used as offset for instance rendering, defines the lower value of gl_InstanceIndex
};
