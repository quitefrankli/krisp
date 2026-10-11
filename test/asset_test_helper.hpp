#pragma once

#include <resource_loader/asset_registry.hpp>
#include <entity_component_system/ecs.hpp>
#include <utility.hpp>

#include <tiny_gltf.h>

#include <fmt/format.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

namespace TestAssets
{
template<typename Action>
auto resource_errors_as_load_errors(Action&& action)
{
	try
	{
		return action();
	}
	catch (const ResourceLoadError&)
	{
		throw;
	}
	catch (const std::runtime_error& error)
	{
		throw ResourceLoadError(error.what());
	}
}

inline std::string kind_name(const AssetKind kind)
{
	switch (kind)
	{
		case AssetKind::Mesh: return "mesh";
		case AssetKind::Material: return "material";
		case AssetKind::Texture: return "texture";
		case AssetKind::Skeleton: return "skeleton";
		case AssetKind::Animation: return "animation";
		case AssetKind::Assembly: return "assembly";
	}
	throw std::logic_error("unknown asset kind");
}

inline std::string export_name(const std::string_view source, const AssetKind kind, const AssetSelector& selector)
{
	return fmt::format("test asset {} {} {} {} {} {} {} {} {}",
		source, kind_name(kind), selector.scene, selector.node, selector.primitive,
		selector.material, selector.image, selector.skin, selector.animation);
}

inline std::vector<AssetExport> exports_for(const std::string_view source)
{
	return AssetRegistry::describe_source(source, [source](const AssetKind kind, const AssetSelector& selector) {
		return export_name(source, kind, selector);
	});
}

inline ResourceLoader::LoadOptions default_import_options()
{
	ResourceLoader::LoadOptions result;
	result.generate_missing_tangents = true;
	return result;
}

inline std::vector<Asset> register_document(
	AssetRegistry& registry,
	const std::string_view source,
	ResourceLoader::LoadOptions import = default_import_options())
{
	return resource_errors_as_load_errors([&] {
		import.scene_index.reset();
		import.target_skeleton.reset();
		AssetLoadOptions options;
		options.import = import;
		return registry.load_asset(source, options);
	});
}

inline ResourceLoader::LoadedModel load_model_document(
	ECS& ecs,
	std::string_view source,
	const ResourceLoader::LoadOptions& options);

inline ResourceLoader::LoadedModel load_model(
	ECS& ecs,
	const std::string_view source,
	std::optional<ResourceLoader::LoadOptions> options = std::nullopt)
{
	return resource_errors_as_load_errors([&] {
		auto import = options.value_or(default_import_options());
		const auto target_skeleton = import.target_skeleton;
		const auto selected_scene = import.scene_index;
		import.scene_index.reset();
		import.target_skeleton.reset();
		const bool custom_import = import.strict
			|| !import.generate_missing_normals
			|| !import.allow_non_triangle_primitives
			|| (!import.generate_missing_tangents && !target_skeleton && !selected_scene);
		if (custom_import)
			return load_model_document(ecs, source, ResourceLoader::LoadOptions{
				.scene_index = selected_scene,
				.target_skeleton = target_skeleton,
				.generate_missing_normals = import.generate_missing_normals,
				.generate_missing_tangents = import.generate_missing_tangents,
				.allow_non_triangle_primitives = import.allow_non_triangle_primitives,
				.strict = import.strict,
			});
		const auto assets = register_document(ecs.assets(), source, default_import_options());
		AssemblyAssetHandle assembly;
		if (selected_scene)
		{
			const auto found = std::ranges::find_if(assets, [scene = *selected_scene](const Asset& asset) {
				const auto* value = std::get_if<AssemblyAsset>(&asset);
				return value && value->source.scene == scene;
			});
			if (found != assets.end())
				assembly = std::get<AssemblyAsset>(*found).handle;
		}
		else
			assembly = AssetRegistry::default_assembly(assets);
		if (!assembly)
			throw ResourceLoadError("Test asset has no assembly for the selected scene");
		return ecs.assets().instantiate_model(ecs, assembly, target_skeleton);
	});
}

inline ResourceLoader::LoadedModel load_model_document(
	ECS& ecs,
	const std::string_view source,
	const ResourceLoader::LoadOptions& options)
{
	return resource_errors_as_load_errors([&] {
		const auto document = ResourceLoader::read_document(Utility::get_resource(source));
		return ResourceLoader::load_model_document(ecs, document, source, options);
	});
}

inline ResourceLoader::LoadedAnimations load_animations(
	ECS& ecs,
	const std::string_view source,
	const SkeletonID target_skeleton)
{
	return resource_errors_as_load_errors([&] {
		auto assets = register_document(ecs.assets(), source);
		std::ranges::sort(assets, [](const Asset& left, const Asset& right) {
			const auto* l = std::get_if<AnimationAsset>(&left);
			const auto* r = std::get_if<AnimationAsset>(&right);
			if (l && r)
				return std::tie(l->source.animation, l->source.skin) <
				       std::tie(r->source.animation, r->source.skin);
			return l != nullptr;
		});
		ResourceLoader::LoadedAnimations result;
		for (const auto& asset : assets)
			if (const auto* animation = std::get_if<AnimationAsset>(&asset))
				result.animations.push_back(ecs.assets().bind_animation(ecs, animation->handle, target_skeleton));
		if (result.animations.empty())
			throw ResourceLoadError("Test animation asset contains no clips");
		return result;
	});
}

inline MaterialHandle fetch_texture(
	MaterialSystem& materials,
	const std::string_view source,
	const ETextureSemantic semantic = ETextureSemantic::BASE_COLOR)
{
	return resource_errors_as_load_errors([&] {
		AssetRegistry registry;
		AssetLoadOptions options;
		options.name = std::string(source);
		registry.load_asset(source, options);
		return registry.acquire_texture(materials, options.name, semantic);
	});
}

inline TextureAssetHandle register_texture(
	AssetRegistry& registry,
	const std::string_view source,
	const std::string_view name = {})
{
	AssetLoadOptions options;
	options.name = name.empty() ? std::string(source) : std::string(name);
	registry.load_asset(source, options);
	return registry.find<AssetKind::Texture>(options.name);
}
}
