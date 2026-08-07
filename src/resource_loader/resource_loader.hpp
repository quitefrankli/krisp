#pragma once

#include "renderable/material.hpp"
#include "entity_component_system/skeletal.hpp"
#include "renderable/renderable.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <memory>
#include <optional>
#include <filesystem>
#include <stdexcept>


class Object;
class ECS;
struct SkeletalComponent;
struct TextureData;

namespace tinygltf
{
	class Model;
	class Primitive;
}

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
		Maths::Transform onload_transform;
		std::vector<ImportWarning> warnings;
	};

	struct LoadedAnimations
	{
		std::vector<AnimationID> animations;
		std::vector<ImportWarning> warnings;
	};

	static MaterialHandle fetch_texture(
		MaterialSystem& materials,
		std::string_view logical_resource_name,
		ETextureSemantic semantic = ETextureSemantic::BASE_COLOR);
	static LoadedModel load_model(ECS& ecs, std::string_view filename);
	static LoadedModel load_model(ECS& ecs, std::string_view filename, const LoadOptions& options);
	static LoadedAnimations load_animations(ECS& ecs, std::string_view filename, SkeletonID target_skeleton);

private:
	struct LoadedMaterial
	{
		MatVec ids;
		std::vector<std::pair<MaterialID, int>> image_sources;
	};

	MaterialHandle load_texture(
		MaterialSystem& materials,
		// Resolved filesystem path used to read the texture data.
		const std::filesystem::path& resolved_file_path,
		// Caller-facing name retained as TextureMaterial provenance.
		std::string_view logical_resource_name,
		ETextureSemantic semantic);
	LoadedMaterial load_material(
		MaterialSystem& materials,
		const tinygltf::Primitive& primitive,
		const tinygltf::Model& model,
		std::vector<MaterialHandle>& owners);

private:
	std::unordered_map<int, LoadedMaterial> gltf_material_to_material;
	std::unordered_map<uint64_t, MaterialID> gltf_image_to_material;

	static ResourceLoader global_resource_loader;
};
