#include <entity_component_system/physics/physics.hpp>
#include <entity_component_system/physics/gravity.hpp>
#include <entity_component_system/ecs.hpp>
#include <objects/object.hpp>
#include <maths.hpp>

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <cmath>

static constexpr float G = 0.5f;
static constexpr float TOLERANCE = 0.02f;

class OrbitalPhysicsTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ecs = &ECS::get();
		ecs->get_gravity_system().set_gravity_type(GravitySystem::GravityType::TRUE);
	}

	void simulate_steps(int steps, float dt)
	{
		for (int i = 0; i < steps; ++i)
		{
			ecs->process(dt);
		}
	}

	float calculate_orbital_energy(const Object& obj1, const PhysicsComponent& phys1,
	                                const Object& obj2, const PhysicsComponent& phys2)
	{
		const glm::vec3 r = obj2.get_position() - obj1.get_position();
		const float distance = glm::length(r);
		const float kinetic = 0.5f * phys1.mass * glm::dot(phys1.velocity, phys1.velocity);
		const float potential = -G * phys1.mass * phys2.mass / distance;
		return kinetic + potential;
	}

	glm::vec3 calculate_angular_momentum(const Object& obj, const PhysicsComponent& phys, const glm::vec3& center)
	{
		const glm::vec3 r = obj.get_position() - center;
		return phys.mass * glm::cross(r, phys.velocity);
	}

	ECS* ecs;
};

TEST_F(OrbitalPhysicsTest, gravitational_force_magnitude)
{
	Object obj1, obj2;
	obj1.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
	obj2.set_position(glm::vec3(10.0f, 0.0f, 0.0f));

	ecs->add_object(obj1);
	ecs->add_object(obj2);

	PhysicsComponent p1;
	p1.mass = 100.0f;
	p1.position = glm::vec3(0.0f, 0.0f, 0.0f);
	p1.velocity = glm::vec3(0.0f);

	PhysicsComponent p2;
	p2.mass = 10.0f;
	p2.position = glm::vec3(10.0f, 0.0f, 0.0f);
	p2.velocity = glm::vec3(0.0f);

	ecs->add_physics_entity(obj1.get_id(), p1);
	ecs->add_physics_entity(obj2.get_id(), p2);

	ecs->process(0.01f);

	const float distance = 10.0f;
	const float expected_force = G * p1.mass * p2.mass / (distance * distance);

	ASSERT_NEAR(glm::length(obj2.get_position() - p2.position), expected_force * 0.01f * 0.01f / (2.0f * p2.mass), TOLERANCE);

	ecs->remove_object(obj1.get_id());
	ecs->remove_object(obj2.get_id());
}

TEST_F(OrbitalPhysicsTest, circular_orbit_stability)
{
	const float star_mass = 1000.0f;
	const float planet_mass = 1.0f;
	const float orbit_radius = 15.0f;
	const float orbital_speed = std::sqrt(G * star_mass / orbit_radius);

	Object star, planet;
	star.set_position(glm::vec3(0.0f));
	planet.set_position(glm::vec3(orbit_radius, 0.0f, 0.0f));

	ecs->add_object(star);
	ecs->add_object(planet);

	PhysicsComponent star_phys;
	star_phys.mass = star_mass;
	star_phys.position = glm::vec3(0.0f);
	star_phys.velocity = glm::vec3(0.0f);

	PhysicsComponent planet_phys;
	planet_phys.mass = planet_mass;
	planet_phys.position = glm::vec3(orbit_radius, 0.0f, 0.0f);
	planet_phys.velocity = glm::vec3(0.0f, 0.0f, orbital_speed);

	ecs->add_physics_entity(star.get_id(), star_phys);
	ecs->add_physics_entity(planet.get_id(), planet_phys);

	const float initial_distance = orbit_radius;
	simulate_steps(200, 0.005f);

	const float final_distance = glm::length(planet.get_position() - star.get_position());
	ASSERT_NEAR(final_distance, initial_distance, orbit_radius * 0.2f);

	ecs->remove_object(star.get_id());
	ecs->remove_object(planet.get_id());
}

TEST_F(OrbitalPhysicsTest, energy_conservation_two_body)
{
	const float star_mass = 500.0f;
	const float planet_mass = 10.0f;
	const float orbit_radius = 20.0f;
	const float orbital_speed = std::sqrt(G * star_mass / orbit_radius);

	Object star, planet;
	star.set_position(glm::vec3(0.0f));
	planet.set_position(glm::vec3(orbit_radius, 0.0f, 0.0f));

	ecs->add_object(star);
	ecs->add_object(planet);

	PhysicsComponent star_phys;
	star_phys.mass = star_mass;
	star_phys.position = glm::vec3(0.0f);
	star_phys.velocity = glm::vec3(0.0f);

	PhysicsComponent planet_phys;
	planet_phys.mass = planet_mass;
	planet_phys.position = glm::vec3(orbit_radius, 0.0f, 0.0f);
	planet_phys.velocity = glm::vec3(0.0f, 0.0f, orbital_speed);

	ecs->add_physics_entity(star.get_id(), star_phys);
	ecs->add_physics_entity(planet.get_id(), planet_phys);

	const float initial_energy = calculate_orbital_energy(planet, planet_phys, star, star_phys);

	simulate_steps(100, 0.005f);

	const PhysicsComponent* final_star_phys_ptr = ecs->_get_physics_component(star.get_id());
	const PhysicsComponent* final_planet_phys_ptr = ecs->_get_physics_component(planet.get_id());
	ASSERT_NE(final_star_phys_ptr, nullptr);
	ASSERT_NE(final_planet_phys_ptr, nullptr);

	const float final_energy = calculate_orbital_energy(planet, *final_planet_phys_ptr, star, *final_star_phys_ptr);

	ASSERT_NEAR(final_energy, initial_energy, std::abs(initial_energy) * 0.2f);

	ecs->remove_object(star.get_id());
	ecs->remove_object(planet.get_id());
}

TEST_F(OrbitalPhysicsTest, angular_momentum_conservation)
{
	const float star_mass = 800.0f;
	const float planet_mass = 5.0f;
	const float orbit_radius = 25.0f;
	const float orbital_speed = std::sqrt(G * star_mass / orbit_radius);

	Object star, planet;
	star.set_position(glm::vec3(0.0f));
	planet.set_position(glm::vec3(orbit_radius, 0.0f, 0.0f));

	ecs->add_object(star);
	ecs->add_object(planet);

	PhysicsComponent star_phys;
	star_phys.mass = star_mass;
	star_phys.position = glm::vec3(0.0f);
	star_phys.velocity = glm::vec3(0.0f);

	PhysicsComponent planet_phys;
	planet_phys.mass = planet_mass;
	planet_phys.position = glm::vec3(orbit_radius, 0.0f, 0.0f);
	planet_phys.velocity = glm::vec3(0.0f, 0.0f, orbital_speed);

	ecs->add_physics_entity(star.get_id(), star_phys);
	ecs->add_physics_entity(planet.get_id(), planet_phys);

	const glm::vec3 initial_L = calculate_angular_momentum(planet, planet_phys, star.get_position());

	simulate_steps(150, 0.005f);

	const PhysicsComponent* final_planet_phys_ptr = ecs->_get_physics_component(planet.get_id());
	const PhysicsComponent* final_star_phys_ptr = ecs->_get_physics_component(star.get_id());
	ASSERT_NE(final_planet_phys_ptr, nullptr);
	ASSERT_NE(final_star_phys_ptr, nullptr);

	const glm::vec3 final_L = calculate_angular_momentum(planet, *final_planet_phys_ptr, star.get_position());

	ASSERT_NEAR(glm::length(final_L), glm::length(initial_L), glm::length(initial_L) * 0.2f);

	ecs->remove_object(star.get_id());
	ecs->remove_object(planet.get_id());
}
