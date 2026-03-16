#include <game_engine.hpp>
#include <iapplication.hpp>
#include <config.hpp>
#include <utility.hpp>
#include <objects/cubemap.hpp>
#include <renderable/mesh_factory.hpp>
#include <renderable/material_factory.hpp>
#include <resource_loader/resource_loader.hpp>
#include <entity_component_system/particle_system.hpp>
#include <entity_component_system/light_source.hpp>
#include <entity_component_system/material_system.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


int main(int argc, char* argv[])
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<DummyApplication>();

	engine.spawn_object<CubeMap>();

	// Ground
	ColorMaterial ground_mat{};
	ground_mat.data.diffuse = {0.15f, 0.25f, 0.08f};
	ground_mat.data.ambient = {0.05f, 0.08f, 0.02f};
	ground_mat.data.specular = glm::vec3(0.05f);
	ground_mat.data.shininess = 8.0f;
	const auto ground_mat_id = MaterialSystem::add(std::make_unique<ColorMaterial>(ground_mat));
	auto& ground = engine.spawn_object<Object>(Renderable{
		.mesh_id = MeshFactory::cube_id(),
		.material_ids = { ground_mat_id }
	});
	ground.set_position({0.0f, -0.55f, 0.0f});
	ground.set_scale({30.0f, 0.1f, 30.0f});

	// Campfire model
	auto campfire_model = ResourceLoader::load_model(Utility::get_model("campfire_rgba.glb"));
	for (auto& mesh : campfire_model.meshes)
	{
		auto& obj = engine.spawn_object<Object>(mesh.renderables);
		obj.set_name(mesh.name);
	}

	// Fire light source
	auto& light_obj = engine.spawn_object<Object>(Renderable{
		.mesh_id = MeshFactory::sphere_id(),
		.material_ids = { MaterialFactory::fetch_preset(EMaterialPreset::LIGHT_SOURCE) },
		.pipeline_render_type = ERenderType::COLOR
	});
	light_obj.set_position({0.0f, 0.6f, 0.0f});
	light_obj.set_scale(glm::vec3(0.05f));
	engine.get_ecs().add_light_source(light_obj.get_id(), LightComponent{
		.intensity = 1.5f,
		.color = {1.0f, 0.5f, 0.1f}
	});

	// Fire particles
	ParticleEmitterConfig fire{};
	fire.max_particles = 800;
	fire.emission_rate = 200.0f;
	fire.min_lifetime = 0.5f;
	fire.max_lifetime = 1.5f;
	fire.min_size = 0.05f;
	fire.max_size = 0.25f;
	fire.start_color = {1.0f, 0.6f, 0.0f, 1.0f};
	fire.end_color   = {1.0f, 0.1f, 0.0f, 0.0f};
	fire.velocity_min = {-0.3f, 1.5f, -0.3f};
	fire.velocity_max = { 0.3f, 3.0f,  0.3f};
	engine.spawn_particle_emitter(fire);

	// Ember/spark particles
	ParticleEmitterConfig embers{};
	embers.max_particles = 200;
	embers.emission_rate = 30.0f;
	embers.min_lifetime = 1.0f;
	embers.max_lifetime = 3.0f;
	embers.min_size = 0.02f;
	embers.max_size = 0.06f;
	embers.start_color = {1.0f, 0.9f, 0.3f, 1.0f};
	embers.end_color   = {0.5f, 0.1f, 0.0f, 0.0f};
	embers.velocity_min = {-1.0f, 2.0f, -1.0f};
	embers.velocity_max = { 1.0f, 5.0f,  1.0f};
	engine.spawn_particle_emitter(embers);

	// // Forest trees from tree model (CC0 by Kenney)
	// auto tree_model = ResourceLoader::load_model(Utility::get_model("tree_uncompressed.glb"));
	// const float tree_radius = 10.0f;
	// const int num_trees = 8;
	// for (int i = 0; i < num_trees; ++i)
	// {
	// 	const float angle = glm::radians(360.0f / num_trees * i);
	// 	const glm::vec3 pos{std::cos(angle) * tree_radius, -0.5f, std::sin(angle) * tree_radius};

	// 	std::vector<Renderable> all_renderables;
	// 	for (auto& mesh : tree_model.meshes)
	// 		all_renderables.insert(all_renderables.end(), mesh.renderables.begin(), mesh.renderables.end());

	// 	auto& tree = engine.spawn_object<Object>(all_renderables);
	// 	tree.set_position(pos);
	// 	tree.set_scale(glm::vec3(1.5f));
	// }

	engine.run();
}
