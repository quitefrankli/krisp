#include "cubemap.hpp"
#include "objects.hpp"
#include "maths.hpp"
#include "utility.hpp"
#include "shapes/shape_factory.hpp"
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

	Material material;
	const auto texture_path = Utility::get().get_textures_path().string();
	ResourceLoader& resource_loader = ResourceLoader::get();
	for (int i = 0; i < texture_order.size(); i++)
	{
		material.texture = resource_loader.fetch_texture(fmt::format("{}/skybox/{}.bmp", texture_path, texture_order[i]));
		// current system only supports 1 material per shape, so in order to pass multiple textures
		// as in the case of cubemaps, we need to have 5 other dummy shapes
		shapes.emplace_back(
			i == 0 ? ShapeFactory::cube(ShapeFactory::EVertexType::TEXTURE) : 
			std::make_unique<TexShape>())->set_material(material);
	}
}