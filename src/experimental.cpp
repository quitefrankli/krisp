#include "experimental.hpp"
#include "utility.hpp"
#include "entity_component_system/particle_system.hpp"
#include "camera.hpp"
#include "renderable/renderable.hpp"

#include <fmt/core.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <quill/LogMacros.h>


void spawn_test_particles(GameEngine& engine);

void Experimental::process()
{
    fmt::print("Experimental process triggered!\n");

    ma_engine engine;
    ma_engine_init(nullptr, &engine);
    auto audio_file = Utility::get_audio_path() / "wav2.wav";
    ma_engine_play_sound(&engine, audio_file.string().c_str(), nullptr);
	spawn_test_particles(this->engine);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    ma_engine_uninit(&engine);
}

void Experimental::process(float time_delta)
{
}

void spawn_test_particles(GameEngine& engine)
{
	// Create a test particle emitter at the camera position
	ParticleEmitterConfig config;
	config.max_particles = 500;
	config.emission_rate = 200.0f;        // 200 particles per second burst
	config.min_lifetime = 0.5f;
	config.max_lifetime = 3.0f;
	config.min_size = 0.05f;
	config.max_size = 0.15f;
	config.start_color = { 1.0f, 0.8f, 0.2f, 1.0f };  // Yellow-orange
	config.end_color = { 1.0f, 0.2f, 0.0f, 0.0f };    // Fade to transparent red
	config.velocity_min = { -2.0f, -2.0f, -2.0f };
	config.velocity_max = { 2.0f, 2.0f, 2.0f };
	config.rotation_speed_min = -3.0f;
	config.rotation_speed_max = 3.0f;
	config.loop = false;  // One-shot burst
	
	auto& emitter1 = engine.spawn_particle_emitter(config);
	// engine.get_ecs().add_clickable_entity(emitter1.get_id());
	// engine.get_ecs().add_collider(emitter1.get_id(), std::make_unique<SphereCollider>());
	// emitter1.set_visibility(true);
	emitter1.set_position(engine.get_camera().get_position());
	
	// Also spawn a second emitter with different colors slightly offset
	config.start_color = { 0.2f, 0.8f, 1.0f, 1.0f };  // Cyan
	config.end_color = { 0.0f, 0.2f, 1.0f, 0.0f };    // Fade to transparent blue
	config.velocity_min = { -1.0f, 0.0f, -1.0f };
	config.velocity_max = { 1.0f, 3.0f, 1.0f };       // Upward bias
	
	auto& emitter2 = engine.spawn_particle_emitter(config);
	// engine.get_ecs().add_clickable_entity(emitter2.get_id());
	// engine.get_ecs().add_collider(emitter2.get_id(), std::make_unique<SphereCollider>());
	// emitter2.set_visibility(true);
	glm::vec3 offset_pos = engine.get_camera().get_position() + glm::vec3(0.5f, 0.0f, 0.0f);
	emitter2.set_position(offset_pos);
	
	LOG_INFO(Utility::get_logger(), "Spawned test particles at camera position");
}
