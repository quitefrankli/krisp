#include "render_frame.hpp"

#include <functional>
#include <stdexcept>


std::vector<glm::mat4> compose_transform_hierarchy(
	const std::span<const glm::mat4> local_transforms,
	const std::span<const uint32_t> parent_indices)
{
	if (local_transforms.size() != parent_indices.size())
		throw std::invalid_argument("compose_transform_hierarchy: transform and parent counts differ");

	const size_t count = local_transforms.size();
	for (const uint32_t parent : parent_indices)
		if (parent != RENDER_FRAME_NO_PARENT && parent >= count)
			throw std::invalid_argument("compose_transform_hierarchy: invalid parent index");

	enum class ResolutionState : uint8_t
	{
		UNRESOLVED,
		RESOLVING,
		RESOLVED,
	};

	std::vector<glm::mat4> composed(count);
	std::vector<ResolutionState> states(count, ResolutionState::UNRESOLVED);
	std::function<void(uint32_t)> resolve = [&](const uint32_t index)
	{
		if (states[index] == ResolutionState::RESOLVED)
			return;
		if (states[index] == ResolutionState::RESOLVING)
			throw std::invalid_argument("compose_transform_hierarchy: parent cycle");

		states[index] = ResolutionState::RESOLVING;
		const uint32_t parent = parent_indices[index];
		if (parent == RENDER_FRAME_NO_PARENT)
			composed[index] = local_transforms[index];
		else
		{
			resolve(parent);
			composed[index] = composed[parent] * local_transforms[index];
		}
		states[index] = ResolutionState::RESOLVED;
	};

	for (uint32_t index = 0; index < count; ++index)
		resolve(index);

	return composed;
}

std::vector<SDS::Bone> compose_bone_transforms(
	const std::span<const glm::mat4> local_transforms,
	const RenderSkeletonDefinition& definition)
{
	std::vector<uint32_t> parent_indices;
	parent_indices.reserve(definition.bones.size());
	for (const auto& bone : definition.bones)
		parent_indices.push_back(bone.parent_index);

	const auto model_transforms = compose_transform_hierarchy(local_transforms, parent_indices);
	std::vector<SDS::Bone> result(definition.bones.size());
	for (size_t index = 0; index < definition.bones.size(); ++index)
	{
		result[index].inverse_transform = definition.bones[index].inverse_bind_pose;
		result[index].final_transform =
			model_transforms[index] * definition.bones[index].inverse_bind_pose;
	}
	return result;
}

void RenderFrameMailbox::publish_completed(RenderFramePtr frame)
{
	if (!frame)
		throw std::invalid_argument("RenderFrameMailbox::publish_completed: frame is empty");

	const CompletedRenderFramesPtr previous_publication =
		latest.load(std::memory_order_acquire);
	auto publication = std::make_shared<const CompletedRenderFrames>(CompletedRenderFrames{
		.current = std::move(frame),
		.previous = previous_publication ? previous_publication->current : nullptr,
	});
	latest.store(std::move(publication), std::memory_order_release);
}

CompletedRenderFramesPtr RenderFrameMailbox::load_latest() const
{
	return latest.load(std::memory_order_acquire);
}
