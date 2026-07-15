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

MatVec ResourceLoader::load_material(const tinygltf::Primitive& primitive, const tinygltf::Model& model)
{
	if (primitive.material >= 0)
	{
		if (gltf_material_to_mat_ids.contains(primitive.material))
		{
			const auto& material_ids = gltf_material_to_mat_ids.at(primitive.material);
			for (const auto material_id : material_ids)
				MaterialSystem::register_owner(material_id);
			return material_ids;
		}

		const auto& mat = model.materials[primitive.material];
		const auto& color_texture = mat.pbrMetallicRoughness.baseColorTexture;
		const auto& normal_texture = mat.normalTexture;
		if (normal_texture.index >= 0 && color_texture.index < 0)
			throw std::runtime_error(fmt::format(
				"ResourceLoader: material {} has a normal texture but no base-color texture",
				primitive.material));
		if (normal_texture.index >= 0 && normal_texture.texCoord != 0)
			throw std::runtime_error(fmt::format(
				"ResourceLoader: material {} normal texture uses unsupported TEXCOORD_{}",
				primitive.material, normal_texture.texCoord));

		if (color_texture.index >= 0)
		{
			const auto load_gltf_texture = [&](const int texture_index, const ETextureSemantic semantic)
			{
				if (texture_index < 0 || texture_index >= static_cast<int>(model.textures.size()))
					throw std::runtime_error(fmt::format(
						"ResourceLoader: material {} references an invalid texture", primitive.material));
				const auto& texture = model.textures[texture_index];
				if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
					throw std::runtime_error(fmt::format(
						"ResourceLoader: texture {} references an invalid image", texture_index));
				const tinygltf::Image& image = model.images[texture.source];
				if (image.width <= 0 || image.height <= 0 || image.component < 1 || image.component > 4)
					throw std::runtime_error(fmt::format(
						"ResourceLoader: texture {} has unsupported image dimensions or channels", texture_index));
				const size_t pixel_count = static_cast<size_t>(image.width) * image.height;
				if (image.image.size() < pixel_count * static_cast<size_t>(image.component))
					throw std::runtime_error(fmt::format(
						"ResourceLoader: texture {} image data is truncated", texture_index));
				std::vector<unsigned char> rgba(pixel_count * 4);
				for (size_t pixel = 0; pixel < pixel_count; ++pixel)
				{
					const auto* source = image.image.data() + pixel * image.component;
					auto* destination = rgba.data() + pixel * 4;
					if (image.component == 1 || image.component == 2)
						destination[0] = destination[1] = destination[2] = source[0];
					else
					{
						destination[0] = source[0];
						destination[1] = source[1];
						destination[2] = source[2];
					}
					destination[3] = image.component == 2 ? source[1]
						: image.component == 4 ? source[3] : 255;
				}
				TextureMaterial texture_material;
				texture_material.width = image.width;
				texture_material.height = image.height;
				texture_material.channels = 4;
				texture_material.data_len = rgba.size();
				texture_material.semantic = semantic;
				texture_material.data = std::make_unique<RawTextureDataGLTF>(std::move(rgba));
				return MaterialSystem::add(std::make_unique<TextureMaterial>(std::move(texture_material)));
			};

			MatVec material_ids{ load_gltf_texture(color_texture.index, ETextureSemantic::BASE_COLOR) };
			if (normal_texture.index >= 0)
				material_ids.push_back(load_gltf_texture(normal_texture.index, ETextureSemantic::NORMAL));
			gltf_material_to_mat_ids[primitive.material] = material_ids;
			return material_ids;
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
			MatVec material_ids{ mat_id };
			gltf_material_to_mat_ids[primitive.material] = material_ids;
			return material_ids;
		}
	}

	return { MaterialFactory::fetch_preset(EMaterialPreset::PLASTIC) };
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
