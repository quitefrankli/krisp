#pragma once

#include "objects/object.hpp"

class ECS;

// Shared skinned-character behaviour for player-controlled characters and
// NPCs. Subclasses decide how a clip is chosen; this class avoids restarting
// a looping clip every frame.
class Character : public Object
{
public:
	Character(std::vector<Renderable> renderables, SkeletonID skeleton_id);

	void play_looping_animation(ECS& ecs, AnimationID animation);
	AnimationID get_active_animation() const { return active_animation; }
	bool has_active_animation() const { return has_animation; }

private:
	AnimationID active_animation;
	bool has_animation = false;
};
