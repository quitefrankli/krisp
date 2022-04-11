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
};