#include "render_draw_list.hpp"

#include "graphics_renderable.hpp"

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
		renderable_id)
		< std::tie(
			other.render_type,
		other.alpha_mode,
		other.mesh_id,
		other.material_ids,
		other.renderable_id);
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
	const RenderableID renderable_id,
	const RenderableDefinition& renderable)
{
	return {
		.render_type = renderable.pipeline_render_type,
		.alpha_mode = renderable.alpha_mode,
		.mesh_id = renderable.get_mesh_id(),
		.material_ids = renderable.get_material_ids(),
		.renderable_id = renderable_id,
	};
}

float renderable_distance_squared(
	const glm::vec3& camera_position,
	const glm::mat4& model_transform)
{
	const glm::vec3 delta = glm::vec3(model_transform[3]) - camera_position;
	return glm::dot(delta, delta);
}

void GraphicsDrawLists::rebuild(
	const std::unordered_map<RenderableID, std::unique_ptr<GraphicsRenderable>>& renderables)
{
	items.clear();
	opaque_items.clear();
	blended_items.clear();
	overlay_opaque_items.clear();
	overlay_blended_items.clear();
	shadow_items.clear();
	items.reserve(renderables.size());

	for (const auto& [id, graphics_renderable] : renderables)
	{
		const auto& definition = graphics_renderable->get_definition();
		items.push_back({
			.graphics_renderable = graphics_renderable.get(),
			.renderable = &definition,
			.sort_key = make_render_sort_key(id, definition),
		});
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
