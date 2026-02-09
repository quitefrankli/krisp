#include "particle_system.hpp"
#include "ecs.hpp"

#include <algorithm>
#include <random>


static std::random_device rd;
static std::mt19937 gen(rd());

static float random_float(float min, float max)
{
	std::uniform_real_distribution<float> dis(min, max);
	return dis(gen);
}

static glm::vec3 random_vec3(const glm::vec3& min, const glm::vec3& max)
{
	return glm::vec3(
		random_float(min.x, max.x),
		random_float(min.y, max.y),
		random_float(min.z, max.z)
	);
}

static glm::vec4 lerp_color(const glm::vec4& a, const glm::vec4& b, float t)
{
	return a + (b - a) * t;
}

// ParticleEmitter implementation

ParticleEmitter::ParticleEmitter(const ParticleEmitterConfig& config) :
	config(config),
	loop(config.loop)
{
	particles.reserve(config.max_particles);
}

ParticleEmitter::~ParticleEmitter() = default;

void ParticleEmitter::update(float delta_time)
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
		float t = 1.0f - (particle.lifetime / particle.max_lifetime);
		particle.color = lerp_color(config.start_color, config.end_color, t);
	}

	// Remove dead particles
	particles.erase(
		std::remove_if(particles.begin(), particles.end(),
			[](const Particle& p) { return p.lifetime <= 0.0f; }),
		particles.end()
	);
}

void ParticleEmitter::emit(uint32_t count)
{
	for (uint32_t i = 0; i < count && particles.size() < config.max_particles; ++i)
	{
		Particle particle;
		particle.position = position;
		particle.lifetime = random_float(config.min_lifetime, config.max_lifetime);
		particle.max_lifetime = particle.lifetime;
		particle.size = random_float(config.min_size, config.max_size);
		particle.rotation = random_float(0.0f, 6.28318f); // 0 to 2*PI
		particle.rotation_speed = random_float(config.rotation_speed_min, config.rotation_speed_max);
		particle.velocity = random_vec3(config.velocity_min, config.velocity_max);
		particle.color = config.start_color;
		
		particles.push_back(particle);
	}
}

// ParticleSystem implementation

ParticleSystem::ParticleSystem() = default;
ParticleSystem::~ParticleSystem() = default;

void ParticleSystem::update(float delta_time)
{
	for (auto& emitter : emitters)
	{
		emitter->update(delta_time);
	}
	
	// Remove dead emitters (non-looping emitters with no particles)
	emitters.erase(
		std::remove_if(emitters.begin(), emitters.end(),
			[](const std::unique_ptr<ParticleEmitter>& e) { return !e->is_alive(); }),
		emitters.end()
	);
}

ParticleEmitter& ParticleSystem::create_emitter(ObjectID object_id, const ParticleEmitterConfig& config)
{
	auto emitter = std::make_unique<ParticleEmitter>(config);
	emitter->set_position(get_ecs().get_object(object_id).get_position());
	emitters.push_back(std::move(emitter));
	
	size_t index = emitters.size() - 1;
	object_emitters[object_id].push_back(index);
	
	return *emitters[index];
}

void ParticleSystem::add_emitter(std::unique_ptr<ParticleEmitter>&& emitter)
{
	emitters.push_back(std::move(emitter));
}

void ParticleSystem::remove_emitters(ObjectID object_id)
{
	auto it = object_emitters.find(object_id);
	if (it != object_emitters.end())
	{
		// Mark emitters for removal by making them non-looping and setting lifetime to 0
		for (size_t index : it->second)
		{
			if (index < emitters.size() && emitters[index])
			{
				emitters[index]->set_enabled(false);
			}
		}
		object_emitters.erase(it);
	}
}

void ParticleSystem::prepare_render_data(std::vector<SDS::ParticleInstanceData>& out_instance_data)
{
	out_instance_data.clear();
	
	for (const auto& emitter : emitters)
	{
		for (const auto& particle : emitter->get_particles())
		{
			SDS::ParticleInstanceData instance;
			instance.model = glm::translate(glm::mat4(1.0f), particle.position);
			instance.color = particle.color;
			instance.size = particle.size;
			instance.rotation = particle.rotation;
			
			out_instance_data.push_back(instance);
		}
	}
}
