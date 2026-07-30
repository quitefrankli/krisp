#pragma once

#include "identifications.hpp"
#include "render_frame.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>


class GraphicsEngineObject;

enum class RenderableDrawClass
{
	OPAQUE,
	BLENDED,
	OVERLAY_OPAQUE,
	OVERLAY_BLENDED,
};

struct RenderSortKey
{
	ERenderType render_type;
	EAlphaMode alpha_mode;
	MeshID mesh_id;
	std::vector<MaterialID> material_ids;
	ObjectID object_id;
	uint32_t renderable_index;

	bool operator<(const RenderSortKey& other) const;
};

RenderableDrawClass classify_renderable(const RenderableDefinition& renderable);
bool renderable_casts_shadow(const RenderableDefinition& renderable);
RenderSortKey make_render_sort_key(
	ObjectID object_id,
	uint32_t renderable_index,
	const RenderableDefinition& renderable);
float renderable_distance_squared(
	const glm::vec3& camera_position,
	const glm::mat4& object_transform,
	const RenderableDefinition& renderable);

struct GraphicsDrawItem
{
	const GraphicsEngineObject* object;
	const RenderableDefinition* renderable;
	uint32_t renderable_index;
	RenderSortKey sort_key;
};

class GraphicsDrawLists
{
public:
	void rebuild(
		const std::unordered_map<ObjectID, std::unique_ptr<GraphicsEngineObject>>& objects);

	const std::vector<GraphicsDrawItem>& all() const { return items; }
	const std::vector<const GraphicsDrawItem*>& opaque() const { return opaque_items; }
	const std::vector<const GraphicsDrawItem*>& blended() const { return blended_items; }
	const std::vector<const GraphicsDrawItem*>& overlay_opaque() const
	{
		return overlay_opaque_items;
	}
	const std::vector<const GraphicsDrawItem*>& overlay_blended() const
	{
		return overlay_blended_items;
	}
	const std::vector<const GraphicsDrawItem*>& shadow() const { return shadow_items; }

private:
	std::vector<GraphicsDrawItem> items;
	std::vector<const GraphicsDrawItem*> opaque_items;
	std::vector<const GraphicsDrawItem*> blended_items;
	std::vector<const GraphicsDrawItem*> overlay_opaque_items;
	std::vector<const GraphicsDrawItem*> overlay_blended_items;
	std::vector<const GraphicsDrawItem*> shadow_items;
};
