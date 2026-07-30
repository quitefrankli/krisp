#include <game_engine.hpp>
#include <camera.hpp>
#include <iapplication.hpp>
#include <interface/gizmo.hpp>
#include <game_objects/player_character.hpp>

#include "test_helper.hpp"
#include "mock_graphics_engine.hpp"
#include "mock_window.hpp"
#include "renderable/mesh_factory.hpp"
#include "renderable/material_factory.hpp"
#include "entity_component_system/mesh_system.hpp"
#include "entity_component_system/material_system.hpp"
#include "serialization/serializer.hpp"
#include "serialization/resource_provenance.hpp"
#include "utility.hpp"

#include <gtest/gtest.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>


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
	void handle_command(SpawnObjectCmd& cmd) override
	{
		if (on_spawn)
			on_spawn(cmd);
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
	std::function<void(SpawnObjectCmd&)> on_spawn;
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
	explicit TestableGameEngine(std::unique_ptr<IApplication> application) :
		GameEngine(std::make_unique<MockWindow>(),
				   std::move(application),
				   std::make_unique<GameEngineTestsMockGraphicsEngine>())
	{
	}

	MockWindow& get_mock_window()
	{
		return static_cast<MockWindow&>(get_window());
	}
};

class GameEngineTests : public testing::Test
{
public:
	TestableGameEngine engine;
	GameEngineTestsMockGraphicsEngine& get_mock_gfx() { return static_cast<GameEngineTestsMockGraphicsEngine&>(engine.get_graphics_engine()); }
};

namespace
{
class PlayerlessNormalApplication : public DummyApplication
{
public:
	bool allows_playerless_normal_mode() const override { return true; }
};

class MouseButtonApplication : public DummyApplication
{
public:
	explicit MouseButtonApplication(bool& pressed) : pressed(pressed) {}

	void on_mouse_button(GameEngine&, const MouseInput& input) override
	{
		if (input.eq(EMouseButton::LEFT, EKeyModifier::NONE, EInputAction::PRESS))
			pressed = true;
	}

private:
	bool& pressed;
};

std::filesystem::path save_path(const std::string_view name)
{
	return Utility::get_saves_path() / (std::string(name) + ".yaml");
}

size_t count_persistent_objects(const GameEngine& engine)
{
	return std::ranges::count_if(engine.get_objects(), [](const auto& entry) {
		return !entry.second->is_transient();
	});
}
}

TEST_F(GameEngineTests, Constructor)
{
	EXPECT_EQ(engine.get_window().get_glfw_window(), nullptr);
}

TEST_F(GameEngineTests, camera_render_helpers_are_engine_owned_transient_objects)
{
	const auto* focus = engine.get_camera().focus_obj;
	const auto* upvector = engine.get_camera().upvector_obj;

	ASSERT_NE(focus, nullptr);
	ASSERT_NE(upvector, nullptr);
	EXPECT_TRUE(focus->is_transient());
	EXPECT_TRUE(upvector->is_transient());
	EXPECT_EQ(engine.get_object(focus->get_id()), focus);
	EXPECT_EQ(engine.get_object(upvector->get_id()), upvector);
	EXPECT_EQ(count_persistent_objects(engine), 0);
}

TEST_F(GameEngineTests, tab_does_not_leave_editor_mode_without_a_player)
{
	EXPECT_EQ(engine.get_game_mode(), EGameMode::EDITOR);

	engine.key_callback({ GLFW_KEY_TAB, EKeyModifier::NONE, EInputAction::PRESS });
	EXPECT_EQ(engine.get_game_mode(), EGameMode::EDITOR);
	EXPECT_EQ(engine.get_active_player(), nullptr);
}

TEST(GameEngine, opted_in_application_can_enter_normal_mode_without_a_player)
{
	TestableGameEngine engine(std::make_unique<PlayerlessNormalApplication>());

	engine.set_game_mode(EGameMode::NORMAL);

	EXPECT_EQ(engine.get_game_mode(), EGameMode::NORMAL);
	EXPECT_EQ(engine.get_active_player(), nullptr);
	EXPECT_FALSE(static_cast<GameEngineTestsMockGraphicsEngine&>(
		engine.get_graphics_engine()).engine_ui_active);
}

TEST(GameEngine, routes_uncaptured_left_clicks_to_the_application)
{
	bool pressed = false;
	TestableGameEngine engine(std::make_unique<MouseButtonApplication>(pressed));
	const MouseInput click{
		EMouseButton::LEFT, EKeyModifier::NONE, EInputAction::PRESS };

	engine.mouse_button_callback(click, true);
	EXPECT_FALSE(pressed);

	engine.mouse_button_callback(click, false);
	EXPECT_TRUE(pressed);
}

TEST_F(GameEngineTests, tab_toggles_modes_and_cycles_available_players)
{
	auto& first = engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});
	auto& second = engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});

	engine.key_callback({ GLFW_KEY_TAB, EKeyModifier::NONE, EInputAction::PRESS });
	EXPECT_EQ(engine.get_game_mode(), EGameMode::NORMAL);
	EXPECT_EQ(engine.get_active_player(), &first);

	engine.key_callback({ GLFW_KEY_TAB, EKeyModifier::NONE, EInputAction::PRESS });
	EXPECT_EQ(engine.get_game_mode(), EGameMode::EDITOR);

	engine.key_callback({ GLFW_KEY_TAB, EKeyModifier::NONE, EInputAction::PRESS });
	EXPECT_EQ(engine.get_game_mode(), EGameMode::NORMAL);
	EXPECT_EQ(engine.get_active_player(), &second);
}

TEST_F(GameEngineTests, normal_mode_captures_the_cursor_and_editor_mode_releases_it)
{
	engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});

	engine.set_game_mode(EGameMode::NORMAL);
	EXPECT_TRUE(engine.get_window().is_cursor_captured());

	engine.set_game_mode(EGameMode::EDITOR);
	EXPECT_FALSE(engine.get_window().is_cursor_captured());
}

TEST_F(GameEngineTests, game_mode_activates_only_its_ui_layer)
{
	engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});

	EXPECT_TRUE(get_mock_gfx().engine_ui_active);
	EXPECT_FALSE(get_mock_gfx().application_ui_active);

	engine.set_game_mode(EGameMode::NORMAL);
	EXPECT_FALSE(get_mock_gfx().engine_ui_active);
	EXPECT_TRUE(get_mock_gfx().application_ui_active);

	engine.set_game_mode(EGameMode::EDITOR);
	EXPECT_TRUE(get_mock_gfx().engine_ui_active);
	EXPECT_FALSE(get_mock_gfx().application_ui_active);
}

TEST_F(GameEngineTests, normal_mode_disables_and_rejects_gizmo_selection)
{
	auto& object = engine.spawn_object<Object>();
	engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});
	auto& gizmo = engine.get_gizmo();
	gizmo.init();
	gizmo.select_object(&object);
	ASSERT_TRUE(gizmo.is_active());

	engine.set_game_mode(EGameMode::NORMAL);
	EXPECT_FALSE(gizmo.is_active());
	EXPECT_EQ(gizmo.get_selected_object(), nullptr);

	gizmo.select_object(&object);
	EXPECT_FALSE(gizmo.is_active());
	EXPECT_EQ(gizmo.get_selected_object(), nullptr);
}

TEST_F(GameEngineTests, normal_mode_scroll_zooms_even_when_gui_wants_the_mouse)
{
	engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});
	engine.get_camera().look_at(Maths::zero_vec, { 0.0f, 2.0f, -5.0f });
	engine.set_game_mode(EGameMode::NORMAL);
	const float normal_focal_length = engine.get_camera().get_focal_length();

	engine.scroll_callback(1.0, true);
	EXPECT_LT(engine.get_camera().get_focal_length(), normal_focal_length);

	engine.set_game_mode(EGameMode::EDITOR);
	const float editor_focal_length = engine.get_camera().get_focal_length();
	engine.scroll_callback(1.0, true);
	EXPECT_FLOAT_EQ(engine.get_camera().get_focal_length(), editor_focal_length);
}

TEST_F(GameEngineTests, normal_mode_routes_movement_to_the_active_player)
{
	PlayerDefinition definition;
	definition.movement_speed = 2.0f;
	auto& player = engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, definition);
	engine.get_camera().look_at(Maths::forward_vec, { 0.0f, 0.0f, -2.0f });
	engine.set_game_mode(EGameMode::NORMAL);

	engine.key_callback({ GLFW_KEY_W, EKeyModifier::NONE, EInputAction::PRESS });
	engine.main_loop(0.5f);

	EXPECT_TRUE(glm_equal(player.get_position(), Maths::forward_vec));
	const glm::vec3 camera_right =
		engine.get_camera().focus_obj->get_rotation() * Maths::right_vec;
	EXPECT_TRUE(glm_equal(
		engine.get_camera().get_focus(),
		player.get_position() + definition.camera_focus_offset
			+ camera_right * definition.camera_horizontal_offset));
}

TEST_F(GameEngineTests, player_rotation_does_not_rotate_the_follow_camera)
{
	auto& player = engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});
	engine.get_camera().look_at(Maths::zero_vec, { 0.0f, 2.0f, -5.0f });
	engine.set_game_mode(EGameMode::NORMAL);
	const glm::vec3 initial_direction = glm::normalize(
		engine.get_camera().get_focus() - engine.get_camera().get_position());

	engine.key_callback({ GLFW_KEY_A, EKeyModifier::NONE, EInputAction::PRESS });
	engine.main_loop(0.5f);

	const glm::vec3 moved_direction = glm::normalize(
		engine.get_camera().get_focus() - engine.get_camera().get_position());
	EXPECT_TRUE(glm_equal(moved_direction, initial_direction));
}

TEST_F(GameEngineTests, normal_mode_orbits_camera_without_a_mouse_button)
{
	engine.spawn_object<PlayerCharacter>(
		std::vector<Renderable>{}, PlayerDefinition{});
	engine.get_camera().look_at(Maths::zero_vec, { 0.0f, 2.0f, -5.0f });
	engine.get_mock_window().set_cursor_pos(Maths::zero_vec);
	engine.set_game_mode(EGameMode::NORMAL);
	const glm::vec3 initial_direction = glm::normalize(
		engine.get_camera().get_focus() - engine.get_camera().get_position());

	engine.get_mock_window().set_cursor_pos({ 0.02f, 0.0f });
	engine.main_loop(0.1f);

	const glm::vec3 rotated_direction = glm::normalize(
		engine.get_camera().get_focus() - engine.get_camera().get_position());
	EXPECT_FALSE(glm_equal(rotated_direction, initial_direction));
	EXPECT_GT(rotated_direction.x, 0.0f);
	EXPECT_LT(std::acos(glm::clamp(
		glm::dot(rotated_direction, initial_direction), -1.0f, 1.0f)), 0.2f);
}

TEST_F(GameEngineTests, spawn_cubemap_creates_a_generic_object)
{
	engine.spawn_cubemap();

	ASSERT_EQ(count_persistent_objects(engine), 1);
	const auto object = std::ranges::find_if(engine.get_objects(), [](const auto& entry) {
		return !entry.second->is_transient();
	})->second;
	ASSERT_EQ(object->renderables.size(), 1);
	EXPECT_EQ(object->renderables.front().pipeline_render_type, ERenderType::CUBEMAP);
	EXPECT_EQ(object->renderables.front().material_owners.size(), 6);
	EXPECT_FALSE(object->renderables.front().casts_shadow);
}

TEST_F(GameEngineTests, scene_save_rejects_procedural_meshes)
{
	ColorMaterial color;
	color.data.diffuse = { 0.2f, 0.3f, 0.4f };
	auto original_owner = MaterialSystem::add(std::make_unique<ColorMaterial>(color));
	auto mesh_owner = MeshSystem::add(MeshFactory::cube());
	Renderable renderable{ .pipeline_render_type = ERenderType::COLOR,
		.mesh_owner = mesh_owner, .material_owners = { original_owner } };
	engine.spawn_object<Object>(renderable);
	const std::string save_name = "krisp_scene_material_test";
	const auto path = save_path(save_name);

	EXPECT_THROW(engine.save_scene(save_name), SerializationError);
	std::filesystem::remove(path);
}

TEST_F(GameEngineTests, scene_load_restores_ecs_before_spawning_graphics_objects)
{
	auto& object = engine.spawn_object<Object>();
	const ObjectID object_id = object.get_id();
	engine.get_ecs().add_collider(object_id, std::make_unique<BoxCollider>());
	const std::string save_name = "krisp_scene_spawn_after_ecs_test";
	const auto path = save_path(save_name);
	engine.save_scene(save_name);

	bool observed_spawn = false;
	get_mock_gfx().on_spawn = [&](SpawnObjectCmd& cmd)
	{
		if (cmd.object && cmd.object->get_id() == object_id)
		{
			observed_spawn = true;
			EXPECT_NE(engine.get_ecs().get_collider(object_id), nullptr);
		}
	};

	engine.load_scene(save_name);
	std::filesystem::remove(path);

	EXPECT_TRUE(observed_spawn);
}

TEST_F(GameEngineTests, scene_save_rejects_procedural_materials)
{
	auto original_owner = ResourceLoader::fetch_texture("texture1.jpg");
	auto mesh_owner = MeshSystem::add(MeshFactory::cube());
	ResourceProvenance::register_mesh(MeshSystem::get_id(mesh_owner), {
		.source = "static_mesh_textured.gltf", .scene = 0, .node = 0, .primitive = 0 });
	Renderable renderable{ .pipeline_render_type = ERenderType::STANDARD,
		.mesh_owner = mesh_owner, .material_owners = { original_owner } };
	engine.spawn_object<Object>(renderable);
	const std::string save_name = "krisp_scene_texture_test";
	const auto path = save_path(save_name);

	EXPECT_THROW(engine.save_scene(save_name), SerializationError);
	std::filesystem::remove(path);
	ResourceProvenance::clear();
}

TEST_F(GameEngineTests, scene_round_trips_imported_mesh_and_embedded_material_by_provenance)
{
	const auto model_path = "static_mesh_textured.gltf";
	auto model = ResourceLoader::load_model(engine.get_ecs(), model_path);
	ASSERT_EQ(model.meshes.size(), 1);
	auto& object = engine.spawn_object<Object>(model.meshes.front().renderables);
	const ObjectID object_id = object.get_id();
	const std::string save_name = "krisp_scene_imported_resource_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	std::ifstream yaml(path);
	const std::string contents((std::istreambuf_iterator<char>(yaml)), {});
	EXPECT_NE(contents.find("mesh_source:"), std::string::npos);
	EXPECT_EQ(contents.find("mesh_id:"), std::string::npos);
	EXPECT_EQ(contents.find("source: image"), std::string::npos);

	EXPECT_NO_THROW(engine.load_scene(save_name));
	std::filesystem::remove(path);
	const auto* restored = engine.get_object(object_id);
	ASSERT_NE(restored, nullptr);
	ASSERT_EQ(restored->renderables.size(), 1);
	EXPECT_TRUE(MeshSystem::contains(restored->renderables.front().get_mesh_id()));
	EXPECT_TRUE(MaterialSystem::contains(restored->renderables.front().get_material_id(0)));
}

TEST_F(GameEngineTests, scene_round_trips_imported_skeleton_and_animation_by_provenance)
{
	auto model = ResourceLoader::load_model(
		engine.get_ecs(), "simple_test_model.gltf");
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_TRUE(model.meshes.front().skeleton_id.has_value());
	const auto animations = ResourceLoader::load_animations(
		engine.get_ecs(), "standalone_animation.gltf",
		*model.meshes.front().skeleton_id);
	ASSERT_FALSE(animations.animations.empty());
	auto& object = engine.spawn_object<Object>(model.meshes.front().renderables);
	engine.get_ecs().attach_skeleton(object.get_id(), *model.meshes.front().skeleton_id);
	const ObjectID object_id = object.get_id();
	engine.get_ecs().play_animation(*model.meshes.front().skeleton_id, animations.animations.front(), true);
	const std::string save_name = "krisp_scene_imported_animation_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	std::ifstream yaml(path);
	const std::string contents((std::istreambuf_iterator<char>(yaml)), {});
	EXPECT_NE(contents.find("imported_source:"), std::string::npos);
	EXPECT_EQ(contents.find("key_frames:"), std::string::npos);

	EXPECT_NO_THROW(engine.load_scene(save_name));
	std::filesystem::remove(path);
	const auto* restored = engine.get_object(object_id);
	ASSERT_NE(restored, nullptr);
	const auto restored_skeleton = engine.get_ecs().get_skeleton_id(restored->get_id());
	ASSERT_TRUE(restored_skeleton.has_value());
	EXPECT_NO_THROW(engine.get_ecs().get_skeletal_component(*restored_skeleton));
}

TEST_F(GameEngineTests, scene_round_trips_looping_standalone_animation_by_provenance)
{
	auto model = ResourceLoader::load_model(
		engine.get_ecs(), "simple_test_model.gltf");
	ASSERT_TRUE(model.meshes.front().skeleton_id.has_value());
	auto& object = engine.spawn_object<Object>(model.meshes.front().renderables);
	const ObjectID object_id = object.get_id();
	engine.get_ecs().attach_skeleton(object.get_id(), *model.meshes.front().skeleton_id);
	const auto imported = ResourceLoader::load_animations(
		engine.get_ecs(), "standalone_animation.gltf",
		*model.meshes.front().skeleton_id);
	ASSERT_FALSE(imported.animations.empty());
	engine.get_ecs().play_animation(*model.meshes.front().skeleton_id, imported.animations.front(), true);
	const std::string save_name = "krisp_scene_standalone_animation_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	EXPECT_NO_THROW(engine.load_scene(save_name));
	std::filesystem::remove(path);
	const auto restored_skeleton = engine.get_ecs().get_skeleton_id(object_id);
	ASSERT_TRUE(restored_skeleton.has_value());
	EXPECT_NO_THROW(engine.get_ecs().process(0.1f));
}

TEST_F(GameEngineTests, scene_load_restores_camera_state)
{
	auto& camera = engine.get_camera();
	camera.look_at({ 1.0f, 2.0f, 3.0f }, { -4.0f, 5.0f, -6.0f });
	camera.set_mode(Camera::Mode::FPV);
	camera.set_orthographic_projection({ -3.0f, 7.0f });
	camera.toggle_projection();
	const std::string save_name = "krisp_scene_camera_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	camera.look_at(Maths::zero_vec, { 0.0f, 3.0f, -3.0f });
	camera.set_mode(Camera::Mode::ORBIT);
	camera.toggle_projection();
	engine.load_scene(save_name);
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
	const std::string save_name = "krisp_scene_gizmo_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	EXPECT_FALSE(engine.get_gizmo().is_active());
	EXPECT_NO_THROW(engine.load_scene(save_name));
	std::filesystem::remove(path);

	const auto* restored = engine.get_object(object_id);
	ASSERT_NE(restored, nullptr);
	EXPECT_TRUE(glm_equal(restored->get_position(), glm::vec3(2.0f, 3.0f, 4.0f)));
	EXPECT_FALSE(engine.get_gizmo().is_active());
}

TEST_F(GameEngineTests, reset_scene_reregisters_transient_gizmo_colliders)
{
	engine.get_gizmo().init();
	const auto count_transient = [this]() {
		return std::ranges::count_if(engine.get_ecs().get_all_colliders(), [](const auto& entry) {
			return entry.second.persistence == ColliderPersistence::Transient;
		});
	};

	EXPECT_EQ(count_transient(), 10);
	for (const auto& [id, collider] : engine.get_ecs().get_all_colliders())
		if (collider.persistence == ColliderPersistence::Transient)
		{
			const auto* helper = engine.get_object(id);
			ASSERT_NE(helper, nullptr);
			EXPECT_TRUE(helper->is_transient());
		}
	engine.reset_scene();
	EXPECT_EQ(count_transient(), 10);
}

TEST_F(GameEngineTests, reset_scene_preserves_transient_helpers_and_removes_scene_objects)
{
	auto* const focus = engine.get_camera().focus_obj;
	auto* const upvector = engine.get_camera().upvector_obj;
	const ObjectID scene_object_id = engine.spawn_object<Object>().get_id();
	engine.get_gizmo().init();
	std::vector<Object*> helpers;
	std::vector<MeshID> helper_meshes;
	std::vector<MaterialID> helper_materials;
	for (const auto& [_, object] : engine.get_objects())
		if (object->is_transient())
		{
			helpers.push_back(object.get());
			for (const auto& renderable : object->renderables)
			{
				helper_meshes.push_back(renderable.get_mesh_id());
				const auto material_ids = renderable.get_material_ids();
				helper_materials.insert(helper_materials.end(), material_ids.begin(), material_ids.end());
			}
		}
	ASSERT_EQ(helpers.size(), 12);

	engine.reset_scene();

	EXPECT_EQ(engine.get_object(scene_object_id), nullptr);
	EXPECT_EQ(engine.get_camera().focus_obj, focus);
	EXPECT_EQ(engine.get_camera().upvector_obj, upvector);
	for (const auto* helper : helpers)
		EXPECT_EQ(engine.get_object(helper->get_id()), helper);
	for (const MeshID mesh_id : helper_meshes)
		EXPECT_TRUE(MeshSystem::contains(mesh_id));
	for (const MaterialID material_id : helper_materials)
		EXPECT_TRUE(MaterialSystem::contains(material_id));
}

TEST_F(GameEngineTests, scene_serialization_omits_transient_helpers)
{
	engine.get_gizmo().init();
	engine.spawn_object<Object>();
	const std::string save_name = "krisp_scene_transient_helpers_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	std::ifstream stream(path);
	std::ostringstream contents;
	contents << stream.rdbuf();
	const auto saved = Deserializer::parse(contents.str());
	std::filesystem::remove(path);

	EXPECT_EQ(saved.child("objects").elements().size(), 1);
	const auto keys = saved.keys();
	EXPECT_EQ(std::ranges::find(keys, "materials"), keys.end());
}

TEST_F(GameEngineTests, gizmo_hit_testing_uses_registered_ecs_colliders)
{
	auto& object = engine.spawn_object<Object>();
	auto& gizmo = engine.get_gizmo();
	gizmo.init();
	gizmo.select_object(&object);
	const Maths::Ray ray({ 0.0f, 0.0f, -2.0f }, Maths::forward_vec);

	EXPECT_TRUE(gizmo.check_collision(ray));
}

TEST_F(GameEngineTests, centre_cube_ecs_collider_preserves_corner_hit_region)
{
	auto& object = engine.spawn_object<Object>();
	auto& gizmo = engine.get_gizmo();
	gizmo.init();
	gizmo.select_object(&object);
	gizmo.select_object(&object); // switch to scale mode

	EXPECT_TRUE(gizmo.check_collision(Maths::Ray(
		{ 0.0f, 0.0f, -2.0f }, Maths::forward_vec)));
	EXPECT_TRUE(gizmo.check_collision(Maths::Ray(
		{ 0.14f, 0.14f, -2.0f }, Maths::forward_vec)));
	EXPECT_FALSE(gizmo.check_collision(Maths::Ray(
		{ 0.5f, 0.5f, -2.0f }, Maths::forward_vec)));
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
	auto mesh_owner = MeshSystem::add(MeshFactory::icosahedron());
	auto material_owner = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const MeshID mesh_id = MeshSystem::get_id(mesh_owner);
	const MaterialID material_id = MaterialSystem::get_id(material_owner);
	Renderable renderable;
	renderable.mesh_owner = mesh_owner;
	renderable.material_owners = { material_owner };
	auto& obj = engine.spawn_object(std::make_shared<Object>(renderable));
	renderable = {};
	mesh_owner.reset();
	material_owner.reset();
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
	auto mesh_owner = MeshSystem::add(MeshFactory::icosahedron());
	auto material_owner = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const MeshID mesh_id = MeshSystem::get_id(mesh_owner);
	const MaterialID material_id = MaterialSystem::get_id(material_owner);
	auto external_material_owner = MaterialSystem::acquire(material_id);
	Renderable renderable;
	renderable.mesh_owner = mesh_owner;
	renderable.material_owners = { material_owner };
	auto& obj = engine.spawn_object(std::make_shared<Object>(renderable));
	renderable = {};
	mesh_owner.reset();
	material_owner.reset();
	const auto obj_id = obj.get_id();

	engine.delete_object(obj_id);
	engine.get_graphics_engine().increment_num_objs_deleted();
	engine.main_loop(1.0f);

	ASSERT_EQ(get_mock_gfx().meshes_to_destroy.size(), 1);
	ASSERT_EQ(get_mock_gfx().materials_to_destroy.size(), 0);

	ASSERT_EQ(get_mock_gfx().meshes_to_destroy[0], mesh_id);
	external_material_owner.reset();
	engine.main_loop(1.0f);
}

TEST_F(GameEngineTests, shared_renderable_resources_are_retained_for_each_spawned_object)
{
	auto mesh_owner = MeshSystem::add(MeshFactory::icosahedron());
	auto material_owner = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const MeshID mesh_id = MeshSystem::get_id(mesh_owner);
	const MaterialID material_id = MaterialSystem::get_id(material_owner);
	Renderable renderable{
		.mesh_owner = mesh_owner,
		.material_owners = { material_owner },
	};
	const auto first_id = engine.spawn_object<Object>(renderable).get_id();
	const auto second_id = engine.spawn_object<Object>(renderable).get_id();
	renderable = {};
	const auto mesh_lifetime = std::weak_ptr(mesh_owner);
	const auto material_lifetime = std::weak_ptr(material_owner);
	mesh_owner.reset();
	material_owner.reset();

	EXPECT_EQ(mesh_lifetime.use_count(), 2);
	EXPECT_EQ(material_lifetime.use_count(), 2);

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
	EXPECT_TRUE(mesh_lifetime.expired());
	EXPECT_TRUE(material_lifetime.expired());
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
		auto mesh_owner = MeshSystem::add(MeshFactory::icosahedron());
		auto material_owner = MaterialSystem::add(std::make_unique<ColorMaterial>());
		mesh_ids.push_back(MeshSystem::get_id(mesh_owner));
		material_ids.push_back(MaterialSystem::get_id(material_owner));
		Renderable renderable;
		renderable.mesh_owner = mesh_owner;
		renderable.material_owners = { material_owner };
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
	const auto path = "normal_mapped_shared_material.gltf";
	auto model = ResourceLoader::load_model(engine.get_ecs(), path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	const auto material_ids = model.meshes[0].renderables[0].get_material_ids();
	ASSERT_EQ(material_ids.size(), 2);

	auto& object = engine.spawn_object<Object>(model.meshes[0].renderables);
	engine.delete_object(object.get_id());
	engine.main_loop(1.0f);

	EXPECT_TRUE(get_mock_gfx().materials_to_destroy.empty());
	EXPECT_TRUE(MaterialSystem::contains(material_ids[0]));
	EXPECT_TRUE(MaterialSystem::contains(material_ids[1]));

	model = {};
	engine.main_loop(1.0f);
	ASSERT_EQ(get_mock_gfx().materials_to_destroy.size(), 2);
	EXPECT_EQ(get_mock_gfx().materials_to_destroy, material_ids);
	EXPECT_FALSE(MaterialSystem::contains(material_ids[0]));
	EXPECT_FALSE(MaterialSystem::contains(material_ids[1]));
}

TEST_F(GameEngineTests, deleting_object_during_skeletal_animation_is_safe)
{
	const auto path = "simple_test_model.gltf";
	auto model = ResourceLoader::load_model(engine.get_ecs(), path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 1);
	const auto skeleton_id = model.meshes[0].skeleton_id;
	ASSERT_TRUE(skeleton_id.has_value());
	const auto animations = ResourceLoader::load_animations(
		engine.get_ecs(), "standalone_animation.gltf", *skeleton_id);
	ASSERT_EQ(animations.animations.size(), 2);

	auto& object = engine.spawn_object<Object>(model.meshes[0].renderables);
	engine.get_ecs().attach_skeleton(object.get_id(), *skeleton_id);
	engine.get_ecs().play_animation(*skeleton_id, animations.animations[0], true);
	engine.main_loop(0.1f);

	engine.delete_object(object.get_id());
	EXPECT_NO_THROW(engine.main_loop(0.1f));
}

TEST_F(GameEngineTests, replaces_one_renderable_texture_and_preserves_other_slots)
{
	auto old_diffuse_owner = ResourceLoader::fetch_texture("texture5.jpg");
	auto old_normal_owner = ResourceLoader::fetch_texture(
		"texture6.jpg", ETextureSemantic::NORMAL);
	const MaterialID old_diffuse = MaterialSystem::get_id(old_diffuse_owner);
	const MaterialID old_normal = MaterialSystem::get_id(old_normal_owner);
	Renderable first;
	first.mesh_owner = MeshSystem::add(MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	first.material_owners = { old_diffuse_owner, old_normal_owner };
	first.pipeline_render_type = ERenderType::STANDARD;
	Renderable second = first;
	auto& object = engine.spawn_object<Object>(std::vector<Renderable>{ first, second });
	old_diffuse_owner.reset();
	old_normal_owner.reset();
	const auto old_diffuse_owners = old_diffuse_owner.use_count();
	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR, "texture5.jpg");
	EXPECT_TRUE(get_mock_gfx().material_updates.empty());
	EXPECT_EQ(old_diffuse_owner.use_count(), old_diffuse_owners);

	engine.replace_renderable_texture(
		object.get_id(), 1, ETextureSemantic::BASE_COLOR, "texture4.png");

	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	const auto& update = get_mock_gfx().material_updates[0];
	EXPECT_EQ(update.object_id, object.get_id());
	EXPECT_EQ(update.renderable_index, 1);
	EXPECT_EQ(update.normal, old_normal);
	EXPECT_TRUE(update.retired.empty());
	EXPECT_EQ(object.renderables[0].get_material_ids(), (MatVec{ old_diffuse, old_normal }));
	ASSERT_EQ(object.renderables[1].material_owners.size(), 2);
	EXPECT_EQ(object.renderables[1].get_material_id(0), update.diffuse);
	EXPECT_EQ(object.renderables[1].get_material_id(1), old_normal);
	EXPECT_NE(update.diffuse, old_diffuse);
	const auto& replacement = dynamic_cast<const TextureMaterial&>(MaterialSystem::get(update.diffuse));
	EXPECT_EQ(replacement.semantic, ETextureSemantic::BASE_COLOR);
}

TEST_F(GameEngineTests, texture_replacement_with_procedural_material_is_not_serializable)
{
	auto model = ResourceLoader::load_model(engine.get_ecs(), "static_mesh_textured.gltf");
	ASSERT_EQ(model.meshes.size(), 1);
	auto& object = engine.spawn_object<Object>(model.meshes[0].renderables);

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR, "texture4.png");
	ResourceProvenance::erase_material(object.renderables[0].get_material_id(0));

	const std::string save_name = "krisp_scene_replaced_texture_test";
	const auto path = save_path(save_name);
	EXPECT_THROW(engine.save_scene(save_name), SerializationError);
	std::filesystem::remove(path);
}

TEST_F(GameEngineTests, removes_normal_and_uses_white_for_missing_diffuse)
{
	auto diffuse_owner = ResourceLoader::fetch_texture("texture2.jpg");
	auto normal_owner = ResourceLoader::fetch_texture("texture3.jpg", ETextureSemantic::NORMAL);
	const MaterialID diffuse = MaterialSystem::get_id(diffuse_owner);
	const MaterialID normal = MaterialSystem::get_id(normal_owner);
	Renderable renderable;
	renderable.mesh_owner = MeshSystem::add(
		MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	renderable.material_owners = { diffuse_owner, normal_owner };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(renderable);
	renderable = {};
	diffuse_owner.reset();
	normal_owner.reset();

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::NORMAL, std::nullopt);
	ASSERT_EQ(object.renderables[0].get_material_ids(), (MatVec{ diffuse }));
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	EXPECT_FALSE(get_mock_gfx().material_updates[0].normal);
	EXPECT_EQ(get_mock_gfx().material_updates[0].retired, (MatVec{ normal }));
	EXPECT_FALSE(MaterialSystem::contains(normal));

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR, std::nullopt);
	const auto white = object.renderables[0].get_material_id(0);
	EXPECT_EQ(object.renderables[0].get_material_ids(), (MatVec{ white }));
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(MaterialSystem::get(white)).source, "(none)");
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 2);
	EXPECT_EQ(get_mock_gfx().material_updates[1].diffuse, white);
	EXPECT_EQ(get_mock_gfx().material_updates[1].retired, (MatVec{ diffuse }));
	EXPECT_FALSE(MaterialSystem::contains(diffuse));
}

TEST_F(GameEngineTests, replaces_specular_maps)
{
	auto diffuse_owner = ResourceLoader::fetch_texture("texture2.jpg");
	const MaterialID diffuse = MaterialSystem::get_id(diffuse_owner);
	Renderable renderable;
	renderable.mesh_owner = MeshSystem::add(
		MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	renderable.material_owners = { diffuse_owner };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(renderable);
	renderable = {};
	diffuse_owner.reset();

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::SPECULAR,
		"texture4.png");
	const TexturedMatGroup group(object.renderables[0].material_owners);
	ASSERT_TRUE(group.specular_mat);
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	const auto& update = get_mock_gfx().material_updates.back();
	EXPECT_EQ(update.specular, group.specular_mat);
}

TEST_F(GameEngineTests, texture_replacement_matches_owners_by_semantic)
{
	auto diffuse_owner = ResourceLoader::fetch_texture("texture2.jpg");
	auto specular_owner = MaterialSystem::add(MaterialFactory::fetch_black_texture());
	const MaterialID old_diffuse = MaterialSystem::get_id(diffuse_owner);
	const MaterialID specular = MaterialSystem::get_id(specular_owner);
	Renderable renderable;
	renderable.mesh_owner = MeshSystem::add(
		MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	renderable.material_owners = { specular_owner, diffuse_owner };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(std::move(renderable));
	diffuse_owner.reset();
	specular_owner.reset();

	engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR, "texture4.png");

	const TexturedMatGroup group(object.renderables[0].material_owners);
	EXPECT_NE(group.base_color_mat, old_diffuse);
	EXPECT_EQ(group.specular_mat, specular);
	EXPECT_FALSE(MaterialSystem::contains(old_diffuse));
}

TEST_F(GameEngineTests, sets_a_matte_specular_fallback)
{
	auto diffuse_owner = ResourceLoader::fetch_texture("texture2.jpg");
	const MaterialID diffuse = MaterialSystem::get_id(diffuse_owner);
	Renderable renderable;
	renderable.mesh_owner = MeshSystem::add(
		MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	renderable.material_owners = { diffuse_owner };
	renderable.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(renderable);
	renderable = {};
	diffuse_owner.reset();

	engine.set_renderable_specular_matte(object.get_id(), 0);

	const TexturedMatGroup group(object.renderables[0].material_owners);
	ASSERT_TRUE(group.specular_mat);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(*group.specular_mat)).source, "(matte)");
	ASSERT_EQ(get_mock_gfx().material_updates.size(), 1);
	EXPECT_EQ(get_mock_gfx().material_updates.back().specular, group.specular_mat);
}

TEST_F(GameEngineTests, rejected_texture_replacements_leave_materials_unchanged)
{
	auto material_owner = ResourceLoader::fetch_texture("texture1.jpg");
	const MaterialID material = MaterialSystem::get_id(material_owner);
	Renderable textured;
	textured.mesh_owner = MeshSystem::add(
		MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	textured.material_owners = { material_owner };
	textured.pipeline_render_type = ERenderType::STANDARD;
	auto& object = engine.spawn_object<Object>(textured);
	textured = {};
	material_owner.reset();
	const auto original = object.renderables[0].get_material_ids();

	EXPECT_THROW(engine.replace_renderable_texture(
		object.get_id(), 1, ETextureSemantic::BASE_COLOR, std::nullopt), std::runtime_error);
	EXPECT_THROW(engine.replace_renderable_texture(
		object.get_id(), 0, ETextureSemantic::BASE_COLOR,
		"does_not_exist.png"), ResourceLoadError);
	EXPECT_EQ(object.renderables[0].get_material_ids(), original);
	EXPECT_TRUE(get_mock_gfx().material_updates.empty());

	Renderable colour = Renderable::make_default(
		MeshSystem::add(MeshFactory::cube(MeshFactory::EVertexType::COLOR)));
	auto& colour_object = engine.spawn_object<Object>(colour);
	EXPECT_THROW(engine.replace_renderable_texture(
		colour_object.get_id(), 0, ETextureSemantic::BASE_COLOR, std::nullopt), std::runtime_error);
	EXPECT_TRUE(get_mock_gfx().material_updates.empty());
}
