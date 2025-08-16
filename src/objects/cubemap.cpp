#include "cubemap.hpp"
#include "objects.hpp"
#include "maths.hpp"
#include "utility.hpp"
#include "renderable/mesh_factory.hpp"
#include "entity_component_system/material_system.hpp"
#include "resource_loader/resource_loader.hpp"

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

	const auto texture_path = Utility::get_textures_path().string();
	Renderable renderable;
	renderable.pipeline_render_type = ERenderType::CUBEMAP;
	renderable.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::COLOR); // I'm not sure why COLOR seems to work, TEXTURE seems to break
	CubeMapMatGroup material_group;
	for (const auto& texture_name : texture_order)
	{
		material_group.cube_map_mats.push_back(ResourceLoader::fetch_texture(fmt::format("{}/skybox/{}.jpg", 
																			 texture_path, 
																			 texture_name)));
	}
	renderable.material_ids = material_group.get_materials();

	renderables = { renderable };
}