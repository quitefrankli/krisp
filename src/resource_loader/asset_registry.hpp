#pragma once

#include "resource_loader.hpp"
#include "serialization/resource_provenance.hpp"

#include <array>
#include <memory>
#include <variant>

struct AssetEntry;
enum class AssetKind
{
	Mesh,
	Material,
	Texture,
	Skeleton,
	Animation,
	Assembly
};
std::string_view asset_kind_name(AssetKind);

// Source selectors use existing importer indices, never scene runtime IDs.
struct AssetSelector
{
	int scene = -1;
	int node = -1;
	int primitive = -1;
	int material = -1;
	int image = -1;
	int skin = -1;
	int animation = -1;
	auto operator<=>(const AssetSelector &) const = default;
};

struct AssetExport
{
	std::string name;
	AssetKind kind = AssetKind::Texture;
	std::string member;
	AssetSelector selector;
	auto operator<=>(const AssetExport &) const = default;
};

struct AssetLoadOptions
{
	// Optional aliases for loose sources. Package names come from its index.
	std::string name;
	std::vector<AssetExport> exports;
	ResourceLoader::LoadOptions import = [] {
		ResourceLoader::LoadOptions options;
		options.generate_missing_tangents = true;
		return options;
	}();
	bool eager = false;
};

template<AssetKind Kind> class AssetHandle
{
public:
	AssetHandle() = default;
	explicit operator bool() const { return static_cast<bool>(entry); }
	std::string_view name() const;
	bool operator==(const AssetHandle &) const = default;

private:
	explicit AssetHandle(std::shared_ptr<AssetEntry> value) : entry(std::move(value)) {}
	std::shared_ptr<AssetEntry> entry;
	friend class AssetRegistry;
};

using MeshAssetHandle = AssetHandle<AssetKind::Mesh>;
using MaterialAssetHandle = AssetHandle<AssetKind::Material>;
using TextureAssetHandle = AssetHandle<AssetKind::Texture>;
using SkeletonAssetHandle = AssetHandle<AssetKind::Skeleton>;
using AnimationAssetHandle = AssetHandle<AssetKind::Animation>;
using AssemblyAssetHandle = AssetHandle<AssetKind::Assembly>;

template<AssetKind Kind> struct AssetDescriptor
{
	AssetHandle<Kind> handle;
	AssetSelector source;
	// Counts available without decoding; e.g. vertices, bones, or node instances.
	size_t count = 0;
};

using MeshAsset = AssetDescriptor<AssetKind::Mesh>;
using MaterialAsset = AssetDescriptor<AssetKind::Material>;
using TextureAsset = AssetDescriptor<AssetKind::Texture>;
using SkeletonAsset = AssetDescriptor<AssetKind::Skeleton>;
using AnimationAsset = AssetDescriptor<AssetKind::Animation>;
using AssemblyAsset = AssetDescriptor<AssetKind::Assembly>;
using Asset = std::variant<MeshAsset, MaterialAsset, TextureAsset, SkeletonAsset, AnimationAsset, AssemblyAsset>;

// Session-wide names and lazy CPU data. Runtime realization uses existing stores.
// Realization and persistence operate on the owning ECS's game thread.
class AssetRegistry
{
public:
	AssetRegistry();
	~AssetRegistry();
	AssetRegistry(const AssetRegistry &) = delete;
	AssetRegistry &operator=(const AssetRegistry &) = delete;
	std::vector<Asset> load_asset(std::string_view source, const AssetLoadOptions &options = {});
	static AssemblyAssetHandle default_assembly(std::span<const Asset> assets);
	void unload_asset(std::string_view source);
	bool contains(std::string_view name) const;
	std::vector<Asset> registered_assets() const;

	template<AssetKind Kind> AssetHandle<Kind> find(std::string_view name) const
	{
		return AssetHandle<Kind>(find_entry(name, Kind));
	}
	MaterialHandle acquire_texture(MaterialSystem &, const TextureAssetHandle &,
	                               ETextureSemantic = ETextureSemantic::BASE_COLOR);
	MaterialHandle acquire_texture(MaterialSystem &store, std::string_view name,
	                               ETextureSemantic semantic = ETextureSemantic::BASE_COLOR)
	{
		return acquire_texture(store, find<AssetKind::Texture>(name), semantic);
	}
	MeshHandle acquire_mesh(MeshSystem &, const MeshAssetHandle &);
	MeshHandle acquire_mesh(MeshSystem &store, std::string_view name)
	{
		return acquire_mesh(store, find<AssetKind::Mesh>(name));
	}
	std::vector<MaterialHandle> acquire_material(MaterialSystem &, const MaterialAssetHandle &);
	std::vector<MaterialHandle> acquire_material(MaterialSystem &store, std::string_view name)
	{
		return acquire_material(store, find<AssetKind::Material>(name));
	}
	SkeletonID instantiate_skeleton(ECS &, const SkeletonAssetHandle &);
	SkeletonID instantiate_skeleton(ECS &ecs, std::string_view name)
	{
		return instantiate_skeleton(ecs, find<AssetKind::Skeleton>(name));
	}
	AnimationID bind_animation(ECS &, const AnimationAssetHandle &, SkeletonID);
	AnimationID bind_animation(ECS &ecs, std::string_view name, SkeletonID skeleton)
	{
		return bind_animation(ecs, find<AssetKind::Animation>(name), skeleton);
	}
	ResourceLoader::LoadedModel instantiate_model(ECS &, const AssemblyAssetHandle &,
	                                              std::optional<SkeletonID> target = {});
	ResourceLoader::LoadedModel instantiate_model(ECS &ecs, std::string_view name,
	                                              std::optional<SkeletonID> target = {})
	{
		return instantiate_model(ecs, find<AssetKind::Assembly>(name), target);
	}

	// Persistence explicitly registers sources before resolving their saved selectors.
	ResourceLoader::LoadedModel restore_model(ECS &, const ImportedResourceProvenance &);
	MaterialHandle restore_texture(MaterialSystem &, const ImportedResourceProvenance &);
	ResourceLoader::LoadedAnimations restore_animations(ECS &, const ImportedResourceProvenance &, SkeletonID);

	// File-picker naming helper: caller supplies every name through a callback.
	static std::vector<AssetExport>
	describe_source(std::string_view source,
	                const std::function<std::string(AssetKind, const AssetSelector &)> &author_name);

private:
	struct State;
	std::unique_ptr<State> state;
	std::shared_ptr<AssetEntry> find_entry(std::string_view, AssetKind) const;
	std::vector<Asset> restore_registration(const ImportedResourceProvenance &);
};

std::string_view asset_handle_name(const std::shared_ptr<AssetEntry> &);
template<AssetKind Kind> std::string_view AssetHandle<Kind>::name() const { return asset_handle_name(entry); }
