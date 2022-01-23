#include "resource_loader.hpp"

#include "maths.hpp"
#include "objects/object.hpp"
#include "analytics.hpp"

#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <unordered_map>


void ResourceLoader::load_mesh(Object& object, const std::string& filename, const glm::mat4& transform = glm::mat4(1.0f))
{
	Analytics analytics;
	analytics.quick_timer_start();

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
		std::unordered_map<Vertex, uint32_t> unique_vertices;
		
		Shape new_shape;
		new_shape.indices.reserve(shape.mesh.indices.size());
		new_shape.vertices.reserve(attrib.vertices.size()/3); // attrib uses float to store x/y/z
		
		for (auto& index : shape.mesh.indices)
		{
			std::pair<Vertex, uint32_t> new_pair{};
			new_pair.second = new_shape.vertices.size();

			Vertex& new_vertex = new_pair.first;
			memcpy(&new_vertex.pos, &attrib.vertices[3 * index.vertex_index], sizeof(new_vertex.pos));
			new_vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			auto& unique_vertex_element = unique_vertices.insert(std::move(new_pair));
			if (unique_vertex_element.second)
			{
				new_shape.indices.push_back(new_shape.vertices.size());
				new_shape.vertices.push_back(std::move(new_vertex));
			} else // if already occupied, set index to the index in which said vertex occupies
			{
				new_shape.indices.push_back(unique_vertex_element.first->second);
			}
		}
		
		if (transform != glm::mat4(1.0f))
		{
			new_shape.transform_vertices(transform);
		}

		new_shape.generate_normals();

		object.shapes.push_back(std::move(new_shape));
	}

	analytics.quick_timer_stop("ResourceLoader::load_mesh: load time");

	std::cout << "ResourceLoader::load_mesh: load complete!"
		<< "\n\ttotal indices: " << object.get_num_vertex_indices()
		<< "\n\ttotal vertices: " << object.get_num_unique_vertices() << '\n';
}