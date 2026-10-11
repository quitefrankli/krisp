#pragma once

#include "renderable/material.hpp"
#include "entity_component_system/skeletal.hpp"
#include "renderable/renderable.hpp"
#include "serialization/resource_provenance.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <memory>
#include <span>
#include <functional>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <utility>


class Object;
class ECS;
struct SkeletalComponent;
struct TextureData;

namespace tinygltf
{
	class Model;
	class Primitive;
}

// Internal decoded glTF document shared by resource import paths. Decoding is
// independent from ECS mutation so callers can retain and reuse CPU data.
struct ResourceDocument
{
	std::shared_ptr<tinygltf::Model> model;
	std::string warning;
};

class ResourceLoadError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

class ResourceLoader
{
public:
	enum class Setting
	{
		DEFAULT,
		ZERO_MESH, // per mesh center moved to (0,0,0)
		ZERO_XZ,   // per mesh center moved to (0,y,0) and bottom of mesh is at y=0
	};

	// One mesh-node instance from the selected model scene. It groups the
	// node's primitive draw records with its transform and optional skeleton.
	// Multiple LoadedMeshes may reference different instances of the same
	// source glTF mesh.
	struct LoadedMesh
	{
		std::string name;
		std::vector<Renderable> renderables;
		std::optional<SkeletonID> skeleton_id;
		int source_node = -1;
		int source_skin = -1;
	};

	struct ImportWarning
	{
		std::string message;
	};

	struct LoadOptions
	{
		std::optional<int> scene_index;
		std::optional<SkeletonID> target_skeleton;
		bool generate_missing_normals = true;
		bool generate_missing_tangents = false;
		bool allow_non_triangle_primitives = true;
		bool strict = false;
	};

	// Complete result of importing one model scene. It owns the imported
	// mesh-node instances, a model-wide placement transform, and any
	// non-fatal diagnostics produced during import.
	struct LoadedModel
	{
		std::vector<LoadedMesh> meshes;
		// Owners retained for direct material selectors that do not create an
		// assembly renderable.
		std::vector<MaterialHandle> retained_materials;
		Maths::Transform onload_transform;
		std::vector<ImportWarning> warnings;
	};

	struct LoadedAnimations
	{
		std::vector<AnimationID> animations;
		std::vector<ImportWarning> warnings;
	};

	struct DecodedModel
	{
		LoadedModel model;
		std::unordered_map<int, std::vector<Bone>> skins;
		std::unordered_map<MeshID, ImportedResourceProvenance> mesh_provenance;
		std::unordered_map<MaterialID, ImportedResourceProvenance> material_provenance;
	};

	using DependencyReader = std::function<std::vector<unsigned char>(std::string_view)>;
	static ResourceDocument read_document(const std::filesystem::path& file_path);
	static ResourceDocument read_document(
		std::span<const unsigned char> bytes,
		std::string_view logical_name,
		DependencyReader dependency_reader = {});
	static DecodedModel decode_model(
		const ResourceDocument& document,
		std::string_view provenance_source,
		const LoadOptions& options,
		MeshSystem& meshes,
		MaterialSystem& materials,
		const std::vector<Bone>* target_bones = nullptr,
		ResourceLoader* material_cache = nullptr);
	static std::vector<Bone> decode_skeleton(const ResourceDocument& document, int skin_index);
	static SkeletalAnimation decode_animation(
		const ResourceDocument& document,
		int animation_index,
		int skin_index);
	static LoadedModel load_model_document(
		ECS& ecs,
		const ResourceDocument& document,
		std::string_view provenance_source,
		const LoadOptions& options);
	static LoadedAnimations load_animations_document(
		ECS& ecs,
		const ResourceDocument& document,
		std::string_view provenance_source,
		SkeletonID target_skeleton);
	static MaterialHandle load_texture_bytes(
		MaterialSystem& materials,
		std::span<const unsigned char> bytes,
		std::string_view logical_resource_name,
		ETextureSemantic semantic);
	static std::vector<MaterialHandle> decode_material(
		const ResourceDocument& document,
		int material_index,
		MaterialSystem& materials,
		ResourceLoader* material_cache = nullptr);
	static MaterialHandle decode_image(
		const ResourceDocument& document,
		int image_index,
		MaterialSystem& materials,
		ETextureSemantic semantic,
		std::string_view source,
		ResourceLoader* material_cache = nullptr);
	std::vector<std::pair<MaterialID, int>> material_image_sources(int material_index) const
	{
		return gltf_material_to_material.at(material_index).image_sources;
	}

private:
	struct LoadedMaterial
	{
		MatVec ids;
		std::vector<std::pair<MaterialID, int>> image_sources;
	};

	LoadedMaterial load_material(
		MaterialSystem& materials,
		const tinygltf::Primitive& primitive,
		const tinygltf::Model& model,
		std::vector<MaterialHandle>& owners);

private:
	std::unordered_map<int, LoadedMaterial> gltf_material_to_material;
	std::unordered_map<uint64_t, MaterialID> gltf_image_to_material;
	std::vector<MaterialHandle> cached_material_owners;

};
