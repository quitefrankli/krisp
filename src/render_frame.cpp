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

	std::unordered_map<RenderableID, const RenderableDefinition*>
		next_renderable_definitions;
	next_renderable_definitions.reserve(frame->renderables.size());
	for (const auto& state : frame->renderables)
	{
		if (!state.definition)
			throw std::invalid_argument(
				"RenderFrameMailbox::publish_completed: renderable definition is empty");
		const RenderableID id = state.definition->id;
		if (!next_renderable_definitions.emplace(id, state.definition.get()).second)
			throw std::logic_error(
				"RenderFrameMailbox::publish_completed: duplicate renderable ID");
		if (const auto active = active_renderable_definitions.find(id);
			active != active_renderable_definitions.end())
		{
			if (active->second != state.definition.get())
				throw std::logic_error(
					"RenderFrameMailbox::publish_completed: renderable definition changed for an existing ID");
		}
		else if (seen_renderable_ids.contains(id))
		{
			throw std::logic_error(
				"RenderFrameMailbox::publish_completed: retired renderable ID was reintroduced");
		}
	}

	std::unordered_map<SkeletonID, const RenderSkeletonDefinition*>
		next_skeleton_definitions;
	next_skeleton_definitions.reserve(frame->skeletons.size());
	for (const auto& pose : frame->skeletons)
	{
		if (!pose.definition)
			throw std::invalid_argument(
				"RenderFrameMailbox::publish_completed: skeleton definition is empty");
		const SkeletonID id = pose.definition->id;
		if (!next_skeleton_definitions.emplace(id, pose.definition.get()).second)
			throw std::logic_error(
				"RenderFrameMailbox::publish_completed: duplicate skeleton ID");
		if (const auto active = active_skeleton_definitions.find(id);
			active != active_skeleton_definitions.end())
		{
			if (active->second != pose.definition.get())
				throw std::logic_error(
					"RenderFrameMailbox::publish_completed: skeleton definition changed for an existing ID");
		}
		else if (seen_skeleton_ids.contains(id))
		{
			throw std::logic_error(
				"RenderFrameMailbox::publish_completed: retired skeleton ID was reintroduced");
		}
	}

	const CompletedRenderFramesPtr previous_publication =
		latest.load(std::memory_order_acquire);
	auto publication = std::make_shared<const CompletedRenderFrames>(CompletedRenderFrames{
		.current = std::move(frame),
		.previous = previous_publication ? previous_publication->current : nullptr,
	});
	for (const auto& [id, _] : next_renderable_definitions)
		seen_renderable_ids.insert(id);
	for (const auto& [id, _] : next_skeleton_definitions)
		seen_skeleton_ids.insert(id);
	active_renderable_definitions = std::move(next_renderable_definitions);
	active_skeleton_definitions = std::move(next_skeleton_definitions);
	latest.store(std::move(publication), std::memory_order_release);
}

CompletedRenderFramesPtr RenderFrameMailbox::load_latest() const
{
	return latest.load(std::memory_order_acquire);
}
