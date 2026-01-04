#pragma once

#include "maths.hpp"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>


// Encapsulates every variable needed by physics engine and common to force systems
struct PhysicsComponent
{
	float mass = 1.0f; // in kg
	
	glm::vec3 position = Maths::zero_vec;
	glm::vec3 velocity = Maths::zero_vec;
	glm::quat angular_velocity = Maths::identity_quat;

	glm::vec3 acceleration = Maths::zero_vec;

	// used by physics engine
	glm::vec3 _net_force = Maths::zero_vec;
};