#pragma once

#include "identifications.hpp"
#include "force_system.hpp"

#include <unordered_map>


class ECS;
struct PhysicsComponent;

class GravitySystem : public ForceSystem
{
public:
	virtual void compute_forces(const float delta_secs,
								std::unordered_map<ObjectID, PhysicsComponent>& physics_entities) override;

public:
	enum class GravityType
	{
		DISABLED,
		EARTH_LIKE, // gravity only pulls objects down
		TRUE	// gravity pulls objects towards each other
	};

	void set_gravity_type(const GravityType type) { gravity_type = type; }

private:
	void process_simplified_gravity(const float delta_secs, 
									std::unordered_map<ObjectID, PhysicsComponent>& physics_entities);
	void process_generic_gravity(const float delta_secs, 
								 std::unordered_map<ObjectID, PhysicsComponent>& physics_entities);

private:
	GravityType gravity_type = GravityType::EARTH_LIKE;
};