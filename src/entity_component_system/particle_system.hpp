#pragma once

#include "shared_data_structures.hpp"
#include "renderable/material.hpp"
#include "identifications.hpp"
#include "objects/object.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>


struct ParticleEmitterConfig
{
	uint32_t max_particles = 1000;
	float emission_rate = 100.0f;      // Particles per second
	float min_lifetime = 1.0f;
	float max_lifetime = 3.0f;
	float min_size = 0.1f;
	float max_size = 0.5f;
	glm::vec4 start_color = { 1.0f, 1.0f, 1.0f, 1.0f };
	glm::vec4 end_color = { 1.0f, 1.0f, 1.0f, 0.0f };
	glm::vec3 velocity_min = { -1.0f, 0.0f, -1.0f };
	glm::vec3 velocity_max = { 1.0f, 1.0f, 1.0f };
	float rotation_speed_min = 0.0f;
	float rotation_speed_max = 1.0f;
	bool loop = true;
	std::optional<MaterialID> material_id; // Optional texture
};

class ECS;

class ParticleSystem
{
public:
	void process(float delta_time);
	
	void spawn_particle_emitter(EntityID entity_id, const ParticleEmitterConfig& config);
	
	// Get particle data for rendering (fills instance data buffer)
	void prepare_render_data(std::vector<SDS::ParticleInstanceData>& out_instance_data);

	void remove_entity(EntityID id);

protected:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

private:
	struct Particle
	{
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec4 color;
		float size;
		float rotation;
		float lifetime;       // Remaining lifetime
		float rotation_speed;
	};

	struct Emitter
	{
		Emitter(const ParticleEmitterConfig& config, const Object& parent_object);

		void process(float delta_time);
		void emit(uint32_t count);
		
		bool is_alive() const { return config.loop || !particles.empty(); }

		ParticleEmitterConfig config;
		std::vector<Particle> particles;
		float emission_accumulator = 0.0f;
		bool enabled = true;
		const Object& parent_object;
	};

	std::unordered_map<EntityID, std::unique_ptr<Emitter>> emitters;
};
