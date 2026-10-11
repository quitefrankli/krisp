#include <resource_loader/asset_registry.hpp>
#include <entity_component_system/ecs.hpp>
#include <serialization/resource_provenance.hpp>
#include <utility.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
enum class PackageExportMutation
{
	None,
	RemoveFirst,
	OmitFirstName,
	EmptyFirstName
};

void append_u32(std::vector<unsigned char>& bytes, const std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

void append_u64(std::vector<unsigned char>& bytes, const std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

std::vector<unsigned char> read_file(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream || stream.tellg() < 0)
		throw std::runtime_error("Unable to read package test input");
	const auto size = static_cast<std::size_t>(stream.tellg());
	std::vector<unsigned char> bytes(size);
	stream.seekg(0);
	if (size && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
		throw std::runtime_error("Unable to read package test input");
	return bytes;
}

class TemporaryPackage
{
public:
	TemporaryPackage(const std::map<std::string, std::vector<unsigned char>>& files,
	                const nlohmann::json& exports,
	                const PackageExportMutation mutation = PackageExportMutation::None)
	{
		static unsigned sequence = 0;
		const auto directory = Utility::get_test_data_path() / "kassets";
		std::filesystem::create_directories(directory);
		path = directory / ("asset_package_test_" + std::to_string(sequence++) + ".kasset");

		nlohmann::json index = {{"files", nlohmann::json::array()}, {"exports", exports}};
		if (!index["exports"].empty())
			switch (mutation)
			{
			case PackageExportMutation::None:
				break;
			case PackageExportMutation::RemoveFirst:
				index["exports"].erase(index["exports"].begin());
				break;
			case PackageExportMutation::OmitFirstName:
				index["exports"][0].erase("name");
				break;
			case PackageExportMutation::EmptyFirstName:
				index["exports"][0]["name"] = "";
				break;
			}
		std::uint64_t relative_offset = 0;
		std::vector<std::uint64_t> relative_offsets;
		for (const auto& [member, bytes] : files)
		{
			index["files"].push_back({{"path", member}, {"offset", relative_offset}, {"size", bytes.size()}});
			relative_offsets.push_back(relative_offset);
			relative_offset += bytes.size();
		}

		std::uint64_t base_offset = 24;
		std::string serialized;
		while (true)
		{
			for (std::size_t i = 0; i < relative_offsets.size(); ++i)
				index["files"][i]["offset"] = base_offset + relative_offsets[i];
			serialized = index.dump();
			const auto updated = 24 + serialized.size();
			if (updated == base_offset)
				break;
			base_offset = updated;
		}

		std::vector<unsigned char> archive{'K', 'A', 'S', 'S', 'E', 'T', 0, 0};
		append_u32(archive, 1);
		append_u32(archive, 0);
		append_u64(archive, serialized.size());
		archive.insert(archive.end(), serialized.begin(), serialized.end());
		for (const auto& [member, bytes] : files)
			archive.insert(archive.end(), bytes.begin(), bytes.end());
		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		stream.write(reinterpret_cast<const char*>(archive.data()), static_cast<std::streamsize>(archive.size()));
	}

	~TemporaryPackage()
	{
		std::error_code ignored;
		std::filesystem::remove(path, ignored);
	}

	std::filesystem::path path;
};

nlohmann::json selector_json(const AssetSelector& selector)
{
	return {{"scene", selector.scene}, {"node", selector.node}, {"primitive", selector.primitive},
	        {"material", selector.material}, {"image", selector.image}, {"skin", selector.skin},
	        {"animation", selector.animation}};
}

std::string export_name(const AssetKind kind, const AssetSelector& selector)
{
	return std::to_string(static_cast<int>(kind)) + "/" + std::to_string(selector.scene) + "/" +
	       std::to_string(selector.node) + "/" + std::to_string(selector.primitive) + "/" +
	       std::to_string(selector.material) + "/" + std::to_string(selector.image) + "/" +
	       std::to_string(selector.skin) + "/" + std::to_string(selector.animation);
}

struct PackageModelFixture
{
	explicit PackageModelFixture(
		const bool external_image = false,
		const PackageExportMutation mutation = PackageExportMutation::None)
	{
		const auto model_path = Utility::get_test_data_path() / "simple_test_model.gltf";
		const auto original = read_file(model_path);
		auto document = nlohmann::json::parse(original);
		document["images"][0].erase("bufferView");
		document["images"][0]["uri"] = external_image
			? "../../simple_test_model.gltf" : "../textures/model-image.jpg";
		const auto serialized = document.dump();
		std::vector<unsigned char> model(serialized.begin(), serialized.end());

		std::vector<AssetExport> parts = AssetRegistry::describe_source(
			"simple_test_model.gltf", [](const AssetKind kind, const AssetSelector& selector) {
				return export_name(kind, selector);
			});
		nlohmann::json exports = nlohmann::json::array();
		for (auto& part : parts)
		{
			part.member = "models/character.gltf";
			exports.push_back({{"name", part.name}, {"kind", asset_kind_name(part.kind)},
			                  {"file", part.member}, {"selector", selector_json(part.selector)}});
		}
		exports.push_back({{"name", "texture/standalone"}, {"kind", "texture"},
		                  {"file", "textures/standalone.jpg"}, {"selector", selector_json({})}});
		exports.push_back({{"name", "texture/broken"}, {"kind", "texture"},
		                  {"file", "textures/broken.jpg"}, {"selector", selector_json({})}});

		const auto texture = read_file(Utility::get_texture("texture.jpg"));
		std::map<std::string, std::vector<unsigned char>> files{
			{"models/character.gltf", std::move(model)},
			{"textures/broken.jpg", {'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e'}},
			{"textures/standalone.jpg", texture},
		};
		if (!external_image)
			files.emplace("textures/model-image.jpg", texture);
		package = std::make_unique<TemporaryPackage>(files, exports, mutation);
		source_name = "kassets/" + package->path.filename().string();
	}

	std::unique_ptr<TemporaryPackage> package;
	std::string source_name;
};
} // namespace

TEST(AssetPackage, RegistersNamedMixedExportsAndLoadsOnlyPackageDependencies)
{
	PackageModelFixture fixture;
	AssetRegistry registry;
	const auto assets = registry.load_asset(fixture.source_name);
	std::set<AssetKind> kinds;
	for (const auto& asset : assets)
		std::visit([&](const auto& value) {
			using Value = std::remove_cvref_t<decltype(value)>;
			if constexpr (std::is_same_v<Value, MeshAsset>) kinds.insert(AssetKind::Mesh);
			else if constexpr (std::is_same_v<Value, MaterialAsset>) kinds.insert(AssetKind::Material);
			else if constexpr (std::is_same_v<Value, TextureAsset>) kinds.insert(AssetKind::Texture);
			else if constexpr (std::is_same_v<Value, SkeletonAsset>) kinds.insert(AssetKind::Skeleton);
			else if constexpr (std::is_same_v<Value, AnimationAsset>) kinds.insert(AssetKind::Animation);
			else if constexpr (std::is_same_v<Value, AssemblyAsset>) kinds.insert(AssetKind::Assembly);
		}, asset);
	EXPECT_TRUE(registry.contains("texture/standalone"));
	EXPECT_TRUE(registry.contains("texture/broken"));
	EXPECT_TRUE(kinds.contains(AssetKind::Mesh));
	EXPECT_TRUE(kinds.contains(AssetKind::Material));
	EXPECT_TRUE(kinds.contains(AssetKind::Texture));
	EXPECT_TRUE(kinds.contains(AssetKind::Skeleton));
	EXPECT_TRUE(kinds.contains(AssetKind::Animation));
	EXPECT_TRUE(kinds.contains(AssetKind::Assembly));

	ECS ecs;
	const auto assembly = registry.find<AssetKind::Assembly>(export_name(AssetKind::Assembly, {.scene = 0}));
	const auto loaded = registry.instantiate_model(ecs, assembly);
	ASSERT_FALSE(loaded.meshes.empty());
	ASSERT_FALSE(loaded.meshes.front().renderables.empty());
	const auto mesh_id = loaded.meshes.front().renderables.front().get_mesh_id();
	const auto* provenance = ResourceProvenance::mesh(mesh_id);
	ASSERT_NE(provenance, nullptr);
	EXPECT_EQ(provenance->member, "models/character.gltf");
	ASSERT_TRUE(provenance->registration);
	EXPECT_FALSE(provenance->registration->empty());
	const auto skeleton = registry.instantiate_skeleton(
		ecs, export_name(AssetKind::Skeleton, {.skin = 0}));
	const auto* skeleton_source = ResourceProvenance::skeleton(skeleton);
	ASSERT_NE(skeleton_source, nullptr);
	EXPECT_EQ(skeleton_source->member, "models/character.gltf");
	const auto animation = registry.bind_animation(
		ecs, export_name(AssetKind::Animation, {.skin = 0, .animation = 0}), skeleton);
	const auto* animation_source = ResourceProvenance::animation(animation);
	ASSERT_NE(animation_source, nullptr);
	EXPECT_EQ(animation_source->member, "models/character.gltf");

	const auto standalone = registry.acquire_texture(ecs.get_material_system(), "texture/standalone");
	ASSERT_TRUE(standalone);
	const auto* texture_source = ResourceProvenance::material(standalone->get_id());
	ASSERT_NE(texture_source, nullptr);
	EXPECT_EQ(texture_source->member, "textures/standalone.jpg");

	EXPECT_THROW(registry.acquire_texture(ecs.get_material_system(), "texture/broken"), ResourceLoadError);

	AssetRegistry restored_registry;
	ECS restored_ecs;
	const auto restored = restored_registry.restore_model(restored_ecs, *provenance);
	ASSERT_FALSE(restored.meshes.empty());
	EXPECT_EQ(restored.meshes.front().renderables.size(), loaded.meshes.front().renderables.size());
}

TEST(AssetPackage, RejectsEscapingDependencyEvenWhenLooseFileExists)
{
	PackageModelFixture fixture(true);
	AssetRegistry registry;
	EXPECT_THROW(registry.load_asset(fixture.source_name), ResourceLoadError);
	EXPECT_TRUE(registry.registered_assets().empty());
}

TEST(AssetPackage, RequiresNamesForEveryExport)
{
	PackageModelFixture fixture(false, PackageExportMutation::OmitFirstName);
	AssetRegistry registry;
	EXPECT_THROW(registry.load_asset(fixture.source_name), ResourceLoadError);
	EXPECT_TRUE(registry.registered_assets().empty());
}

TEST(AssetPackage, RejectsEmptyExportNamesWithoutPublishingAnything)
{
	PackageModelFixture fixture(false, PackageExportMutation::EmptyFirstName);
	AssetRegistry registry;
	EXPECT_THROW(registry.load_asset(fixture.source_name), ResourceLoadError);
	EXPECT_TRUE(registry.registered_assets().empty());
}

TEST(AssetPackage, RejectsUnnamedDocumentPartsWithoutPublishingAnything)
{
	PackageModelFixture fixture(false, PackageExportMutation::RemoveFirst);
	AssetRegistry registry;
	EXPECT_THROW(registry.load_asset(fixture.source_name), ResourceLoadError);
	EXPECT_TRUE(registry.registered_assets().empty());
}
