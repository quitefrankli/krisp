#pragma once

#include "renderable/material.hpp"
#include "entity_component_system/skeletal.hpp"
#include "renderable/renderable.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <cstddef>
#include <memory>
#include <optional>


class Object;
struct RawTextureData; // internal use only
struct SkeletalComponent;

namespace tinygltf
{
	class Model;
	class Primitive;
}

class ResourceLoader
{
public:
	using TextureID = uint32_t;

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
		std::vector<Bone> bones;
		std::unordered_map<std::string, std::vector<BoneAnimation>> animations;
		// add other custom parameters here, that we want to return from the resource loader i.e.
		// PipelineType
	};

	~ResourceLoader();

	static ResourceLoader& get() { return global_resource_loader; }

	MaterialTexture fetch_texture(const std::string_view file);

	LoadedModel load_model(const std::string_view file);

private:
	void load_texture(const std::string_view file);
	TextureID get_next_texture_id() { return global_texture_id_counter++; }
	struct TextureData;
	MaterialTexture create_material_texture(TextureData& texture_data);
	MaterialID load_material(const tinygltf::Primitive& primitive, tinygltf::Model& model);

private:
	struct TextureData
	{
		TextureData() = default;
		TextureData(const TextureData& other) = default;
		TextureData(TextureData&& other) noexcept = default;
		~TextureData();
		std::unique_ptr<RawTextureData> data;
		int width = 0;
		int height = 0;
		int channels = 0;
		uint32_t texture_id = 0;
	};
	std::unordered_map<std::string, TextureID> texture_name_to_id;
	std::unordered_map<TextureID, TextureData> cached_textures;
	static TextureID global_texture_id_counter;

	static ResourceLoader global_resource_loader;
};