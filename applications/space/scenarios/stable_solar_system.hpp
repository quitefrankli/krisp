#pragma once

#include <game_engine.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
#include <entity_component_system/material_system.hpp>
#include <entity_component_system/physics/physics_component.hpp>

#include "planet.hpp"


namespace Scenarios
{

inline void setup_orbital_system(GameEngine& engine)
{
	// Create the star (central body)
	Renderable star_renderable{
		.mesh_id = MeshFactory::sphere_id(MeshFactory::EVertexType::COLOR, MeshFactory::GenerationMethod::ICO_SPHERE, 100),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE) },
		.casts_shadow = false,
	};
	LightComponent sun_light;
	sun_light.intensity = 1.0f;
	sun_light.color = glm::vec3(1.0f, 0.9f, 0.2f);
	Planet& star = engine.spawn_object<Planet>(star_renderable);
	star.set_name("Star");
	star.set_position(glm::vec3(0.0f, 0.0f, 0.0f));
	star.set_scale(glm::vec3(3.0f, 3.0f, 3.0f));
	engine.get_ecs().add_light_source(star.get_id(), sun_light);

	PhysicsComponent star_physics;
	star_physics.mass = 5000.0f;
	star_physics.position = glm::vec3(0.0f, 0.0f, 0.0f);
	star_physics.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	engine.get_ecs().add_physics_entity(star.get_id(), star_physics);

	ParticleEmitterConfig star_trail;
	star_trail.max_particles = 500;
	star_trail.emission_rate = 50.0f;
	star_trail.min_lifetime = 2.0f;
	star_trail.max_lifetime = 4.0f;
	star_trail.min_size = 0.2f;
	star_trail.max_size = 0.4f;
	star_trail.start_color = glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);
	star_trail.end_color = glm::vec4(1.0f, 0.5f, 0.0f, 0.0f);
	star_trail.velocity_min = glm::vec3(-0.1f, -0.1f, -0.1f);
	star_trail.velocity_max = glm::vec3(0.1f, 0.1f, 0.1f);
	engine.get_ecs().spawn_particle_emitter(star.get_id(), star_trail);

	// Create planet 1
	ColorMaterial planet1_material;
	planet1_material.data.diffuse = glm::vec3(0.2f, 0.5f, 1.0f);

	Renderable planet1_renderable = Renderable::make_default(MeshFactory::sphere_id());
	planet1_renderable.material_ids.push_back(MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(planet1_material))));
	Planet& planet1 = engine.spawn_object<Planet>(planet1_renderable);
	planet1.set_name("Planet 1");
	planet1.set_position(glm::vec3(25.0f, 0.0f, 0.0f));
	planet1.set_scale(glm::vec3(1.0f, 1.0f, 1.0f));

	PhysicsComponent planet1_physics;
	planet1_physics.mass = 10.0f;
	planet1_physics.velocity = glm::vec3(0.0f, 0.0f, 8.0f);
	engine.get_ecs().add_physics_entity(planet1.get_id(), planet1_physics);

	ParticleEmitterConfig planet1_trail;
	planet1_trail.max_particles = 800;
	planet1_trail.emission_rate = 80.0f;
	planet1_trail.min_lifetime = 3.0f;
	planet1_trail.max_lifetime = 5.0f;
	planet1_trail.min_size = 0.15f;
	planet1_trail.max_size = 0.3f;
	planet1_trail.start_color = glm::vec4(0.2f, 0.5f, 1.0f, 1.0f);
	planet1_trail.end_color = glm::vec4(0.0f, 0.3f, 0.8f, 0.0f);
	planet1_trail.velocity_min = glm::vec3(-0.05f, -0.05f, -0.05f);
	planet1_trail.velocity_max = glm::vec3(0.05f, 0.05f, 0.05f);
	engine.get_ecs().spawn_particle_emitter(planet1.get_id(), planet1_trail);

	// Create planet 2
	ColorMaterial planet2_material;
	planet2_material.data.diffuse = glm::vec3(1.0f, 0.3f, 0.2f);

	Renderable planet2_renderable = Renderable::make_default(MeshFactory::sphere_id());
	planet2_renderable.material_ids.push_back(MaterialSystem::add(std::make_unique<ColorMaterial>(std::move(planet2_material))));
	Planet& planet2 = engine.spawn_object<Planet>(planet2_renderable);
	planet2.set_name("Planet 2");
	planet2.set_position(glm::vec3(-25.0f, 0.0f, 0.0f));
	planet2.set_scale(glm::vec3(0.8f, 0.8f, 0.8f));

	PhysicsComponent planet2_physics;
	planet2_physics.mass = 8.0f;
	planet2_physics.velocity = glm::vec3(0.0f, 0.0f, -6.0f);
	engine.get_ecs().add_physics_entity(planet2.get_id(), planet2_physics);

	ParticleEmitterConfig planet2_trail;
	planet2_trail.max_particles = 700;
	planet2_trail.emission_rate = 70.0f;
	planet2_trail.min_lifetime = 3.0f;
	planet2_trail.max_lifetime = 5.0f;
	planet2_trail.min_size = 0.12f;
	planet2_trail.max_size = 0.25f;
	planet2_trail.start_color = glm::vec4(1.0f, 0.3f, 0.2f, 1.0f);
	planet2_trail.end_color = glm::vec4(0.8f, 0.1f, 0.0f, 0.0f);
	planet2_trail.velocity_min = glm::vec3(-0.05f, -0.05f, -0.05f);
	planet2_trail.velocity_max = glm::vec3(0.05f, 0.05f, 0.05f);
	engine.get_ecs().spawn_particle_emitter(planet2.get_id(), planet2_trail);
}

} // namespace Scenarios
