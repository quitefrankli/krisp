#include "experimental.hpp"
#include "utility.hpp"
#include "entity_component_system/particle_system.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include "camera.hpp"
#include "renderable/renderable.hpp"
#include "renderable/mesh.hpp"
#include "renderable/mesh_maths.hpp"
#include "renderable/material.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"

#include <fmt/core.h>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <quill/LogMacros.h>

#include <cmath>
#include <random>
#include <algorithm>
#include <vector>


void spawn_test_particles(GameEngine& engine);
MeshPtr generate_terrain_mesh(int grid_size, float scale, float height_scale, MaterialID texture_mat_id);
MaterialID generate_terrain_texture(int width, int height);

// Simple Perlin noise implementation for terrain generation
class PerlinNoise
{
private:
    std::vector<int> permutation;
    static constexpr int PERMUTATION_SIZE = 256;

    static float fade(float t)
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static float lerp(float a, float b, float t)
    {
        return a + t * (b - a);
    }

    static float grad(int hash, float x, float y, float z)
    {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

public:
    PerlinNoise(uint32_t seed = 0)
    {
        permutation.resize(PERMUTATION_SIZE * 2);
        
        // Initialize permutation table
        std::iota(permutation.begin(), permutation.begin() + PERMUTATION_SIZE, 0);
        
        // Shuffle with seed
        std::mt19937 generator(seed);
        std::shuffle(permutation.begin(), permutation.begin() + PERMUTATION_SIZE, generator);
        
        // Duplicate for overflow safety
        for (int i = 0; i < PERMUTATION_SIZE; i++)
        {
            permutation[PERMUTATION_SIZE + i] = permutation[i];
        }
    }

    float noise(float x, float y, float z = 0.0f) const
    {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        int Z = static_cast<int>(std::floor(z)) & 255;

        x -= std::floor(x);
        y -= std::floor(y);
        z -= std::floor(z);

        float u = fade(x);
        float v = fade(y);
        float w = fade(z);

        int A = permutation[X] + Y;
        int AA = permutation[A] + Z;
        int AB = permutation[A + 1] + Z;
        int B = permutation[X + 1] + Y;
        int BA = permutation[B] + Z;
        int BB = permutation[B + 1] + Z;

        return lerp(
            lerp(
                lerp(grad(permutation[AA], x, y, z), grad(permutation[BA], x - 1, y, z), u),
                lerp(grad(permutation[AB], x, y - 1, z), grad(permutation[BB], x - 1, y - 1, z), u),
                v
            ),
            lerp(
                lerp(grad(permutation[AA + 1], x, y, z - 1), grad(permutation[BA + 1], x - 1, y, z - 1), u),
                lerp(grad(permutation[AB + 1], x, y - 1, z - 1), grad(permutation[BB + 1], x - 1, y - 1, z - 1), u),
                v
            ),
            w
        );
    }

    // Octave noise for more detailed terrain
    float octave_noise(float x, float y, int octaves, float persistence = 0.5f) const
    {
        float total = 0.0f;
        float frequency = 1.0f;
        float amplitude = 1.0f;
        float max_amplitude = 0.0f;

        for (int i = 0; i < octaves; i++)
        {
            total += noise(x * frequency, y * frequency) * amplitude;
            max_amplitude += amplitude;
            amplitude *= persistence;
            frequency *= 2.0f;
        }

        return total / max_amplitude;
    }
};

// Custom texture data holder for procedurally generated textures
struct ProceduralTextureData : public TextureData
{
    std::vector<std::byte> data;

    ProceduralTextureData(size_t size) : data(size) {}

    virtual std::byte* get() override 
    { 
        return data.data(); 
    }
};

// Generate a colorful terrain texture using Perlin noise
MaterialID generate_terrain_texture(int width, int height)
{
    PerlinNoise perlin(42);
    PerlinNoise color_variation(123);
    
    TextureMaterial material;
    material.width = width;
    material.height = height;
    material.channels = 4; // RGBA
    material.data_len = width * height * 4;
    
    auto texture_data = std::make_unique<ProceduralTextureData>(material.data_len);
    auto* pixels = reinterpret_cast<uint8_t*>(texture_data->get());
    
    const int octaves = 4;
    const float scale = 4.0f; // Texture coordinate scale
    
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            // Normalize to 0-1 range
            float nx = (x / (float)width) * scale;
            float ny = (y / (float)height) * scale;
            
            // Get height value for biome determination
            float height_val = perlin.octave_noise(nx, ny, octaves, 0.5f);
            height_val += perlin.octave_noise(nx * 2.0f, ny * 2.0f, 2, 0.5f) * 0.3f;
            float normalized_height = (height_val + 1.0f) * 0.5f;
            
            // Biome noise for color patches
            float biome_noise = color_variation.octave_noise(nx * 0.5f, ny * 0.5f, 2, 0.5f);
            
            glm::vec3 color;
            
            if (normalized_height < 0.15f)
            {
                // Water level - deep blue to turquoise
                color = glm::mix(
                    glm::vec3(0.0f, 0.3f, 0.6f),
                    glm::vec3(0.0f, 0.6f, 0.8f),
                    normalized_height / 0.15f
                );
                // Add shimmer
                color += glm::vec3(0.1f, 0.15f, 0.2f) * color_variation.noise(nx * 16.0f, ny * 16.0f);
            }
            else if (normalized_height < 0.35f)
            {
                // Beach/sand
                color = glm::mix(
                    glm::vec3(0.95f, 0.85f, 0.4f),
                    glm::vec3(0.9f, 0.7f, 0.3f),
                    (normalized_height - 0.15f) / 0.2f
                );
                color += glm::vec3(0.05f, 0.05f, 0.1f) * biome_noise;
            }
            else if (normalized_height < 0.55f)
            {
                // Grasslands - vibrant greens
                glm::vec3 grass_dark(0.1f, 0.5f, 0.1f);
                glm::vec3 grass_bright(0.4f, 0.8f, 0.2f);
                glm::vec3 grass_golden(0.6f, 0.7f, 0.1f);
                
                float t = (normalized_height - 0.35f) / 0.2f;
                if (biome_noise > 0.3f) {
                    color = glm::mix(grass_dark, grass_bright, t);
                } else {
                    color = glm::mix(grass_dark, grass_golden, t);
                }
                
                // Flower patches
                float flower_noise = color_variation.noise(nx * 12.0f, ny * 12.0f);
                if (flower_noise > 0.75f) {
                    glm::vec3 flower_color = glm::mix(
                        glm::vec3(0.9f, 0.4f, 0.7f),
                        glm::vec3(0.7f, 0.3f, 0.9f),
                        color_variation.noise(nx * 20.0f, ny * 20.0f)
                    );
                    color = glm::mix(color, flower_color, 0.6f);
                }
            }
            else if (normalized_height < 0.75f)
            {
                // Forest - greens to autumn colors
                glm::vec3 forest_green(0.0f, 0.4f, 0.15f);
                glm::vec3 autumn_orange(0.9f, 0.5f, 0.1f);
                glm::vec3 autumn_red(0.8f, 0.2f, 0.1f);
                
                float t = (normalized_height - 0.55f) / 0.2f;
                if (biome_noise > 0.4f) {
                    color = glm::mix(forest_green, autumn_orange, t);
                } else {
                    color = glm::mix(forest_green, autumn_red, t);
                }
            }
            else if (normalized_height < 0.9f)
            {
                // Rocky mountains
                glm::vec3 rock_brown(0.5f, 0.35f, 0.2f);
                glm::vec3 rock_gray(0.5f, 0.5f, 0.55f);
                glm::vec3 rock_purple(0.4f, 0.35f, 0.5f);
                
                float t = (normalized_height - 0.75f) / 0.15f;
                if (biome_noise > 0.0f) {
                    color = glm::mix(rock_brown, rock_gray, t);
                } else {
                    color = glm::mix(rock_brown, rock_purple, t);
                }
                
                color += glm::vec3(0.1f, 0.08f, 0.05f) * color_variation.noise(nx * 8.0f, ny * 8.0f);
            }
            else
            {
                // Snow peaks
                glm::vec3 snow_white(0.95f, 0.95f, 0.98f);
                glm::vec3 ice_blue(0.7f, 0.85f, 0.95f);
                glm::vec3 snow_pink(0.95f, 0.9f, 0.92f);
                
                float t = (normalized_height - 0.9f) / 0.1f;
                if (biome_noise > 0.5f) {
                    color = glm::mix(snow_white, ice_blue, t);
                } else {
                    color = glm::mix(snow_white, snow_pink, t);
                }
                
                color += glm::vec3(0.1f) * std::max(0.0f, color_variation.noise(nx * 32.0f, ny * 32.0f) - 0.5f);
            }
            
            // Fine texture noise
            float texture_noise = color_variation.noise(nx * 24.0f, ny * 24.0f) * 0.08f;
            color += glm::vec3(texture_noise);
            color = glm::clamp(color, 0.0f, 1.0f);
            
            // Write pixel (RGBA)
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(color.r * 255);
            pixels[idx + 1] = static_cast<uint8_t>(color.g * 255);
            pixels[idx + 2] = static_cast<uint8_t>(color.b * 255);
            pixels[idx + 3] = 255; // Full alpha
        }
    }
    
    material.data = std::move(texture_data);
    return MaterialSystem::add(std::make_unique<TextureMaterial>(std::move(material)));
}

// Generate a 3D terrain mesh using Perlin noise with texture coordinates
MeshPtr generate_terrain_mesh(int grid_size, float scale, float height_scale, MaterialID texture_mat_id)
{
    TexVertices vertices;
    VertexIndices indices;

    PerlinNoise perlin(42);
    const int octaves = 4;

    // Generate vertices in a grid
    for (int z = 0; z <= grid_size; z++)
    {
        for (int x = 0; x <= grid_size; x++)
        {
            // Normalize coordinates to center the terrain
            float nx = (x - grid_size * 0.5f) * scale;
            float nz = (z - grid_size * 0.5f) * scale;

            // Get height using multiple octaves of Perlin noise
            float height = perlin.octave_noise(nx, nz, octaves, 0.5f);
            
            // Add some finer detail with higher frequency noise
            height += perlin.octave_noise(nx * 2.0f, nz * 2.0f, 2, 0.5f) * 0.3f;
            height += perlin.octave_noise(nx * 4.0f, nz * 4.0f, 1, 0.5f) * 0.15f;
            
            // Scale height
            float y = height * height_scale;

            // Texture coordinates (repeat 4 times across the terrain)
            float u = (x / (float)grid_size) * 4.0f;
            float v = (z / (float)grid_size) * 4.0f;

            vertices.emplace_back(SDS::TexVertex{
                glm::vec3(static_cast<float>(x) - grid_size * 0.5f, y, static_cast<float>(z) - grid_size * 0.5f),
                glm::vec3(0.0f, 1.0f, 0.0f), // Placeholder normal, will be calculated
                glm::vec2(u, v)
            });
        }
    }

    // Generate indices for triangles
    for (int z = 0; z < grid_size; z++)
    {
        for (int x = 0; x < grid_size; x++)
        {
            int current = z * (grid_size + 1) + x;
            int next = current + 1;
            int below = (z + 1) * (grid_size + 1) + x;
            int below_next = below + 1;

            // Two triangles per quad
            // First triangle
            indices.push_back(current);
            indices.push_back(below);
            indices.push_back(next);

            // Second triangle
            indices.push_back(next);
            indices.push_back(below);
            indices.push_back(below_next);
        }
    }

    // Generate normals for proper lighting
    generate_normals(vertices, indices);

    return std::make_unique<TexMesh>(std::move(vertices), std::move(indices));
}

void Experimental::process()
{
    fmt::print("Experimental process triggered!\n");

    // Generate the terrain texture
    LOG_INFO(Utility::get_logger(), "Generating terrain texture...");
    MaterialID terrain_texture = generate_terrain_texture(512, 512);
    
    // Generate and spawn the terrain mesh with texture
    LOG_INFO(Utility::get_logger(), "Generating terrain mesh...");
    auto terrain_mesh = generate_terrain_mesh(128, 0.05f, 8.0f, terrain_texture);
    const auto mesh_id = MeshSystem::add(std::move(terrain_mesh));
    
    // Create renderable with texture material
    Renderable renderable;
    renderable.mesh_id = mesh_id;
    renderable.material_ids = { terrain_texture };
    renderable.pipeline_render_type = ERenderType::STANDARD;
    renderable.casts_shadow = true;
    
    // Create an object with the terrain
    auto& terrain_obj = engine.spawn_object<Object>(renderable);
    terrain_obj.set_name("Perlin Terrain");
    
    // Position it below the camera for good viewing
    auto& camera = engine.get_camera();
    glm::vec3 camera_pos = camera.get_position();
    // terrain_obj.set_position(glm::vec3(camera_pos.x, camera_pos.y - 15.0f, camera_pos.z));
    
    // Scale it for good visibility
    terrain_obj.set_scale(glm::vec3(0.5f, 1.0f, 0.5f));
    
    LOG_INFO(Utility::get_logger(), "Spawned Perlin noise terrain at position ({}, {}, {})", 
             terrain_obj.get_position().x, 
             terrain_obj.get_position().y, 
             terrain_obj.get_position().z);

    ma_engine audio_engine;
    ma_engine_init(nullptr, &audio_engine);
    auto audio_file = Utility::get_audio("wav2.wav");
    ma_engine_play_sound(&audio_engine, audio_file.string().c_str(), nullptr);
    spawn_test_particles(this->engine);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    ma_engine_uninit(&audio_engine);
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
    emitter1.set_position(engine.get_camera().get_position());
    
    // Also spawn a second emitter with different colors slightly offset
    config.start_color = { 0.2f, 0.8f, 1.0f, 1.0f };  // Cyan
    config.end_color = { 0.0f, 0.2f, 1.0f, 0.0f };    // Fade to transparent blue
    config.velocity_min = { -1.0f, 0.0f, -1.0f };
    config.velocity_max = { 1.0f, 3.0f, 1.0f };       // Upward bias
    
    auto& emitter2 = engine.spawn_particle_emitter(config);
    glm::vec3 offset_pos = engine.get_camera().get_position() + glm::vec3(0.5f, 0.0f, 0.0f);
    emitter2.set_position(offset_pos);
    
    LOG_INFO(Utility::get_logger(), "Spawned test particles at camera position");
}
