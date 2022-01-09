#include "resource_loader.hpp"

#include "maths.hpp"
#include "objects.hpp"

#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>


void ResourceLoader::load_mesh(Object& object, const std::string& filename, const glm::mat4& transform = glm::mat4(1.0f))
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str()))
	{
		throw std::runtime_error(err);
	}

	object.shapes.reserve(shapes.size());
	for (auto& shape : shapes)
	{
		Shape new_shape;
		new_shape.indices.reserve(shape.mesh.indices.size());
		new_shape.vertices.reserve(attrib.vertices.size()/3); // attrib uses float to store x/y/z
		
		for (auto& index : shape.mesh.indices)
		{
			Vertex new_vertex;
			new_shape.indices.push_back(new_shape.indices.size());
			memcpy(&new_vertex.pos, &attrib.vertices[3 * index.vertex_index], sizeof(new_vertex.pos));
			new_vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};
			new_shape.vertices.push_back(std::move(new_vertex));
		}
		
		if (transform != glm::mat4(1.0f))
		{
			new_shape.transform_vertices(transform);
		}

		new_shape.generate_normals();

		object.shapes.push_back(std::move(new_shape));
	}

	std::cout << "ResourceLoader::load_mesh: load complete"
		<< "\n\ttotal indices: " << object.get_num_vertex_indices()
		<< "\n\ttotal vertices: " << object.get_num_unique_vertices() << '\n';
}