#include "light_source.hpp"

#include "objects/objects.hpp"


LightSource::LightSource(const glm::vec3& color) :
	color(color),
	IClickable((Object&)(*this))
{
	Sphere sphere;
	shapes = std::move(sphere.shapes);
}