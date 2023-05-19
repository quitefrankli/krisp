#pragma once

#include "animation.hpp"
#include "skeletal.hpp"
#include "light_source.hpp"
#include "collider_ecs.hpp"
#include "clickable.hpp"
#include "objects/object.hpp"

#include <unordered_map>
#include <memory>


enum class ECSComponentType
{
	ANIMATION,
	RIGID_BODY,
};

class ECS : 
	public SkeletalSystem, 
	public AnimationSystem,
	public LightSystem,
	public ColliderSystem,
	public ClickableSystem
{
public:
	ECS();
	~ECS();

	void process(const float delta_secs);

	virtual ECS& get_ecs() override { return *this; }
	virtual const ECS& get_ecs() const override { return *this; }

	// ECSComponent& get_component(const ECSComponentType type) { return *components[type]; }


	// animation system automatically removes animation components when they are finished
	// void remove_animation(const ObjectID id) { animation.remove_component(id); }

	// Used by GameEngine
	void add_object(Object& object) { objects.emplace(object.get_id(), &object); }
	void remove_object(const ObjectID id);

	// Used by ECSComponents
	Object& get_object(const ObjectID id);
	const Object& get_object(const ObjectID id) const;

private:
	// std::unordered_map<ECSComponentType, std::unique_ptr<ECSComponent>> components;
	
	std::unordered_map<ObjectID, Object*> objects;
};