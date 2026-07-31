#include "game_engine.hpp"

#include "camera.hpp"

#include <glm/vector_relational.hpp>

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <unordered_set>


namespace
{
bool matrices_equal(const glm::mat4& lhs, const glm::mat4& rhs)
{
	for (glm::length_t column = 0; column < 4; ++column)
		if (!glm::all(glm::equal(lhs[column], rhs[column])))
			return false;
	return true;
}

bool renderable_matches(
	const RenderableDefinition& definition,
	const RenderableID id,
	const RenderableAttachment& attachment)
{
	const auto& renderable = attachment.renderable;
	return definition.pipeline_render_type == renderable.pipeline_render_type
		&& definition.alpha_mode == renderable.alpha_mode
		&& definition.alpha_cutoff == renderable.alpha_cutoff
		&& definition.opacity == renderable.opacity
		&& definition.casts_shadow == renderable.casts_shadow
		&& definition.render_on_top == renderable.render_on_top
		&& definition.id == id
		&& definition.object_id == attachment.object_id
		&& definition.skeleton_id == attachment.skeleton_id
		&& definition.mesh_owner == renderable.mesh_owner
		&& definition.material_owners == renderable.material_owners;
}

RenderableDefinition make_renderable_definition(
	const RenderableID id,
	const RenderableAttachment& attachment,
	const RenderDefinitionVersion version)
{
	const auto& renderable = attachment.renderable;
	return {
		.pipeline_render_type = renderable.pipeline_render_type,
		.alpha_mode = renderable.alpha_mode,
		.alpha_cutoff = renderable.alpha_cutoff,
		.opacity = renderable.opacity,
		.casts_shadow = renderable.casts_shadow,
		.render_on_top = renderable.render_on_top,
		.id = id,
		.version = version,
		.object_id = attachment.object_id,
		.skeleton_id = attachment.skeleton_id,
		.mesh_owner = renderable.mesh_owner,
		.material_owners = renderable.material_owners,
	};
}

bool skeleton_definition_matches(
	const RenderSkeletonDefinition& definition,
	const SkeletalRenderStateSnapshot& snapshot)
{
	if (definition.bones.size() != snapshot.parent_indices.size()
		|| definition.bones.size() != snapshot.inverse_bind_poses.size())
		return false;

	for (size_t index = 0; index < definition.bones.size(); ++index)
		if (definition.bones[index].parent_index != snapshot.parent_indices[index]
			|| !matrices_equal(
				definition.bones[index].inverse_bind_pose,
				snapshot.inverse_bind_poses[index]))
			return false;
	return true;
}
}


RenderableDefinitionPtr GameEngine::get_renderable_definition(
	const RenderableID id,
	const RenderableAttachment& attachment)
{
	const auto cached = renderable_definitions.find(id);
	if (cached != renderable_definitions.end()
		&& renderable_matches(*cached->second, id, attachment))
		return cached->second;

	auto definition = std::make_shared<const RenderableDefinition>(
		make_renderable_definition(id, attachment, next_render_definition_version++));
	renderable_definitions.insert_or_assign(id, definition);
	return definition;
}

RenderSkeletonDefinitionPtr GameEngine::get_render_skeleton_definition(
	const SkeletonID id,
	const SkeletalRenderStateSnapshot& snapshot)
{
	const auto cached = render_skeleton_definitions.find(id);
	if (cached != render_skeleton_definitions.end()
		&& skeleton_definition_matches(*cached->second, snapshot))
		return cached->second;

	std::vector<RenderBoneDefinition> bones;
	bones.reserve(snapshot.parent_indices.size());
	for (size_t index = 0; index < snapshot.parent_indices.size(); ++index)
		bones.push_back({
			.parent_index = snapshot.parent_indices[index],
			.inverse_bind_pose = snapshot.inverse_bind_poses[index],
		});

	auto definition = std::make_shared<const RenderSkeletonDefinition>(RenderSkeletonDefinition{
		.id = id,
		.version = next_render_definition_version++,
		.bones = std::move(bones),
	});
	render_skeleton_definitions.insert_or_assign(id, definition);
	return definition;
}

RenderFrame GameEngine::build_render_frame()
{
	RenderFrame frame;
	frame.frame_number = next_render_frame_number;
	frame.camera = {
		.view = camera->get_view(),
		.projection = camera->get_projection(),
		.position = camera->get_position(),
	};

	const auto renderable_ids = ecs.get_renderable_ids();
	frame.renderables.reserve(renderable_ids.size());
	std::vector<SkeletonID> attached_skeletons;
	for (const RenderableID id : renderable_ids)
	{
		const auto& attachment = ecs.get_renderable(id);
		if (attachment.skeleton_id)
			attached_skeletons.push_back(*attachment.skeleton_id);
		frame.renderables.push_back({
			.definition = get_renderable_definition(id, attachment),
			.model_transform = ecs.get_renderable_transform(id),
			.visible = ecs.get_renderable_visibility(id),
		});
	}

	std::ranges::sort(attached_skeletons);
	const auto unique_skeletons = std::ranges::unique(attached_skeletons);
	attached_skeletons.erase(unique_skeletons.begin(), attached_skeletons.end());
	frame.skeletons.reserve(attached_skeletons.size());
	for (const SkeletonID id : attached_skeletons)
	{
		auto snapshot = ecs.get_skeletal_component(id).snapshot_render_state();
		frame.skeletons.push_back({
			.definition = get_render_skeleton_definition(id, snapshot),
			.local_transforms = std::move(snapshot.local_transforms),
		});
	}

	ecs.prepare_render_data(frame.particles);
	if (ecs.has_light_source())
	{
		const ObjectID light_id = ecs.get_global_light_source();
		if (!ecs.has_object(light_id))
			throw std::runtime_error(
				"GameEngine::build_render_frame: active light object is missing");
		const LightComponent* component = ecs.get_light_component(light_id);
		frame.active_light = RenderLightState{
			.object_id = light_id,
			.position = glm::vec3(ecs.get_transform(light_id)[3]),
			.intensity = component->intensity,
			.color = component->color,
		};
	}

	std::erase_if(renderable_definitions, [this](const auto& entry) {
		return !ecs.has_renderable(entry.first);
	});
	const auto live_skeleton_ids = ecs.get_skeleton_ids();
	const std::unordered_set<SkeletonID> live_skeletons(
		live_skeleton_ids.begin(), live_skeleton_ids.end());
	std::erase_if(render_skeleton_definitions, [&live_skeletons](const auto& entry) {
		return !live_skeletons.contains(entry.first);
	});

	++next_render_frame_number;
	return frame;
}

void GameEngine::publish_completed_render_frame()
{
	graphics_engine->publish_completed_render_frame(
		std::make_shared<const RenderFrame>(build_render_frame()));
}
