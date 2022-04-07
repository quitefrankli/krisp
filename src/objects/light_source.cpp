#include "light_source.hpp"

#include "objects/objects.hpp"


LightSource::LightSource(const glm::vec3& color) :
	color(color)
{
	Sphere sphere;
	shapes = std::move(sphere.shapes);
}