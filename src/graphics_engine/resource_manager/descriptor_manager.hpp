#pragma once

#include "constants.hpp"
#include "graphics_engine/graphics_engine_base_module.hpp"
#include "graphics_buffer_manager.hpp"


class GraphicsDescriptorManager : public GraphicsEngineBaseModule
{
public:
	GraphicsDescriptorManager(GraphicsEngine& engine, const GraphicsBufferManager& buffer_manager);
	virtual ~GraphicsDescriptorManager() override;

	// returned layout is owned by this class, therefore it's not necessary to free the return value
	VkDescriptorSetLayout request_dset_layout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);

	VkDescriptorSet reserve_dset(const VkDescriptorSetLayout layout);

	// if X number of dsets of layout A are desired, then X number of layout A's must be passed in
	std::vector<VkDescriptorSet> reserve_dsets(const std::vector<VkDescriptorSetLayout>& layouts);

	void free_dset(VkDescriptorSet set);
	void free_dsets(std::vector<VkDescriptorSet>& dsets);

	void create_descriptor_pool();

	VkDescriptorPool& get_descriptor_pool() { return descriptor_pool; }

public: // accessors for specific descriptors/layouts
	const VkDescriptorSetLayout& get_low_freq_dset_layout() const { return low_freq_dset_layout; }
	const VkDescriptorSetLayout& get_shadow_map_dset_layout() const { return shadow_map_dset_layout; }
	const VkDescriptorSetLayout& get_per_renderable_frame_dset_layout() const
	{
		return per_renderable_frame_dset_layout;
	}
	const VkDescriptorSetLayout& get_renderable_dset_layout() const { return renderable_dset_layout; }
#if 0 // Ray tracing is unsupported; retained for future repair.
	const VkDescriptorSetLayout& get_mesh_data_dset_layout() const { return mesh_data_dset_layout; }
	const VkDescriptorSetLayout& get_raytracing_tlas_dset_layout() const { return raytracing_tlas_dset_layout; }
#endif

	std::vector<VkDescriptorSetLayout> get_rasterization_descriptor_set_layouts() const;
#if 0
	std::vector<VkDescriptorSetLayout> get_raytracing_descriptor_set_layouts() const;
#endif

	VkDescriptorSet get_global_dset(uint32_t frame_idx) const { return global_dsets[frame_idx]; }
#if 0
	VkDescriptorSet get_mesh_data_dset() const { return mesh_data_dset; }
#endif

private:
	void setup_descriptor_set_layouts();
	void allocate_global_dset(VkBuffer global_buffer, const std::vector<uint32_t>& global_buffer_offsets);
#if 0
	void allocate_mesh_data_dset(VkBuffer mapping_buffer, VkBuffer vertex_buffer, VkBuffer index_buffer);
#endif

	static constexpr uint32_t MAX_LOW_FREQ_DESCRIPTOR_SETS =
		CSTS::UPPERBOUND_SWAPCHAIN_IMAGES; // for GUBO i.e. camera & lighting
	static constexpr uint32_t MAX_RENDERABLE_INSTANCES =
		GraphicsBufferManager::NUM_EXPECTED_RENDERABLES
		* CSTS::MAX_CONCURRENT_RENDER_RESOURCE_SETS;
	static constexpr uint32_t MAX_RENDERABLE_FRAME_DESCRIPTOR_SETS =
		MAX_RENDERABLE_INSTANCES * CSTS::UPPERBOUND_SWAPCHAIN_IMAGES;
	static constexpr uint32_t MAX_RENDERABLE_DESCRIPTOR_SETS =
		MAX_RENDERABLE_INSTANCES;
#if 0
	static constexpr int MAX_RAY_TRACING_DESCRIPTOR_SETS = 1000;
	static constexpr int MAX_MESH_DATA_DESCRIPTOR_SETS = 1;
#endif
	static constexpr uint32_t MAX_UNIFORM_BUFFER_DESCRIPTORS =
		MAX_LOW_FREQ_DESCRIPTOR_SETS + MAX_RENDERABLE_FRAME_DESCRIPTOR_SETS;
	static constexpr uint32_t MAX_STORAGE_BUFFER_DESCRIPTORS =
		MAX_RENDERABLE_FRAME_DESCRIPTOR_SETS + MAX_RENDERABLE_DESCRIPTOR_SETS;
	static constexpr uint32_t MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTORS =
		3 * MAX_RENDERABLE_DESCRIPTOR_SETS;
	static constexpr uint32_t MAX_ENGINE_DESCRIPTOR_SETS = 64;
	static constexpr uint32_t MAX_IMGUI_DESCRIPTOR_SETS = 50;
	static constexpr uint32_t MAX_DESCRIPTOR_SETS =
		MAX_LOW_FREQ_DESCRIPTOR_SETS
		+ MAX_RENDERABLE_FRAME_DESCRIPTOR_SETS
		+ MAX_RENDERABLE_DESCRIPTOR_SETS
		+ MAX_ENGINE_DESCRIPTOR_SETS
		+ MAX_IMGUI_DESCRIPTOR_SETS;
#if 0
	static_assert(
		MAX_LOW_FREQ_DESCRIPTOR_SETS + MAX_HIGH_FREQ_DESCRIPTOR_SETS +
		MAX_RAY_TRACING_DESCRIPTOR_SETS + MAX_MESH_DATA_DESCRIPTOR_SETS +
		MAX_IMGUI_DESCRIPTOR_SETS <= MAX_DESCRIPTOR_SETS,
		"GraphicsResourceManager: too many descriptor sets!");
#endif

	VkDescriptorPool descriptor_pool;
	std::vector<VkDescriptorSetLayout> all_dset_layouts;
	VkDescriptorSetLayout low_freq_dset_layout;
	VkDescriptorSetLayout shadow_map_dset_layout;
	VkDescriptorSetLayout per_renderable_frame_dset_layout;
	VkDescriptorSetLayout renderable_dset_layout;
#if 0
	VkDescriptorSetLayout mesh_data_dset_layout;
	VkDescriptorSetLayout raytracing_tlas_dset_layout;
#endif

	// 1 dset per swapchain frame, currently only used for camera and global lighting
	std::vector<VkDescriptorSet> global_dsets;
#if 0
	VkDescriptorSet mesh_data_dset;
#endif

};
