#include "cubemap.hpp"
#include "objects.hpp"
#include "maths.hpp"
#include "utility.hpp"
#include "renderable/mesh_factory.hpp"
#include "entity_component_system/material_system.hpp"
#include "resource_loader.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fmt/format.h>


CubeMap::CubeMap()
{
	const std::vector<std::string> texture_order = {
		"right",
		"left",
		"top",
		"bottom",
		"front",
		"back"
	};

	const auto texture_path = Utility::get().get_textures_path().string();
	ResourceLoader& resource_loader = ResourceLoader::get();
	Renderable renderable;
	renderable.pipeline_render_type = EPipelineType::CUBEMAP;
	renderable.mesh = MeshFactory::cube_id(MeshFactory::EVertexType::COLOR);
	for (auto i = 0; i < texture_order.size(); i++)
	{
		Material material;
		material.texture = resource_loader.fetch_texture(fmt::format("{}/skybox/{}.bmp", texture_path, texture_order[i]));
		const auto material_id = MaterialSystem::add(std::make_unique<Material>(std::move(material)));
		renderable.materials.push_back(material_id);
	}

	renderables = { renderable };
}