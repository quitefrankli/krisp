#include "render_draw_list.hpp"

#include "graphics_engine_object.hpp"

#include <algorithm>
#include <ranges>
#include <tuple>


bool RenderSortKey::operator<(const RenderSortKey& other) const
{
	return std::tie(
		render_type,
		alpha_mode,
		mesh_id,
		material_ids,
		object_id,
		renderable_index)
		< std::tie(
			other.render_type,
			other.alpha_mode,
			other.mesh_id,
			other.material_ids,
			other.object_id,
			other.renderable_index);
}

RenderableDrawClass classify_renderable(const RenderableDefinition& renderable)
{
	if (renderable.render_on_top)
		return renderable.alpha_mode == EAlphaMode::BLEND
			? RenderableDrawClass::OVERLAY_BLENDED
			: RenderableDrawClass::OVERLAY_OPAQUE;
	return renderable.alpha_mode == EAlphaMode::BLEND
		? RenderableDrawClass::BLENDED
		: RenderableDrawClass::OPAQUE;
}

bool renderable_casts_shadow(const RenderableDefinition& renderable)
{
	return renderable.casts_shadow && renderable.alpha_mode != EAlphaMode::BLEND;
}

RenderSortKey make_render_sort_key(
	const ObjectID object_id,
	const uint32_t renderable_index,
	const RenderableDefinition& renderable)
{
	return {
		.render_type = renderable.pipeline_render_type,
		.alpha_mode = renderable.alpha_mode,
		.mesh_id = renderable.get_mesh_id(),
		.material_ids = renderable.get_material_ids(),
		.object_id = object_id,
		.renderable_index = renderable_index,
	};
}

float renderable_distance_squared(
	const glm::vec3& camera_position,
	const glm::mat4& object_transform,
	const RenderableDefinition& renderable)
{
	const glm::vec3 delta =
		glm::vec3(renderable.get_model_transform(object_transform)[3]) - camera_position;
	return glm::dot(delta, delta);
}

void GraphicsDrawLists::rebuild(
	const std::unordered_map<ObjectID, std::unique_ptr<GraphicsEngineObject>>& objects)
{
	size_t renderable_count = 0;
	for (const auto& [_, object] : objects)
		renderable_count += object->get_renderables().size();

	items.clear();
	opaque_items.clear();
	blended_items.clear();
	overlay_opaque_items.clear();
	overlay_blended_items.clear();
	shadow_items.clear();
	items.reserve(renderable_count);

	for (const auto& [id, object] : objects)
	{
		for (uint32_t index = 0; index < object->get_renderables().size(); ++index)
		{
			const auto& renderable = object->get_renderables()[index];
			items.push_back({
				.object = object.get(),
				.renderable = &renderable,
				.renderable_index = index,
				.sort_key = make_render_sort_key(id, index, renderable),
			});
		}
	}

	for (const auto& item : items)
	{
		switch (classify_renderable(*item.renderable))
		{
		case RenderableDrawClass::OPAQUE:
			opaque_items.push_back(&item);
			break;
		case RenderableDrawClass::BLENDED:
			blended_items.push_back(&item);
			break;
		case RenderableDrawClass::OVERLAY_OPAQUE:
			overlay_opaque_items.push_back(&item);
			break;
		case RenderableDrawClass::OVERLAY_BLENDED:
			overlay_blended_items.push_back(&item);
			break;
		}
		if (renderable_casts_shadow(*item.renderable))
			shadow_items.push_back(&item);
	}

	const auto state_order = [](const GraphicsDrawItem* lhs, const GraphicsDrawItem* rhs) {
		return lhs->sort_key < rhs->sort_key;
	};
	std::ranges::sort(opaque_items, state_order);
	std::ranges::sort(overlay_opaque_items, state_order);
	std::ranges::sort(shadow_items, state_order);
}
