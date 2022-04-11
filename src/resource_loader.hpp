#pragma once

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>


class Object;

class ResourceLoader
{
public:
	Object load_object(const std::string_view mesh, 
					   const std::vector<std::string_view>& textures, 
					   const glm::mat4& transform = glm::mat4(1.0f)); // per vertex transform
	Object load_object(const std::string_view mesh,
					   const glm::vec3& color,
					   const glm::mat4& transform = glm::mat4(1.0f)); // per vertex transform

	// for when there are many objects within a single mesh
	// each shape within the mesh will be associated with a single object
	// there is an expectation of a single texture per mesh
	std::vector<Object> load_objects(const std::string_view mesh, const std::vector<std::string_view>& textures);
};