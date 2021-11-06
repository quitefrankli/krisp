#include "animator.hpp"


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

void Animator::process(float delta_time)
{
	for (auto animation_it = animations.begin(); animation_it != animations.end();)
	{
		auto& animation = *animation_it;
		animation.progress += delta_time;
		if (animation.progress > animation.duration)
		{
			animations.erase(animation_it++);
			continue;
		}

		auto cur_transform = glm::mix(animation.initial_transform, animation.final_transform, 
			animation.progress/animation.duration);
		animation.object->set_transform(cur_transform);

		animation_it++;
	}
}