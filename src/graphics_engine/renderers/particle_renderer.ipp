#pragma once

#include "renderers.hpp"
#include "graphics_engine/graphics_engine.hpp"
#include "entity_component_system/particle_system.hpp"
#include "entity_component_system/ecs.hpp"


class ParticleRenderer : public Renderer
{
public:
	ParticleRenderer(GraphicsEngine& engine);
	~ParticleRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::PARTICLE; }
	virtual VkImageView get_output_image_view(uint32_t frame_idx) override { return nullptr; }
	virtual VkRenderPass get_render_pass() override;

	// Called by RasterizationRenderer to render particles within its render pass
	void render_particles(VkCommandBuffer command_buffer, uint32_t frame_index);

private:
	void create_unit_quad_buffer();
	void create_instance_buffer();
	void update_instance_buffer(uint32_t frame_index);

	// Unit quad vertex buffer (shared across all particles)
	VkBuffer unit_quad_buffer = VK_NULL_HANDLE;
	VkDeviceMemory unit_quad_buffer_memory = VK_NULL_HANDLE;
	
	// Instance data buffers (per frame in flight)
	std::vector<VkBuffer> instance_buffers;
	std::vector<VkDeviceMemory> instance_buffer_memories;
	std::vector<VkDeviceSize> instance_buffer_sizes;
	
	static constexpr uint32_t MAX_PARTICLES = 10000;
	static constexpr uint32_t QUAD_VERTICES = 6; // 2 triangles = 6 vertices
};

ParticleRenderer::ParticleRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	// Note: We don't create our own render pass, we use the rasterization renderer's
	create_unit_quad_buffer();
	create_instance_buffer();
}

ParticleRenderer::~ParticleRenderer()
{
	auto logical_device = get_logical_device();
	
	if (unit_quad_buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(logical_device, unit_quad_buffer, nullptr);
	}
	if (unit_quad_buffer_memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(logical_device, unit_quad_buffer_memory, nullptr);
	}
	
	for (size_t i = 0; i < instance_buffers.size(); ++i)
	{
		if (instance_buffers[i] != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(logical_device, instance_buffers[i], nullptr);
		}
		if (instance_buffer_memories[i] != VK_NULL_HANDLE)
		{
			vkFreeMemory(logical_device, instance_buffer_memories[i], nullptr);
		}
	}
	
	// Note: We don't own the render pass (using rasterization renderer's)
	// and we don't create framebuffers anymore
}

void ParticleRenderer::create_unit_quad_buffer()
{
	// Create a unit quad centered at origin
	// Each vertex: vec2 position + vec2 texcoord
	float quad_vertices[] = {
		// Position      // TexCoord
		-0.5f, -0.5f,    0.0f, 0.0f,  // Bottom-left
		 0.5f, -0.5f,    1.0f, 0.0f,  // Bottom-right
		 0.5f,  0.5f,    1.0f, 1.0f,  // Top-right
		-0.5f, -0.5f,    0.0f, 0.0f,  // Bottom-left
		 0.5f,  0.5f,    1.0f, 1.0f,  // Top-right
		-0.5f,  0.5f,    0.0f, 1.0f   // Top-left
	};
	
	VkDeviceSize buffer_size = sizeof(quad_vertices);
	
	// Create staging buffer
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;
	get_rsrc_mgr().create_buffer_deprecated(
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		staging_buffer,
		staging_buffer_memory);
	
	// Copy data to staging buffer
	void* data;
	vkMapMemory(get_logical_device(), staging_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, quad_vertices, (size_t)buffer_size);
	vkUnmapMemory(get_logical_device(), staging_buffer_memory);
	
	// Create device local buffer
	get_rsrc_mgr().create_buffer_deprecated(
		buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		unit_quad_buffer,
		unit_quad_buffer_memory);
	
	// Copy from staging to device local
	get_graphics_engine().copy_buffer(staging_buffer, unit_quad_buffer, buffer_size);
	
	// Cleanup staging buffer
	vkDestroyBuffer(get_logical_device(), staging_buffer, nullptr);
	vkFreeMemory(get_logical_device(), staging_buffer_memory, nullptr);
}

void ParticleRenderer::create_instance_buffer()
{
	uint32_t frames_in_flight = get_graphics_engine().get_num_swapchain_images();
	
	// Safety check: ensure we have at least one frame
	if (frames_in_flight == 0)
	{
		frames_in_flight = 3; // Default to triple buffering
	}
	
	instance_buffers.resize(frames_in_flight);
	instance_buffer_memories.resize(frames_in_flight);
	instance_buffer_sizes.resize(frames_in_flight, 0);
	
	VkDeviceSize initial_size = sizeof(SDS::ParticleInstanceData) * 100; // Start small, will grow as needed
	
	for (uint32_t i = 0; i < frames_in_flight; ++i)
	{
		get_rsrc_mgr().create_buffer_deprecated(
			initial_size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			instance_buffers[i],
			instance_buffer_memories[i]);
		instance_buffer_sizes[i] = initial_size;
	}
}

void ParticleRenderer::update_instance_buffer(uint32_t frame_index)
{
	// Safety check: ensure buffers are allocated
	if (frame_index >= instance_buffers.size())
	{
		return;
	}
	
	// Get particle data from particle system
	auto& ecs = get_graphics_engine().get_ecs();
	std::vector<SDS::ParticleInstanceData> instance_data;
	ecs.prepare_render_data(instance_data);
	
	if (instance_data.empty())
	{
		return;
	}
	
	VkDeviceSize required_size = sizeof(SDS::ParticleInstanceData) * instance_data.size();
	
	// Resize buffer if needed
	if (required_size > instance_buffer_sizes[frame_index])
	{
		vkDestroyBuffer(get_logical_device(), instance_buffers[frame_index], nullptr);
		vkFreeMemory(get_logical_device(), instance_buffer_memories[frame_index], nullptr);
		
		get_rsrc_mgr().create_buffer_deprecated(
			required_size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			instance_buffers[frame_index],
			instance_buffer_memories[frame_index]);
		instance_buffer_sizes[frame_index] = required_size;
	}
	
	// Copy data to buffer
	void* data;
	vkMapMemory(get_logical_device(), instance_buffer_memories[frame_index], 0, required_size, 0, &data);
	memcpy(data, instance_data.data(), (size_t)required_size);
	vkUnmapMemory(get_logical_device(), instance_buffer_memories[frame_index]);
}

void ParticleRenderer::allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view)
{
	// Use the rasterization renderer's framebuffer
	// We don't need to create our own framebuffer since we're using the same render pass
	// The particles will be rendered in the same render pass as the main scene
	
	// Just add a null framebuffer entry to maintain the frame indexing
	this->frame_buffers.push_back(VK_NULL_HANDLE);
}

void ParticleRenderer::submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index)
{
	// Note: Particle rendering is now done within the RasterizationRenderer's render pass
	// This function is kept for compatibility but doesn't do anything
	// The actual rendering happens via render_particles() called by RasterizationRenderer
}

void ParticleRenderer::render_particles(VkCommandBuffer command_buffer, uint32_t frame_index)
{
	// Note: This function is called within the rasterization render pass
	// We don't begin/end a render pass here, just bind the pipeline and draw
	
	update_instance_buffer(frame_index);
	
	// Check if we have any particles to render
	auto& ecs = get_graphics_engine().get_ecs();
	std::vector<SDS::ParticleInstanceData> instance_data;
	ecs.prepare_render_data(instance_data);
	
	if (instance_data.empty())
	{
		return;
	}
	
	// Safety check: ensure instance buffer is allocated
	if (frame_index >= instance_buffers.size() || instance_buffers[frame_index] == VK_NULL_HANDLE)
	{
		return;
	}
	
	// Bind global descriptor set
	std::vector<VkDescriptorSet> per_frame_dsets = {
		get_rsrc_mgr().get_global_dset(frame_index)
	};
	
	// Get particle pipeline
	const auto* pipeline = get_graphics_engine().get_pipeline_mgr().fetch_pipeline({
		ERenderType::PARTICLE, EPipelineModifier::NONE });
	
	if (!pipeline)
	{
		return;
	}
	
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->graphics_pipeline);
	
	vkCmdBindDescriptorSets(command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline->pipeline_layout,
		SDS::RASTERIZATION_LOW_FREQ_SET_OFFSET,
		per_frame_dsets.size(),
		per_frame_dsets.data(),
		0,
		nullptr);
	
	// Bind vertex buffers
	VkBuffer vertex_buffers[] = { unit_quad_buffer, instance_buffers[frame_index] };
	VkDeviceSize offsets[] = { 0, 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 2, vertex_buffers, offsets);
	
	// Draw particles as instanced
	vkCmdDraw(command_buffer, QUAD_VERTICES, static_cast<uint32_t>(instance_data.size()), 0, 0);
}

VkRenderPass ParticleRenderer::get_render_pass()
{
	// Use the rasterization renderer's render pass
	return get_graphics_engine().get_renderer_mgr().
		get_renderer(ERendererType::RASTERIZATION).get_render_pass();
}
