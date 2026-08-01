#include "scene_resources.hpp"

#include "entity_component_system/ecs.hpp"
#include "resource_loader/resource_loader.hpp"
#include "serialization/resource_provenance.hpp"
#include "serialization/serialization_helpers.hpp"

#include <array>
#include <bit>
#include <fstream>
#include <limits>
#include <ranges>

namespace
{
constexpr std::array<std::byte, 8> MESH_MAGIC{std::byte{'K'}, std::byte{'R'}, std::byte{'I'}, std::byte{'S'},
                                              std::byte{'P'}, std::byte{'M'}, std::byte{'0'}, std::byte{'1'}};
constexpr std::uint32_t MESH_VERSION = 1;

// Mesh .dat files use an explicitly versioned, little-endian format rather
// than dumping C++ structs. This keeps the save independent of padding, host
// endianness, and changes to the in-memory vertex types.
enum class MeshLayout : std::uint32_t
{
	Color = 1,
	Textured = 2,
	Skinned = 3
};

bool has_key(const Deserializer &in, const std::string_view key)
{
	const auto keys = in.keys();
	return std::ranges::find(keys, key) != keys.end();
}

std::filesystem::path resource_path(const std::filesystem::path &directory, const std::string &filename,
                                    const std::string &yaml_path)
{
	// Resource names are deliberately restricted to files directly inside the
	// save directory. A crafted scene.yaml must not escape that directory.
	const std::filesystem::path relative(filename);
	if (relative.empty() || relative.is_absolute() || relative.has_parent_path() || relative.filename() != relative ||
	    relative.extension() != ".dat")
	{
		throw SerializationError("Invalid resource path at " + yaml_path);
	}
	return directory / relative;
}

void write_source(Serializer out, const ImportedResourceProvenance &source)
{
	out.write("kind", source.kind == EExternalResourceKind::Texture ? "texture" : "model");
	out.write("path", source.source);
	out.write("scene", source.scene);
	out.write("node", source.node);
	out.write("primitive", source.primitive);
	out.write("material", source.material);
	out.write("texture", source.texture);
	out.write("skin", source.skin);
	out.write("animation", source.animation);
}

ImportedResourceProvenance read_source(const Deserializer &in)
{
	const auto kind = in.read<std::string>("kind");
	if (kind != "model" && kind != "texture")
		throw SerializationError("Unsupported external resource kind at " + in.path());
	return {
		.kind = kind == "texture" ? EExternalResourceKind::Texture : EExternalResourceKind::Model,
		.source = in.read<std::string>("path"),
		.scene = in.read<int>("scene"),
		.node = in.read<int>("node"),
		.primitive = in.read<int>("primitive"),
		.material = in.read<int>("material"),
		.texture = in.read<int>("texture"),
		.skin = in.read<int>("skin"),
		.animation = in.read<int>("animation"),
	};
}

std::string semantic_name(const ETextureSemantic semantic)
{
	switch (semantic)
	{
	case ETextureSemantic::BASE_COLOR:
		return "base_color";
	case ETextureSemantic::NORMAL:
		return "normal";
	case ETextureSemantic::SPECULAR:
		return "specular";
	case ETextureSemantic::COUNT:
		break;
	}
	throw SerializationError("Unsupported texture semantic");
}

ETextureSemantic read_semantic(const Deserializer &in)
{
	const auto value = in.as<std::string>();
	if (value == "base_color")
		return ETextureSemantic::BASE_COLOR;
	if (value == "normal")
		return ETextureSemantic::NORMAL;
	if (value == "specular")
		return ETextureSemantic::SPECULAR;
	throw SerializationError("Unsupported texture semantic at " + in.path());
}

std::string format_name(const ETextureFormat format)
{
	switch (format)
	{
	case ETextureFormat::RGBA8:
		return "rgba8";
	case ETextureFormat::BC3:
		return "bc3";
	}
	throw SerializationError("Unsupported texture format");
}

ETextureFormat read_format(const Deserializer &in)
{
	const auto value = in.as<std::string>();
	if (value == "rgba8")
		return ETextureFormat::RGBA8;
	if (value == "bc3")
		return ETextureFormat::BC3;
	throw SerializationError("Unsupported texture format at " + in.path());
}

void validate_texture(const TextureMaterial &texture, const std::string &path)
{
	if (!texture.width || !texture.height || texture.channels != 4)
		throw SerializationError("Invalid generated texture dimensions at " + path);
	std::size_t mip_total = 0;
	for (const auto size : texture.mip_sizes)
	{
		if (size > std::numeric_limits<std::size_t>::max() - mip_total)
			throw SerializationError("Generated texture mip sizes overflow at " + path);
		mip_total += size;
	}
	if (!texture.mip_sizes.empty() && mip_total != texture.data_len)
		throw SerializationError("Generated texture mip sizes do not match its payload at " + path);
	if (texture.format == ETextureFormat::RGBA8)
	{
		const auto pixels = static_cast<std::size_t>(texture.width) * texture.height;
		if (pixels > std::numeric_limits<std::size_t>::max() / texture.channels ||
		    (texture.mip_sizes.empty() && pixels * texture.channels != texture.data_len))
			throw SerializationError("Invalid RGBA8 texture payload size at " + path);
	}
	else if (texture.format == ETextureFormat::BC3 && texture.mip_sizes.empty())
	{
		throw SerializationError("BC3 texture has no mip metadata at " + path);
	}
}

void append_u32(std::vector<std::byte> &bytes, const std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<std::byte>((value >> shift) & 0xff));
}

void append_u64(std::vector<std::byte> &bytes, const std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes.push_back(static_cast<std::byte>((value >> shift) & 0xff));
}

void append_float(std::vector<std::byte> &bytes, const float value)
{
	append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

template<glm::length_t Length> void append_vec(std::vector<std::byte> &bytes, const glm::vec<Length, float> &value)
{
	for (glm::length_t index = 0; index < Length; ++index)
		append_float(bytes, value[index]);
}

void write_mesh_file(const std::filesystem::path &path, const Mesh &mesh, MeshLayout layout)
{
	// Header: magic, format version, vertex layout, vertex count, and index
	// count. Vertex attributes and uint32 indices follow in layout order.
	std::vector<std::byte> bytes(MESH_MAGIC.begin(), MESH_MAGIC.end());
	append_u32(bytes, MESH_VERSION);
	append_u32(bytes, static_cast<std::uint32_t>(layout));
	append_u64(bytes, mesh.get_num_unique_vertices());
	append_u64(bytes, mesh.get_num_vertex_indices());
	if (const auto *typed = dynamic_cast<const ColorMesh *>(&mesh))
	{
		for (const auto &vertex : typed->get_vertices())
		{
			append_vec(bytes, vertex.pos);
			append_vec(bytes, vertex.normal);
		}
	}
	else if (const auto *typed = dynamic_cast<const TexMesh *>(&mesh))
	{
		for (const auto &vertex : typed->get_vertices())
		{
			append_vec(bytes, vertex.pos);
			append_vec(bytes, vertex.normal);
			append_vec(bytes, vertex.texCoord);
			append_vec(bytes, vertex.tangent);
		}
	}
	else if (const auto *typed = dynamic_cast<const SkinnedMesh *>(&mesh))
	{
		for (const auto &vertex : typed->get_vertices())
		{
			append_vec(bytes, vertex.bone_ids);
			append_vec(bytes, vertex.bone_weights);
			append_vec(bytes, vertex.pos);
			append_vec(bytes, vertex.normal);
			append_vec(bytes, vertex.texCoord);
			append_vec(bytes, vertex.tangent);
		}
	}
	else
	{
		throw SerializationError("Unsupported generated mesh type");
	}
	for (const auto index : mesh.get_indices())
		append_u32(bytes, index);

	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream || !stream.write(reinterpret_cast<const char *>(bytes.data()), bytes.size()))
		throw SerializationError("Unable to write mesh resource: " + path.string());
}

class BinaryReader
{
public:
	explicit BinaryReader(std::vector<std::byte> bytes) : bytes(std::move(bytes)) {}

	void require_magic()
	{
		if (bytes.size() < MESH_MAGIC.size() || !std::equal(MESH_MAGIC.begin(), MESH_MAGIC.end(), bytes.begin()))
			throw SerializationError("Invalid mesh resource magic");
		offset = MESH_MAGIC.size();
	}

	std::uint32_t u32()
	{
		require(4);
		std::uint32_t value = 0;
		for (unsigned index = 0; index < 4; ++index)
			value |= std::to_integer<std::uint32_t>(bytes[offset++]) << (index * 8);
		return value;
	}

	std::uint64_t u64()
	{
		require(8);
		std::uint64_t value = 0;
		for (unsigned index = 0; index < 8; ++index)
			value |= std::to_integer<std::uint64_t>(bytes[offset++]) << (index * 8);
		return value;
	}

	float floating() { return std::bit_cast<float>(u32()); }

	template<glm::length_t Length> glm::vec<Length, float> vec()
	{
		glm::vec<Length, float> value;
		for (glm::length_t index = 0; index < Length; ++index)
			value[index] = floating();
		return value;
	}

	bool finished() const { return offset == bytes.size(); }
	std::size_t remaining() const { return bytes.size() - offset; }

private:
	void require(const std::size_t count)
	{
		// Check before every scalar read so truncated or malicious files cannot
		// make the decoder read beyond its owned byte buffer.
		if (count > bytes.size() - offset)
			throw SerializationError("Truncated mesh resource");
	}

	std::vector<std::byte> bytes;
	std::size_t offset = 0;
};

std::unique_ptr<Mesh> read_mesh_file(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
		throw SerializationError("Unable to open mesh resource: " + path.string());
	const auto length = stream.tellg();
	if (length < 0)
		throw SerializationError("Unable to size mesh resource: " + path.string());
	std::vector<std::byte> bytes(static_cast<std::size_t>(length));
	stream.seekg(0);
	if (!bytes.empty() && !stream.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
		throw SerializationError("Unable to read mesh resource: " + path.string());
	BinaryReader reader(std::move(bytes));
	reader.require_magic();
	if (reader.u32() != MESH_VERSION)
		throw SerializationError("Unsupported mesh resource version: " + path.string());
	const auto layout = static_cast<MeshLayout>(reader.u32());
	const auto vertex_count_u64 = reader.u64();
	const auto index_count_u64 = reader.u64();
	if (vertex_count_u64 > std::numeric_limits<std::uint32_t>::max() ||
	    index_count_u64 > std::numeric_limits<std::uint32_t>::max())
		throw SerializationError("Mesh resource count is too large: " + path.string());
	const auto vertex_count = static_cast<std::size_t>(vertex_count_u64);
	const auto index_count = static_cast<std::size_t>(index_count_u64);
	std::size_t floats_per_vertex;
	switch (layout)
	{
	case MeshLayout::Color:
		floats_per_vertex = 6;
		break;
	case MeshLayout::Textured:
		floats_per_vertex = 12;
		break;
	case MeshLayout::Skinned:
		floats_per_vertex = 20;
		break;
	default:
		throw SerializationError("Unsupported mesh resource layout: " + path.string());
	}
	if (vertex_count > std::numeric_limits<std::size_t>::max() / floats_per_vertex / sizeof(float) ||
	    index_count > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t))
		throw SerializationError("Mesh resource size overflows address space: " + path.string());
	const auto vertex_bytes = vertex_count * floats_per_vertex * sizeof(float);
	const auto index_bytes = index_count * sizeof(std::uint32_t);
	if (vertex_bytes > std::numeric_limits<std::size_t>::max() - index_bytes ||
	    reader.remaining() != vertex_bytes + index_bytes)
		throw SerializationError("Mesh resource size mismatch: " + path.string());
	std::vector<std::uint32_t> indices;
	indices.reserve(index_count);
	if (layout == MeshLayout::Color)
	{
		ColorVertices vertices;
		vertices.reserve(vertex_count);
		for (std::size_t index = 0; index < vertex_count; ++index)
			vertices.push_back({.pos = reader.vec<3>(), .normal = reader.vec<3>()});
		for (std::size_t index = 0; index < index_count; ++index)
		{
			const auto vertex = reader.u32();
			if (vertex >= vertex_count)
				throw SerializationError("Mesh resource index is out of range: " + path.string());
			indices.push_back(vertex);
		}
		if (!reader.finished())
			throw SerializationError("Unexpected trailing mesh resource data: " + path.string());
		return std::make_unique<ColorMesh>(std::move(vertices), std::move(indices));
	}
	if (layout == MeshLayout::Textured)
	{
		TexVertices vertices;
		vertices.reserve(vertex_count);
		for (std::size_t index = 0; index < vertex_count; ++index)
			vertices.push_back({.pos = reader.vec<3>(),
			                    .normal = reader.vec<3>(),
			                    .texCoord = reader.vec<2>(),
			                    .tangent = reader.vec<4>()});
		for (std::size_t index = 0; index < index_count; ++index)
		{
			const auto vertex = reader.u32();
			if (vertex >= vertex_count)
				throw SerializationError("Mesh resource index is out of range: " + path.string());
			indices.push_back(vertex);
		}
		if (!reader.finished())
			throw SerializationError("Unexpected trailing mesh resource data: " + path.string());
		return std::make_unique<TexMesh>(std::move(vertices), std::move(indices));
	}
	if (layout == MeshLayout::Skinned)
	{
		SkinnedVertices vertices;
		vertices.reserve(vertex_count);
		for (std::size_t index = 0; index < vertex_count; ++index)
		{
			SDS::SkinnedVertex vertex;
			vertex.bone_ids = reader.vec<4>();
			vertex.bone_weights = reader.vec<4>();
			vertex.pos = reader.vec<3>();
			vertex.normal = reader.vec<3>();
			vertex.texCoord = reader.vec<2>();
			vertex.tangent = reader.vec<4>();
			vertices.push_back(vertex);
		}
		for (std::size_t index = 0; index < index_count; ++index)
		{
			const auto vertex = reader.u32();
			if (vertex >= vertex_count)
				throw SerializationError("Mesh resource index is out of range: " + path.string());
			indices.push_back(vertex);
		}
		if (!reader.finished())
			throw SerializationError("Unexpected trailing mesh resource data: " + path.string());
		return std::make_unique<SkinnedMesh>(std::move(vertices), std::move(indices));
	}
	throw SerializationError("Unsupported mesh resource layout: " + path.string());
}

std::vector<std::byte> read_payload(const std::filesystem::path &path, const std::size_t expected)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream || stream.tellg() < 0 || static_cast<std::uintmax_t>(stream.tellg()) != expected)
		throw SerializationError("Texture resource size mismatch: " + path.string());
	std::vector<std::byte> bytes(expected);
	stream.seekg(0);
	if (expected && !stream.read(reinterpret_cast<char *>(bytes.data()), expected))
		throw SerializationError("Unable to read texture resource: " + path.string());
	return bytes;
}
} // namespace

SceneResourceWriter::SceneResourceWriter(Serializer &document, const ECS &ecs, std::filesystem::path directory) :
	ecs(ecs),
	directory(std::move(directory)),
	resources(document.map("resources")),
	meshes(resources.sequence("meshes")),
	materials(resources.sequence("materials"))
{
}

void SceneResourceWriter::write_mesh_reference(Serializer out, const MeshID id)
{
	// Provenance is the dividing line between imported and generated data. An
	// imported resource is reproducible from its source and is therefore never
	// copied into the save directory.
	if (const auto *source = ResourceProvenance::mesh(id))
	{
		write_source(out.map("source"), *source);
		return;
	}
	write_generated_mesh(id);
	out.write("resource_id", id.get_underlying());
}

void SceneResourceWriter::write_material_reference(Serializer out, const MaterialID id)
{
	if (const auto *source = ResourceProvenance::material(id))
	{
		write_source(out.map("source"), *source);
		return;
	}
	write_generated_material(id);
	out.write("resource_id", id.get_underlying());
}

void SceneResourceWriter::write_generated_mesh(const MeshID id)
{
	if (!written_meshes.emplace(id, true).second)
		return;
	const auto &mesh = ecs.get_mesh_system().get(id);
	MeshLayout layout;
	if (dynamic_cast<const ColorMesh *>(&mesh))
		layout = MeshLayout::Color;
	else if (dynamic_cast<const TexMesh *>(&mesh))
		layout = MeshLayout::Textured;
	else if (dynamic_cast<const SkinnedMesh *>(&mesh))
		layout = MeshLayout::Skinned;
	else
		throw SerializationError("Unsupported generated mesh type");
	const auto filename = "mesh_" + std::to_string(id.get_underlying()) + ".dat";
	write_mesh_file(directory / filename, mesh, layout);
	auto entry = meshes.append_map();
	entry.write("id", id.get_underlying());
	entry.write("file", filename);
}

void SceneResourceWriter::write_generated_material(const MaterialID id)
{
	if (!written_materials.emplace(id, true).second)
		return;
	const auto &material = ecs.get_material_system().get(id);
	auto entry = materials.append_map();
	entry.write("id", id.get_underlying());
	if (const auto *color = dynamic_cast<const ColorMaterial *>(&material))
	{
		// Color materials are small structured values, so keeping their parameters
		// in YAML is more readable than creating another binary resource.
		entry.write("type", "color");
		auto parameters = entry.map("parameters");
		Serialization::write_vec3(parameters, "ambient", color->data.ambient);
		Serialization::write_vec3(parameters, "diffuse", color->data.diffuse);
		Serialization::write_vec3(parameters, "specular", color->data.specular);
		Serialization::write_vec3(parameters, "emissive", color->data.emissive);
		parameters.write("shininess", color->data.shininess);
		parameters.write("texture_flags", color->data.texture_flags);
		return;
	}
	const auto *texture = dynamic_cast<const TextureMaterial *>(&material);
	if (!texture)
		throw SerializationError("Unsupported generated material type");
	validate_texture(*texture, "generated material " + std::to_string(id.get_underlying()));
	if (texture->data_len && (!texture->data || !texture->data->get()))
		throw SerializationError("Generated texture has no pixel data");
	const auto filename = "texture_" + std::to_string(id.get_underlying()) + ".dat";
	// Generated texture payloads are already encoded according to format and mip
	// metadata. Store those bytes verbatim; scene.yaml carries their interpretation.
	std::ofstream stream(directory / filename, std::ios::binary | std::ios::trunc);
	if (!stream ||
	    (texture->data_len && !stream.write(reinterpret_cast<const char *>(texture->data->get()), texture->data_len)))
		throw SerializationError("Unable to write texture resource: " + filename);
	entry.write("type", "texture");
	entry.write("file", filename);
	entry.write("data_len", texture->data_len);
	entry.write("width", texture->width);
	entry.write("height", texture->height);
	entry.write("channels", texture->channels);
	entry.write("format", format_name(texture->format));
	entry.write("semantic", semantic_name(texture->semantic));
	entry.write("source", texture->source);
	auto mips = entry.sequence("mip_sizes");
	for (const auto size : texture->mip_sizes)
		mips.append(size);
}

SceneResourceReader::SceneResourceReader(ECS &ecs, std::filesystem::path directory) :
	ecs(ecs), directory(std::move(directory))
{
}

void SceneResourceReader::load_model_source(const Deserializer &source)
{
	const auto path = source.read<std::string>("path");
	const auto scene = source.read<int>("scene");
	const auto key = path + "#" + std::to_string(scene);
	if (imported_models.contains(key))
		return;
	ResourceLoader::LoadOptions options;
	if (scene >= 0)
		options.scene_index = scene;
	options.generate_missing_tangents = true;
	imported_models.emplace(key, ResourceLoader::load_model(ecs, path, options));
}

void SceneResourceReader::prepare(const Deserializer &document)
{
	// First restore imported model sources. Loading one model may register its
	// meshes, materials, skeletons, and animations together, so each model/scene
	// pair is loaded at most once even if many saved references point into it.
	const auto ecs_document = has_key(document, "ecs") ? document.child("ecs") : document;
	if (has_key(ecs_document, "skeletal_system"))
		for (const auto &entry : ecs_document.child("skeletal_system").elements())
			if (has_key(entry, "imported_source"))
			{
				const auto source = entry.child("imported_source");
				ImportedResourceProvenance provenance{
					.source = source.read<std::string>("path"),
					.scene = source.read<int>("scene"),
					.node = source.read<int>("node"),
					.skin = source.read<int>("skin"),
					.animation = source.read<int>("animation"),
				};
				if (!ResourceProvenance::find_skeleton(provenance))
					load_model_source(source);
			}
	if (has_key(ecs_document, "renderable_system"))
		for (const auto &entry : ecs_document.child("renderable_system").elements())
		{
			const auto mesh = entry.child("mesh");
			if (has_key(mesh, "source"))
			{
				const auto source = read_source(mesh.child("source"));
				if (source.kind == EExternalResourceKind::Model &&
				    !ResourceProvenance::find_mesh(ecs.get_mesh_system(), source))
					load_model_source(mesh.child("source"));
			}
			for (const auto &material : entry.child("materials").elements())
				if (has_key(material, "source"))
				{
					const auto source = read_source(material.child("source"));
					if (source.kind == EExternalResourceKind::Model &&
					    !ResourceProvenance::find_material(ecs.get_material_system(), source))
						load_model_source(material.child("source"));
				}
		}

	// Generated resources are reconstructed before component systems deserialize.
	// The maps retain owning handles and also preserve sharing between renderables.
	const auto saved_resources = document.child("resources");
	for (const auto &entry : saved_resources.child("meshes").elements())
	{
		const auto id = entry.read<std::uint64_t>("id");
		const auto file = resource_path(directory, entry.read<std::string>("file"), entry.child("file").path());
		if (!meshes.emplace(id, ecs.get_mesh_system().add(read_mesh_file(file))).second)
			throw SerializationError("Duplicate generated mesh resource at " + entry.path());
	}
	for (const auto &entry : saved_resources.child("materials").elements())
	{
		const auto id = entry.read<std::uint64_t>("id");
		std::unique_ptr<Material> material;
		const auto type = entry.read<std::string>("type");
		if (type == "color")
		{
			auto color = std::make_unique<ColorMaterial>();
			const auto parameters = entry.child("parameters");
			color->data.ambient = Serialization::read_vec3(parameters, "ambient");
			color->data.diffuse = Serialization::read_vec3(parameters, "diffuse");
			color->data.specular = Serialization::read_vec3(parameters, "specular");
			color->data.emissive = Serialization::read_vec3(parameters, "emissive");
			color->data.shininess = parameters.read<float>("shininess");
			color->data.texture_flags = parameters.read<int>("texture_flags");
			material = std::move(color);
		}
		else if (type == "texture")
		{
			auto texture = std::make_unique<TextureMaterial>();
			texture->data_len = entry.read<std::size_t>("data_len");
			texture->width = entry.read<std::uint32_t>("width");
			texture->height = entry.read<std::uint32_t>("height");
			texture->channels = entry.read<std::uint32_t>("channels");
			texture->format = read_format(entry.child("format"));
			texture->semantic = read_semantic(entry.child("semantic"));
			texture->source = entry.read<std::string>("source");
			for (const auto &mip : entry.child("mip_sizes").elements())
				texture->mip_sizes.push_back(mip.as<std::size_t>());
			validate_texture(*texture, entry.path());
			const auto file = resource_path(directory, entry.read<std::string>("file"), entry.child("file").path());
			// Unlike externally loaded images, this payload has no loader/cache owner.
			// OwnedTextureData keeps the restored bytes alive with the material.
			texture->data = std::make_unique<OwnedTextureData>(read_payload(file, texture->data_len));
			material = std::move(texture);
		}
		else
		{
			throw SerializationError("Unsupported generated material type at " + entry.path());
		}
		if (!materials.emplace(id, ecs.get_material_system().add(std::move(material))).second)
			throw SerializationError("Duplicate generated material resource at " + entry.path());
	}
}

MeshHandle SceneResourceReader::read_mesh_reference(const Deserializer &in)
{
	if (has_key(in, "resource_id"))
	{
		const auto id = in.read<std::uint64_t>("resource_id");
		if (!meshes.contains(id))
			throw SerializationError("Missing generated mesh resource at " + in.path());
		return meshes.at(id);
	}
	const auto source = read_source(in.child("source"));
	if (source.kind != EExternalResourceKind::Model)
		throw SerializationError("Mesh source is not an external model at " + in.path());
	const auto id = ResourceProvenance::find_mesh(ecs.get_mesh_system(), source);
	if (!id)
		throw SerializationError("Missing imported mesh resource at " + in.path());
	return ecs.get_mesh_system().acquire(*id);
}

MaterialHandle SceneResourceReader::read_material_reference(const Deserializer &in)
{
	if (has_key(in, "resource_id"))
	{
		const auto id = in.read<std::uint64_t>("resource_id");
		if (!materials.contains(id))
			throw SerializationError("Missing generated material resource at " + in.path());
		return materials.at(id);
	}
	const auto source = read_source(in.child("source"));
	if (source.kind == EExternalResourceKind::Texture)
	{
		const auto semantic = static_cast<ETextureSemantic>(source.texture);
		if (semantic < ETextureSemantic::BASE_COLOR || semantic >= ETextureSemantic::COUNT)
			throw SerializationError("Invalid external texture semantic at " + in.path());
		const auto key = source.source + "#" + std::to_string(source.texture);
		if (!imported_textures.contains(key))
			imported_textures.emplace(
				key, ResourceLoader::fetch_texture(ecs.get_material_system(), source.source, semantic));
		return imported_textures.at(key);
	}
	const auto id = ResourceProvenance::find_material(ecs.get_material_system(), source);
	if (!id)
		throw SerializationError("Missing imported material resource at " + in.path());
	return ecs.get_material_system().acquire(*id);
}

void SceneResourceReader::register_renderable_id(const RenderableID saved, const RenderableID restored)
{
	if (!renderable_ids.emplace(saved, restored).second)
		throw SerializationError("Duplicate saved renderable ID " + std::to_string(saved.get_underlying()));
}

void SceneResourceReader::register_skeleton_id(const SkeletonID saved, const SkeletonID restored)
{
	if (!skeleton_ids.emplace(saved, restored).second)
		throw SerializationError("Duplicate saved skeleton ID " + std::to_string(saved.get_underlying()));
}

RenderableID SceneResourceReader::read_renderable_id(const RenderableID saved) const
{
	const auto found = renderable_ids.find(saved);
	if (found == renderable_ids.end())
		throw SerializationError("Unknown saved renderable ID " + std::to_string(saved.get_underlying()));
	return found->second;
}

SkeletonID SceneResourceReader::read_skeleton_id(const SkeletonID saved) const
{
	const auto found = skeleton_ids.find(saved);
	if (found == skeleton_ids.end())
		throw SerializationError("Unknown saved skeleton ID " + std::to_string(saved.get_underlying()));
	return found->second;
}
