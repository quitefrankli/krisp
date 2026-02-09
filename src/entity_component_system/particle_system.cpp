#include "particle_system.hpp"
#include "ecs.hpp"
#include "maths.hpp"

#include <algorithm>
#include <random>


ParticleSystem::Emitter::Emitter(const ParticleEmitterConfig& config, const Object& parent_object) :
	config(config),
	parent_object(parent_object)
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
		
		// Interpolate color based on lifetime
		float t = particle.lifetime / config.max_lifetime;
		particle.color = Maths::lerp(config.start_color, config.end_color, t);
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
		particles.push_back(Particle{
			.position = parent_object.get_position(),
			.velocity = Maths::RandomUniform(config.velocity_min, config.velocity_max),
			.color = config.start_color,
			.size = Maths::RandomUniform(config.min_size, config.max_size),
			.rotation = Maths::RandomUniform(0.0f, Maths::PI * 2.0f),
			.lifetime = Maths::RandomUniform(config.min_lifetime, config.max_lifetime),
			.rotation_speed = Maths::RandomUniform(config.rotation_speed_min, config.rotation_speed_max)
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
	const Object& parent_object = get_ecs().get_object(entity_id);
	emitters.emplace(entity_id, std::make_unique<Emitter>(config, parent_object));
}

void ParticleSystem::remove_entity(EntityID entity_id)
{
	emitters.erase(entity_id);
}

void ParticleSystem::prepare_render_data(std::vector<SDS::ParticleInstanceData>& out_instance_data)
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
