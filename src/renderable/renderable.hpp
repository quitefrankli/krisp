#pragma once

#include "identifications.hpp"
#include "maths.hpp"
#include "renderable/render_types.hpp"
#include "renderable/material_group.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"

#include <string>
#include <vector>

class ECS;

// A renderable encapsulates per-draw resource data. Object grouping and an
// optional skeleton binding live on the ECS RenderableAttachment.
// There are multiple materials because we may need multiple maps i.e. texture, normal, uv maps
struct Renderable
{
	std::string name;
	ERenderType pipeline_render_type = ERenderType::COLOR; // TODO: this default value is not good, it should be unassigned
	EShadingMode shading_mode = EShadingMode::LIT;
	EAlphaMode alpha_mode = EAlphaMode::OPAQUE;
	float alpha_cutoff = 0.5f;
	float opacity = 1.0f;
	bool casts_shadow = true;
	bool render_on_top = false;
	// Asset-local placement, kept separate from the owning object's gameplay transform.
	Maths::Transform local_transform;
	MeshHandle mesh_owner;
	std::vector<MaterialHandle> material_owners;

	MeshID get_mesh_id() const;
	MaterialID get_material_id(size_t index) const;
	MatVec get_material_ids() const;

	glm::mat4 get_model_transform(const glm::mat4& gameplay_transform) const
	{
		return gameplay_transform * local_transform.get_mat4();
	}

	static Renderable make_default(ECS& ecs);
	static Renderable make_default(ECS& ecs, MeshHandle mesh_owner);
};
