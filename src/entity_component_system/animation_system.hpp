#pragma once

#include "ecs_component.hpp"
#include "maths.hpp"


class AnimationECS;

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
	friend AnimationECS;
	float elapsed_secs = 0;
};

class AnimationECS : public ECSComponent
{
public:
	AnimationECS(ECSManager& manager);
	virtual ~AnimationECS() override = default;

	void process(const float delta_secs);

	void add_entity(const ObjectID id, const AnimationSequence& sequence);
	void remove_entity(const ObjectID id) { entities.erase(id); }

private:
	std::unordered_map<ObjectID, AnimationSequence> entities;
};