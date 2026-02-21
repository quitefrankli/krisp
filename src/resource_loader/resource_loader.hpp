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


class Object;
struct SkeletalComponent;
struct TextureData;

namespace tinygltf
{
	class Model;
	class Primitive;
}

class ResourceLoader
{
public:
	enum class Setting
	{
		DEFAULT,
		ZERO_MESH, // per mesh center moved to (0,0,0)
		ZERO_XZ,   // per mesh center moved to (0,y,0) and bottom of mesh is at y=0
	};

	// used as a return value when loading models
	struct LoadedModel
	{
		std::vector<Renderable> renderables;
		std::vector<AnimationID> animations;
		Maths::Transform onload_transform;
	};

	static MaterialID fetch_texture(const std::filesystem::path& file_path);
	static LoadedModel load_model(const std::filesystem::path& file_path);

private:
	MaterialID load_texture(const std::filesystem::path& file_path);
	MaterialID load_material(const tinygltf::Primitive& primitive, tinygltf::Model& model);

private:
	std::unordered_map<std::string, MaterialID> texture_name_to_mat_id;

	static ResourceLoader global_resource_loader;
};
