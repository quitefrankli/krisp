#include "resource_loader.hpp"
#include "renderable/material.hpp"
#include "entity_component_system/material_system.hpp"

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
#include <unordered_set>


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
	constexpr std::array<unsigned char, 8> PNG_MAGIC{
		0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
	if (byte_count >= static_cast<int>(DDS_MAGIC.size())
		&& std::equal(DDS_MAGIC.begin(), DDS_MAGIC.end(), bytes))
	{
		image->as_is = true;
		image->mimeType = "image/vnd-ms.dds";
		image->image.assign(bytes, bytes + byte_count);
		return true;
	}
	if (byte_count >= static_cast<int>(PNG_MAGIC.size())
		&& std::equal(PNG_MAGIC.begin(), PNG_MAGIC.end(), bytes))
		image->mimeType = "image/png";
	else if (byte_count >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff)
		image->mimeType = "image/jpeg";
	else
	{
		image->mimeType = "application/x-krisp-unsupported-image";
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

namespace
{
std::string gltf_material_label(const tinygltf::Material& material, const size_t index)
{
	return material.name.empty()
		? fmt::format("{}", index)
		: fmt::format("{} ('{}')", index, material.name);
}

PbrMaterial::TextureSampler validate_gltf_sampler(
	const tinygltf::Model& model,
	const tinygltf::Texture& texture,
	const int texture_index,
	const std::string& label)
{
	const auto reject = [&label](const std::string_view feature)
	{
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: material {} uses unsupported feature {}", label, feature));
	};
	if (!texture.extensions.empty())
		reject(fmt::format("texture extension '{}'", texture.extensions.begin()->first));
	if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
		reject(fmt::format("texture {} with invalid image", texture_index));
	const auto& image = model.images[texture.source];
	if (!image.extensions.empty())
		reject(fmt::format("image extension '{}'", image.extensions.begin()->first));
	if (image.as_is || (image.mimeType != "image/png" && image.mimeType != "image/jpeg"))
		reject(fmt::format("texture {} image format '{}'", texture_index,
			image.mimeType.empty() ? "unknown" : image.mimeType));

	if (texture.sampler < 0)
		return PbrMaterial::TextureSampler::REPEAT;
	if (texture.sampler >= static_cast<int>(model.samplers.size()))
		reject(fmt::format("texture {} with invalid sampler", texture_index));
	const auto& sampler = model.samplers[texture.sampler];
	if (!sampler.extensions.empty())
		reject(fmt::format("sampler extension '{}'", sampler.extensions.begin()->first));
	if (sampler.magFilter != -1 && sampler.magFilter != TINYGLTF_TEXTURE_FILTER_LINEAR)
		reject(fmt::format("texture {} magnification filter {}", texture_index, sampler.magFilter));
	if (sampler.minFilter != -1 && sampler.minFilter != TINYGLTF_TEXTURE_FILTER_LINEAR)
		reject(fmt::format("texture {} minification filter {}", texture_index, sampler.minFilter));
	if (sampler.wrapS != sampler.wrapT)
		reject(fmt::format("texture {} with different S/T wrap modes", texture_index));
	if (sampler.wrapS == TINYGLTF_TEXTURE_WRAP_REPEAT)
		return PbrMaterial::TextureSampler::REPEAT;
	if (sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE)
		return PbrMaterial::TextureSampler::CLAMP_TO_EDGE;
	reject(fmt::format("texture {} wrap mode {}", texture_index, sampler.wrapS));
	return PbrMaterial::TextureSampler::REPEAT;
}

template<typename TextureInfo>
PbrMaterial::TextureSampler validate_gltf_texture_info(
	const tinygltf::Model& model,
	const TextureInfo& texture_info,
	const std::string_view name,
	const std::string& label)
{
	const auto reject = [&label](const std::string_view feature)
	{
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: material {} uses unsupported feature {}", label, feature));
	};
	if (texture_info.texCoord != 0)
		reject(fmt::format("{} TEXCOORD_{}", name, texture_info.texCoord));
	if (!texture_info.extensions.empty())
		reject(fmt::format("{} extension '{}'", name, texture_info.extensions.begin()->first));
	if (texture_info.index < 0 || texture_info.index >= static_cast<int>(model.textures.size()))
		reject(fmt::format("{} with invalid texture", name));
	return validate_gltf_sampler(model, model.textures[texture_info.index], texture_info.index, label);
}

void validate_gltf_material(
	const tinygltf::Model& model,
	const tinygltf::Material& material,
	const size_t index)
{
	const auto label = gltf_material_label(material, index);
	const auto reject = [&label](const std::string_view feature)
	{
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: material {} uses unsupported feature {}", label, feature));
	};
	const auto& pbr = material.pbrMetallicRoughness;
	if (material.occlusionTexture.index >= 0)
		reject("occlusionTexture");
	if (material.emissiveTexture.index >= 0)
		reject("emissiveTexture");
	if (std::ranges::any_of(material.emissiveFactor, [](const double factor)
		{ return factor != 0.0; }))
		reject("emissiveFactor");
	if (material.alphaMode != "OPAQUE")
		reject(fmt::format("alphaMode '{}'", material.alphaMode));
	if (material.doubleSided)
		reject("doubleSided");
	if (!material.extensions.empty())
		reject(fmt::format("extension '{}'", material.extensions.begin()->first));
	if (!pbr.extensions.empty())
		reject(fmt::format("pbrMetallicRoughness extension '{}'", pbr.extensions.begin()->first));

	const auto valid_factor = [](const double factor)
	{
		return std::isfinite(factor) && factor >= 0.0 && factor <= 1.0;
	};
	if (pbr.baseColorFactor.size() != 4 || !std::ranges::all_of(pbr.baseColorFactor, valid_factor))
		reject("baseColorFactor outside [0, 1]");
	if (!valid_factor(pbr.metallicFactor))
		reject("metallicFactor outside [0, 1]");
	if (!valid_factor(pbr.roughnessFactor))
		reject("roughnessFactor outside [0, 1]");
	if (!std::isfinite(material.normalTexture.scale))
		reject("non-finite normalTexture.scale");

	if (pbr.baseColorTexture.index >= 0)
		validate_gltf_texture_info(model, pbr.baseColorTexture, "baseColorTexture", label);
	if (pbr.metallicRoughnessTexture.index >= 0)
		validate_gltf_texture_info(
			model, pbr.metallicRoughnessTexture, "metallicRoughnessTexture", label);
	if (material.normalTexture.index >= 0)
		validate_gltf_texture_info(model, material.normalTexture, "normalTexture", label);
}

void validate_gltf_materials(
	const tinygltf::Model& model,
	const std::unordered_set<int>& used_materials,
	std::vector<ResourceLoader::ImportWarning>& warnings,
	const bool strict)
{
	for (size_t index = 0; index < model.materials.size(); ++index)
	{
		try
		{
			validate_gltf_material(model, model.materials[index], index);
		}
		catch (const ResourceLoadError& error)
		{
			if (used_materials.contains(static_cast<int>(index)))
				throw;
			if (strict)
				throw ResourceLoadError(fmt::format(
					"{}; unused material warning promoted by strict mode", error.what()));
			warnings.push_back({ fmt::format("{}; unused material was skipped", error.what()) });
		}
	}

	std::unordered_set<int> material_textures;
	for (const auto& material : model.materials)
	{
		const auto retain = [&material_textures](const int texture)
		{
			if (texture >= 0)
				material_textures.insert(texture);
		};
		retain(material.pbrMetallicRoughness.baseColorTexture.index);
		retain(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
		retain(material.normalTexture.index);
		retain(material.occlusionTexture.index);
		retain(material.emissiveTexture.index);
	}
	const auto report_unused = [&warnings, strict](
		const ResourceLoadError& error, const std::string_view declaration)
	{
		if (strict)
			throw ResourceLoadError(fmt::format(
				"{}; unused {} warning promoted by strict mode", error.what(), declaration));
		warnings.push_back({ fmt::format(
			"{}; unused {} was skipped", error.what(), declaration) });
	};
	for (int index = 0; index < static_cast<int>(model.textures.size()); ++index)
	{
		if (material_textures.contains(index))
			continue;
		try
		{
			validate_gltf_sampler(model, model.textures[index], index, "unused declaration");
		}
		catch (const ResourceLoadError& error)
		{
			report_unused(error, "texture");
		}
	}

	std::unordered_set<int> texture_images;
	std::unordered_set<int> texture_samplers;
	for (const auto& texture : model.textures)
	{
		if (texture.source >= 0)
			texture_images.insert(texture.source);
		if (texture.sampler >= 0)
			texture_samplers.insert(texture.sampler);
	}
	for (int index = 0; index < static_cast<int>(model.images.size()); ++index)
	{
		if (texture_images.contains(index))
			continue;
		const auto& image = model.images[index];
		if (image.extensions.empty() && !image.as_is
			&& (image.mimeType == "image/png" || image.mimeType == "image/jpeg"))
			continue;
		const auto format = image.mimeType.empty() ? "unknown" : image.mimeType;
		const ResourceLoadError error(!image.extensions.empty()
			? fmt::format("ResourceLoader: unused image uses unsupported extension '{}'",
				image.extensions.begin()->first)
			: fmt::format("ResourceLoader: unused image uses unsupported format '{}'", format));
		report_unused(error, "image");
	}
	for (int index = 0; index < static_cast<int>(model.samplers.size()); ++index)
	{
		if (texture_samplers.contains(index))
			continue;
		const auto& sampler = model.samplers[index];
		const bool supported_filter = (sampler.magFilter == -1
			|| sampler.magFilter == TINYGLTF_TEXTURE_FILTER_LINEAR)
			&& (sampler.minFilter == -1
				|| sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR);
		const bool supported_wrap = sampler.wrapS == sampler.wrapT
			&& (sampler.wrapS == TINYGLTF_TEXTURE_WRAP_REPEAT
				|| sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE);
		if (sampler.extensions.empty() && supported_filter && supported_wrap)
			continue;
		const ResourceLoadError error(fmt::format(
			"ResourceLoader: unused sampler {} uses unsupported settings", index));
		report_unused(error, "sampler");
	}
}
}

ResourceLoader::LoadedMaterial ResourceLoader::load_material(
	MaterialSystem& materials,
	const tinygltf::Primitive& primitive,
	const tinygltf::Model& model,
	std::vector<MaterialHandle>& owners)
{
	if (primitive.material >= 0)
	{
		if (primitive.material >= static_cast<int>(model.materials.size()))
			throw ResourceLoadError(fmt::format(
				"ResourceLoader: primitive references invalid material {}", primitive.material));
		if (gltf_material_to_material.contains(primitive.material))
		{
			const auto& material = gltf_material_to_material.at(primitive.material);
			for (const auto material_id : material.ids)
				owners.push_back(materials.acquire(material_id));
			return material;
		}

		const auto& mat = model.materials[primitive.material];
		const auto& pbr = mat.pbrMetallicRoughness;
		PbrMaterial::TextureSlots slots;
		std::vector<MaterialHandle> texture_owners;
		std::vector<std::pair<MaterialID, int>> image_sources;
		const auto load_gltf_texture = [&](const auto& texture_info, const ETextureSemantic semantic)
		{
			const int texture_index = texture_info.index;
			const auto& texture = model.textures.at(texture_index);
			const auto sampler = validate_gltf_sampler(
				model, texture, texture_index, gltf_material_label(mat, primitive.material));
			const uint64_t cache_key = static_cast<uint64_t>(texture.source)
				* static_cast<uint64_t>(ETextureSemantic::COUNT)
				+ static_cast<uint64_t>(semantic);
			MaterialHandle owner;
			if (gltf_image_to_material.contains(cache_key))
			{
				owner = materials.acquire(gltf_image_to_material.at(cache_key));
			}
			else
			{
				const auto& image = model.images.at(texture.source);
				if (image.width <= 0 || image.height <= 0 || image.component < 1
					|| image.component > 4 || image.bits != 8)
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: texture {} has unsupported image dimensions or channels",
						texture_index));
				if (static_cast<size_t>(image.width)
					> std::numeric_limits<size_t>::max() / static_cast<size_t>(image.height))
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: texture {} image dimensions overflow address space",
						texture_index));
				const size_t pixel_count = static_cast<size_t>(image.width)
					* static_cast<size_t>(image.height);
				if (pixel_count > std::numeric_limits<size_t>::max() / 4
					|| image.image.size() < pixel_count * static_cast<size_t>(image.component))
					throw ResourceLoadError(fmt::format(
						"ResourceLoader: texture {} image data is truncated", texture_index));
				std::vector<std::byte> rgba(pixel_count * 4);
				for (size_t pixel = 0; pixel < pixel_count; ++pixel)
				{
					const auto* source = image.image.data() + pixel * image.component;
					auto* destination = rgba.data() + pixel * 4;
					if (image.component == 1 || image.component == 2)
						destination[0] = destination[1] = destination[2]
							= static_cast<std::byte>(source[0]);
					else
					{
						destination[0] = static_cast<std::byte>(source[0]);
						destination[1] = static_cast<std::byte>(source[1]);
						destination[2] = static_cast<std::byte>(source[2]);
					}
					destination[3] = static_cast<std::byte>(image.component == 2 ? source[1]
						: image.component == 4 ? source[3] : 255);
				}
				auto texture_material = std::make_unique<TextureMaterial>();
				texture_material->width = static_cast<uint32_t>(image.width);
				texture_material->height = static_cast<uint32_t>(image.height);
				texture_material->channels = 4;
				texture_material->data_len = rgba.size();
				texture_material->mip_sizes = { rgba.size() };
				texture_material->semantic = semantic;
				texture_material->source = image.uri.empty()
					? fmt::format("glTF image {}", texture.source) : image.uri;
				texture_material->data = std::make_unique<OwnedTextureData>(std::move(rgba));
				owner = materials.add(std::move(texture_material));
				gltf_image_to_material.emplace(cache_key, owner->get_id());
			}
			image_sources.emplace_back(owner->get_id(), texture.source);
			texture_owners.push_back(owner);
			return PbrMaterial::TextureBinding{ owner->get_id(), sampler };
		};

		if (pbr.baseColorTexture.index >= 0)
			slots.base_color = load_gltf_texture(
				pbr.baseColorTexture, ETextureSemantic::BASE_COLOR);
		if (pbr.metallicRoughnessTexture.index >= 0)
			slots.metallic_roughness = load_gltf_texture(
				pbr.metallicRoughnessTexture, ETextureSemantic::METALLIC_ROUGHNESS);
		if (mat.normalTexture.index >= 0)
			slots.normal = load_gltf_texture(mat.normalTexture, ETextureSemantic::NORMAL);

		auto material = std::make_unique<PbrMaterial>(
			glm::vec4(
				static_cast<float>(pbr.baseColorFactor[0]),
				static_cast<float>(pbr.baseColorFactor[1]),
				static_cast<float>(pbr.baseColorFactor[2]),
				static_cast<float>(pbr.baseColorFactor[3])),
			static_cast<float>(pbr.metallicFactor),
			static_cast<float>(pbr.roughnessFactor),
			std::move(slots),
			static_cast<float>(mat.normalTexture.scale));
		owners.push_back(materials.add(std::move(material)));
		LoadedMaterial loaded{
			.ids = { owners.back()->get_id() },
			.image_sources = std::move(image_sources),
		};
		for (auto& texture_owner : texture_owners)
		{
			loaded.ids.push_back(texture_owner->get_id());
			owners.push_back(std::move(texture_owner));
		}
		gltf_material_to_material[primitive.material] = loaded;
		return loaded;
	}

	owners.push_back(materials.add(std::make_unique<PbrMaterial>()));
	return { .ids = { owners.back()->get_id() } };
}

MaterialHandle ResourceLoader::load_texture(
	MaterialSystem& materials,
	const std::filesystem::path& resolved_file_path,
	const std::string_view logical_resource_name,
	const ETextureSemantic semantic)
{
	if (!std::filesystem::exists(resolved_file_path))
	{
		throw ResourceLoadError(fmt::format("ResourceLoader::load_texture: filename does not exist! {}", resolved_file_path.string()));
	}

	if (resolved_file_path.extension() == ".dds")
	{
		auto material = load_dds_texture(resolved_file_path);
		material.semantic = semantic;
		material.source = logical_resource_name;
		return materials.add(std::make_unique<TextureMaterial>(std::move(material)));
	}

	TextureMaterial material;
	material.semantic = semantic;
	material.source = logical_resource_name;
	const auto filename_str = resolved_file_path.string();
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

	return materials.add(std::make_unique<TextureMaterial>(std::move(material)));
}
