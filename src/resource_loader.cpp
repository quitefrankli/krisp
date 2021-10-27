#include "resource_loader.hpp"

#include "objects.hpp"

#include <tiny_obj_loader.h>


void ResourceLoader::load_mesh(Object& object, const std::string& filename)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	std::string path = "../resources/models/viking_room.obj";
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path.c_str()))
	{
		throw std::runtime_error(err);
	}

	for (auto& shape : shapes)
	{
		Shape new_shape;
		for (auto& index : shape.mesh.indices)
		{
			Vertex new_vertex;
			new_vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};
			new_vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};
			new_shape.vertices.push_back(std::move(new_vertex));
		}
		object.shapes.push_back(std::move(new_shape));
	}
}