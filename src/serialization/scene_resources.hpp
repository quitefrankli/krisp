#pragma once

#include "entity_component_system/material_system.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "resource_loader/resource_loader.hpp"
#include "serialization/resource_provenance.hpp"
#include "serialization/serializer.hpp"

#include <filesystem>
#include <unordered_map>
#include <vector>

class ECS;

// Serializes resources referenced by the scene. Resources with imported
// provenance remain references to their external model/image; resources that
// were created inside Krisp are written once to the save directory and
// referenced by a save-local ID from scene.yaml.
class SceneResourceWriter
{
public:
	SceneResourceWriter(Serializer &document, const ECS &ecs, std::filesystem::path directory);

	void write_mesh_reference(Serializer out, MeshID id);
	void write_material_reference(Serializer out, MaterialID id);
	void write_imported_source(Serializer out, const ImportedResourceProvenance& source);

private:
	void write_generated_mesh(MeshID id);
	void write_generated_material(MaterialID id);

	const ECS &ecs;
	std::filesystem::path directory;
	Serializer resources;
	Serializer meshes;
	Serializer materials;
	Serializer asset_sources;
	struct AssetSourceDescriptor
	{
		std::shared_ptr<const std::string> registration;
		std::uint64_t id;
	};
	std::unordered_map<std::string, std::vector<AssetSourceDescriptor>> asset_source_ids;
	std::uint64_t next_asset_source_id = 0;
	// A resource may be shared by many renderables. These sets prevent duplicate
	// YAML entries and duplicate .dat files while preserving that sharing.
	std::unordered_map<MeshID, bool> written_meshes;
	std::unordered_map<MaterialID, bool> written_materials;
};

// Resolves scene.yaml resource references into live ECS handles. prepare()
// must run before ECS component deserialization: it imports external models and
// reconstructs generated resources so later component readers can acquire them.
class SceneResourceReader
{
public:
	SceneResourceReader(ECS &ecs, std::filesystem::path directory);

	void prepare(const Deserializer &document);
	ImportedResourceProvenance read_imported_source(const Deserializer& in) const;
	MeshHandle read_mesh_reference(const Deserializer &in);
	MaterialHandle read_material_reference(const Deserializer &in);
	void register_renderable_id(RenderableID saved, RenderableID restored);
	void register_skeleton_id(SkeletonID saved, SkeletonID restored);
	RenderableID read_renderable_id(RenderableID saved) const;
	SkeletonID read_skeleton_id(SkeletonID saved) const;

private:
	void load_model_source(const Deserializer &source);

	ECS &ecs;
	std::filesystem::path directory;
	std::unordered_map<std::uint64_t, MeshHandle> meshes;
	std::unordered_map<std::uint64_t, MaterialHandle> materials;
	std::unordered_map<std::string, ResourceLoader::LoadedModel> imported_models;
	std::unordered_map<std::string, MaterialHandle> imported_textures;
	std::unordered_map<std::uint64_t, ImportedResourceProvenance> asset_sources;
	// Renderable and skeleton IDs express relationships within a save, but cannot
	// be reused as runtime identities. Loading assigns fresh IDs and records these
	// maps so equipment, attachments, and animations can resolve saved references.
	std::unordered_map<RenderableID, RenderableID> renderable_ids;
	std::unordered_map<SkeletonID, SkeletonID> skeleton_ids;
};
