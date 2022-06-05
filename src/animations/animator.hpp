#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <list>
#include <queue>
#include <memory>


class Object;

class Animation
{
public:
	struct Kinematics
	{
		glm::vec3 pos{};
		glm::vec3 scale{};
		glm::quat quat = glm::quat_identity<float, glm::highp>();
	};
	std::shared_ptr<Object> object;

	glm::mat4 initial_transform;
	glm::mat4 final_transform;

	// seconds
	float progress = 0;
	float duration = 5.0f;

	bool process(float delta_time);
};

class SequentialAnimation
{
public:
	std::queue<Animation> animations;
	bool process(float delta_time);
};

class Animator
{
public:
	void add_animation(std::shared_ptr<Object>& object, glm::mat4& final_transform);
	void add_animation(std::shared_ptr<Object>& object, const glm::mat4& initial_transform, const glm::mat4& final_transform);
	void add_animation(SequentialAnimation&& animation);
	void process(float delta_time);

private:
	std::list<Animation> animations;
	std::list<SequentialAnimation> sequentials;
};