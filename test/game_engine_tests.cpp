#include <game_engine.hpp>
#include <iapplication.hpp>

#include "mock_graphics_engine.hpp"
#include "mock_window.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include "utility.hpp"

#include <gtest/gtest.h>


class GameEngineTestsMockGraphicsEngine : public MockGraphicsEngine
{
public:
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

	std::vector<MeshID> meshes_to_destroy;
	std::vector<MaterialID> materials_to_destroy;
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
	auto model = ResourceLoader::load_model(path);
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
	auto model = ResourceLoader::load_model(path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.animations.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto skeleton_id = model.meshes[0].renderables[0].skeleton_id;
	ASSERT_TRUE(skeleton_id.has_value());

	auto& object = engine.spawn_object<Object>(model.meshes[0].renderables);
	engine.get_ecs().play_animation(*skeleton_id, model.animations[0], true);
	engine.main_loop(0.1f);

	engine.delete_object(object.get_id());
	EXPECT_NO_THROW(engine.main_loop(0.1f));
}
