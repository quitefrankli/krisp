#include "cubemap.hpp"
#include "objects.hpp"
#include "shapes/shapes.hpp"
#include "maths.hpp"
#include "utility.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>


CubeMap::CubeMap()
{
	Shapes::Square left, right, front, back, top, bottom;
	glm::mat4 idt(1.0f);
	glm::mat4 transform;

	transform = idt;
	// for transformation relative to world apply right->left, for local transformation it's left->right
	transform = glm::translate(transform, glm::vec3(-0.5f, 0.0f, 0.0f));
	transform = glm::rotate(transform, -Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	left.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.5f, 0.0f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	right.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, 0.5f));
	front.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, -0.5f));
	transform = glm::rotate(transform, Maths::deg2rad(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	back.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, 0.5f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	top.transform_vertices(transform);

	transform = idt;
	transform = glm::translate(transform, glm::vec3(0.0f, -0.5f, 0.0f));
	transform = glm::rotate(transform, Maths::deg2rad(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	bottom.transform_vertices(transform);

	const auto texture_path = Utility::get().get_textures_path().string();
	front.texture = texture_path + "/skybox/DaylightBox_Front.bmp";
	left.texture = texture_path + "/skybox/DaylightBox_Left.bmp";
	right.texture = texture_path + "/skybox/DaylightBox_Right.bmp";
	back.texture = texture_path + "/skybox/DaylightBox_Back.bmp";
	top.texture = texture_path + "/skybox/DaylightBox_Top.bmp";
	bottom.texture = texture_path + "/skybox/DaylightBox_Bottom.bmp";

	shapes.push_back(std::move(left));
	shapes.push_back(std::move(right));
	shapes.push_back(std::move(front));
	shapes.push_back(std::move(back));
	shapes.push_back(std::move(top));
	shapes.push_back(std::move(bottom));
}