#include "asset_registry.hpp"
#include "kasset.hpp"
#include "entity_component_system/ecs.hpp"
#include "utility.hpp"

#include <tiny_gltf.h>
#include <fstream>
#include <mutex>
#include <set>
#include <map>
#include <algorithm>

namespace
{
using Json = nlohmann::json;
constexpr size_t maximum_metadata_size = 16 * 1024 * 1024;

std::string kind_name(AssetKind kind) { return std::string(asset_kind_name(kind)); }

AssetKind parse_kind(std::string_view name)
{
	for (auto kind : {AssetKind::Mesh, AssetKind::Material, AssetKind::Texture, AssetKind::Skeleton,
	                  AssetKind::Animation, AssetKind::Assembly})
		if (kind_name(kind) == name)
			return kind;
	throw ResourceLoadError("Unsupported asset kind: " + std::string(name));
}

Json selector_json(const AssetSelector &s)
{
	return {{"scene", s.scene}, {"node", s.node}, {"primitive", s.primitive}, {"material", s.material},
	        {"image", s.image}, {"skin", s.skin}, {"animation", s.animation}};
}

AssetSelector parse_selector(const Json &j)
{
	return {j.value("scene", -1), j.value("node", -1), j.value("primitive", -1), j.value("material", -1),
	        j.value("image", -1), j.value("skin", -1), j.value("animation", -1)};
}

std::vector<unsigned char> read_bytes(const std::filesystem::path &path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file || file.tellg() < 0)
		throw ResourceLoadError("Cannot open asset: " + path.string());
	const auto size = static_cast<uint64_t>(file.tellg());
	if (size > std::numeric_limits<size_t>::max() || size > std::numeric_limits<std::streamsize>::max())
		throw ResourceLoadError("Asset exceeds addressable input size");
	std::vector<unsigned char> bytes(static_cast<size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
		throw ResourceLoadError("Cannot read asset: " + path.string());
	return bytes;
}

uint32_t read_u32(const unsigned char *p)
{
	return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

Json metadata_from_bytes(std::span<const unsigned char> bytes, std::string_view file)
{
	if (std::filesystem::path(file).extension() == ".glb")
	{
		if (bytes.size() < 20 || read_u32(bytes.data()) != 0x46546c67 || read_u32(bytes.data() + 4) != 2 ||
		    read_u32(bytes.data() + 16) != 0x4e4f534a)
			throw ResourceLoadError("Invalid GLB metadata: " + std::string(file));
		const size_t length = read_u32(bytes.data() + 12);
		if (length > maximum_metadata_size || length > bytes.size() - 20)
			throw ResourceLoadError("Invalid GLB JSON length");
		return Json::parse(bytes.begin() + 20, bytes.begin() + 20 + length);
	}
	if (bytes.size() > maximum_metadata_size)
		throw ResourceLoadError("glTF metadata exceeds size limit");
	return Json::parse(bytes.begin(), bytes.end());
}

Json read_metadata(const std::filesystem::path &path)
{
	// Read only the GLB JSON chunk; registration does not read its geometry blob.
	if (path.extension() != ".glb")
		return metadata_from_bytes(read_bytes(path), path.string());
	std::ifstream file(path, std::ios::binary);
	std::array<unsigned char, 20> header{};
	if (!file.read(reinterpret_cast<char *>(header.data()), header.size()))
		throw ResourceLoadError("Truncated GLB header");
	const size_t length = read_u32(header.data() + 12);
	if (length > maximum_metadata_size)
		throw ResourceLoadError("GLB metadata exceeds size limit");
	std::vector<unsigned char> bytes(header.begin(), header.end());
	bytes.resize(20 + length);
	if (!file.read(reinterpret_cast<char *>(bytes.data() + 20), length))
		throw ResourceLoadError("Truncated GLB JSON");
	return metadata_from_bytes(bytes, path.string());
}

Json read_metadata(const KassetContainer &package, const std::string &member)
{
	if (std::filesystem::path(member).extension() != ".glb")
		return metadata_from_bytes(package.read_prefix(member, maximum_metadata_size + 1), member);
	auto header = package.read_prefix(member, 20);
	if (header.size() != 20)
		throw ResourceLoadError("Truncated package GLB header");
	const size_t length = read_u32(header.data() + 12);
	if (length > maximum_metadata_size)
		throw ResourceLoadError("Package GLB metadata exceeds size limit");
	return metadata_from_bytes(package.read_prefix(member, 20 + length), member);
}

bool is_document(std::string_view name)
{
	auto extension = std::filesystem::path(name).extension();
	return extension == ".gltf" || extension == ".glb";
}

Json registration_json(const AssetLoadOptions &options, const std::vector<AssetExport> &exports)
{
	Json list = Json::array();
	for (const auto &e : exports)
		list.push_back({{"name", e.name},
		                {"kind", kind_name(e.kind)},
		                {"file", e.member},
		                {"selector", selector_json(e.selector)}});
	return {{"exports", list},
	        {"import",
	         {{"generate_missing_normals", options.import.generate_missing_normals},
	          {"generate_missing_tangents", options.import.generate_missing_tangents},
	          {"allow_non_triangle_primitives", options.import.allow_non_triangle_primitives},
	          {"strict", options.import.strict}}}};
}

AssetLoadOptions parse_registration(std::string_view value)
{
	const auto json = Json::parse(value);
	AssetLoadOptions options;
	for (const auto &e : json.at("exports"))
		options.exports.push_back({e.at("name"), parse_kind(e.at("kind").get<std::string>()),
		                           e.value("file", std::string{}),
		                           parse_selector(e.value("selector", Json::object()))});
	const auto &i = json.at("import");
	options.import.generate_missing_normals = i.at("generate_missing_normals");
	options.import.generate_missing_tangents = i.at("generate_missing_tangents");
	options.import.allow_non_triangle_primitives = i.at("allow_non_triangle_primitives");
	options.import.strict = i.at("strict");
	return options;
}

struct DocumentData
{
	ResourceLoader material_decoder;
	Json metadata;
	std::optional<ResourceDocument> document;
	std::map<int, std::shared_ptr<ResourceLoader::DecodedModel>> models;
	std::map<int, std::vector<MaterialHandle>> materials;
	std::map<std::pair<int, ETextureSemantic>, MaterialHandle> images;
	std::map<int, std::vector<Bone>> skeletons;
	std::map<std::pair<int, int>, SkeletalAnimation> animations;
};

struct AssetSource
{
	std::string path;
	std::filesystem::path resolved;
	std::shared_ptr<KassetContainer> package;
	AssetLoadOptions options;
	std::shared_ptr<const std::string> registration;
	MeshSystem meshes;
	MaterialSystem materials;
	std::map<std::string, DocumentData> documents;
	std::recursive_mutex mutex;
	std::vector<unsigned char> bytes(const std::string &member) const
	{
		try
		{
			return package ? package->read(member) : read_bytes(resolved);
		} catch (const std::runtime_error &error)
		{
			throw ResourceLoadError(error.what());
		}
	}
	ResourceDocument &document(const std::string &member)
	{
		auto &data = documents.at(member);
		if (!data.document)
		{
			if (!package)
				data.document = ResourceLoader::read_document(resolved);
			else
			{
				auto contents = bytes(member);
				data.document = ResourceLoader::read_document(contents, member, [this, member](std::string_view uri) {
					return package->read(KassetContainer::resolve(member, uri));
				});
			}
		}
		return *data.document;
	}
};
} // namespace

struct AssetEntry
{
	AssetExport definition;
	size_t count = 0;
	std::shared_ptr<AssetSource> source;
};

struct AssetRegistry::State
{
	std::map<std::string, std::shared_ptr<AssetEntry>, std::less<>> names;
	std::map<std::string, std::shared_ptr<AssetSource>, std::less<>> sources;
	mutable std::recursive_mutex mutex;
	std::vector<std::shared_ptr<AssetEntry>> entries;
};

namespace
{
ImportedResourceProvenance provenance(const AssetEntry &e, ImportedResourceProvenance p = {})
{
	p.source = e.source->path;
	p.member = e.definition.member;
	p.registration = e.source->registration;
	return p;
}

ImportedResourceProvenance selected_provenance(const AssetEntry &e)
{
	const auto &s = e.definition.selector;
	return provenance(e, {.scene = s.scene,
	                      .node = s.node,
	                      .primitive = s.primitive,
	                      .material = s.material,
	                      .image = s.image,
	                      .skin = s.skin,
	                      .animation = s.animation});
}

ResourceLoader::DecodedModel &decoded_model(const std::shared_ptr<AssetEntry> &entry)
{
	auto &source = *entry->source;
	const auto &member = entry->definition.member;
	int scene = entry->definition.selector.scene;
	if (scene < 0)
		scene = source.documents.at(member).metadata.value("scene", 0);
	auto &cached = source.documents.at(member).models[scene];
	if (!cached)
	{
		auto options = source.options.import;
		options.scene_index = scene;
		cached = std::make_shared<ResourceLoader::DecodedModel>(
			ResourceLoader::decode_model(source.document(member), source.path, options, source.meshes, source.materials,
		                                 nullptr, &source.documents.at(member).material_decoder));
	}
	return *cached;
}

MaterialHandle texture_data(const std::shared_ptr<AssetEntry> &entry, ETextureSemantic semantic)
{
	if (semantic == ETextureSemantic::COUNT)
		throw ResourceLoadError("Invalid texture semantic");
	auto &source = *entry->source;
	auto &data = source.documents.at(entry->definition.member);
	const int image = entry->definition.selector.image;
	auto &cache = data.images[{image, semantic}];
	if (!cache)
	{
		if (is_document(source.package ? entry->definition.member : source.path))
			cache = ResourceLoader::decode_image(
				source.document(entry->definition.member), image, source.materials, semantic,
				entry->definition.name.empty() ? source.path : entry->definition.name, &data.material_decoder);
		else
		{
			auto bytes = source.bytes(entry->definition.member);
			cache = ResourceLoader::load_texture_bytes(
				source.materials, bytes, entry->definition.name.empty() ? source.path : entry->definition.name,
				semantic);
		}
	}
	return cache;
}

std::vector<MaterialHandle> &material_data(const std::shared_ptr<AssetEntry> &entry)
{
	auto &source = *entry->source;
	auto &data = source.documents.at(entry->definition.member);
	auto &result = data.materials[entry->definition.selector.material];
	if (result.empty())
		result = ResourceLoader::decode_material(source.document(entry->definition.member),
		                                         entry->definition.selector.material, source.materials,
		                                         &data.material_decoder);
	return result;
}

std::vector<Bone> &skeleton_data(const std::shared_ptr<AssetEntry> &entry)
{
	auto &source = *entry->source;
	auto &data = source.documents.at(entry->definition.member);
	const int skin = entry->definition.selector.skin;
	if (!data.skeletons.contains(skin))
		data.skeletons.emplace(skin, ResourceLoader::decode_skeleton(source.document(entry->definition.member), skin));
	return data.skeletons.at(skin);
}

SkeletalAnimation &animation_data(const std::shared_ptr<AssetEntry> &entry)
{
	auto &source = *entry->source;
	auto &data = source.documents.at(entry->definition.member);
	auto key = std::pair(entry->definition.selector.animation, entry->definition.selector.skin);
	if (!data.animations.contains(key))
		data.animations.emplace(
			key, ResourceLoader::decode_animation(source.document(entry->definition.member), key.first, key.second));
	return data.animations.at(key);
}

MeshHandle mesh_data(const std::shared_ptr<AssetEntry> &entry)
{
	auto &model = decoded_model(entry);
	for (const auto &group : model.model.meshes)
		if (group.source_node == entry->definition.selector.node)
		{
			const int primitive = entry->definition.selector.primitive;
			if (primitive >= 0 && static_cast<size_t>(primitive) < group.renderables.size())
				return group.renderables[primitive].mesh_owner;
		}
	throw ResourceLoadError("Mesh selector does not occur in its selected scene: " + entry->definition.name);
}

void materialize(const std::shared_ptr<AssetEntry> &entry)
{
	const std::lock_guard lock(entry->source->mutex);
	switch (entry->definition.kind)
	{
	case AssetKind::Mesh:
		mesh_data(entry);
		break;
	case AssetKind::Material:
		material_data(entry);
		break;
	case AssetKind::Texture:
		texture_data(entry, ETextureSemantic::BASE_COLOR);
		break;
	case AssetKind::Skeleton:
		skeleton_data(entry);
		break;
	case AssetKind::Animation:
		animation_data(entry);
		break;
	case AssetKind::Assembly:
		decoded_model(entry);
		break;
	}
}

template<AssetKind Kind> Asset make_descriptor(const std::shared_ptr<AssetEntry> &entry, AssetHandle<Kind> handle)
{
	return AssetDescriptor<Kind>{std::move(handle), entry->definition.selector, entry->count};
}

// Every node primitive, authored material/image/skin/clip, and scene has an export.
std::vector<std::pair<AssetKind, AssetSelector>> selectors(const Json &j)
{
	std::vector<std::pair<AssetKind, AssetSelector>> result;
	const auto empty = Json::array();
	const auto &nodes = j.value("nodes", empty);
	const auto &meshes = j.value("meshes", empty);
	const auto &scenes = j.value("scenes", empty);
	for (size_t scene = 0; scene < scenes.size(); ++scene)
	{
		result.emplace_back(AssetKind::Assembly, AssetSelector{.scene = static_cast<int>(scene)});
		std::set<int> visited;
		std::function<void(int)> visit = [&](int node) {
			if (node < 0 || static_cast<size_t>(node) >= nodes.size())
				throw ResourceLoadError("Invalid glTF node metadata");
			if (!visited.insert(node).second)
				return;
			const auto &n = nodes[node];
			if (n.contains("mesh"))
			{
				const int mesh = n.at("mesh");
				if (mesh < 0 || static_cast<size_t>(mesh) >= meshes.size())
					throw ResourceLoadError("Invalid glTF mesh metadata");
				const auto &primitives = meshes[mesh].at("primitives");
				for (size_t primitive = 0; primitive < primitives.size(); ++primitive)
					result.emplace_back(AssetKind::Mesh, AssetSelector{.scene = static_cast<int>(scene),
					                                                   .node = node,
					                                                   .primitive = static_cast<int>(primitive)});
			}
			for (const auto &child : n.value("children", empty))
				visit(child.get<int>());
		};
		for (const auto &node : scenes[scene].value("nodes", empty))
			visit(node.get<int>());
	}
	for (size_t i = 0; i < j.value("materials", empty).size(); ++i)
		result.emplace_back(AssetKind::Material, AssetSelector{.material = static_cast<int>(i)});
	for (size_t i = 0; i < j.value("images", empty).size(); ++i)
		result.emplace_back(AssetKind::Texture, AssetSelector{.image = static_cast<int>(i)});
	for (size_t i = 0; i < j.value("skins", empty).size(); ++i)
		result.emplace_back(AssetKind::Skeleton, AssetSelector{.skin = static_cast<int>(i)});
	const auto &skins = j.value("skins", empty);
	const auto &animations = j.value("animations", empty);
	for (size_t i = 0; i < animations.size(); ++i)
	{
		bool matched = false;
		for (size_t skin = 0; skin < skins.size(); ++skin)
		{
			const auto &joints = skins[skin].at("joints");
			bool targets_skin = false;
			for (const auto &channel : animations[i].value("channels", empty))
			{
				const int node = channel.at("target").value("node", -1);
				targets_skin = targets_skin || std::find(joints.begin(), joints.end(), node) != joints.end();
			}
			if (targets_skin)
			{
				matched = true;
				result.emplace_back(AssetKind::Animation,
				                    AssetSelector{.skin = static_cast<int>(skin), .animation = static_cast<int>(i)});
			}
		}
		if (!matched)
			throw ResourceLoadError("Animation does not target a supported skeletal rig");
	}
	return result;
}

size_t validate_export(AssetExport &e, const Json &metadata, bool document)
{
	if (e.name.find('\0') != std::string::npos)
		throw ResourceLoadError("Asset names must not contain null characters");
	if (!document)
	{
		if (e.kind != AssetKind::Texture || e.selector != AssetSelector{})
			throw ResourceLoadError("Loose image export must be a texture without document selectors");
		return 0;
	}
	const auto &s = e.selector;
	auto check = [&](const char *array, int index) -> const Json & {
		if (index < 0 || !metadata.contains(array) || static_cast<size_t>(index) >= metadata.at(array).size())
			throw ResourceLoadError("Invalid " + std::string(array) + " selector for " + e.name);
		return metadata.at(array).at(index);
	};
	switch (e.kind)
	{
	case AssetKind::Assembly:
		return check("scenes", s.scene).value("nodes", Json::array()).size();
	case AssetKind::Mesh: {
		check("scenes", s.scene);
		const auto &node = check("nodes", s.node);
		const auto &mesh = check("meshes", node.at("mesh"));
		if (s.primitive < 0 || static_cast<size_t>(s.primitive) >= mesh.at("primitives").size())
			throw ResourceLoadError("Invalid primitive selector");
		const auto &p = mesh.at("primitives").at(s.primitive);
		return check("accessors", p.at("attributes").at("POSITION")).at("count").get<size_t>();
	}
	case AssetKind::Material:
		check("materials", s.material);
		return 0;
	case AssetKind::Texture:
		check("images", s.image);
		return 0;
	case AssetKind::Skeleton:
		return check("skins", s.skin).at("joints").size();
	case AssetKind::Animation:
		check("skins", s.skin);
		return check("animations", s.animation).at("channels").size();
	}
	return 0;
}

std::vector<size_t> joint_mapping(const std::vector<Bone> &source, const std::vector<Bone> &target, bool check_bind)
{
	const auto &a = source;
	const auto &b = target;
	if (a.size() != b.size())
		throw ResourceLoadError("Asset skeleton has incompatible joint count");
	std::map<std::string, size_t> names;
	for (size_t i = 0; i < b.size(); ++i)
		if (b[i].name.empty() || !names.emplace(b[i].name, i).second)
			throw ResourceLoadError("Target skeleton needs unique named joints");
	std::set<std::string> seen;
	std::vector<size_t> mapping;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].name.empty() || !seen.insert(a[i].name).second || !names.contains(a[i].name))
			throw ResourceLoadError("Asset skeleton needs compatible named joints");
		const size_t index = names.at(a[i].name);
		const auto parent_name = [](const std::vector<Bone> &bones, const Bone &bone) -> std::string_view {
			if (bone.parent_node == Bone::NO_PARENT)
				return {};
			if (bone.parent_node >= bones.size())
				throw ResourceLoadError("Invalid skeleton parent index");
			return bones[bone.parent_node].name;
		};
		if (parent_name(a, a[i]) != parent_name(b, b[index]))
			throw ResourceLoadError("Asset skeleton has incompatible hierarchy");
		if (check_bind)
			for (int col = 0; col < 4; ++col)
				for (int row = 0; row < 4; ++row)
					if (!std::isfinite(source[i].inverse_bind_pose.get_mat4()[col][row]) ||
					    !std::isfinite(target[index].inverse_bind_pose.get_mat4()[col][row]) ||
					    std::abs(source[i].inverse_bind_pose.get_mat4()[col][row] -
					             target[index].inverse_bind_pose.get_mat4()[col][row]) > 0.001f)
						throw ResourceLoadError("Asset skeleton has incompatible inverse bind poses");
		mapping.push_back(index);
	}
	return mapping;
}
} // namespace

AssetRegistry::AssetRegistry() : state(std::make_unique<State>()) {}
AssetRegistry::~AssetRegistry() = default;
std::string_view asset_kind_name(AssetKind kind)
{
	switch (kind)
	{
	case AssetKind::Mesh:
		return "mesh";
	case AssetKind::Material:
		return "material";
	case AssetKind::Texture:
		return "texture";
	case AssetKind::Skeleton:
		return "skeleton";
	case AssetKind::Animation:
		return "animation";
	case AssetKind::Assembly:
		return "assembly";
	}
	throw ResourceLoadError("Invalid asset kind");
}
std::string_view asset_handle_name(const std::shared_ptr<AssetEntry> &entry)
{
	if (!entry)
		throw ResourceLoadError("Empty asset handle");
	return entry->definition.name;
}

std::shared_ptr<AssetEntry> AssetRegistry::find_entry(std::string_view name, AssetKind kind) const
{
	const std::lock_guard lock(state->mutex);
	auto it = state->names.find(name);
	if (it == state->names.end())
		throw ResourceLoadError("Asset is not registered: " + std::string(name));
	if (it->second->definition.kind != kind)
		throw ResourceLoadError("Asset has wrong type: " + std::string(name));
	return it->second;
}

bool AssetRegistry::contains(std::string_view name) const
{
	const std::lock_guard lock(state->mutex);
	return state->names.contains(name);
}

std::vector<Asset> AssetRegistry::registered_assets() const
{
	const std::lock_guard lock(state->mutex);
	std::vector<Asset> result;
	for (const auto &e : state->entries)
		switch (e->definition.kind)
		{
		case AssetKind::Mesh:
			result.push_back(make_descriptor(e, MeshAssetHandle(e)));
			break;
		case AssetKind::Material:
			result.push_back(make_descriptor(e, MaterialAssetHandle(e)));
			break;
		case AssetKind::Texture:
			result.push_back(make_descriptor(e, TextureAssetHandle(e)));
			break;
		case AssetKind::Skeleton:
			result.push_back(make_descriptor(e, SkeletonAssetHandle(e)));
			break;
		case AssetKind::Animation:
			result.push_back(make_descriptor(e, AnimationAssetHandle(e)));
			break;
		case AssetKind::Assembly:
			result.push_back(make_descriptor(e, AssemblyAssetHandle(e)));
			break;
		}
	return result;
}


AssemblyAssetHandle AssetRegistry::default_assembly(std::span<const Asset> assets)
{
	AssemblyAssetHandle result;
	for (const auto &asset : assets)
		if (const auto *assembly = std::get_if<AssemblyAsset>(&asset))
		{
			if (!assembly->handle)
				throw ResourceLoadError("Empty assembly asset handle");
			const auto &entry = *assembly->handle.entry;
			const auto &metadata = entry.source->documents.at(entry.definition.member).metadata;
			if (assembly->source.scene == metadata.value("scene", 0))
			{
				if (result)
					throw ResourceLoadError(
						"Multiple documents have default assemblies; select an assembly explicitly");
				result = assembly->handle;
			}
		}
	if (!result)
		throw ResourceLoadError("Asset source contains no default assembly");
	return result;
}

std::vector<AssetExport>
AssetRegistry::describe_source(std::string_view source,
                               const std::function<std::string(AssetKind, const AssetSelector &)> &name)
{
	Json metadata;
	try
	{
		metadata = read_metadata(Utility::get_resource(source));
	} catch (const Json::exception &error)
	{
		throw ResourceLoadError("Invalid asset metadata: " + std::string(error.what()));
	} catch (const std::runtime_error &error)
	{
		throw ResourceLoadError(error.what());
	}
	std::vector<AssetExport> result;
	for (const auto &[kind, selector] : selectors(metadata))
		result.push_back({name(kind, selector), kind, {}, selector});
	return result;
}

std::vector<Asset> AssetRegistry::load_asset(std::string_view path, const AssetLoadOptions &options)
{
	const std::lock_guard lock(state->mutex);
	try
	{
		if (options.import.target_skeleton || options.import.scene_index)
			throw ResourceLoadError(
				"Registration uses export scene selectors; target skeletons belong to instantiation");
		if (const auto existing = state->sources.find(path); existing != state->sources.end())
		{
			const auto &original = existing->second->options;
			if (original.name != options.name ||
			    registration_json(original, original.exports) != registration_json(options, options.exports))
				throw ResourceLoadError("Asset source is already registered with different names or options");
			if (options.eager)
				for (const auto &entry : state->entries)
					if (entry->source == existing->second)
						materialize(entry);
			auto result = registered_assets();
			std::erase_if(result, [&](const Asset &a) {
				return std::visit([&](const auto &v) { return v.handle.entry->source != existing->second; }, a);
			});
			return result;
		}
		auto source = std::make_shared<AssetSource>();
		source->path = std::string(path);
		source->resolved = Utility::get_resource(path);
		source->options = options;
		auto exports = options.exports;
		if (source->resolved.extension() == ".kasset")
		{
			if (!options.name.empty() || !exports.empty())
				throw ResourceLoadError("Package names are authored in its index");
			source->package = KassetContainer::open(source->resolved);
			for (const auto &e : source->package->index().at("exports"))
				exports.push_back({e.at("name"), parse_kind(e.at("kind").get<std::string>()), e.at("file"),
				                   parse_selector(e.value("selector", Json::object()))});
		}
		else if (!is_document(path))
		{
			const auto ext = source->resolved.extension();
			if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".dds")
				throw ResourceLoadError("Unsupported asset source format");
			if (!exports.empty())
				throw ResourceLoadError("Loose texture accepts a single optional name");
			exports.push_back({options.name, AssetKind::Texture, {}, {}});
		}
		else
		{
			if (!options.name.empty())
				throw ResourceLoadError("Document aliases use export selectors");
			const auto metadata = read_metadata(source->resolved);
			for (const auto &[kind, selector] : selectors(metadata))
				if (std::none_of(exports.begin(), exports.end(), [&](const auto &e) {
						return e.kind == kind && e.selector == selector && e.member.empty();
					}))
					exports.push_back({{}, kind, {}, selector});
			source->documents[{}].metadata = metadata;
		}
		if (exports.empty())
			throw ResourceLoadError("Asset source has no exports");
		std::set<std::string> names;
		std::vector<std::shared_ptr<AssetEntry>> entries;
		for (auto e : exports)
		{
			if (!source->package && !e.member.empty())
				throw ResourceLoadError("Loose assets cannot select package members");
			if (source->package && e.name.empty())
				throw ResourceLoadError("Package exports require explicit names");
			if (!e.name.empty() && !names.insert(e.name).second)
				throw ResourceLoadError("Duplicate asset name: " + e.name);
			auto [it, inserted] = source->documents.try_emplace(e.member);
			const auto file = source->package ? e.member : source->path;
			if (!is_document(file))
			{
				const auto ext = std::filesystem::path(file).extension();
				if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".dds")
					throw ResourceLoadError("Unsupported exported image format: " + file);
			}
			if (it->second.metadata.is_null() && is_document(file))
				it->second.metadata =
					source->package ? read_metadata(*source->package, e.member) : read_metadata(source->resolved);
			if (inserted && source->package && is_document(file))
				for (const auto *category : {"buffers", "images"})
					for (const auto &dependency : it->second.metadata.value(category, Json::array()))
						if (dependency.contains("uri"))
						{
							const auto uri = dependency.at("uri").get<std::string>();
							if (!uri.starts_with("data:"))
								source->package->read_prefix(KassetContainer::resolve(e.member, uri), 0);
						}
			const auto count = validate_export(e, it->second.metadata, is_document(file));
			entries.push_back(std::make_shared<AssetEntry>(AssetEntry{e, count, source}));
		}
		// Packages must name every exposed document part, including dependency images.
		for (const auto &[member, data] : source->documents)
			if (source->package && !data.metadata.is_null())
				for (const auto &[kind, selector] : selectors(data.metadata))
					if (std::none_of(exports.begin(), exports.end(), [&](const auto &e) {
							if (e.member != member || e.kind != kind)
								return false;
							return e.selector == selector;
						}))
						throw ResourceLoadError("Missing explicit " + kind_name(kind) + " export name in " +
						                        std::string(path));
		source->registration = std::make_shared<const std::string>(registration_json(options, exports).dump());

		for (const auto &e : entries)
			if (state->names.contains(e->definition.name))
				throw ResourceLoadError("Asset name already registered: " + e->definition.name);
		if (options.eager)
			for (const auto &e : entries)
				materialize(e);
		// Allocate only the new nodes before committing the registration.
		decltype(state->names) next_names;
		decltype(state->sources) next_sources;
		for (const auto &e : entries)
			if (!e->definition.name.empty())
				next_names.emplace(e->definition.name, e);
		next_sources.emplace(std::string(path), source);
		state->entries.reserve(state->entries.size() + entries.size());
		state->names.merge(next_names);
		state->sources.merge(next_sources);
		state->entries.insert(state->entries.end(), entries.begin(), entries.end());
		auto result = registered_assets();
		std::erase_if(result, [&](const Asset &a) {
			return std::visit([&](const auto &v) { return v.handle.entry->source != source; }, a);
		});
		return result;
	} catch (const Json::exception &e)
	{
		throw ResourceLoadError("Invalid asset metadata: " + std::string(e.what()));
	} catch (const ResourceLoadError &)
	{
		throw;
	} catch (const std::runtime_error &e)
	{
		throw ResourceLoadError(e.what());
	}
}

void AssetRegistry::unload_asset(std::string_view path)
{
	const std::lock_guard lock(state->mutex);
	auto it = state->sources.find(path);
	if (it == state->sources.end())
		throw ResourceLoadError("Asset source is not registered: " + std::string(path));
	auto source = it->second;
	std::erase_if(state->names, [&](const auto &pair) { return pair.second->source == source; });
	std::erase_if(state->entries, [&](const auto &entry) { return entry->source == source; });
	state->sources.erase(it);
}

MaterialHandle AssetRegistry::acquire_texture(MaterialSystem &store, const TextureAssetHandle &handle,
                                              ETextureSemantic semantic)
{
	if (!handle)
		throw ResourceLoadError("Empty texture asset handle");
	const std::lock_guard lock(handle.entry->source->mutex);
	auto owner = texture_data(handle.entry, semantic);
	auto result = store.add_shared(owner->shared_content());
	auto p = selected_provenance(*handle.entry);
	p.kind = EExternalResourceKind::Texture;
	p.texture_semantic = static_cast<int>(semantic);
	ResourceProvenance::register_material(result->get_id(), p);
	return result;
}

MeshHandle AssetRegistry::acquire_mesh(MeshSystem &store, const MeshAssetHandle &handle)
{
	if (!handle)
		throw ResourceLoadError("Empty mesh asset handle");
	const std::lock_guard lock(handle.entry->source->mutex);
	auto owner = mesh_data(handle.entry);
	auto result = store.add_shared(owner->shared_content());
	ResourceProvenance::register_mesh(result->get_id(), selected_provenance(*handle.entry));
	return result;
}

std::vector<MaterialHandle> AssetRegistry::acquire_material(MaterialSystem &store, const MaterialAssetHandle &handle)
{
	if (!handle)
		throw ResourceLoadError("Empty material asset handle");
	const std::lock_guard lock(handle.entry->source->mutex);
	std::vector<MaterialHandle> result;
	for (const auto &owner : material_data(handle.entry))
		result.push_back(store.add_shared(owner->shared_content()));
	if (!result.empty())
		ResourceProvenance::register_material(result.front()->get_id(), selected_provenance(*handle.entry));
	for (const auto &[id, image] :
	     handle.entry->source->documents.at(handle.entry->definition.member)
	         .material_decoder.material_image_sources(handle.entry->definition.selector.material))
	{
		const auto &texture = dynamic_cast<const TextureMaterial &>(store.get(id));
		auto p = provenance(*handle.entry, {.image = image});
		p.kind = EExternalResourceKind::Texture;
		p.texture_semantic = static_cast<int>(texture.semantic);
		ResourceProvenance::register_material(id, p);
	}
	return result;
}

SkeletonID AssetRegistry::instantiate_skeleton(ECS &ecs, const SkeletonAssetHandle &handle)
{
	if (!handle)
		throw ResourceLoadError("Empty skeleton asset handle");
	const std::lock_guard lock(handle.entry->source->mutex);
	auto id = ecs.add_skeleton(skeleton_data(handle.entry));
	ResourceProvenance::register_skeleton(id, selected_provenance(*handle.entry));
	return id;
}

AnimationID AssetRegistry::bind_animation(ECS &ecs, const AnimationAssetHandle &handle, SkeletonID skeleton)
{
	if (!handle)
		throw ResourceLoadError("Empty animation asset handle");
	const std::lock_guard lock(handle.entry->source->mutex);
	const auto &clip = animation_data(handle.entry);
	const auto &source = skeleton_data(handle.entry);
	const auto &target = ecs.get_skeletal_component(skeleton).get_bones();
	auto mapping = joint_mapping(source, target, false);
	std::vector<BoneAnimation> tracks(target.size());
	for (size_t i = 0; i < mapping.size(); ++i)
		tracks[mapping[i]] = clip.bone_animations.at(i);
	const auto p = selected_provenance(*handle.entry);
	std::vector<std::string> joint_order;
	for (const auto &bone : target)
		joint_order.push_back(bone.name);
	for (const auto &[id, animation] : ecs.get_skeletal_animations())
		if (animation.joint_order == joint_order)
			if (const auto *existing = ResourceProvenance::animation(id))
				if (existing->source == p.source && existing->member == p.member && same_registration(*existing, p) &&
				    existing->skin == p.skin && existing->animation == p.animation)
					return id;
	auto id = ecs.add_skeletal_animation(clip.name, std::move(tracks), make_skeletal_rig_signature(target),
	                                     handle.entry->source->path, std::move(joint_order));
	ResourceProvenance::register_animation(id, p);
	return id;
}

ResourceLoader::LoadedModel AssetRegistry::instantiate_model(ECS &ecs, const AssemblyAssetHandle &handle,
                                                             std::optional<SkeletonID> target)
{
	if (!handle)
		throw ResourceLoadError("Empty assembly asset handle");
	const std::lock_guard lock(handle.entry->source->mutex);
	auto &decoded = decoded_model(handle.entry);
	ResourceLoader::LoadedModel result = decoded.model;
	std::optional<ResourceLoader::DecodedModel> remapped;
	if (target)
	{
		const auto &bones = ecs.get_skeletal_component(*target).get_bones();
		bool reorder = false;
		for (const auto &[skin, source] : decoded.skins)
		{
			auto mapping = joint_mapping(source, bones, true);
			for (size_t i = 0; i < mapping.size(); ++i)
				reorder = reorder || mapping[i] != i;
		}
		if (reorder)
		{
			auto options = handle.entry->source->options.import;
			options.scene_index = handle.entry->definition.selector.scene;
			options.target_skeleton = target;
			remapped = ResourceLoader::decode_model(
				handle.entry->source->document(handle.entry->definition.member), handle.entry->source->path, options,
				handle.entry->source->meshes, handle.entry->source->materials, &bones,
				&handle.entry->source->documents.at(handle.entry->definition.member).material_decoder);
			result = remapped->model;
		}
	}
	const auto &actual = remapped ? *remapped : decoded;
	for (auto &mesh : result.meshes)
	{
		if (mesh.source_skin >= 0)
		{
			mesh.skeleton_id = target ? *target : ecs.add_skeleton(actual.skins.at(mesh.source_skin));
			auto p = provenance(
				*handle.entry,
				{.scene = handle.entry->definition.selector.scene, .node = mesh.source_node, .skin = mesh.source_skin});
			if (!target)
				ResourceProvenance::register_skeleton(*mesh.skeleton_id, p);
		}
		for (auto &renderable : mesh.renderables)
		{
			const auto mesh_id = renderable.mesh_owner->get_id();
			renderable.mesh_owner = ecs.get_mesh_system().add_shared(renderable.mesh_owner->shared_content());
			ResourceProvenance::register_mesh(mesh_id, provenance(*handle.entry, actual.mesh_provenance.at(mesh_id)));
			for (auto &owner : renderable.material_owners)
			{
				const auto id = owner->get_id();
				owner = ecs.get_material_system().add_shared(owner->shared_content());
				if (actual.material_provenance.contains(id))
					ResourceProvenance::register_material(id,
					                                      provenance(*handle.entry, actual.material_provenance.at(id)));
			}
		}
	}
	return result;
}

std::vector<Asset> AssetRegistry::restore_registration(const ImportedResourceProvenance &p)
{
	const std::lock_guard lock(state->mutex);
	if (const auto existing = state->sources.find(p.source); existing != state->sources.end())
	{
		if (existing->second->registration != p.registration && *existing->second->registration != registration_text(p))
			throw ResourceLoadError("Saved source registration disagrees with the active registry");
		auto result = registered_assets();
		std::erase_if(result, [&](const Asset &a) {
			return std::visit([&](const auto &v) { return v.handle.entry->source != existing->second; }, a);
		});
		return result;
	}
	if (registration_text(p).empty())
		throw ResourceLoadError("Imported resource has no asset registration descriptor");
	auto options = parse_registration(registration_text(p));
	if (!p.member.empty())
		options.exports.clear(); // Package owns its export names.
	else if (!is_document(p.source))
	{
		options.name = options.exports.at(0).name;
		options.exports.clear();
	}
	else
		std::erase_if(options.exports, [](const auto &e) { return e.name.empty(); });
	auto result = load_asset(p.source, options);
	if (registration_text(p) != *state->sources.at(p.source)->registration)
		throw ResourceLoadError("Saved source registration disagrees with its source metadata");
	return result;
}

ResourceLoader::LoadedModel AssetRegistry::restore_model(ECS &ecs, const ImportedResourceProvenance &p)
{
	auto values = restore_registration(p);
	if (p.scene < 0 && p.node < 0)
		for (const auto &a : values)
		{
			ResourceLoader::LoadedModel result;
			if (const auto *material = std::get_if<MaterialAsset>(&a))
				if (material->source.material == p.material && material->handle.entry->definition.member == p.member)
				{
					result.retained_materials = acquire_material(ecs.get_material_system(), material->handle);
					return result;
				}
			if (const auto *skeleton = std::get_if<SkeletonAsset>(&a))
				if (skeleton->source.skin == p.skin && skeleton->handle.entry->definition.member == p.member)
				{
					instantiate_skeleton(ecs, skeleton->handle);
					return result;
				}
		}
	for (const auto &a : values)
		if (const auto *model = std::get_if<AssemblyAsset>(&a))
			if (model->source.scene == p.scene || (p.scene < 0 && model->source.scene == 0))
			{
				auto entry = model->handle.entry;
				if (entry->definition.member == p.member)
					return instantiate_model(ecs, model->handle);
			}
	throw ResourceLoadError("Saved assembly selector is not registered");
}

MaterialHandle AssetRegistry::restore_texture(MaterialSystem &store, const ImportedResourceProvenance &p)
{
	auto values = restore_registration(p);
	for (const auto &a : values)
		if (const auto *texture = std::get_if<TextureAsset>(&a))
			if (texture->source.image == p.image && texture->handle.entry->definition.member == p.member)
				return acquire_texture(store, texture->handle, static_cast<ETextureSemantic>(p.texture_semantic));
	throw ResourceLoadError("Saved texture selector is not registered");
}

ResourceLoader::LoadedAnimations AssetRegistry::restore_animations(ECS &ecs, const ImportedResourceProvenance &p,
                                                                   SkeletonID skeleton)
{
	auto values = restore_registration(p);
	ResourceLoader::LoadedAnimations result;
	for (const auto &a : values)
		if (const auto *clip = std::get_if<AnimationAsset>(&a))
			if (clip->source.animation == p.animation && clip->source.skin == p.skin &&
			    clip->handle.entry->definition.member == p.member)
				result.animations.push_back(bind_animation(ecs, clip->handle, skeleton));
	if (result.animations.empty())
		throw ResourceLoadError("Saved animation selector is not registered");
	return result;
}
