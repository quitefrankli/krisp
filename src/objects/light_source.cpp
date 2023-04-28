#include "light_source.hpp"

#include "objects/objects.hpp"
#include "shapes/shape_factory.hpp"


LightSource::LightSource(const glm::vec3& color) :
	color(color),
	IClickable((Object&)(*this))
{
	shapes.push_back(ShapeFactory::sphere());
	shapes.back()->set_material(Material::create_material(EMaterialType::LIGHT_SOURCE));
}