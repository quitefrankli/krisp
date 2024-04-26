#include "renderer.hpp"
#include "rasterization_renderer.ipp"
#include "gui_renderer.ipp"
#include "raytracing_renderer.ipp"
#include "offscreen_gui_viewport_renderer.ipp"
#include "shadowmap_renderer.ipp"
#include "quad_renderer.ipp"
#include "renderer_manager.ipp"
#include "graphics_engine/graphics_engine.hpp"
#include "game_engine.hpp"


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
	return this->get_graphics_engine().get_extent();
}

void Renderer::draw_object(VkCommandBuffer command_buffer,
						   uint32_t frame_index,
						   const GraphicsEngineObject& object, 
						   const GraphicsEnginePipeline& pipeline)
{
	// NOTE:A the vertex and index buffers contain the data for all the 'vertex_sets/shapes' concatenated together
	
	// TODO: fix this
	VkDeviceSize buffer_offset = object.get_game_object().renderables.empty() ? 
		get_rsrc_mgr().get_vertex_buffer_offset(object.get_id()) :
		get_rsrc_mgr().get_vertex_buffer_offset(object.get_game_object().renderables[0].mesh);
	VkBuffer buffer = get_rsrc_mgr().get_vertex_buffer();
	vkCmdBindVertexBuffers(
		command_buffer, 
		0, 										// first buffer in vertex buffers array
		1, 										// number of vertex buffers
		&buffer, 								// array of vertex buffers to bind
		&buffer_offset							// byte offset to start from for each buffer
	);
	
	// TODO: fix this
	const size_t index_buffer_offset = object.get_game_object().renderables.empty() ?
		get_rsrc_mgr().get_index_buffer_offset(object.get_id()) :
		get_rsrc_mgr().get_index_buffer_offset(object.get_game_object().renderables[0].mesh);
	vkCmdBindIndexBuffer(
		command_buffer,
		get_rsrc_mgr().get_index_buffer(),
		index_buffer_offset,
		VK_INDEX_TYPE_UINT32
	);

	std::vector<VkDescriptorSet> object_dsets = { object.get_dset(frame_index) };
	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline.pipeline_layout,
		SDS::RASTERIZATION_HIGH_FREQ_PER_OBJ_SET_OFFSET,
		object_dsets.size(),
		object_dsets.data(),
		0,
		nullptr);

	// this really should be per object, we will adjust in the future
	int total_vertex_offset = 0;
	int total_index_offset = 0;
	for (const auto& shape : object.get_shapes())
	{
		// descriptor binding, we need to bind the descriptor set for each swap chain image and for each vertex_set with different descriptor set
		std::vector<VkDescriptorSet> shape_dsets = { shape.get_dset() };
		vkCmdBindDescriptorSets(command_buffer, 
								VK_PIPELINE_BIND_POINT_GRAPHICS, // unlike vertex buffer, descriptor sets are not unique to the graphics pipeline, compute pipeline is also possible
								pipeline.pipeline_layout, 
								SDS::RASTERIZATION_HIGH_FREQ_PER_SHAPE_SET_OFFSET, // offset
								shape_dsets.size(), // number of sets to bind
								shape_dsets.data(),
								0,
								nullptr);

		vkCmdDrawIndexed(
			command_buffer,
			shape.get_num_vertex_indices(),	// vertex count
			1,	// instance count
			total_index_offset,	// first index
			total_vertex_offset,	// first vertex index (used for offsetting and defines the lowest value of gl_VertexIndex)
			0);	// first instance, used as offset for instance rendering, defines the lower value of gl_InstanceIndex

		// this is not get_num_vertex_indices() because we want to offset the vertex set essentially
		// see NOTE:A
		total_vertex_offset += shape.get_num_unique_vertices();
		total_index_offset += shape.get_num_vertex_indices();
	}
};

constexpr uint32_t Renderer::get_num_inflight_frames()
{
	return GraphicsEngine::get_num_swapchain_images();	
}