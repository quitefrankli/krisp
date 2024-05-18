#pragma once

#include "maths.hpp"
#include "identifications.hpp"

#include <unordered_map>


class AnimationSystem;
class ECS;

struct AnimationSequence
{
	AnimationSequence(const Maths::Transform& t1, const Maths::Transform& t2, const float duration_secs, const bool is_relative = false) :
		initial_transform(t1),
		final_transform(t2),
		duration_secs(duration_secs),
		is_relative(is_relative)
	{
	}

	Maths::Transform initial_transform;
	Maths::Transform final_transform;
	float duration_secs;
	bool is_relative = false; // whether to use relative or global transform

private:
	friend AnimationSystem;
	float elapsed_secs = 0;
};

// Only for simple object animations involving simple transforms
class AnimationSystem
{
public:
	void animate(const ObjectID id, const AnimationSequence& sequence)
	{ 
		add_entity(id, sequence); 
	}

protected:
	virtual ECS& get_ecs() = 0;
	void process(const float delta_secs);
	void remove_entity(const ObjectID id) { entities.erase(id); }
	void add_entity(const ObjectID id, const AnimationSequence& sequence);

private:
	std::unordered_map<ObjectID, AnimationSequence> entities;
};