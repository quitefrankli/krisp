#include "resource_loader.hpp"

#include "maths.hpp"
#include "objects/object.hpp"
#include "analytics.hpp"

#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>

#include <iostream>
#include <unordered_map>


static void load_object_impl(Object& object, 
		const std::string_view mesh, 
		const std::function<void(Vertex& new_vertex, tinyobj::attrib_t& attrib, tinyobj::index_t& index)>& vertex_loader,
		const glm::mat4& transform);

Object ResourceLoader::load_object(const std::string_view mesh, 
								   const std::vector<std::string_view>& textures,
								   const glm::mat4& transform)
{
	Object object;
	const auto vertex_loader = [](Vertex& new_vertex, tinyobj::attrib_t& attrib, tinyobj::index_t& index) {
		memcpy(&new_vertex.pos, &attrib.vertices[3 * index.vertex_index], sizeof(new_vertex.pos));
		new_vertex.texCoord = {
			attrib.texcoords[2 * index.texcoord_index + 0],
			1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
		};
	};

	load_object_impl(object, mesh, vertex_loader, transform);

	if (textures.size() != object.shapes.size())
	{
		throw std::runtime_error("ResourceLoader: number of textures provided != number of shapes loaded");
	}
	auto textures_it = textures.begin();
	for (auto& shape : object.shapes)
	{
		shape.texture = *textures_it++;
	}

	fmt::print("ResourceLoader::load_object: loaded {}, shapes:={}, vertices:={}, indices:={}, texures:={}\n",
		mesh, object.shapes.size(), object.get_num_unique_vertices(), object.get_num_vertex_indices(), textures.size());

	object.set_render_type(ERenderType::STANDARD); // use textured render

	return object;
}

Object ResourceLoader::load_object(const std::string_view mesh,
								   const glm::vec3& color,
					 			   const glm::mat4& transform)
{
	Object object;
	const auto vertex_loader = [&color](Vertex& new_vertex, tinyobj::attrib_t& attrib, tinyobj::index_t& index) {
		memcpy(&new_vertex.pos, &attrib.vertices[3 * index.vertex_index], sizeof(new_vertex.pos));
		new_vertex.color = color;
	};

	load_object_impl(object, mesh, vertex_loader, transform);

	fmt::print("ResourceLoader::load_object: loaded {}, shapes:={}, vertices:={}, indices:={}\n",
		mesh, object.shapes.size(), object.get_num_unique_vertices(), object.get_num_vertex_indices());

	object.set_render_type(ERenderType::COLOR); // use colored render

	return object;
}

static void load_object_impl(Object& object, 
	const std::string_view mesh, 
	const std::function<void(Vertex& new_vertex, tinyobj::attrib_t& attrib, tinyobj::index_t& index)>& vertex_loader,
	const glm::mat4& transform)
{
	Analytics analytics;
	analytics.quick_timer_start();

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, mesh.data()))
	{
		throw std::runtime_error(err);
	}

	if (shapes.empty() || shapes[0].mesh.indices.empty())
	{
		const auto error_msg = std::string("ResourceLoader::load_object: empty shapes or indices for - ") + mesh.data();
		throw std::runtime_error(error_msg);
	}

	object.shapes.reserve(shapes.size());
	for (auto& shape : shapes)
	{
		std::unordered_map<Vertex, uint32_t> unique_vertices;
		
		Shape new_shape;
		new_shape.indices.reserve(shape.mesh.indices.size());
		new_shape.vertices.reserve(attrib.vertices.size()/3); // attrib uses float to store x/y/z
		
		for (tinyobj::index_t& index : shape.mesh.indices)
		{
			std::pair<Vertex, uint32_t> new_pair{};
			new_pair.second = new_shape.vertices.size();

			Vertex& new_vertex = new_pair.first;
			vertex_loader(new_vertex, attrib, index);

			auto& unique_vertex_element = unique_vertices.insert(std::move(new_pair));
			if (unique_vertex_element.second)
			{
				new_shape.indices.emplace_back(new_shape.vertices.size());
				new_shape.vertices.push_back(std::move(new_vertex));
			} else // if already occupied, set index to the index in which said vertex occupies
			{
				new_shape.indices.emplace_back(unique_vertex_element.first->second);
			}
		}
		
		if (transform != glm::mat4(1.0f))
		{
			new_shape.transform_vertices(transform);
		}

		new_shape.generate_normals();
		new_shape.name = shape.name;

		object.shapes.push_back(std::move(new_shape));
	}

	analytics.quick_timer_stop("ResourceLoader::load_mesh: load time");
}

std::vector<Object> ResourceLoader::load_objects(const std::string_view mesh,
                                                 const std::vector<std::string_view>& textures,
                                                 const ResourceLoader::Setting setting)
{
	Object object;
	const auto vertex_loader = [](Vertex& new_vertex, tinyobj::attrib_t& attrib, tinyobj::index_t& index) {
		memcpy(&new_vertex.pos, &attrib.vertices[3 * index.vertex_index], sizeof(new_vertex.pos));
		new_vertex.texCoord = { attrib.texcoords[2 * index.texcoord_index + 0],
			                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1] };
	};

	load_object_impl(object, mesh, vertex_loader, glm::mat4(1.0f));
	if (textures.size() != object.shapes.size())
	{
		throw std::runtime_error("ResourceLoader: number of textures provided != number of shapes loaded");
	}
	const int total_unique_vertices = object.get_num_unique_vertices();
	const int total_vertex_indices = object.get_num_vertex_indices();

	// now move each of the shapes into a separate object
	std::vector<Object> objects;
	objects.reserve(object.shapes.size());
	auto textures_it = textures.begin();
	for (auto& shape : object.shapes)
	{
		shape.texture = *textures_it++;
		switch (setting)
		{
			case ResourceLoader::Setting::ZERO_MESH: {
				shape.aabb = AABB(shape);
				const glm::vec3 center = (shape.aabb.max_bound + shape.aabb.min_bound) / 2.0f;
				shape.translate_vertices(-center);
				shape.aabb -= center;
				break;
			}
			case ResourceLoader::Setting::ZERO_XZ: {
				shape.aabb = AABB(shape);
				glm::vec3 center = (shape.aabb.max_bound + shape.aabb.min_bound) / 2.0f;
				center.y = shape.aabb.min_bound.y;
				shape.translate_vertices(-center);
				shape.aabb -= center;
				break;
			}
			default:
				break;
		}
		auto& new_obj = objects.emplace_back();
		new_obj.set_name(shape.name);
		new_obj.set_aabb(shape.aabb);
		new_obj.shapes.push_back(std::move(shape));
		new_obj.set_render_type(ERenderType::STANDARD); // use textured render
	}

	fmt::print(
		"ResourceLoader::load_object: loaded {}, objects:={}, total vertices:={}, total indices:={}, texures:={}\n",
		mesh,
		objects.size(),
		total_unique_vertices,
		total_vertex_indices,
		textures.size());

	return objects;
}