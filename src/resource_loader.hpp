#pragma once

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>
#include <string_view>


class Object;

class ResourceLoader
{
public:
	enum class Setting
	{
		DEFAULT,
		ZERO_MESH, // per shape center moved to (0,0,0)
		ZERO_XZ,   // per shape center moved to (0,y,0) and bottom of mesh is at y=0
	};

	// ONLY SUPPORTS TEXTURED MESHES FOR NOW
	Object load_object(const std::string_view mesh, 
					   const std::vector<std::string_view>& textures, 
					   const glm::mat4& transform = glm::mat4(1.0f)); // per vertex transform
	Object load_object(const std::string_view mesh,
					   const glm::vec3& color,
					   const glm::mat4& transform = glm::mat4(1.0f)); // per vertex transform

	// // for when there are many objects within a single mesh
	// // each shape within the mesh will be associated with a single object
	// // there is an expectation of a single texture per mesh
	// std::vector<Object> load_objects(const std::string_view mesh,
	//                                  const std::vector<std::string_view>& textures,
	//                                  const Setting setting = Setting::DEFAULT);

	// assigns a single texture to each shape
	static void assign_object_texture(Object& object, const std::string_view texture);
	// assigns a texture to every shape
	static void assign_object_texture(Object& object, const std::vector<std::string_view> textures);
};