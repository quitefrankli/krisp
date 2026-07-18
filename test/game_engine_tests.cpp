#include <game_engine.hpp>
#include <camera.hpp>
#include <iapplication.hpp>
#include <interface/gizmo.hpp>

#include "test_helper.hpp"
#include "mock_graphics_engine.hpp"
#include "mock_window.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include "utility.hpp"

#include <gtest/gtest.h>

#include <filesystem>


class GameEngineTestsMockGraphicsEngine : public MockGraphicsEngine
{
public:
	struct MaterialUpdate
	{
		ObjectID object_id;
		size_t renderable_index;
		MaterialID diffuse;
		std::optional<MaterialID> normal;
		std::optional<MaterialID> specular;
		std::vector<MaterialID> retired;
	};

	virtual void handle_command(DestroyResourcesCmd& cmd) override
	{
		for (const auto& mesh_id : cmd.mesh_ids)
		{
			meshes_to_destroy.push_back(mesh_id);
		}
		for (const auto& material_id : cmd.material_ids)
		{
			materials_to_destroy.push_back(material_id);
		}
	}
	void handle_command(UpdateRenderableMaterialsCmd& cmd) override
	{
		material_updates.push_back({
			cmd.object_id,
			cmd.renderable_index,
			cmd.diffuse_material,
			cmd.normal_material,
			cmd.specular_material,
			cmd.retired_materials,
		});
	}

	std::vector<MeshID> meshes_to_destroy;
	std::vector<MaterialID> materials_to_destroy;
	std::vector<MaterialUpdate> material_updates;
};

class TestableGameEngine : public GameEngine
{
public:
	TestableGameEngine() :
		GameEngine(std::make_unique<MockWindow>(), 
				   std::make_unique<DummyApplication>(), 
				   std::make_unique<GameEngineTestsMockGraphicsEngine>())
	{
	}
};

class GameEngineTests : public testing::Test
{
public:
	TestableGameEngine engine;
	GameEngineTestsMockGraphicsEngine& get_mock_gfx() { return static_cast<GameEngineTestsMockGraphicsEngine&>(engine.get_graphics_engine()); }
};

TEST_F(GameEngineTests, Constructor)
{
	EXPECT_EQ(engine.get_window().get_glfw_window(), nullptr);
}

TEST_F(GameEngineTests, spawn_cubemap_creates_a_generic_object)
{
	engine.spawn_cubemap();

	ASSERT_EQ(engine.get_objects().size(), 1);
	const auto& object = engine.get_objects().begin()->second;
	ASSERT_EQ(object->renderables.size(), 1);
	EXPECT_EQ(object->renderables.front().pipeline_render_type, ERenderType::CUBEMAP);
	EXPECT_EQ(object->renderables.front().material_ids.size(), 6);
}

TEST_F(GameEngineTests, scene_load_remaps_released_color_materials)
{
	ColorMaterial color;
	color.data.diffuse = { 0.2f, 0.3f, 0.4f };
	const MaterialID original_material = MaterialSystem::add(std::make_unique<ColorMaterial>(color));
	auto& object = engine.spawn_object<Object>(Renderable{
		MeshFactory::cube_id(), { original_material }, ERenderType::COLOR });
	const ObjectID object_id = object.get_id();
	const auto path = std::filesystem::temp_directory_path() / "krisp_scene_material_test.yaml";

	engine.save_scene(path);
	engine.load_scene(path);
	std::filesystem::remove(path);

	const auto* restored = engine.get_object(object_id);
	ASSERT_NE(restored, nullptr);
	ASSERT_EQ(restored->renderables.size(), 1);
	EXPECT_NE(restored->renderables[0].material_ids.front(), original_material);
	EXPECT_TRUE(MaterialSystem::contains(restored->renderables[0].material_ids.front()));
}

TEST_F(GameEngineTests, scene_load_remaps_released_texture_materials)
{
	const MaterialID original_material = ResourceLoader::fetch_texture("texture1.jpg");
	auto& object = engine.spawn_object<Object>(Renderable{
		MeshFactory::cube_id(), { original_material }, ERenderType::STANDARD });
	const ObjectID object_id = object.get_id();
	const auto path = std::filesystem::temp_directory_path() / "krisp_scene_texture_test.yaml";

	engine.save_scene(path);
	engine.load_scene(path);
	std::filesystem::remove(path);

	const auto* restored = engine.get_object(object_id);
	ASSERT_NE(restored, nullptr);
	EXPECT_TRUE(MaterialSystem::contains(restored->renderables[0].material_ids.front()));
}

TEST_F(GameEngineTests, scene_load_restores_camera_state)
{
	auto& camera = engine.get_camera();
	camera.look_at({ 1.0f, 2.0f, 3.0f }, { -4.0f, 5.0f, -6.0f });
	camera.set_mode(Camera::Mode::FPV);
	camera.set_orthographic_projection({ -3.0f, 7.0f });
	camera.toggle_projection();
	const auto path = std::filesystem::temp_directory_path() / "krisp_scene_camera_test.yaml";

	engine.save_scene(path);
	camera.look_at(Maths::zero_vec, { 0.0f, 3.0f, -3.0f });
	camera.set_mode(Camera::Mode::ORBIT);
	camera.toggle_projection();
	engine.load_scene(path);
	std::filesystem::remove(path);

	EXPECT_TRUE(glm_equal(camera.get_position(), glm::vec3(-4.0f, 5.0f, -6.0f)));
	EXPECT_TRUE(glm_equal(camera.get_focus(), glm::vec3(1.0f, 2.0f, 3.0f)));
	EXPECT_EQ(camera.get_mode(), Camera::Mode::FPV);
	EXPECT_GT(glm::distance(camera.get_ray({ 0.0f, 0.0f }).origin, camera.get_position()), 0.01f);
}

TEST_F(GameEngineTests, scene_load_ignores_transient_gizmo_parent)
{
	auto& object = engine.spawn_object<Object>();
	object.set_position({ 2.0f, 3.0f, 4.0f });
	const ObjectID object_id = object.get_id();
	engine.get_gizmo().init();
	engine.get_gizmo().select_object(&object);
	const auto path = std::filesystem::temp_directory_path() / "krisp_scene_gizmo_test.yaml";

	engine.save_scene(path);
	EXPECT_FALSE(engine.get_gizmo().is_active());
	EXPECT_NO_THROW(engine.load_scene(path));
	std::filesystem::remove(path);

	const auto* restored = engine.get_object(object_id);
	ASSERT_NE(restored, nullptr);
	EXPECT_TRUE(glm_equal(restored->get_position(), glm::vec3(2.0f, 3.0f, 4.0f)));
	EXPECT_FALSE(engine.get_gizmo().is_active());
}

TEST(GameEngineOwnershipTests, engines_have_isolated_ecs_instances)
{
	TestableGameEngine first;
	TestableGameEngine second;
	first.get_ecs().add_light_source(ObjectID::generate_new_id(), LightComponent{});

	EXPECT_NE(&first.get_ecs(), &second.get_ecs());
	EXPECT_TRUE(first.get_ecs().has_light_source());
	EXPECT_FALSE(second.get_ecs().has_light_source());
}

TEST_F(GameEngineTests, reset_scene_replaces_ecs_state_and_preserves_its_address)
{
	ECS* const original_ecs = &engine.get_ecs();
	auto& object = engine.spawn_object<Object>();
	const ObjectID object_id = object.get_id();
	engine.get_ecs().add_light_source(object.get_id(), LightComponent{});
	engine.get_ecs().add_collider(object.get_id(), std::make_unique<BoxCollider>());
	engine.get_ecs().add_physics_entity(object.get_id(), PhysicsComponent{});
	engine.get_ecs().spawn_particle_emitter(object.get_id(), ParticleEmitterConfig{});
	Bone root;
	root.name = "Root";
	const SkeletonID skeleton = engine.get_ecs().add_skeleton({ root });
	const auto rig = make_skeletal_rig_signature(engine.get_ecs().get_skeletal_component(skeleton).get_bones());
	engine.get_ecs().add_skeletal_animation("Idle", { BoneAnimation{} }, rig);

	engine.reset_scene();

	EXPECT_EQ(&engine.get_ecs(), original_ecs);
	EXPECT_EQ(engine.get_object(object_id), nullptr);
	EXPECT_FALSE(engine.get_ecs().has_light_source());
	EXPECT_TRUE(engine.get_ecs().get_all_colliders().empty());
	EXPECT_TRUE(engine.get_ecs().get_skeletal_animations().empty());
	EXPECT_EQ(engine.get_ecs()._get_physics_component(object_id), nullptr);
	std::vector<SDS::ParticleInstanceData> particles;
	engine.get_ecs().prepare_render_data(particles);
	EXPECT_TRUE(particles.empty());
	EXPECT_NO_THROW(engine.get_ecs().spawn_tileset(1, 1, 1.0f));
	engine.reset_scene();
}

TEST_F(GameEngineTests, main_loop)
{
    engine.main_loop(1.0f);
}

TEST_F(GameEngineTests, spawning_and_deleting_objects)
{
	auto& obj = engine.spawn_object(std::make_shared<Object>());
	const auto id = obj.get_id();
	ASSERT_TRUE(engine.get_object(id));

	engine.delete_object(id);
	engine.get_graphics_engine().increment_num_objs_deleted();
	engine.main_loop(1.0f);
	ASSERT_FALSE(engine.get_object(id));
}

TEST_F(GameEngineTests, resource_cleanup)
{
	const auto mesh_id = MeshSystem::add(MeshFactory::icosahedron());
	const auto material_id = MaterialSystem::add(std::make_unique<ColorMaterial>());
	Renderable renderable;
	renderable.mesh_id = mesh_id;
	renderable.material_ids = { material_id };
	auto& obj = engine.spawn_object(std::make_shared<Object>(renderable));
	const auto obj_id = obj.get_id();

	engine.delete_object(obj_id);
	engine.get_graphics_engine().increment_num_objs_deleted();
	engine.main_loop(1.0f);

	ASSERT_EQ(get_mock_gfx().meshes_to_destroy.size(), 1);
	ASSERT_EQ(get_mock_gfx().materials_to_destroy.size(), 1);

	ASSERT_EQ(get_mock_gfx().meshes_to_destroy[0], mesh_id);
	ASSERT_EQ(get_mock_gfx().materials_to_destroy[0], material_id);
}

TEST_F(GameEngineTests, dont_cleanup_resource_if_not_ready)
{
	const auto mesh_id = MeshSystem::add(MeshFactory::icosahedron());
	const auto material_id = MaterialSystem::add(std::make_unique<ColorMaterial>());
	MaterialSystem::register_owner(material_id); // multiple owners of this material
	Renderable renderable;
	renderable.mesh_id = mesh_id;
	renderable.material_ids = { material_id };
	auto& obj = engine.spawn_object(std::make_shared<Object>(renderable));
	const auto obj_id = obj.get_id();

	engine.delete_object(obj_id);
	engine.get_graphics_engine().increment_num_objs_deleted();
	engine.main_loop(1.0f);

	ASSERT_EQ(get_mock_gfx().meshes_to_destroy.size(), 1);
	ASSERT_EQ(get_mock_gfx().materials_to_destroy.size(), 0);

	ASSERT_EQ(get_mock_gfx().meshes_to_destroy[0], mesh_id);
}

TEST_F(GameEngineTests, shared_renderable_resources_are_retained_for_each_spawned_object)
{
	const auto mesh_id = MeshSystem::add(MeshFactory::icosahedron());
	const auto material_id = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const Renderable renderable{
		.mesh_id = mesh_id,
		.material_ids = { material_id },
	};
	const auto first_id = engine.spawn_object<Object>(renderable).get_id();
	const auto second_id = engine.spawn_object<Object>(renderable).get_id();

	EXPECT_EQ(MeshSystem::get_num_owners(mesh_id), 2);
	EXPECT_EQ(MaterialSystem::get_num_owners(material_id), 2);

	engine.delete_object(first_id);
	engine.main_loop(1.0f);
	EXPECT_TRUE(MeshSystem::contains(mesh_id));
	EXPECT_TRUE(MaterialSystem::contains(material_id));
	EXPECT_TRUE(get_mock_gfx().meshes_to_destroy.empty());
	EXPECT_TRUE(get_mock_gfx().materials_to_destroy.empty());

	engine.delete_object(second_id);
	engine.main_loop(1.0f);
	EXPECT_FALSE(MeshSystem::contains(mesh_id));
	EXPECT_FALSE(MaterialSystem::contains(material_id));
	EXPECT_EQ(get_mock_gfx().meshes_to_destroy, (std::vector<MeshID>{ mesh_id }));
	EXPECT_EQ(get_mock_gfx().materials_to_destroy, (std::vector<MaterialID>{ material_id }));
}

TEST_F(GameEngineTests, deleting_multiple_objects_destroys_each_resource_once)
{
	std::vector<MeshID> mesh_ids;
	std::vector<MaterialID> material_ids;
	std::vector<ObjectID> object_ids;
	for (int i = 0; i < 2; ++i)
	{
		mesh_ids.push_back(MeshSystem::add(MeshFactory::icosahedron()));
		material_ids.push_back(MaterialSystem::add(std::make_unique<ColorMaterial>()));
		Renderable renderable;
		renderable.mesh_id = mesh_ids.back();
		renderable.material_ids = { material_ids.back() };
		object_ids.push_back(engine.spawn_object(std::make_shared<Object>(renderable)).get_id());
	}

	for (const auto object_id : object_ids)
		engine.delete_object(object_id);
	engine.main_loop(1.0f);

	ASSERT_EQ(get_mock_gfx().meshes_to_destroy.size(), 2);
	ASSERT_EQ(get_mock_gfx().materials_to_destroy.size(), 2);
	EXPECT_EQ(get_mock_gfx().meshes_to_destroy, mesh_ids);
	EXPECT_EQ(get_mock_gfx().materials_to_destroy, material_ids);
}

TEST_F(GameEngineTests, deleting_imported_object_with_shared_normal_material_is_safe)
{
	const auto path = Utility::get_top_level_path()/"test/data/normal_mapped_shared_material.gltf";
	auto model = ResourceLoader::load_model(engine.get_ecs(), path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	const auto material_ids = model.meshes[0].renderables[0].material_ids;
	ASSERT_EQ(material_ids.size(), 2);

	auto& object = engine.spawn_object<Object>(model.meshes[0].renderables);
	engine.delete_object(object.get_id());
	engine.main_loop(1.0f);

	ASSERT_EQ(get_mock_gfx().materials_to_destroy.size(), 2);
	EXPECT_EQ(get_mock_gfx().materials_to_destroy, material_ids);
	EXPECT_EQ(MaterialSystem::get_num_owners(material_ids[0]), 0);
	EXPECT_EQ(MaterialSystem::get_num_owners(material_ids[1]), 0);
}

TEST_F(GameEngineTests, deleting_object_during_skeletal_animation_is_safe)
{
	const auto path = Utility::get_top_level_path()/"test/data/simple_test_model.gltf";
	auto model = ResourceLoader::load_model(engine.get_ecs(), path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.animations.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto skeleton_id = model.meshes[0].skeleton_id;
	ASSERT_TRUE(skeleton_id.has_value());

	auto& object = engine.spawn_object<Object>(model.meshes[0].renderables, skeleton_id);
	engine.get_ecs().play_animation(*skeleton_id, model.animations[0], true);
	engine.main_loop(0.1f);

	engine.delete_object(object.get_id());
	EXPECT_NO_THROW(engine.main_loop(0.1f));
}

TEST_F(GameEngineTests, replaces_one_renderable_texture_and_preserves_other_slots)
{
	const auto old_diffuse = ResourceLoader::fetch_texture("texture5.jpg");
	const auto old_normal = ResourceLoader::fetch_texture(
		"texture6.jpg", ETextureSemantic::NORMAL);
	Renderable first;
	first.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
	first.material_ids = { old_diffuse, old_normal };
	first.pipeline_render_type = ERenderType::STANDARD;
	Renderable second = first;
	MaterialSystem::register_owner(old_diffuse);
	MaterialSystem::register_owner(old_normal);
	auto& object = engine.spawn_object<Object>(std::vector<Renderable>{ first, second });
	const auto old_diffuse_owners = MaterialSystem::get_num_owners(old_diffuse);
	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR, Utility::get_texture("texture5.jpg"));
	EXPECT_TRUE(get_mock_gfx().material_updates.empty());
	EXPECT_EQ(MaterialSystem::get_num_owners(old_diffuse), old_diffuse_owners);

	engine.replace_renderable_texture(
		object.get_id(), 1, ETextureSemantic::BASE_COLOR, Utility::get_texture("texture4.png"));

	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	const auto& update = get_mock_gfx().material_updates[0];
	EXPECT_EQ(update.object_id, object.get_id());
	EXPECT_EQ(update.renderable_index, 1);
	EXPECT_EQ(update.normal, old_normal);
	EXPECT_TRUE(update.retired.empty());
	EXPECT_EQ(object.renderables[0].material_ids, (MatVec{ old_diffuse, old_normal }));
	ASSERT_EQ(object.renderables[1].material_ids.size(), 2);
	EXPECT_EQ(object.renderables[1].material_ids[0], update.diffuse);
	EXPECT_EQ(object.renderables[1].material_ids[1], old_normal);
	EXPECT_NE(update.diffuse, old_diffuse);
	const auto& replacement = dynamic_cast<const TextureMaterial&>(MaterialSystem::get(update.diffuse));
	EXPECT_EQ(replacement.semantic, ETextureSemantic::BASE_COLOR);
}

TEST_F(GameEngineTests, texture_replacement_keeps_scene_resource_references_in_sync)
{
	const auto diffuse = ResourceLoader::fetch_texture("texture2.jpg");
	Renderable renderable;
	renderable.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
	renderable.material_ids = { diffuse };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(renderable);

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR, Utility::get_texture("texture4.png"));

	const auto path = std::filesystem::temp_directory_path() / "krisp_scene_replaced_texture_test.yaml";
	engine.save_scene(path);
	EXPECT_NO_THROW(engine.load_scene(path));
	std::filesystem::remove(path);
}

TEST_F(GameEngineTests, removes_normal_and_uses_white_for_missing_diffuse)
{
	const auto diffuse = ResourceLoader::fetch_texture("texture2.jpg");
	const auto normal = ResourceLoader::fetch_texture("texture3.jpg", ETextureSemantic::NORMAL);
	Renderable renderable;
	renderable.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
	renderable.material_ids = { diffuse, normal };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(renderable);

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::NORMAL, std::nullopt);
	ASSERT_EQ(object.renderables[0].material_ids, (MatVec{ diffuse }));
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	EXPECT_FALSE(get_mock_gfx().material_updates[0].normal);
	EXPECT_EQ(get_mock_gfx().material_updates[0].retired, (MatVec{ normal }));
	EXPECT_FALSE(MaterialSystem::contains(normal));

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR, std::nullopt);
	const auto white = MaterialFactory::fetch_white_texture();
	EXPECT_EQ(object.renderables[0].material_ids, (MatVec{ white }));
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 2);
	EXPECT_EQ(get_mock_gfx().material_updates[1].diffuse, white);
	EXPECT_EQ(get_mock_gfx().material_updates[1].retired, (MatVec{ diffuse }));
	EXPECT_FALSE(MaterialSystem::contains(diffuse));
}

TEST_F(GameEngineTests, replaces_specular_maps)
{
	const auto diffuse = ResourceLoader::fetch_texture("texture2.jpg");
	Renderable renderable;
	renderable.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
	renderable.material_ids = { diffuse };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(renderable);

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::SPECULAR,
		Utility::get_texture("texture4.png"));
	const TexturedMatGroup group(object.renderables[0].material_ids);
	ASSERT_TRUE(group.specular_mat);
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	const auto& update = get_mock_gfx().material_updates.back();
	EXPECT_EQ(update.specular, group.specular_mat);
}

TEST_F(GameEngineTests, sets_a_matte_specular_fallback)
{
	const auto diffuse = ResourceLoader::fetch_texture("texture2.jpg");
	Renderable renderable;
	renderable.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
	renderable.material_ids = { diffuse };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(renderable);

	engine.set_renderable_specular_matte(object.get_id(), 0);

	const TexturedMatGroup group(object.renderables[0].material_ids);
	ASSERT_TRUE(group.specular_mat);
	EXPECT_EQ(*group.specular_mat, MaterialFactory::fetch_black_texture());
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	EXPECT_EQ(get_mock_gfx().material_updates.back().specular, group.specular_mat);
}

TEST_F(GameEngineTests, rejected_texture_replacements_leave_materials_unchanged)
{
	const auto material = ResourceLoader::fetch_texture("texture1.jpg");
	Renderable textured;
	textured.mesh_id = MeshFactory::cube_id(MeshFactory::EVertexType::TEXTURE);
	textured.material_ids = { material };
	textured.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(textured);
	const auto original = object.renderables[0].material_ids;

	EXPECT_THROW(engine.replace_renderable_texture(
		object.get_id(), 1, ETextureSemantic::BASE_COLOR, std::nullopt), std::runtime_error);
	EXPECT_THROW(engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR,
		Utility::get_top_level_path()/"test/data/does_not_exist.png"), ResourceLoadError);
	EXPECT_EQ(object.renderables[0].material_ids, original);
	EXPECT_TRUE(get_mock_gfx().material_updates.empty());

	Renderable colour = Renderable::make_default(
		MeshFactory::cube_id(MeshFactory::EVertexType::COLOR));
	auto& colour_object = engine.spawn_object<Object>(colour);
	EXPECT_THROW(engine.replace_renderable_texture(
		colour_object.get_id(), 0, ETextureSemantic::BASE_COLOR, std::nullopt), std::runtime_error);
	EXPECT_TRUE(get_mock_gfx().material_updates.empty());
}
