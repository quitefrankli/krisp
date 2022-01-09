#pragma once

#include <glm/mat4x4.hpp>

#include <string>


class Object;

class ResourceLoader
{
public:
	virtual void load_mesh(Object& object, const std::string& filename, const glm::mat4& transform);
};