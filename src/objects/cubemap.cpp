#include "cubemap.hpp"
#include "objects.hpp"
#include "maths.hpp"
#include "utility.hpp"
#include "shapes/shape_factory.hpp"
#include "resource_loader.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>


CubeMap::CubeMap()
{
	ResourceLoader& resource_loader = ResourceLoader::get();

	auto left = ShapeFactory::quad(ShapeFactory::EVertexType::TEXTURE);
	auto right = ShapeFactory::quad(ShapeFactory::EVertexType::TEXTURE);
	auto front = ShapeFactory::quad(ShapeFactory::EVertexType::TEXTURE);
	auto back = ShapeFactory::quad(ShapeFactory::EVertexType::TEXTURE);
	auto top = ShapeFactory::quad(ShapeFactory::EVertexType::TEXTURE);
	auto bottom = ShapeFactory::quad(ShapeFactory::EVertexType::TEXTURE);

	const auto texture_path = Utility::get().get_textures_path().string();
	Material material;

	const glm::mat4 idt(1.0f);
	glm::mat4 transform;

	transform = idt;
	// for transformation relative to world apply right->left, for local transformation it's left->right
	transform = glm::translate(transform, glm::vec3(-0.5f, 0.0f, 0.0f));
	transform = glm::rotate(transform, -Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	material.texture = resource_loader.fetch_texture(texture_path + "/skybox/left.bmp");
	left->transform_vertices(transform);
	left->set_material(material);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.5f, 0.0f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	material.texture = resource_loader.fetch_texture(texture_path + "/skybox/right.bmp");
	right->transform_vertices(transform);
	right->set_material(material);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.5f));
	material.texture = resource_loader.fetch_texture(texture_path + "/skybox/front.bmp");
	front->transform_vertices(transform);
	front->set_material(material);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, -0.5f));
	transform = glm::rotate(transform, Maths::deg2rad(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	material.texture = resource_loader.fetch_texture(texture_path + "/skybox/back.bmp");
	back->transform_vertices(transform);
	back->set_material(material);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.5f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	material.texture = resource_loader.fetch_texture(texture_path + "/skybox/top.bmp");
	top->transform_vertices(transform);
	top->set_material(material);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, -0.5f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	material.texture = resource_loader.fetch_texture(texture_path + "/skybox/bottom.bmp");
	bottom->transform_vertices(transform);
	bottom->set_material(material);

	shapes.push_back(std::move(left));
	shapes.push_back(std::move(right));
	shapes.push_back(std::move(front));
	shapes.push_back(std::move(back));
	shapes.push_back(std::move(top));
	shapes.push_back(std::move(bottom)) ;
}