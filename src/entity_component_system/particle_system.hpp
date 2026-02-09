#pragma once

#include "shared_data_structures.hpp"
#include "renderable/material.hpp"
#include "identifications.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>


struct Particle
{
	glm::vec3 position;
	glm::vec4 color;
	float size;
	float rotation;
	float lifetime;       // Remaining lifetime
	float max_lifetime;   // Initial lifetime for normalization
	glm::vec3 velocity;
	float rotation_speed;
};

// Emitter configuration
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

class ParticleEmitter
{
public:
	ParticleEmitter(const ParticleEmitterConfig& config);
	~ParticleEmitter();

	void update(float delta_time);
	void emit(uint32_t count);
	
	void set_position(const glm::vec3& position) { this->position = position; }
	const glm::vec3& get_position() const { return position; }
	
	void set_enabled(bool enabled) { this->enabled = enabled; }
	bool is_enabled() const { return enabled; }
	
	const std::vector<Particle>& get_particles() const { return particles; }
	const ParticleEmitterConfig& get_config() const { return config; }
	
	bool is_alive() const { return loop || !particles.empty(); }

private:
	ParticleEmitterConfig config;
	std::vector<Particle> particles;
	glm::vec3 position = glm::vec3(0.0f);
	float emission_accumulator = 0.0f;
	bool enabled = true;
	bool loop = true;
};

class ECS;

class ParticleSystem
{
public:
	ParticleSystem();
	~ParticleSystem();

	void update(float delta_time);
	
	// Create a new particle emitter attached to an object
	ParticleEmitter& create_emitter(ObjectID object_id, const ParticleEmitterConfig& config);
	
	// Add an emitter directly (world-space, not attached to any object)
	void add_emitter(std::unique_ptr<ParticleEmitter>&& emitter);
	
	// Remove all emitters for an object
	void remove_emitters(ObjectID object_id);
	
	// Get all emitters
	const std::vector<std::unique_ptr<ParticleEmitter>>& get_emitters() const { return emitters; }
	
	// Get particle data for rendering (fills instance data buffer)
	void prepare_render_data(std::vector<SDS::ParticleInstanceData>& out_instance_data);

protected:
	virtual ECS& get_ecs() = 0;
	virtual const ECS& get_ecs() const = 0;

private:
	std::vector<std::unique_ptr<ParticleEmitter>> emitters;
	std::unordered_map<ObjectID, std::vector<size_t>> object_emitters; // object_id -> emitter indices
};
