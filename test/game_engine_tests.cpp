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
