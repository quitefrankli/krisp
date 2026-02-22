#include "resource_loader.hpp"
#include "renderable/material.hpp"
#include "entity_component_system/material_system.hpp"
#include "renderable/material_factory.hpp"

#include <stb_image.h>
#include <tiny_gltf.h>
#include <fmt/core.h>

#include <filesystem>


struct RawTextureDataSTB : public TextureData
{
	RawTextureDataSTB(stbi_uc* data) : data(reinterpret_cast<std::byte*>(data)) {}

	virtual ~RawTextureDataSTB() override
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

struct RawTextureDataGLTF : public TextureData
{
	RawTextureDataGLTF(std::vector<unsigned char> data) :
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

void ResourceLoader::load_all_materials(const tinygltf::Model& model)
{
	gltf_material_to_mat_id.clear();
	for (int i = 0; i < static_cast<int>(model.materials.size()); ++i)
	{
		tinygltf::Primitive primitive;
		primitive.material = i;
		gltf_material_to_mat_id[i] = load_material(primitive, model);
	}
}

MaterialID ResourceLoader::load_material(const tinygltf::Primitive& primitive, const tinygltf::Model& model)
{
	if (primitive.material >= 0)
	{
		if (gltf_material_to_mat_id.contains(primitive.material))
		{
			return gltf_material_to_mat_id.at(primitive.material);
		}

		const auto& mat = model.materials[primitive.material];
		const auto& color_texture = mat.pbrMetallicRoughness.baseColorTexture;
		if (color_texture.index >= 0)
		{
			const tinygltf::Image& image = model.images[color_texture.index];
			TextureMaterial new_material;
			new_material.width = image.width;
			new_material.height = image.height;
			new_material.channels = image.component;
			new_material.data = std::make_unique<RawTextureDataGLTF>(image.image);

			const auto mat_id = MaterialSystem::add(std::make_unique<TextureMaterial>(std::move(new_material)));
			gltf_material_to_mat_id[primitive.material] = mat_id;
			return mat_id;
		} else
		{
			ColorMaterial new_material;
			new_material.data.diffuse = glm::vec3(
				mat.pbrMetallicRoughness.baseColorFactor[0],
				mat.pbrMetallicRoughness.baseColorFactor[1],
				mat.pbrMetallicRoughness.baseColorFactor[2]);
			new_material.data.ambient = new_material.data.diffuse;
			new_material.data.specular = (new_material.data.specular + new_material.data.diffuse)/2.0f;
			new_material.data.shininess = 1 - mat.pbrMetallicRoughness.roughnessFactor;

			const auto mat_id = MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(new_material)));
			gltf_material_to_mat_id[primitive.material] = mat_id;
			return mat_id;
		}
	}

	return MaterialFactory::fetch_preset(EMaterialPreset::PLASTIC);
}

MaterialID ResourceLoader::load_texture(const std::filesystem::path& filename)
{
	if (!std::filesystem::exists(filename))
	{
		throw std::runtime_error(fmt::format("ResourceLoader::load_texture: filename does not exist! {}", filename.string()));
	}

	TextureMaterial material;
	const auto filename_str = filename.string();
	material.data = std::make_unique<RawTextureDataSTB>(stbi_load(
		filename_str.c_str(),
		(int*)(&material.width),
		(int*)(&material.height),
		(int*)(&material.channels),
		STBI_rgb_alpha));

	assert(material.channels == 4 || material.channels == 3);
	material.channels = 4;

	if (!material.data->get())
	{
		throw std::runtime_error(fmt::format("failed to load texture image! {}", filename_str));
	}

	const auto mat_id = MaterialSystem::add(std::make_unique<TextureMaterial>(std::move(material)));
	texture_name_to_mat_id[filename_str] = mat_id;
	return mat_id;
}
