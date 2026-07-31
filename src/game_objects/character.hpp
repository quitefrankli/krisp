#pragma once

#include "objects/object.hpp"

class ECS;

// Shared skinned-character behaviour for player-controlled characters and
// NPCs. Subclasses decide how a clip is chosen; this class avoids restarting
// a looping clip every frame.
class Character : public Object
{
public:
	Character() = default;

	void play_looping_animation(
		ECS& ecs, SkeletonID skeleton, AnimationID animation, float transition_secs);
	bool play_one_shot_animation(
		ECS& ecs, SkeletonID skeleton, AnimationID animation, float transition_secs);
	AnimationID get_active_animation() const { return active_animation; }
	bool has_active_animation() const { return has_animation; }

private:
	AnimationID active_animation;
	bool has_animation = false;
};
