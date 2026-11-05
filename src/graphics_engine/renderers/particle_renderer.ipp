#pragma once

#include "renderers.hpp"
#include "graphics_engine/graphics_engine.hpp"

#include <cstring>
#include <optional>


class ParticleRenderer : public Renderer
{
public:
	ParticleRenderer(GraphicsEngine& engine);
	~ParticleRenderer();

	virtual void allocate_per_frame_resources(VkImage presentation_image, VkImageView presentation_image_view) override;
	virtual void submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index) override;
	virtual constexpr ERendererType get_renderer_type() const override { return ERendererType::PARTICLE; }
	virtual VkImageView get_output_image_view(uint32_t frame_idx) override { return nullptr; }

	// Called by RasterizationRenderer to render particles within its rendering scope
	void render_particles(VkCommandBuffer command_buffer, uint32_t frame_index);

private:
	void create_unit_quad_buffer();
	void create_instance_buffer();
	void update_instance_buffer(uint32_t frame_index);

	// Unit quad vertex buffer (shared across all particles)
	std::optional<GraphicsBuffer> unit_quad_buffer;
	
	// Instance data buffers (per frame in flight)
	std::vector<GraphicsBuffer> instance_buffers;
	
	static constexpr uint32_t MAX_PARTICLES = 10000;
	static constexpr uint32_t QUAD_VERTICES = 6; // 2 triangles = 6 vertices
};

ParticleRenderer::ParticleRenderer(GraphicsEngine& engine) :
	Renderer(engine)
{
	create_unit_quad_buffer();
}

ParticleRenderer::~ParticleRenderer() = default;

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
	
	const VkDeviceSize buffer_size = sizeof(quad_vertices);
	unit_quad_buffer.emplace(create_buffer(
		buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		1,
		"particle_unit_quad"));
	get_rsrc_mgr().stage_data_to_buffer(
		unit_quad_buffer->get_buffer(), 0, static_cast<uint32_t>(buffer_size),
		[&quad_vertices](std::byte* destination)
		{
			std::memcpy(destination, quad_vertices, sizeof(quad_vertices));
		});
}

void ParticleRenderer::create_instance_buffer()
{
	const VkDeviceSize initial_size = sizeof(SDS::ParticleInstanceData) * 100;
	instance_buffers.push_back(create_buffer(
		initial_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		1,
		"particle_instances"));
}

void ParticleRenderer::update_instance_buffer(uint32_t frame_index)
{
	// Safety check: ensure buffers are allocated
	if (frame_index >= instance_buffers.size())
	{
		return;
	}
	
	const auto& instance_data = get_graphics_engine().get_render_frame().particles;
	
	if (instance_data.empty())
	{
		return;
	}
	
	VkDeviceSize required_size = sizeof(SDS::ParticleInstanceData) * instance_data.size();
	
	// Resize buffer if needed
	if (required_size > instance_buffers[frame_index].get_capacity())
	{
		instance_buffers[frame_index] = create_buffer(
			required_size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			1,
			"particle_instances");
	}
	
	// Copy data to buffer
	void* data = instance_buffers[frame_index].map();
	std::memcpy(data, instance_data.data(), static_cast<size_t>(required_size));
	try
	{
		instance_buffers[frame_index].flush(0, required_size);
	}
	catch (...)
	{
		instance_buffers[frame_index].unmap();
		throw;
	}
	instance_buffers[frame_index].unmap();
}

void ParticleRenderer::allocate_per_frame_resources(VkImage, VkImageView)
{
	create_instance_buffer();
}

void ParticleRenderer::submit_draw_commands(VkCommandBuffer command_buffer, VkImageView presentation_image_view, uint32_t frame_index)
{
	// The rasterization renderer invokes render_particles within its rendering scope.
}

void ParticleRenderer::render_particles(VkCommandBuffer command_buffer, uint32_t frame_index)
{
	// Called within the rasterization rendering scope.
	
	update_instance_buffer(frame_index);
	
	// Check if we have any particles to render
	const auto& instance_data = get_graphics_engine().get_render_frame().particles;
	
	if (instance_data.empty())
	{
		return;
	}
	
	// Safety check: ensure instance buffer is allocated
	if (frame_index >= instance_buffers.size())
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
	VkBuffer vertex_buffers[] = { unit_quad_buffer->get_buffer(), instance_buffers[frame_index].get_buffer() };
	VkDeviceSize offsets[] = { 0, 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 2, vertex_buffers, offsets);
	
	// Draw particles as instanced
	vkCmdDraw(command_buffer, QUAD_VERTICES, static_cast<uint32_t>(instance_data.size()), 0, 0);
}
