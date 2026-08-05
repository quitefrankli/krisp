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

namespace
{
std::string gltf_material_label(const tinygltf::Material& material, const size_t index)
{
	return material.name.empty()
		? fmt::format("{}", index)
		: fmt::format("{} ('{}')", index, material.name);
}

void validate_factor_only_gltf_material(const tinygltf::Material& material, const size_t index)
{
	const auto label = gltf_material_label(material, index);
	const auto reject = [&label](const std::string_view feature)
	{
		throw ResourceLoadError(fmt::format(
			"ResourceLoader: material {} uses unsupported feature {}", label, feature));
	};
	const auto& pbr = material.pbrMetallicRoughness;
	if (pbr.baseColorTexture.index >= 0)
		reject("baseColorTexture");
	if (pbr.metallicRoughnessTexture.index >= 0)
		reject("metallicRoughnessTexture");
	if (material.normalTexture.index >= 0)
		reject("normalTexture");
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
}

void validate_factor_only_gltf_materials(const tinygltf::Model& model)
{
	for (size_t index = 0; index < model.materials.size(); ++index)
		validate_factor_only_gltf_material(model.materials[index], index);
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
		auto material = std::make_unique<PbrMaterial>(
			glm::vec4(
				static_cast<float>(pbr.baseColorFactor[0]),
				static_cast<float>(pbr.baseColorFactor[1]),
				static_cast<float>(pbr.baseColorFactor[2]),
				static_cast<float>(pbr.baseColorFactor[3])),
			static_cast<float>(pbr.metallicFactor),
			static_cast<float>(pbr.roughnessFactor));
		owners.push_back(materials.add(std::move(material)));
		LoadedMaterial loaded{
			.ids = { owners.back()->get_id() },
		};
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
