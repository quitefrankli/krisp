#include "resource_provenance.hpp"

#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"

namespace
{
std::unordered_map<MeshID, ImportedResourceProvenance> meshes;
std::unordered_map<MaterialID, ImportedResourceProvenance> materials;
std::unordered_map<SkeletonID, ImportedResourceProvenance> skeletons;
std::unordered_map<AnimationID, ImportedResourceProvenance> animations;

template<typename ID>
const ImportedResourceProvenance* find(const std::unordered_map<ID, ImportedResourceProvenance>& values, ID id)
{
	const auto it = values.find(id);
	return it == values.end() ? nullptr : &it->second;
}
}

void ResourceProvenance::register_mesh(MeshID id, ImportedResourceProvenance provenance) { meshes.insert_or_assign(id, std::move(provenance)); }
void ResourceProvenance::register_material(MaterialID id, ImportedResourceProvenance provenance) { materials.insert_or_assign(id, std::move(provenance)); }
void ResourceProvenance::register_skeleton(SkeletonID id, ImportedResourceProvenance provenance) { skeletons.insert_or_assign(id, std::move(provenance)); }
void ResourceProvenance::register_animation(AnimationID id, ImportedResourceProvenance provenance) { animations.insert_or_assign(id, std::move(provenance)); }
const ImportedResourceProvenance* ResourceProvenance::mesh(MeshID id) { return find(meshes, id); }
const ImportedResourceProvenance* ResourceProvenance::material(MaterialID id) { return find(materials, id); }
const ImportedResourceProvenance* ResourceProvenance::skeleton(SkeletonID id) { return find(skeletons, id); }
const ImportedResourceProvenance* ResourceProvenance::animation(AnimationID id) { return find(animations, id); }
std::optional<MeshID> ResourceProvenance::find_mesh(
	const MeshSystem& mesh_system, const ImportedResourceProvenance& provenance)
{
	for (const auto& [id, value] : meshes)
		if (mesh_system.contains(id)
			&& value.kind == provenance.kind && value.source == provenance.source && value.scene == provenance.scene
			&& value.node == provenance.node && value.primitive == provenance.primitive)
			return id;
	return std::nullopt;
}
std::optional<MaterialID> ResourceProvenance::find_material(
	const MaterialSystem& material_system, const ImportedResourceProvenance& provenance)
{
	for (const auto& [id, value] : materials)
		if (material_system.contains(id)
			&& value.kind == provenance.kind && value.source == provenance.source && value.scene == provenance.scene
			&& value.node == provenance.node && value.primitive == provenance.primitive
			&& value.material == provenance.material && value.texture == provenance.texture)
			return id;
	return std::nullopt;
}
std::optional<SkeletonID> ResourceProvenance::find_skeleton(const ImportedResourceProvenance& provenance)
{
	for (const auto& [id, value] : skeletons)
		if (value.kind == provenance.kind && value.source == provenance.source && value.scene == provenance.scene && value.node == provenance.node
			&& value.skin == provenance.skin)
			return id;
	return std::nullopt;
}
std::optional<AnimationID> ResourceProvenance::find_animation(const ImportedResourceProvenance& provenance)
{
	for (const auto& [id, value] : animations)
		if (value.kind == provenance.kind && value.source == provenance.source && value.skin == provenance.skin && value.animation == provenance.animation)
			return id;
	return std::nullopt;
}
void ResourceProvenance::erase_mesh(MeshID id) { meshes.erase(id); }
void ResourceProvenance::erase_material(MaterialID id) { materials.erase(id); }
void ResourceProvenance::erase_skeleton(SkeletonID id) { skeletons.erase(id); }
void ResourceProvenance::erase_animation(AnimationID id) { animations.erase(id); }
void ResourceProvenance::clear()
{
	meshes.clear();
	materials.clear();
	skeletons.clear();
	animations.clear();
}
