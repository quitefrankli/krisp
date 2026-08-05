#pragma once

#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "identifications.hpp"
#include "renderable/render_types.hpp"
#include "shared_data_structures.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>


inline constexpr uint32_t RENDER_FRAME_NO_PARENT = std::numeric_limits<uint32_t>::max();

// Frame-independent draw data. The handles keep the immutable CPU assets alive
// for as long as a published definition can be retained by a consumer.
// Immutable topology for one RenderableID. Structural changes require a new ID.
struct RenderableDefinition
{
	ERenderType pipeline_render_type = ERenderType::COLOR;
	EAlphaMode alpha_mode = EAlphaMode::OPAQUE;
	float alpha_cutoff = 0.5f;
	float opacity = 1.0f;
	bool casts_shadow = true;
	bool render_on_top = false;
	RenderableID id{ 0 };
	std::optional<ObjectID> object_id;
	std::optional<SkeletonID> skeleton_id;
	MeshHandle mesh_owner;
	std::vector<MaterialHandle> material_owners;

	MeshID get_mesh_id() const { return mesh_owner->get_id(); }
	// Direct owner-based access keeps graphics reads off the mutable registries.
	const Mesh& get_mesh() const { return mesh_owner->get(); }
	MaterialID get_material_id(size_t index) const
	{
		return material_owners.at(index)->get_id();
	}
	const Material& get_material(size_t index) const
	{
		return material_owners.at(index)->get();
	}
	std::vector<MaterialID> get_material_ids() const
	{
		std::vector<MaterialID> ids;
		ids.reserve(material_owners.size());
		for (const auto& owner : material_owners)
			ids.push_back(owner->get_id());
		return ids;
	}
};

using RenderableDefinitionPtr = std::shared_ptr<const RenderableDefinition>;

// Frame-varying instance data. The producer publishes an already composed
// model transform and effective visibility for each renderable.
struct RenderableState
{
	RenderableDefinitionPtr definition;
	glm::mat4 model_transform{ 1.0f };
	bool visible = true;
};

// Bind-pose topology is definition data; animated local transforms live in
// RenderSkeletonPose. parent_index addresses its containing bones vector.
struct RenderBoneDefinition
{
	uint32_t parent_index = RENDER_FRAME_NO_PARENT;
	glm::mat4 inverse_bind_pose{ 1.0f };
};

// Immutable topology for one SkeletonID. A different rig requires a new ID.
struct RenderSkeletonDefinition
{
	SkeletonID id;
	std::vector<RenderBoneDefinition> bones;
};

using RenderSkeletonDefinitionPtr = std::shared_ptr<const RenderSkeletonDefinition>;

// local_transforms has the same order and size as definition->bones.
struct RenderSkeletonPose
{
	RenderSkeletonDefinitionPtr definition;
	std::vector<glm::mat4> local_transforms;
};

struct RenderCameraState
{
	glm::mat4 view{ 1.0f };
	glm::mat4 projection{ 1.0f };
	glm::vec3 position{ 0.0f };
};

struct RenderLightState
{
	ObjectID object_id;
	glm::vec3 position{ 0.0f };
	float intensity = 1.0f;
	glm::vec3 color{ 1.0f };
};

// Complete presentation state for a frame. This is state rather than an event
// stream so skipped mailbox publications cannot lose control changes.
struct RenderViewState
{
	ERenderMode render_mode = ERenderMode::RASTERIZED;
	float exposure_ev = 0.0f;
	std::unordered_set<ObjectID> stenciled_objects;
};

// One coherent, completed render snapshot. Consumers only receive const shared
// ownership, so they may retain a frame without observing subsequent writes.
struct RenderFrame
{
	uint64_t frame_number = 0;
	RenderViewState view;
	RenderCameraState camera;
	std::vector<RenderableState> renderables;
	std::vector<RenderSkeletonPose> skeletons;
	std::vector<SDS::ParticleInstanceData> particles;
	std::optional<RenderLightState> active_light;
};

using RenderFramePtr = std::shared_ptr<const RenderFrame>;

// Composes parent-before-child model transforms regardless of input ordering.
// Throws std::invalid_argument for mismatched counts, invalid parents, or cycles.
std::vector<glm::mat4> compose_transform_hierarchy(
	std::span<const glm::mat4> local_transforms,
	std::span<const uint32_t> parent_indices);

// Produces shader-ready bone transforms: composed pose * inverse bind pose.
std::vector<SDS::Bone> compose_bone_transforms(
	std::span<const glm::mat4> local_transforms,
	const RenderSkeletonDefinition& definition);

// previous is empty for the first publication, then refers to the frame that
// was current immediately before the latest publication.
struct CompletedRenderFrames
{
	RenderFramePtr current;
	RenderFramePtr previous;
};

using CompletedRenderFramesPtr = std::shared_ptr<const CompletedRenderFrames>;

class RenderFrameMailbox
{
public:
	RenderFrameMailbox() = default;
	RenderFrameMailbox(const RenderFrameMailbox&) = delete;
	RenderFrameMailbox(RenderFrameMailbox&&) = delete;
	RenderFrameMailbox& operator=(const RenderFrameMailbox&) = delete;
	RenderFrameMailbox& operator=(RenderFrameMailbox&&) = delete;

	// One producer publishes completed immutable frames. Concurrent consumers
	// may retain any returned publication without explicit waiting. Publishing
	// replaces an unconsumed frame: the latest publication always wins.
	// Definition identity is enforced across every publication: an active ID must
	// retain the same definition, and a retired ID cannot be reintroduced.
	void publish_completed(RenderFramePtr frame);
	// Returns empty before the first publication.
	CompletedRenderFramesPtr load_latest() const;

private:
	std::atomic<CompletedRenderFramesPtr> latest;
	std::unordered_map<RenderableID, const RenderableDefinition*>
		active_renderable_definitions;
	std::unordered_map<SkeletonID, const RenderSkeletonDefinition*>
		active_skeleton_definitions;
	std::unordered_set<RenderableID> seen_renderable_ids;
	std::unordered_set<SkeletonID> seen_skeleton_ids;
};
