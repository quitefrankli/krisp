#include <resource_loader/asset_registry.hpp>
#include <entity_component_system/ecs.hpp>
#include <utility.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <atomic>

namespace
{
AssetLoadOptions named_document(std::string_view file)
{
	AssetLoadOptions options;
	options.exports = AssetRegistry::describe_source(file, [](AssetKind kind, const AssetSelector &s) {
		return "sample/" + std::to_string(static_cast<int>(kind)) + "/" + std::to_string(s.scene) + "/" +
		       std::to_string(s.node) + "/" + std::to_string(s.primitive) + "/" + std::to_string(s.material) + "/" +
		       std::to_string(s.image) + "/" + std::to_string(s.skin) + "/" + std::to_string(s.animation);
	});
	return options;
}

class InvalidImage
{
public:
	InvalidImage()
	{
		static std::atomic<unsigned> sequence = 0;
		name = "registry_invalid_" + std::to_string(sequence++) + ".png";
		path = Utility::get_test_data_path() / name;
		std::ofstream(path, std::ios::binary) << "invalid image bytes";
	}
	~InvalidImage()
	{
		std::error_code ignored;
		std::filesystem::remove(path, ignored);
	}
	std::string name;
	std::filesystem::path path;
};
} // namespace

TEST(AssetRegistry, LazyRegistrationDefersDecodeAndEagerFailurePublishesNothing)
{
	InvalidImage image;
	AssetRegistry registry;
	AssetLoadOptions options{.name = "surface"};
	auto assets = registry.load_asset(image.name, options);
	ASSERT_EQ(assets.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<TextureAsset>(assets.front()));
	MaterialSystem materials;
	EXPECT_THROW(registry.acquire_texture(materials, "surface"), ResourceLoadError);
	registry.unload_asset(image.name);
	options.eager = true;
	EXPECT_THROW(registry.load_asset(image.name, options), ResourceLoadError);
	EXPECT_FALSE(registry.contains("surface"));
}

TEST(AssetRegistry, NameLookupsAreTypedAndNeverLoadFiles)
{
	InvalidImage image;
	AssetRegistry registry;
	registry.load_asset(image.name, {.name = "surface"});
	EXPECT_THROW(registry.find<AssetKind::Mesh>("surface"), ResourceLoadError);
	EXPECT_THROW(registry.find<AssetKind::Texture>(image.name), ResourceLoadError);
	EXPECT_EQ(registry.find<AssetKind::Texture>("surface").name(), "surface");
	std::filesystem::remove(image.path);
	EXPECT_NO_THROW(registry.load_asset(image.name, {.name = "surface"}));
}

TEST(AssetRegistry, LooseDocumentsAllowPartialNamesAndCollisionsAreAtomic)
{
	auto options = named_document("simple_test_model.gltf");
	ASSERT_GT(options.exports.size(), 1u);
	auto incomplete = options;
	incomplete.exports.pop_back();
	AssetRegistry partially_named;
	const auto partial_assets = partially_named.load_asset("simple_test_model.gltf", incomplete);
	EXPECT_EQ(partial_assets.size(), options.exports.size());
	EXPECT_TRUE(std::ranges::any_of(partial_assets, [](const Asset &asset) {
		return std::visit([](const auto &descriptor) { return descriptor.handle.name().empty(); }, asset);
	}));
	EXPECT_THROW(partially_named.find<AssetKind::Assembly>("simple_test_model.gltf"), ResourceLoadError);

	AssetRegistry registry;
	const auto assets = registry.load_asset("simple_test_model.gltf", options);
	EXPECT_EQ(assets.size(), options.exports.size());
	EXPECT_EQ(registry.load_asset("simple_test_model.gltf", options).size(), assets.size());
	auto conflicting = options;
	conflicting.exports.front().name = "other";
	EXPECT_THROW(registry.load_asset("simple_test_model.gltf", conflicting), ResourceLoadError);
	EXPECT_EQ(registry.registered_assets().size(), assets.size());
}

TEST(AssetRegistry, LooseSourcesReturnUsableDescriptorsAndDefaultAssembly)
{
	AssetRegistry registry;
	const auto model_assets = registry.load_asset("simple_test_model.gltf");
	const auto assembly = AssetRegistry::default_assembly(model_assets);
	ASSERT_TRUE(assembly);
	EXPECT_TRUE(assembly.name().empty());
	ECS ecs;
	const auto loaded = registry.instantiate_model(ecs, assembly);
	ASSERT_FALSE(loaded.meshes.empty());

	const auto texture_assets = registry.load_asset("texture.jpg");
	ASSERT_EQ(texture_assets.size(), 1u);
	const auto *texture = std::get_if<TextureAsset>(&texture_assets.front());
	ASSERT_NE(texture, nullptr);
	ASSERT_TRUE(texture->handle);
	EXPECT_TRUE(texture->handle.name().empty());
	const auto material = registry.acquire_texture(ecs.get_material_system(), texture->handle);
	EXPECT_TRUE(material);
}

TEST(AssetRegistry, AssemblyInstancesShareGeometryAndHaveIndependentPoses)
{
	ECS ecs;
	auto assets = ecs.assets().load_asset("simple_test_model.gltf", named_document("simple_test_model.gltf"));
	AssemblyAssetHandle assembly;
	for (const auto &asset : assets)
		if (const auto *a = std::get_if<AssemblyAsset>(&asset))
			assembly = a->handle;
	ASSERT_TRUE(assembly);
	auto a = ecs.assets().instantiate_model(ecs, assembly);
	auto b = ecs.assets().instantiate_model(ecs, assembly);
	ASSERT_FALSE(a.meshes.empty());
	ASSERT_TRUE(a.meshes.front().skeleton_id);
	ASSERT_TRUE(b.meshes.front().skeleton_id);
	EXPECT_NE(a.meshes.front().skeleton_id, b.meshes.front().skeleton_id);
	EXPECT_EQ(a.meshes.front().renderables.front().mesh_owner, b.meshes.front().renderables.front().mesh_owner);
	auto &pose = ecs.get_skeletal_component(*a.meshes.front().skeleton_id).get_bone_local_transform(0);
	pose.set_pos({3, 4, 5});
	EXPECT_NE(pose.get_pos(),
	          ecs.get_skeletal_component(*b.meshes.front().skeleton_id).get_bone_local_transform(0).get_pos());
	ecs.assets().unload_asset("simple_test_model.gltf");
	EXPECT_FALSE(ecs.assets().contains(assembly.name()));
	EXPECT_NO_THROW(ecs.assets().instantiate_model(ecs, assembly));
}

TEST(AssetRegistry, SceneResetRetainsRegistrationsAndDecodedGeometry)
{
	ECS ecs;
	const auto assets = ecs.assets().load_asset("simple_test_model.gltf", named_document("simple_test_model.gltf"));
	AssemblyAssetHandle assembly;
	for (const auto &asset : assets)
		if (const auto *a = std::get_if<AssemblyAsset>(&asset))
			assembly = a->handle;
	auto a = ecs.assets().instantiate_model(ecs, assembly);
	const auto id = a.meshes.front().renderables.front().get_mesh_id();
	ecs.reset_preserving_transient_transformations();
	EXPECT_TRUE(ecs.assets().contains(assembly.name()));
	auto b = ecs.assets().instantiate_model(ecs, assembly);
	EXPECT_EQ(id, b.meshes.front().renderables.front().get_mesh_id());
}

TEST(AssetRegistry, DirectNamedMaterialAndSkeletonRestoreTheirSavedSelectors)
{
	ECS ecs;
	auto assets = ecs.assets().load_asset("simple_test_model.gltf", named_document("simple_test_model.gltf"));
	MaterialAssetHandle material;
	SkeletonAssetHandle skeleton;
	for (const auto &asset : assets)
	{
		if (const auto *entry = std::get_if<MaterialAsset>(&asset))
			material = entry->handle;
		if (const auto *entry = std::get_if<SkeletonAsset>(&asset))
			skeleton = entry->handle;
	}
	ASSERT_TRUE(material);
	ASSERT_TRUE(skeleton);

	auto material_owners = ecs.assets().acquire_material(ecs.get_material_system(), material);
	ASSERT_FALSE(material_owners.empty());
	const auto *material_source = ResourceProvenance::material(material_owners.front()->get_id());
	ASSERT_NE(material_source, nullptr);
	const auto saved_material_source = *material_source;
	const auto old_skeleton = ecs.assets().instantiate_skeleton(ecs, skeleton);
	const auto *skeleton_source = ResourceProvenance::skeleton(old_skeleton);
	ASSERT_NE(skeleton_source, nullptr);
	const auto saved_skeleton_source = *skeleton_source;

	ResourceProvenance::erase_material(material_owners.front()->get_id());
	ResourceProvenance::erase_skeleton(old_skeleton);
	ecs.assets().unload_asset("simple_test_model.gltf");

	const auto restored_material = ecs.assets().restore_model(ecs, saved_material_source);
	ASSERT_EQ(restored_material.retained_materials.size(), material_owners.size());
	EXPECT_TRUE(ecs.get_material_system().owns(restored_material.retained_materials.front()));
	EXPECT_NE(ResourceProvenance::material(restored_material.retained_materials.front()->get_id()), nullptr);

	const auto restored_skeleton = ecs.assets().restore_model(ecs, saved_skeleton_source);
	EXPECT_TRUE(restored_skeleton.meshes.empty());
	const auto restored_skeleton_id = ResourceProvenance::find_skeleton(saved_skeleton_source);
	ASSERT_TRUE(restored_skeleton_id);
	EXPECT_NE(*restored_skeleton_id, old_skeleton);
}
