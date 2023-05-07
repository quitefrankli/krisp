#pragma once

#include "ecs_component.hpp"
#include "animation_system.hpp"
#include "objects/object.hpp"

#include <unordered_map>
#include <memory>


enum class ECSComponentType
{
	ANIMATION,
	RIGID_BODY,
};

class ECSManager
{
public:
	ECSManager();
	~ECSManager();

	void process(const float delta_secs);

	// ECSComponent& get_component(const ECSComponentType type) { return *components[type]; }

	void animate(const ObjectID id, const AnimationSequence& sequence) { animation.add_entity(id, sequence); }

	// animation system automatically removes animation components when they are finished
	// void remove_animation(const ObjectID id) { animation.remove_component(id); }

	// Used by GameEngine
	void add_object(Object& object) { objects.emplace(object.get_id(), &object); }
	void remove_object(const ObjectID id);

	// Used by ECSComponents
	Object& get_object(const ObjectID id) { return *objects[id]; }

private:
	// std::unordered_map<ECSComponentType, std::unique_ptr<ECSComponent>> components;
	
	std::unordered_map<ObjectID, Object*> objects;

	AnimationECS animation;
};