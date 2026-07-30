#include "game_engine.hpp"

#include "camera.hpp"

#include <glm/vector_relational.hpp>

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
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
	const Renderable& renderable)
{
	return definition.pipeline_render_type == renderable.pipeline_render_type
		&& definition.alpha_mode == renderable.alpha_mode
		&& definition.alpha_cutoff == renderable.alpha_cutoff
		&& definition.opacity == renderable.opacity
		&& definition.casts_shadow == renderable.casts_shadow
		&& definition.render_on_top == renderable.render_on_top
		&& matrices_equal(definition.local_transform, renderable.local_transform.get_mat4())
		&& definition.mesh_owner == renderable.mesh_owner
		&& definition.material_owners == renderable.material_owners;
}

bool object_definition_matches(
	const RenderObjectDefinition& definition,
	const Object& object,
	const std::optional<SkeletonID> skeleton_id)
{
	if (definition.id != object.get_id()
		|| definition.skeleton_id != skeleton_id
		|| definition.renderables.size() != object.renderables.size())
		return false;

	for (size_t index = 0; index < object.renderables.size(); ++index)
		if (!renderable_matches(definition.renderables[index], object.renderables[index]))
			return false;
	return true;
}

RenderableDefinition make_renderable_definition(const Renderable& renderable)
{
	return {
		.pipeline_render_type = renderable.pipeline_render_type,
		.alpha_mode = renderable.alpha_mode,
		.alpha_cutoff = renderable.alpha_cutoff,
		.opacity = renderable.opacity,
		.casts_shadow = renderable.casts_shadow,
		.render_on_top = renderable.render_on_top,
		.local_transform = renderable.local_transform.get_mat4(),
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


RenderObjectDefinitionPtr GameEngine::get_render_object_definition(
	const Object& object,
	const std::optional<SkeletonID> skeleton_id)
{
	const auto cached = render_object_definitions.find(object.get_id());
	if (cached != render_object_definitions.end()
		&& object_definition_matches(*cached->second, object, skeleton_id))
		return cached->second;

	std::vector<RenderableDefinition> renderables;
	renderables.reserve(object.renderables.size());
	for (const auto& renderable : object.renderables)
		renderables.push_back(make_renderable_definition(renderable));

	auto definition = std::make_shared<const RenderObjectDefinition>(RenderObjectDefinition{
		.id = object.get_id(),
		.version = next_render_definition_version++,
		.renderables = std::move(renderables),
		.skeleton_id = skeleton_id,
	});
	render_object_definitions.insert_or_assign(object.get_id(), definition);
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

	std::vector<const Object*> ordered_objects;
	ordered_objects.reserve(objects.size());
	for (const auto& [_, object] : objects)
		ordered_objects.push_back(object.get());
	std::ranges::sort(ordered_objects, {}, [](const Object* object) {
		return object->get_id().get_underlying();
	});
	if (ordered_objects.size() >= RENDER_FRAME_NO_PARENT)
		throw std::runtime_error("GameEngine::build_render_frame: too many objects");

	std::unordered_map<ObjectID, uint32_t> object_indices;
	object_indices.reserve(ordered_objects.size());
	for (uint32_t index = 0; index < ordered_objects.size(); ++index)
		object_indices.emplace(ordered_objects[index]->get_id(), index);

	frame.objects.reserve(ordered_objects.size());
	std::vector<SkeletonID> attached_skeletons;
	for (const Object* object : ordered_objects)
	{
		const auto skeleton_id = ecs.get_skeleton_id(object->get_id());
		if (skeleton_id)
			attached_skeletons.push_back(*skeleton_id);

		uint32_t parent_index = RENDER_FRAME_NO_PARENT;
		glm::mat4 local_transform;
		if (const auto parent_id = object->get_parent_id())
		{
			const auto parent = object_indices.find(*parent_id);
			if (parent != object_indices.end())
			{
				parent_index = parent->second;
				local_transform = object->get_relative_transform();
			}
			else
				local_transform = object->get_transform();
		}
		else
			local_transform = object->get_transform();

		frame.objects.push_back({
			.definition = get_render_object_definition(*object, skeleton_id),
			.local_transform = local_transform,
			.parent_index = parent_index,
			.visible = object->get_visibility(),
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
		const auto object = object_indices.find(light_id);
		if (object == object_indices.end())
			throw std::runtime_error(
				"GameEngine::build_render_frame: active light object is missing");
		const LightComponent* component = ecs.get_light_component(light_id);
		frame.active_light = RenderLightState{
			.object_index = object->second,
			.intensity = component->intensity,
			.color = component->color,
		};
	}

	std::erase_if(render_object_definitions, [this](const auto& entry) {
		return !objects.contains(entry.first);
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
