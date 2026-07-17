#include "resource_loader.hpp"
#include "renderable/material.hpp"
#include "entity_component_system/material_system.hpp"
#include "renderable/material_factory.hpp"

#include <stb_image.h>
#include <tiny_gltf.h>
#include <fmt/core.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>


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

struct RawTextureDataDDS : public TextureData
{
	explicit RawTextureDataDDS(std::vector<std::byte> data) : data(std::move(data)) {}
	std::byte* get() override { return data.data(); }

private:
	std::vector<std::byte> data;
};

TextureMaterial load_dds_texture_data(
	const unsigned char* bytes,
	const size_t byte_count,
	const std::string_view source)
{
	constexpr uint32_t DDS_MAGIC = 0x20534444;
	constexpr uint32_t FOURCC_DXT5 = 0x35545844;
	constexpr uint32_t DDSCAPS2_CUBEMAP = 0x00000200;
	constexpr uint32_t DDSCAPS2_VOLUME = 0x00200000;

	if (byte_count < 128)
		throw ResourceLoadError(fmt::format("ResourceLoader: DDS header is truncated: {}", source));
	const auto read_u32 = [bytes](const size_t offset)
	{
		return static_cast<uint32_t>(bytes[offset])
			| static_cast<uint32_t>(bytes[offset + 1]) << 8
			| static_cast<uint32_t>(bytes[offset + 2]) << 16
			| static_cast<uint32_t>(bytes[offset + 3]) << 24;
	};
	if (read_u32(0) != DDS_MAGIC || read_u32(4) != 124 || read_u32(76) != 32)
		throw ResourceLoadError(fmt::format("ResourceLoader: invalid DDS header: {}", source));
	if (read_u32(84) != FOURCC_DXT5)
		throw ResourceLoadError("ResourceLoader: only DXT5/BC3 DDS textures are supported");
	if (read_u32(112) & (DDSCAPS2_CUBEMAP | DDSCAPS2_VOLUME))
		throw ResourceLoadError("ResourceLoader: DDS cubemaps and volume textures are not supported");

	TextureMaterial material;
	material.width = read_u32(16);
	material.height = read_u32(12);
	material.channels = 4;
	material.format = ETextureFormat::BC3;
	material.source = std::string(source);
	if (material.width == 0 || material.height == 0)
		throw ResourceLoadError("ResourceLoader: DDS texture has invalid dimensions");

	const uint32_t maximum_mips = 1 + static_cast<uint32_t>(std::floor(
		std::log2(static_cast<double>(std::max(material.width, material.height)))));
	const uint32_t mip_levels = read_u32(28) == 0 ? 1 : read_u32(28);
	if (mip_levels > maximum_mips)
		throw ResourceLoadError("ResourceLoader: DDS texture has an invalid mip count");

	material.mip_sizes.reserve(mip_levels);
	uint32_t mip_width = material.width;
	uint32_t mip_height = material.height;
	for (uint32_t mip = 0; mip < mip_levels; ++mip)
	{
		const size_t blocks_wide = (static_cast<size_t>(mip_width) + 3) / 4;
		const size_t blocks_high = (static_cast<size_t>(mip_height) + 3) / 4;
		if (blocks_wide > std::numeric_limits<size_t>::max() / blocks_high / 16)
			throw ResourceLoadError("ResourceLoader: DDS texture data size overflows address space");
		const size_t size = blocks_wide * blocks_high * 16;
		if (size > byte_count - 128 - material.data_len)
			throw ResourceLoadError(fmt::format("ResourceLoader: DDS mip data is truncated: {}", source));
		material.mip_sizes.push_back(size);
		material.data_len += size;
		mip_width = std::max(1u, mip_width / 2);
		mip_height = std::max(1u, mip_height / 2);
	}

	std::vector<std::byte> payload(material.data_len);
	std::memcpy(payload.data(), bytes + 128, payload.size());
	material.data = std::make_unique<RawTextureDataDDS>(std::move(payload));
	return material;
}

TextureMaterial load_dds_texture(const std::filesystem::path& filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file)
		throw ResourceLoadError(fmt::format("ResourceLoader: failed to open DDS texture: {}", filename.string()));
	const auto file_size = file.tellg();
	if (file_size < 0)
		throw ResourceLoadError(fmt::format("ResourceLoader: failed to read DDS texture: {}", filename.string()));
	std::vector<unsigned char> bytes(static_cast<size_t>(file_size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
		throw ResourceLoadError(fmt::format("ResourceLoader: failed to read DDS texture: {}", filename.string()));
	return load_dds_texture_data(bytes.data(), bytes.size(), filename.string());
}

bool load_gltf_image_data(
	tinygltf::Image* image,
	const int image_index,
	std::string* error,
	std::string* warning,
	const int requested_width,
	const int requested_height,
	const unsigned char* bytes,
	const int byte_count,
	void* user_data)
{
	constexpr std::array<unsigned char, 4> DDS_MAGIC{ 'D', 'D', 'S', ' ' };
	if (byte_count >= static_cast<int>(DDS_MAGIC.size())
		&& std::equal(DDS_MAGIC.begin(), DDS_MAGIC.end(), bytes))
	{
		image->as_is = true;
		image->image.assign(bytes, bytes + byte_count);
		return true;
	}

	return tinygltf::LoadImageData(
		image,
		image_index,
		error,
		warning,
		requested_width,
		requested_height,
		bytes,
		byte_count,
		user_data);
}

ResourceLoader::LoadedMaterial ResourceLoader::load_material(
	const tinygltf::Primitive& primitive,
	const tinygltf::Model& model)
{
	if (primitive.material >= 0)
	{
		if (gltf_material_to_material.contains(primitive.material))
		{
			const auto& material = gltf_material_to_material.at(primitive.material);
			for (const auto material_id : material.ids)
				MaterialSystem::register_owner(material_id);
			return material;
		}

		const auto& mat = model.materials[primitive.material];
		EAlphaMode alpha_mode = EAlphaMode::OPAQUE;
		if (mat.alphaMode == "MASK")
			alpha_mode = EAlphaMode::MASK;
		else if (mat.alphaMode == "BLEND")
			alpha_mode = EAlphaMode::BLEND;
		else if (mat.alphaMode != "OPAQUE")
			throw ResourceLoadError(fmt::format(
				"ResourceLoader: material {} has unsupported alphaMode '{}'", primitive.material, mat.alphaMode));
		const float alpha_cutoff = static_cast<float>(mat.alphaCutoff);
		const float opacity = static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3]);
		const auto& color_texture = mat.pbrMetallicRoughness.baseColorTexture;
		const auto& normal_texture = mat.normalTexture;
		const auto specular_extension_it = mat.extensions.find("KHR_materials_specular");
		if (specular_extension_it != mat.extensions.end() && !specular_extension_it->second.IsObject())
			throw ResourceLoadError("ResourceLoader: KHR_materials_specular must be an object");
		std::optional<int> specular_texture_index;
		if (specular_extension_it != mat.extensions.end())
		{
			const auto& extension = specular_extension_it->second;
			const auto read_specular_texture = [&](const char* name)
			{
				if (!extension.Has(name))
					return;
				const auto& info = extension.Get(name);
				if (!info.IsObject() || !info.Has("index") || !info.Get("index").IsNumber())
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: KHR_materials_specular.{} is invalid", name));
				if (info.Has("texCoord") && !info.Get("texCoord").IsNumber())
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: KHR_materials_specular.{}.texCoord must be numeric", name));
				const int tex_coord = info.Has("texCoord") ? info.Get("texCoord").GetNumberAsInt() : 0;
				if (tex_coord != 0)
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: {} uses unsupported TEXCOORD_{}", name, tex_coord));
				const int index = info.Get("index").GetNumberAsInt();
				if (specular_texture_index && *specular_texture_index != index)
					throw ResourceLoadError(
						"ResourceLoader: only one packed specular texture is supported per material");
				specular_texture_index = index;
			};
			read_specular_texture("specularTexture");
			read_specular_texture("specularColorTexture");
		}
		const bool has_specular_texture = specular_texture_index.has_value();
		if (normal_texture.index >= 0 && color_texture.index < 0)
			throw ResourceLoadError(fmt::format(
				"ResourceLoader: material {} has a normal texture but no base-color texture",
				primitive.material));
		if (normal_texture.index >= 0 && normal_texture.texCoord != 0)
			throw ResourceLoadError(fmt::format(
				"ResourceLoader: material {} normal texture uses unsupported TEXCOORD_{}",
				primitive.material, normal_texture.texCoord));

		if (color_texture.index >= 0 || has_specular_texture)
		{
			const auto load_gltf_texture = [&](const int texture_index, const ETextureSemantic semantic)
			{
				if (texture_index < 0 || texture_index >= static_cast<int>(model.textures.size()))
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: material {} references an invalid texture", primitive.material));
				const auto& texture = model.textures[texture_index];
				if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: texture {} references an invalid image", texture_index));
				const tinygltf::Image& image = model.images[texture.source];
				if (image.as_is && image.image.size() >= 4
					&& std::equal(image.image.begin(), image.image.begin() + 4, "DDS "))
				{
					auto texture_material = load_dds_texture_data(
						image.image.data(),
						image.image.size(),
						image.uri.empty() ? image.name : image.uri);
					texture_material.semantic = semantic;
					return MaterialSystem::add(
						std::make_unique<TextureMaterial>(std::move(texture_material)));
				}
				if (image.width <= 0 || image.height <= 0 || image.component < 1 || image.component > 4)
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: texture {} has unsupported image dimensions or channels", texture_index));
				const size_t pixel_count = static_cast<size_t>(image.width) * image.height;
				if (image.image.size() < pixel_count * static_cast<size_t>(image.component))
					throw ResourceLoadError(fmt::format(
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
				texture_material.source = image.uri.empty() ? image.name : image.uri;
				texture_material.data = std::make_unique<RawTextureDataGLTF>(std::move(rgba));
				return MaterialSystem::add(std::make_unique<TextureMaterial>(std::move(texture_material)));
			};

			LoadedMaterial loaded;
			loaded.ids = { color_texture.index >= 0
				? load_gltf_texture(color_texture.index, ETextureSemantic::BASE_COLOR)
				: MaterialFactory::fetch_white_texture() };
			if (normal_texture.index >= 0)
				loaded.ids.push_back(load_gltf_texture(normal_texture.index, ETextureSemantic::NORMAL));

			if (specular_texture_index)
				loaded.ids.push_back(load_gltf_texture(*specular_texture_index, ETextureSemantic::SPECULAR));
			loaded.alpha_mode = alpha_mode;
			loaded.alpha_cutoff = alpha_cutoff;
			loaded.opacity = opacity;
			gltf_material_to_material[primitive.material] = loaded;
			return loaded;
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
			LoadedMaterial loaded{ .ids = { mat_id }, .alpha_mode = alpha_mode,
				.alpha_cutoff = alpha_cutoff, .opacity = opacity };
			gltf_material_to_material[primitive.material] = loaded;
			return loaded;
		}
	}

	return { .ids = { MaterialFactory::fetch_preset(EMaterialPreset::PLASTIC) } };
}

MaterialID ResourceLoader::load_texture(
	const std::filesystem::path& filename,
	const ETextureSemantic semantic)
{
	if (!std::filesystem::exists(filename))
	{
		throw ResourceLoadError(fmt::format("ResourceLoader::load_texture: filename does not exist! {}", filename.string()));
	}

	if (filename.extension() == ".dds")
	{
		auto material = load_dds_texture(filename);
		material.semantic = semantic;
		const auto mat_id = MaterialSystem::add(
			std::make_unique<TextureMaterial>(std::move(material)), false);
		texture_name_to_mat_id[filename.lexically_normal().string()][static_cast<size_t>(semantic)] = mat_id;
		return mat_id;
	}

	TextureMaterial material;
	material.semantic = semantic;
	material.source = filename.lexically_normal().string();
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
		throw ResourceLoadError(fmt::format("failed to load texture image! {}", filename_str));
	}
	material.data_len = static_cast<size_t>(material.width) * material.height * material.channels;
	material.mip_sizes = { material.data_len };

	const auto mat_id = MaterialSystem::add(
		std::make_unique<TextureMaterial>(std::move(material)), false);
	texture_name_to_mat_id[filename.lexically_normal().string()][static_cast<size_t>(semantic)] = mat_id;
	return mat_id;
}
