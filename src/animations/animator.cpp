#include "animator.hpp"

#include <iostream>


bool Animation::process(float delta_time)
{
	progress += delta_time;
	if (progress > duration)
	{
		return false;
	}

	auto transform = glm::mix(initial_transform, final_transform, progress/duration);
	object->set_transform(transform);

	return true;
}

bool SequentialAnimation::process(float delta_time)
{
	if (animations.empty())
	{
		return false;
	}

	if (!animations.front().process(delta_time))
	{
		animations.pop();
		process(delta_time);
	}

	return true;
}

void Animator::add_animation(std::shared_ptr<Object>& object, glm::mat4& final)
{
	add_animation(object, final, object->get_transform());
}

void Animator::add_animation(std::shared_ptr<Object>& object, glm::mat4& initial_transform, glm::mat4& final_transform)
{
	Animation animation;
	animation.object = object;
	animation.initial_transform = initial_transform;
	animation.final_transform = final_transform;
	animations.push_back(std::move(animation));
}

void Animator::add_animation(SequentialAnimation&& animation)
{
	sequentials.push_back(std::move(animation));
}

void Animator::process(float delta_time)
{
	for (auto animation_it = animations.begin(); animation_it != animations.end();)
	{
		if (!animation_it->process(delta_time))
		{
			animations.erase(animation_it++);
			continue;
		}

		animation_it++;
	}

	for (auto sequential_it = sequentials.begin(); sequential_it != sequentials.end();)
	{
		if (!sequential_it->process(delta_time))
		{
			sequentials.erase(sequential_it++);
			continue;
		}

		sequential_it++;
	}
}