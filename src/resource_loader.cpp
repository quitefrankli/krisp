#include "resource_loader.hpp"

#include "maths.hpp"
#include "objects/object.hpp"
#include "analytics.hpp"

#include <stb_image.h>
#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <fmt/core.h>

#include <iostream>
#include <unordered_map>
#include <filesystem>


ResourceLoader ResourceLoader::global_resource_loader;
ResourceLoader::TextureID ResourceLoader::global_texture_id_counter = 0;

struct RawTextureData
{
	virtual ~RawTextureData()
	{
	}

	virtual std::byte* get() = 0;
};

struct RawTextureDataSTB : public RawTextureData
{
	RawTextureDataSTB(stbi_uc* data)
	{
		this->data = reinterpret_cast<std::byte*>(data);
	}

	~RawTextureDataSTB() 
	{
		stbi_image_free(data); 
	}

	virtual std::byte* get() override 
	{ 
		return data; 
	}

private:
	std::byte* data;
};

struct RawTextureDataGLFT : public RawTextureData
{
	RawTextureDataGLFT(std::vector<unsigned char>&& data) :
		data(std::move(data))
	{
	}

	virtual std::byte* get() override 
	{ 
		return reinterpret_cast<std::byte*>(data.data()); 
	}

private:
	std::vector<unsigned char> data;
};

ResourceLoader::TextureData::~TextureData() 
{
}

ResourceLoader::~ResourceLoader() 
{
}

MaterialTexture ResourceLoader::fetch_texture(const std::string_view file)
{
	if (texture_name_to_id.find(file.data()) == texture_name_to_id.end())
	{
		load_texture(file);
	}

	const auto texture_id = texture_name_to_id[file.data()];
	TextureData& texture_data = cached_textures[texture_id];

	return create_material_texture(texture_data);
}

std::vector<std::unique_ptr<Shape>> ResourceLoader::load_model(const std::string_view file)
{
	const std::string file_str(file);
	tinygltf::Model model;
	std::string err;
	std::string warn;
	tinygltf::TinyGLTF loader;
	if (!loader.LoadASCIIFromFile(&model, &err, &warn, file_str))
	{
		throw std::runtime_error(fmt::format(
			"ResourceLoader::load_model: failed to load model: {}, err {}, warn {}", 
			file,
			err,
			warn));
	}

	using vertex_t = SDS::TexVertex;
	using shape_t = TexShape;

	std::vector<std::unique_ptr<Shape>> shapes;
	for (auto& mesh : model.meshes)
	{
		if (mesh.primitives.size() != 1)
		{
			throw std::runtime_error("ResourceLoader::load_model: only one primitive per mesh is supported");
		}

		auto& primitive = mesh.primitives[0];
		if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
		{
			throw std::runtime_error("ResourceLoader::load_model: only triangles are supported");
		}

		if (primitive.indices < 0)
		{
			throw std::runtime_error("ResourceLoader::load_model: no indices found");
		}

		if (primitive.attributes.find("POSITION") == primitive.attributes.end() ||
			primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end() ||
			primitive.attributes.find("NORMAL") == primitive.attributes.end())
		{
			throw std::runtime_error("ResourceLoader::load_model: missing attributes");
		}

		const auto& pos_accessor = model.accessors[primitive.attributes["POSITION"]];
		const auto& tex_accessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
		const auto& norm_accessor = model.accessors[primitive.attributes["NORMAL"]];
		const auto& index_accessor = model.accessors[primitive.indices];

		if (pos_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || pos_accessor.type != TINYGLTF_TYPE_VEC3)
		{
			throw std::runtime_error("ResourceLoader::load_model: only float vec3 positions are supported");
		}

		if (tex_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || tex_accessor.type != TINYGLTF_TYPE_VEC2)
		{
			throw std::runtime_error("ResourceLoader::load_model: only float vec2 texcoords are supported");
		}

		if (norm_accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || norm_accessor.type != TINYGLTF_TYPE_VEC3)
		{
			throw std::runtime_error("ResourceLoader::load_model: only float vec3 normals are supported");
		}

		if (index_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT && index_accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
		{
			throw std::runtime_error("ResourceLoader::load_model: only unsigned int and unsigned short indices are supported");
		}

		const auto& pos_buffer_view = model.bufferViews[pos_accessor.bufferView];
		const auto& tex_buffer_view = model.bufferViews[tex_accessor.bufferView];
		const auto& norm_buffer_view = model.bufferViews[norm_accessor.bufferView];
		const auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];

		const auto& pos_buffer = model.buffers[pos_buffer_view.buffer];
		const auto& tex_buffer = model.buffers[tex_buffer_view.buffer];
		const auto& norm_buffer = model.buffers[norm_buffer_view.buffer];
		const auto& index_buffer = model.buffers[index_buffer_view.buffer];

		const auto* pos_data = reinterpret_cast<const float*>(&pos_buffer.data[pos_accessor.byteOffset + pos_buffer_view.byteOffset]);
		const auto* tex_data = reinterpret_cast<const float*>(&tex_buffer.data[tex_accessor.byteOffset + tex_buffer_view.byteOffset]);
		const auto* norm_data = reinterpret_cast<const float*>(&norm_buffer.data[norm_accessor.byteOffset + norm_buffer_view.byteOffset]);

		// convert data to our Shape format
		std::vector<vertex_t> vertices;
		vertices.reserve(pos_accessor.count);

		for (size_t i = 0; i < pos_accessor.count; ++i)
		{
			vertex_t vertex;
			vertex.pos = glm::vec3(pos_data[3 * i], pos_data[3 * i + 1], pos_data[3 * i + 2]);
			vertex.texCoord = glm::vec2(tex_data[2 * i], tex_data[2 * i + 1]);
			vertex.normal = glm::vec3(norm_data[3 * i], norm_data[3 * i + 1], norm_data[3 * i + 2]);
			vertices.push_back(vertex);
		}

		std::vector<uint32_t> indices(index_accessor.count);
		if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
		{
			const auto* index_data = 
				reinterpret_cast<const uint16_t*>(&index_buffer.data[index_accessor.byteOffset + index_buffer_view.byteOffset]);
			for (size_t i = 0; i < index_accessor.count; ++i)
			{
				indices[i] = index_data[i];
			}
		} else
		{
			const auto* index_data = 
				reinterpret_cast<const uint32_t*>(&index_buffer.data[index_accessor.byteOffset + index_buffer_view.byteOffset]);
			std::memcpy(indices.data(), index_data, indices.size() * sizeof(indices[0]));
		}

		shape_t new_shape;
		new_shape.set_vertices(std::move(vertices));
		new_shape.set_indices(std::move(indices));

		// apply materials
		const auto& mat = model.materials[primitive.material];
		Material new_material;

		const auto& color_texture = mat.pbrMetallicRoughness.baseColorTexture;
		// has color texture
		if (color_texture.index >= 0)
		{
			tinygltf::Image& image = model.images[color_texture.index];
			TextureData new_texture_data;
			new_texture_data.width = image.width;
			new_texture_data.height = image.height;
			new_texture_data.channels = image.component;
			new_texture_data.data = std::make_unique<RawTextureDataGLFT>(std::move(image.image));
			new_texture_data.texture_id = get_next_texture_id();

			auto pair = cached_textures.emplace(new_texture_data.texture_id, std::move(new_texture_data));
			new_material.texture = create_material_texture(pair.first->second);
		}

		new_material.material_data.diffuse = glm::vec3(
			mat.pbrMetallicRoughness.baseColorFactor[0],
			mat.pbrMetallicRoughness.baseColorFactor[1],
			mat.pbrMetallicRoughness.baseColorFactor[2]);
		new_material.material_data.ambient = new_material.material_data.diffuse;
		new_material.material_data.specular = (new_material.material_data.specular + new_material.material_data.diffuse)/2.0f;
		new_material.material_data.shininess = 1 - mat.pbrMetallicRoughness.roughnessFactor;

		new_shape.set_material(std::move(new_material));

		shapes.push_back(std::make_unique<shape_t>(std::move(new_shape)));
	}

	// new_obj.set_render_type(EPipelineType::STANDARD); // use textured render
	return shapes;
}

void ResourceLoader::assign_object_texture(Object& object, const std::string_view texture)
{
	for (int i = 0; i < object.get_shapes().size(); i++)
	{
		Material mat = object.get_shapes()[i]->get_material();
		mat.texture = fetch_texture(texture);
		object.get_shapes()[i]->set_material(mat);
	}
	object.set_render_type(EPipelineType::STANDARD); // use textured render
}

void ResourceLoader::assign_object_texture(Object& object, const std::vector<std::string_view> textures)
{
	if (object.get_shapes().size() != textures.size())
	{
		throw std::runtime_error("ResourceLoader::assign_object_texture: num textures and num object shapes mismatch!");
	}
	for (int i = 0; i < object.get_shapes().size(); i++)
	{
		Material mat = object.get_shapes()[i]->get_material();
		mat.texture = fetch_texture(textures[i]);
		object.get_shapes()[i]->set_material(mat);
	}
	object.set_render_type(EPipelineType::STANDARD); // use textured render
}

void ResourceLoader::load_texture(const std::string_view file) 
{
	TextureData new_texture;
	new_texture.data = std::make_unique<RawTextureDataSTB>(stbi_load(
		file.data(), 
		&new_texture.width, 
		&new_texture.height, 
		&new_texture.channels, 
		STBI_rgb_alpha));

	if (!new_texture.data.get())
	{
		throw std::runtime_error(fmt::format("failed to load texture image! {}", file));
	}

	const auto new_texture_id = get_next_texture_id();
	new_texture.texture_id = new_texture_id;
	texture_name_to_id.emplace(file.data(), new_texture_id);
	cached_textures.emplace(new_texture_id, std::move(new_texture));
}

MaterialTexture ResourceLoader::create_material_texture(TextureData& texture_data)
{
	MaterialTexture texture;
	texture.data = texture_data.data->get();
	texture.width = texture_data.width;
	texture.height = texture_data.height;
	texture.channels = texture_data.channels;
	texture.texture_id = texture_data.texture_id;

	return texture;
}
