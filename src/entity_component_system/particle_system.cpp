#include "particle_system.hpp"
#include "ecs.hpp"
#include "maths.hpp"

#include <algorithm>
#include <random>


ParticleSystem::Emitter::Emitter(const ParticleEmitterConfig& config, ECS& ecs, const EntityID parent_id) :
	config(config),
	ecs(ecs),
	parent_id(parent_id)
{
	particles.reserve(config.max_particles);
}

void ParticleSystem::Emitter::process(float delta_time)
{
	if (enabled)
	{
		// Emit new particles based on emission rate
		emission_accumulator += config.emission_rate * delta_time;
		uint32_t emit_count = static_cast<uint32_t>(emission_accumulator);
		if (emit_count > 0)
		{
			emit(emit_count);
			emission_accumulator -= emit_count;
		}
	}

	// Update existing particles
	for (auto& particle : particles)
	{
		particle.position += particle.velocity * delta_time;
		particle.rotation += particle.rotation_speed * delta_time;
		particle.lifetime -= delta_time;

		const float progress = particle.initial_lifetime > 0.0f
			? std::clamp(1.0f - particle.lifetime / particle.initial_lifetime, 0.0f, 1.0f)
			: 1.0f;
		particle.color = Maths::lerp(config.start_color, config.end_color, progress);
	}

	// Remove dead particles 
	// this feels extremely expensive, can definitely improve upon
	particles.erase(
		std::remove_if(particles.begin(), particles.end(),
			[](const Particle& p) { return p.lifetime <= 0.0f; }),
		particles.end()
	);
}

void ParticleSystem::Emitter::emit(uint32_t count)
{
	for (uint32_t i = 0; i < count && particles.size() < config.max_particles; ++i)
	{
		const glm::vec3 spawn_offset = Maths::random_uniform(
			config.spawn_offset_min,
			config.spawn_offset_max);
		glm::vec3 position;
		glm::vec3 velocity = Maths::random_uniform(config.velocity_min, config.velocity_max);
		if (config.emission_space == EParticleEmissionSpace::LOCAL)
		{
			position = glm::vec3(
				ecs.get_transform(parent_id) * glm::vec4(spawn_offset, 1.0f));
			velocity = ecs.get_rotation(parent_id) * velocity;
		}
		else
		{
			position = ecs.get_position(parent_id) + spawn_offset;
		}
		const float lifetime = Maths::random_uniform(config.min_lifetime, config.max_lifetime);
		particles.push_back(Particle{
			.position = position,
			.velocity = velocity,
			.color = config.start_color,
			.size = Maths::random_uniform(config.min_size, config.max_size),
			.rotation = Maths::random_uniform(0.0f, Maths::PI * 2.0f),
			.lifetime = lifetime,
			.initial_lifetime = lifetime,
			.rotation_speed = Maths::random_uniform(config.rotation_speed_min, config.rotation_speed_max)
		});
	}
}

void ParticleSystem::process(float delta_time)
{
	for (auto& [_, emitter] : emitters)
	{
		emitter->process(delta_time);
	}
	
	// Remove dead emitters (non-looping emitters with no particles)
	std::erase_if(emitters,
		[](const auto& p) { return !p.second->is_alive(); }
	);
}

void ParticleSystem::spawn_particle_emitter(EntityID entity_id, const ParticleEmitterConfig& config)
{
	assert(emitters.find(entity_id) == emitters.end() && "Entity already has a particle emitter!");
	emitters.emplace(entity_id, std::make_unique<Emitter>(config, get_ecs(), entity_id));
}

void ParticleSystem::remove_entity(EntityID entity_id)
{
	emitters.erase(entity_id);
}

void ParticleSystem::prepare_render_data(
	std::vector<SDS::ParticleInstanceData>& out_instance_data) const
{
	out_instance_data.clear();
	
	for (const auto& [_, emitter] : emitters)
	{
		for (const auto& particle : emitter->particles)
		{
			out_instance_data.push_back(SDS::ParticleInstanceData{
				.model = glm::translate(glm::mat4(1.0f), particle.position),
				.color = particle.color,
				.size = particle.size,
				.rotation = particle.rotation
			});
		}
	}
}
