#pragma once

#include "materials/materials.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <cstddef>
#include <memory>


class Object;
class Shape;

class ResourceLoader
{
public:
	using TextureID = uint32_t;

	enum class Setting
	{
		DEFAULT,
		ZERO_MESH, // per shape center moved to (0,0,0)
		ZERO_XZ,   // per shape center moved to (0,y,0) and bottom of mesh is at y=0
	};

	static ResourceLoader& get() { return global_resource_loader; }

	// ONLY SUPPORTS TEXTURED MESHES FOR NOW
	Object load_object(const std::string_view mesh, 
					   const std::vector<std::string_view>& textures, 
					   const glm::mat4& transform = glm::mat4(1.0f)); // per vertex transform
	Object load_object(const std::string_view mesh,
					   const glm::vec3& color,
					   const glm::mat4& transform = glm::mat4(1.0f)); // per vertex transform

	MaterialTexture fetch_texture(const std::string_view file);

	std::vector<std::unique_ptr<Shape>> load_model(const std::string_view file);

	// // for when there are many objects within a single mesh
	// // each shape within the mesh will be associated with a single object
	// // there is an expectation of a single texture per mesh
	// std::vector<Object> load_objects(const std::string_view mesh,
	//                                  const std::vector<std::string_view>& textures,
	//                                  const Setting setting = Setting::DEFAULT);

	// assigns a single texture to each shape
	void assign_object_texture(Object& object, const std::string_view texture);
	// assigns a texture to every shape
	void assign_object_texture(Object& object, const std::vector<std::string_view> textures);

private:
	void load_texture(const std::string_view file);

private:
	struct TextureData
	{
		std::unique_ptr<std::byte, std::function<void(std::byte*)>> data;
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