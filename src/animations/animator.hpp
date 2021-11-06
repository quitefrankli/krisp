#pragma once

#include "objects.hpp"

#include <list>


class Animation
{
public:
	struct Kinematics
	{
		glm::vec3 pos;
		glm::vec3 scale;
		glm::quat quat;
	};
	std::shared_ptr<Object> object;

	glm::mat4 initial_transform;
	glm::mat4 final_transform;

	// seconds
	float progress = 0;
	float duration = 5.0f;
};

class Animator
{
public:
	void add_animation(std::shared_ptr<Object>& object, glm::mat4& final_transform);
	void add_animation(std::shared_ptr<Object>& object, glm::mat4& initial_transform, glm::mat4& final_transform);
	void process(float delta_time);

private:
	std::list<Animation> animations;
};