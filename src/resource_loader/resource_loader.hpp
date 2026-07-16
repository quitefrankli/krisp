#pragma once

#include "renderable/material.hpp"
#include "entity_component_system/skeletal.hpp"
#include "renderable/renderable.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <memory>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <array>


class Object;
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

	struct LoadedMesh
	{
		std::string name;
		std::vector<Renderable> renderables;
		std::optional<SkeletonID> skeleton_id;
		Maths::Transform transform;
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

	struct LoadedModel
	{
		std::vector<LoadedMesh> meshes;
		std::vector<AnimationID> animations;
		Maths::Transform onload_transform;
		std::vector<ImportWarning> warnings;
	};

	struct LoadedAnimations
	{
		std::vector<AnimationID> animations;
		std::vector<ImportWarning> warnings;
	};

	static MaterialID fetch_texture(
		std::filesystem::path file_path,
		ETextureSemantic semantic = ETextureSemantic::BASE_COLOR);
	static LoadedModel load_model(std::filesystem::path file_path);
	static LoadedModel load_model(std::filesystem::path file_path, const LoadOptions& options);
	static LoadedAnimations load_animations(std::filesystem::path file_path, SkeletonID target_skeleton);

private:
	struct LoadedMaterial
	{
		MatVec ids;
	};

	MaterialID load_texture(
		const std::filesystem::path& file_path,
		ETextureSemantic semantic);
	LoadedMaterial load_material(const tinygltf::Primitive& primitive, const tinygltf::Model& model);

private:
	static constexpr size_t texture_semantic_count = static_cast<size_t>(ETextureSemantic::COUNT);
	std::unordered_map<std::string, std::array<std::optional<MaterialID>, texture_semantic_count>> texture_name_to_mat_id;
	std::unordered_map<int, LoadedMaterial> gltf_material_to_material;

	static ResourceLoader global_resource_loader;
};
