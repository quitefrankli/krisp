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

const RenderableState& find_render_object(const RenderFrame& frame, const ObjectID id)
{
	const auto found = std::ranges::find_if(frame.renderables, [id](const RenderableState& state) {
		return state.definition->object_id == id;
	});
	if (found == frame.renderables.end())
		throw std::runtime_error("render object not found");
	return *found;
}

const RenderableState& find_renderable(const RenderFrame& frame, const RenderableID id)
{
	const auto found = std::ranges::find_if(frame.renderables, [id](const RenderableState& state) {
		return state.definition->id == id;
	});
	if (found == frame.renderables.end())
		throw std::runtime_error("renderable not found");
	return *found;
}

Object& spawn_renderable_object(
	GameEngine& engine, Renderable renderable,
	const std::optional<SkeletonID> skeleton = {})
{
	auto& object = engine.spawn_object<Object>();
	engine.attach_renderable(object.get_id(), std::move(renderable), skeleton);
	return object;
}

RenderableID only_renderable_id(const GameEngine& engine, const ObjectID object_id)
{
	const auto ids = engine.get_ecs().get_renderable_ids(object_id);
	if (ids.size() != 1)
		throw std::runtime_error("object does not have exactly one renderable");
	return ids.front();
}

const RenderSkeletonPose& find_render_skeleton(const RenderFrame& frame, const SkeletonID id)
{
	const auto found = std::ranges::find_if(frame.skeletons, [id](const RenderSkeletonPose& pose) {
		return pose.definition->id == id;
	});
	if (found == frame.skeletons.end())
		throw std::runtime_error("render skeleton not found");
	return *found;
}
}

TEST_F(GameEngineTests, Constructor)
{
	EXPECT_EQ(engine.get_window().get_glfw_window(), nullptr);
}

TEST_F(GameEngineTests, publishes_initial_and_post_update_render_frames)
{
	const auto initial = engine.get_graphics_engine().load_latest_completed_render_frames();
	ASSERT_NE(initial, nullptr);
	ASSERT_NE(initial->current, nullptr);
	EXPECT_EQ(initial->previous, nullptr);
	EXPECT_EQ(initial->current->frame_number, 0);
	EXPECT_EQ(initial->current->renderables.size(), engine.get_ecs().get_renderable_ids().size());
	EXPECT_TRUE(glm_equal(initial->current->camera.view, engine.get_camera().get_view()));
	EXPECT_TRUE(glm_equal(initial->current->camera.projection, engine.get_camera().get_projection()));
	EXPECT_TRUE(glm_equal(initial->current->camera.position, engine.get_camera().get_position()));

	engine.main_loop(0.1f);

	const auto updated = engine.get_graphics_engine().load_latest_completed_render_frames();
	ASSERT_NE(updated, nullptr);
	ASSERT_NE(updated->previous, nullptr);
	EXPECT_EQ(updated->current->frame_number, 1);
	EXPECT_EQ(updated->previous, initial->current);
}

TEST_F(GameEngineTests, snapshots_object_hierarchy_and_reuses_unchanged_definitions)
{
	auto& parent = engine.spawn_object<Object>();
	auto& child = spawn_renderable_object(engine, Renderable::make_default());
	const RenderableID child_renderable = only_renderable_id(engine, child.get_id());
	engine.get_ecs().set_position(parent.get_id(), { 2.0f, 0.0f, 0.0f });
	engine.get_ecs().set_position(child.get_id(), { 2.0f, 3.0f, 0.0f });
	engine.get_ecs().attach_to(child.get_id(), parent.get_id());
	child.set_visibility(false);

	engine.main_loop(0.1f);
	const auto first_frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	const auto& first_child = find_render_object(*first_frame, child.get_id());
	ASSERT_NE(first_child.definition, nullptr);
	EXPECT_FALSE(first_child.visible);
	EXPECT_TRUE(glm_equal(first_child.model_transform, engine.get_ecs().get_transform(child.get_id())));

	const auto first_definition = first_child.definition;
	child.set_visibility(true);
	engine.get_ecs().set_relative_position(child.get_id(), { 0.0f, 4.0f, 0.0f });
	engine.main_loop(0.1f);
	const auto pose_only_frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	EXPECT_EQ(find_render_object(*pose_only_frame, child.get_id()).definition, first_definition);

	auto changed_renderable = engine.get_ecs().get_renderable(child_renderable).renderable;
	changed_renderable.casts_shadow = false;
	const RenderableID replacement_id = engine.get_ecs().replace_renderable(
		child_renderable, std::move(changed_renderable));
	engine.main_loop(0.1f);
	const auto definition_frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	const auto replacement = find_render_object(*definition_frame, child.get_id()).definition;
	EXPECT_NE(replacement, first_definition);
	EXPECT_EQ(replacement->id, replacement_id);
	EXPECT_FALSE(engine.get_ecs().has_renderable(child_renderable));
	EXPECT_FALSE(replacement->casts_shadow);
}

TEST_F(GameEngineTests, snapshots_skeleton_pose_with_immutable_definitions)
{
	Bone root;
	root.relative_transform.set_pos({ 1.0f, 0.0f, 0.0f });
	root.inverse_bind_pose.set_pos({ -1.0f, 0.0f, 0.0f });
	Bone child;
	child.parent_node = 0;
	child.relative_transform.set_pos({ 0.0f, 2.0f, 0.0f });
	const SkeletonID skeleton_id = engine.get_ecs().add_skeleton({ root, child });
	auto& object = engine.spawn_object<Object>();
	auto skinned = Renderable::make_default();
	skinned.pipeline_render_type = ERenderType::SKINNED_COLOR;
	const RenderableID renderable_id =
		engine.attach_renderable(object.get_id(), std::move(skinned), skeleton_id);

	engine.main_loop(0.1f);
	const auto first_frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	const auto first_definition = find_render_skeleton(*first_frame, skeleton_id).definition;
	EXPECT_EQ(find_renderable(*first_frame, renderable_id).definition->skeleton_id, skeleton_id);
	ASSERT_EQ(first_definition->bones.size(), 2);
	EXPECT_EQ(first_definition->bones[1].parent_index, 0);

	engine.get_ecs().get_skeletal_component(skeleton_id)
		.get_bone_local_transform(1).set_pos({ 0.0f, 3.0f, 0.0f });
	engine.main_loop(0.1f);
	const auto pose_frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	const auto& changed_pose = find_render_skeleton(*pose_frame, skeleton_id);
	EXPECT_EQ(changed_pose.definition, first_definition);
	EXPECT_TRUE(glm_equal(
		changed_pose.local_transforms[1],
		glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f))));

	const auto first_object_definition =
		find_renderable(*pose_frame, renderable_id).definition;
	const SkeletonID replacement_skeleton = engine.get_ecs().add_skeleton({ root });
	auto replacement_renderable =
		engine.get_ecs().get_renderable(renderable_id).renderable;
	const RenderableID replacement_renderable_id = engine.get_ecs().add_renderable(
		std::move(replacement_renderable), object.get_id(), replacement_skeleton);
	engine.get_ecs().remove_renderable(renderable_id);
	engine.main_loop(0.1f);
	const auto binding_frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	const auto replacement_object_definition =
		find_renderable(*binding_frame, replacement_renderable_id).definition;
	EXPECT_NE(replacement_object_definition, first_object_definition);
	EXPECT_EQ(replacement_object_definition->skeleton_id, replacement_skeleton);
}

TEST_F(GameEngineTests, snapshots_particles_and_active_light)
{
	ParticleEmitterConfig particle_config;
	particle_config.emission_rate = 1.0f;
	particle_config.max_particles = 1;
	engine.spawn_particle_emitter(particle_config);
	auto& light = engine.spawn_object<Object>();
	const LightComponent light_component{
		.intensity = 2.0f,
		.color = { 0.2f, 0.4f, 0.8f },
	};
	engine.get_ecs().add_light_source(light.get_id(), light_component);

	engine.main_loop(1.0f);

	const auto frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	ASSERT_EQ(frame->particles.size(), 1);
	ASSERT_TRUE(frame->active_light.has_value());
	EXPECT_EQ(frame->active_light->object_id, light.get_id());
	EXPECT_TRUE(glm_equal(
		frame->active_light->position, engine.get_ecs().get_position(light.get_id())));
	EXPECT_FLOAT_EQ(frame->active_light->intensity, light_component.intensity);
	EXPECT_EQ(frame->active_light->color, light_component.color);
}

TEST_F(GameEngineTests, acknowledged_deletion_updates_latest_frame_without_mutating_history)
{
	auto& object = spawn_renderable_object(engine, Renderable::make_default());
	const ObjectID id = object.get_id();
	engine.main_loop(0.1f);
	const auto before_deletion =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	ASSERT_NO_THROW((void)find_render_object(*before_deletion, id));

	engine.delete_object(id);
	engine.main_loop(0.1f);

	const auto publication =
		engine.get_graphics_engine().load_latest_completed_render_frames();
	EXPECT_EQ(publication->previous, before_deletion);
	EXPECT_EQ(std::ranges::find_if(
		publication->current->renderables,
		[id](const RenderableState& state) { return state.definition->object_id == id; }),
		publication->current->renderables.end());
	ASSERT_NO_THROW((void)find_render_object(*before_deletion, id));
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
	auto& first = engine.spawn_object<PlayerCharacter>(PlayerDefinition{});
	auto& second = engine.spawn_object<PlayerCharacter>(PlayerDefinition{});

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
	engine.spawn_object<PlayerCharacter>(PlayerDefinition{});

	engine.set_game_mode(EGameMode::NORMAL);
	EXPECT_TRUE(engine.get_window().is_cursor_captured());

	engine.set_game_mode(EGameMode::EDITOR);
	EXPECT_FALSE(engine.get_window().is_cursor_captured());
}

TEST_F(GameEngineTests, game_mode_activates_only_its_ui_layer)
{
	engine.spawn_object<PlayerCharacter>(PlayerDefinition{});

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
	engine.spawn_object<PlayerCharacter>(PlayerDefinition{});
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
	engine.spawn_object<PlayerCharacter>(PlayerDefinition{});
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
	auto& player = engine.spawn_object<PlayerCharacter>(definition);
	engine.get_camera().look_at(Maths::forward_vec, { 0.0f, 0.0f, -2.0f });
	engine.set_game_mode(EGameMode::NORMAL);

	engine.key_callback({ GLFW_KEY_W, EKeyModifier::NONE, EInputAction::PRESS });
	engine.main_loop(0.5f);

	EXPECT_TRUE(glm_equal(engine.get_ecs().get_position(player.get_id()), Maths::forward_vec));
	const glm::vec3 camera_right =
		engine.get_ecs().get_rotation(engine.get_camera().focus_obj->get_id()) * Maths::right_vec;
	EXPECT_TRUE(glm_equal(
		engine.get_camera().get_focus(),
		engine.get_ecs().get_position(player.get_id()) + definition.camera_focus_offset
			+ camera_right * definition.camera_horizontal_offset));
}

TEST_F(GameEngineTests, player_rotation_does_not_rotate_the_follow_camera)
{
	auto& player = engine.spawn_object<PlayerCharacter>(PlayerDefinition{});
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
	engine.spawn_object<PlayerCharacter>(PlayerDefinition{});
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
	const auto renderable_ids = engine.get_ecs().get_renderable_ids(object->get_id());
	ASSERT_EQ(renderable_ids.size(), 1);
	const auto& renderable = engine.get_ecs().get_renderable(renderable_ids.front()).renderable;
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::CUBEMAP);
	EXPECT_EQ(renderable.material_owners.size(), 6);
	EXPECT_FALSE(renderable.casts_shadow);
}

TEST_F(GameEngineTests, scene_save_rejects_procedural_meshes)
{
	ColorMaterial color;
	color.data.diffuse = { 0.2f, 0.3f, 0.4f };
	auto original_owner = MaterialSystem::add(std::make_unique<ColorMaterial>(color));
	auto mesh_owner = MeshSystem::add(MeshFactory::cube());
	Renderable renderable{ .pipeline_render_type = ERenderType::COLOR,
		.mesh_owner = mesh_owner, .material_owners = { original_owner } };
	spawn_renderable_object(engine, renderable);
	const std::string save_name = "krisp_scene_material_test";
	const auto path = save_path(save_name);

	EXPECT_THROW(engine.save_scene(save_name), SerializationError);
	std::filesystem::remove(path);
}

TEST_F(GameEngineTests, scene_load_is_visible_in_the_next_completed_snapshot)
{
	auto& object = engine.spawn_object<Object>();
	const ObjectID object_id = object.get_id();
	engine.get_ecs().add_collider(object_id, std::make_unique<BoxCollider>());
	const std::string save_name = "krisp_scene_spawn_after_ecs_test";
	const auto path = save_path(save_name);
	engine.save_scene(save_name);

	engine.load_scene(save_name);
	engine.main_loop(0.1f);
	std::filesystem::remove(path);

	EXPECT_NE(engine.get_ecs().get_collider(object_id), nullptr);
	EXPECT_TRUE(engine.get_ecs().get_renderable_ids(object_id).empty());
}

TEST_F(GameEngineTests, scene_save_rejects_procedural_materials)
{
	auto original_owner = ResourceLoader::fetch_texture("texture1.jpg");
	auto mesh_owner = MeshSystem::add(MeshFactory::cube());
	ResourceProvenance::register_mesh(MeshSystem::get_id(mesh_owner), {
		.source = "static_mesh_textured.gltf", .scene = 0, .node = 0, .primitive = 0 });
	Renderable renderable{ .pipeline_render_type = ERenderType::STANDARD,
		.mesh_owner = mesh_owner, .material_owners = { original_owner } };
	spawn_renderable_object(engine, renderable);
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
	auto& object = engine.spawn_object<Object>();
	engine.attach_renderables(object.get_id(), model.meshes.front().renderables);
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
	const auto restored_ids = engine.get_ecs().get_renderable_ids(restored->get_id());
	ASSERT_EQ(restored_ids.size(), 1);
	const auto& restored_renderable = engine.get_ecs().get_renderable(restored_ids.front()).renderable;
	EXPECT_TRUE(MeshSystem::contains(restored_renderable.get_mesh_id()));
	EXPECT_TRUE(MaterialSystem::contains(restored_renderable.get_material_id(0)));
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
	auto& object = engine.spawn_object<Object>();
	engine.attach_renderables(object.get_id(), model.meshes.front().renderables,
		*model.meshes.front().skeleton_id);
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
	const auto restored_ids = engine.get_ecs().get_renderable_ids(restored->get_id());
	ASSERT_FALSE(restored_ids.empty());
	const auto restored_skeleton = engine.get_ecs().get_renderable(restored_ids.front()).skeleton_id;
	ASSERT_TRUE(restored_skeleton);
	EXPECT_NO_THROW(engine.get_ecs().get_skeletal_component(*restored_skeleton));
}

TEST_F(GameEngineTests, scene_round_trips_an_unbound_imported_skeleton)
{
	auto model = ResourceLoader::load_model(engine.get_ecs(), "simple_test_model.gltf");
	ASSERT_TRUE(model.meshes.front().skeleton_id.has_value());
	const std::string save_name = "krisp_scene_unbound_skeleton_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	EXPECT_NO_THROW(engine.load_scene(save_name));
	std::filesystem::remove(path);

	const auto skeletons = engine.get_ecs().get_skeleton_ids();
	ASSERT_EQ(skeletons.size(), 1);
	EXPECT_NO_THROW(engine.get_ecs().get_skeletal_component(skeletons.front()));
}

TEST_F(GameEngineTests, scene_round_trips_looping_standalone_animation_by_provenance)
{
	auto model = ResourceLoader::load_model(
		engine.get_ecs(), "simple_test_model.gltf");
	ASSERT_TRUE(model.meshes.front().skeleton_id.has_value());
	auto& object = engine.spawn_object<Object>();
	const ObjectID object_id = object.get_id();
	engine.attach_renderables(object.get_id(), model.meshes.front().renderables,
		*model.meshes.front().skeleton_id);
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
	const auto restored_ids = engine.get_ecs().get_renderable_ids(object_id);
	ASSERT_FALSE(restored_ids.empty());
	const auto restored_skeleton = engine.get_ecs().get_renderable(restored_ids.front()).skeleton_id;
	ASSERT_TRUE(restored_skeleton);
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
	engine.get_ecs().set_position(object.get_id(), { 2.0f, 3.0f, 4.0f });
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
	EXPECT_TRUE(glm_equal(
		engine.get_ecs().get_position(restored->get_id()),
		glm::vec3(2.0f, 3.0f, 4.0f)));
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
			for (const RenderableID id : engine.get_ecs().get_renderable_ids(object->get_id()))
			{
				const auto& renderable = engine.get_ecs().get_renderable(id).renderable;
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
	engine.main_loop(1.0f);
	ASSERT_FALSE(engine.get_object(id));
}

TEST_F(GameEngineTests, resource_cleanup)
{
	MeshSystem::take_retired();
	MaterialSystem::take_retired();
	auto mesh_owner = MeshSystem::add(MeshFactory::icosahedron());
	auto material_owner = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const MeshID mesh_id = MeshSystem::get_id(mesh_owner);
	const MaterialID material_id = MaterialSystem::get_id(material_owner);
	Renderable renderable;
	renderable.mesh_owner = mesh_owner;
	renderable.material_owners = { material_owner };
	auto& obj = spawn_renderable_object(engine, renderable);
	renderable = {};
	mesh_owner.reset();
	material_owner.reset();
	const auto obj_id = obj.get_id();

	engine.delete_object(obj_id);
	engine.main_loop(1.0f);

	EXPECT_EQ(MeshSystem::take_retired(), (std::vector<MeshID>{ mesh_id }));
	EXPECT_EQ(MaterialSystem::take_retired(), (std::vector<MaterialID>{ material_id }));
}

TEST_F(GameEngineTests, dont_cleanup_resource_if_not_ready)
{
	MeshSystem::take_retired();
	MaterialSystem::take_retired();
	auto mesh_owner = MeshSystem::add(MeshFactory::icosahedron());
	auto material_owner = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const MeshID mesh_id = MeshSystem::get_id(mesh_owner);
	const MaterialID material_id = MaterialSystem::get_id(material_owner);
	auto external_material_owner = MaterialSystem::acquire(material_id);
	Renderable renderable;
	renderable.mesh_owner = mesh_owner;
	renderable.material_owners = { material_owner };
	auto& obj = spawn_renderable_object(engine, renderable);
	renderable = {};
	mesh_owner.reset();
	material_owner.reset();
	const auto obj_id = obj.get_id();

	engine.delete_object(obj_id);
	engine.main_loop(1.0f);

	EXPECT_EQ(MeshSystem::take_retired(), (std::vector<MeshID>{ mesh_id }));
	EXPECT_TRUE(MaterialSystem::take_retired().empty());
	external_material_owner.reset();
	EXPECT_EQ(MaterialSystem::take_retired(), (std::vector<MaterialID>{ material_id }));
}

TEST_F(GameEngineTests, shared_renderable_resources_are_retained_for_each_spawned_object)
{
	MeshSystem::take_retired();
	MaterialSystem::take_retired();
	auto mesh_owner = MeshSystem::add(MeshFactory::icosahedron());
	auto material_owner = MaterialSystem::add(std::make_unique<ColorMaterial>());
	const MeshID mesh_id = MeshSystem::get_id(mesh_owner);
	const MaterialID material_id = MaterialSystem::get_id(material_owner);
	Renderable renderable{
		.mesh_owner = mesh_owner,
		.material_owners = { material_owner },
	};
	const auto first_id = spawn_renderable_object(engine, renderable).get_id();
	const auto second_id = spawn_renderable_object(engine, renderable).get_id();
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

	engine.delete_object(second_id);
	engine.main_loop(1.0f);
	EXPECT_TRUE(MeshSystem::contains(mesh_id));
	EXPECT_TRUE(MaterialSystem::contains(material_id));
	EXPECT_FALSE(mesh_lifetime.expired());
	EXPECT_FALSE(material_lifetime.expired());

	// The last frame containing the object remains available as "previous".
	// Advancing once releases its immutable definition and retained assets.
	engine.main_loop(1.0f);
	EXPECT_FALSE(MeshSystem::contains(mesh_id));
	EXPECT_FALSE(MaterialSystem::contains(material_id));
	EXPECT_TRUE(mesh_lifetime.expired());
	EXPECT_TRUE(material_lifetime.expired());

	EXPECT_EQ(MeshSystem::take_retired(), (std::vector<MeshID>{ mesh_id }));
	EXPECT_EQ(MaterialSystem::take_retired(), (std::vector<MaterialID>{ material_id }));
}

TEST_F(GameEngineTests, deleting_multiple_objects_destroys_each_resource_once)
{
	MeshSystem::take_retired();
	MaterialSystem::take_retired();
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
		object_ids.push_back(spawn_renderable_object(engine, renderable).get_id());
	}

	for (const auto object_id : object_ids)
		engine.delete_object(object_id);
	engine.main_loop(1.0f);

	auto retired_meshes = MeshSystem::take_retired();
	auto retired_materials = MaterialSystem::take_retired();
	std::ranges::sort(retired_meshes);
	std::ranges::sort(retired_materials);
	std::ranges::sort(mesh_ids);
	std::ranges::sort(material_ids);
	EXPECT_EQ(retired_meshes, mesh_ids);
	EXPECT_EQ(retired_materials, material_ids);
}

TEST_F(GameEngineTests, deleting_imported_object_with_shared_normal_material_is_safe)
{
	MaterialSystem::take_retired();
	const auto path = "normal_mapped_shared_material.gltf";
	auto model = ResourceLoader::load_model(engine.get_ecs(), path);
	ASSERT_EQ(model.meshes.size(), 1);
	ASSERT_EQ(model.meshes[0].renderables.size(), 2);
	const auto material_ids = model.meshes[0].renderables[0].get_material_ids();
	ASSERT_EQ(material_ids.size(), 2);

	auto& object = engine.spawn_object<Object>();
	engine.attach_renderables(object.get_id(), model.meshes[0].renderables);
	engine.delete_object(object.get_id());
	engine.main_loop(1.0f);

	EXPECT_TRUE(MaterialSystem::take_retired().empty());
	EXPECT_TRUE(MaterialSystem::contains(material_ids[0]));
	EXPECT_TRUE(MaterialSystem::contains(material_ids[1]));

	model = {};
	auto retired_materials = MaterialSystem::take_retired();
	std::ranges::sort(retired_materials);
	auto expected_materials = material_ids;
	std::ranges::sort(expected_materials);
	EXPECT_EQ(retired_materials, expected_materials);
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

	auto& object = engine.spawn_object<Object>();
	engine.attach_renderables(object.get_id(), model.meshes[0].renderables, *skeleton_id);
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
	auto& object = engine.spawn_object<Object>();
	auto renderable_ids = engine.attach_renderables(
		object.get_id(), std::vector<Renderable>{ first, second });
	old_diffuse_owner.reset();
	old_normal_owner.reset();
	const auto old_diffuse_owners = old_diffuse_owner.use_count();
	renderable_ids[0] = engine.replace_renderable_texture(
		renderable_ids[0], ETextureSemantic::BASE_COLOR, "texture5.jpg");
	EXPECT_EQ(old_diffuse_owner.use_count(), old_diffuse_owners);

	renderable_ids[1] = engine.replace_renderable_texture(
		renderable_ids[1], ETextureSemantic::BASE_COLOR, "texture4.png");

	const auto& first_attachment = engine.get_ecs().get_renderable(renderable_ids[0]).renderable;
	const auto& second_attachment = engine.get_ecs().get_renderable(renderable_ids[1]).renderable;
	EXPECT_EQ(first_attachment.get_material_ids(), (MatVec{ old_diffuse, old_normal }));
	ASSERT_EQ(second_attachment.material_owners.size(), 2);
	const MaterialID replacement_id = second_attachment.get_material_id(0);
	EXPECT_EQ(second_attachment.get_material_id(1), old_normal);
	EXPECT_NE(replacement_id, old_diffuse);
	const auto& replacement =
		dynamic_cast<const TextureMaterial&>(MaterialSystem::get(replacement_id));
	EXPECT_EQ(replacement.semantic, ETextureSemantic::BASE_COLOR);

	engine.main_loop(0.1f);
	const auto frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	EXPECT_EQ(find_renderable(*frame, renderable_ids[0]).definition->get_material_ids(),
		(MatVec{ old_diffuse, old_normal }));
	EXPECT_EQ(find_renderable(*frame, renderable_ids[1]).definition->get_material_ids(),
		(MatVec{ replacement_id, old_normal }));
}

TEST_F(GameEngineTests, texture_replacement_with_procedural_material_is_not_serializable)
{
	auto model = ResourceLoader::load_model(engine.get_ecs(), "static_mesh_textured.gltf");
	ASSERT_EQ(model.meshes.size(), 1);
	auto& object = engine.spawn_object<Object>();
	auto renderable_ids = engine.attach_renderables(
		object.get_id(), model.meshes[0].renderables);

	renderable_ids[0] = engine.replace_renderable_texture(
		renderable_ids[0], ETextureSemantic::BASE_COLOR, "texture4.png");
	ResourceProvenance::erase_material(
		engine.get_ecs().get_renderable(renderable_ids[0]).renderable.get_material_id(0));

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
	auto& object = spawn_renderable_object(engine, renderable);
	RenderableID renderable_id = only_renderable_id(engine, object.get_id());
	renderable = {};
	diffuse_owner.reset();
	normal_owner.reset();

	renderable_id = engine.replace_renderable_texture(
		renderable_id, ETextureSemantic::NORMAL, std::nullopt);
	const auto& attachment = engine.get_ecs().get_renderable(renderable_id).renderable;
	ASSERT_EQ(attachment.get_material_ids(), (MatVec{ diffuse }));
	EXPECT_FALSE(MaterialSystem::contains(normal));

	renderable_id = engine.replace_renderable_texture(
		renderable_id, ETextureSemantic::BASE_COLOR, std::nullopt);
	const auto& white_attachment = engine.get_ecs().get_renderable(renderable_id).renderable;
	const auto white = white_attachment.get_material_id(0);
	EXPECT_EQ(white_attachment.get_material_ids(), (MatVec{ white }));
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(MaterialSystem::get(white)).source, "(none)");
	EXPECT_FALSE(MaterialSystem::contains(diffuse));

	engine.main_loop(0.1f);
	const auto frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	EXPECT_EQ(
		find_renderable(*frame, renderable_id).definition->get_material_ids(),
		(MatVec{ white }));
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
	auto& object = spawn_renderable_object(engine, renderable);
	RenderableID renderable_id = only_renderable_id(engine, object.get_id());
	renderable = {};
	diffuse_owner.reset();

	renderable_id = engine.replace_renderable_texture(
		renderable_id, ETextureSemantic::SPECULAR,
		"texture4.png");
	const TexturedMatGroup group(
		engine.get_ecs().get_renderable(renderable_id).renderable.material_owners);
	ASSERT_TRUE(group.specular_mat);
	engine.main_loop(0.1f);
	const auto frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	const TexturedMatGroup snapshot_group(
		find_renderable(*frame, renderable_id).definition->material_owners);
	EXPECT_EQ(snapshot_group.specular_mat, group.specular_mat);
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
	auto& object = spawn_renderable_object(engine, std::move(renderable));
	RenderableID renderable_id = only_renderable_id(engine, object.get_id());
	diffuse_owner.reset();
	specular_owner.reset();

	renderable_id = engine.replace_renderable_texture(
		renderable_id, ETextureSemantic::BASE_COLOR, "texture4.png");

	const TexturedMatGroup group(
		engine.get_ecs().get_renderable(renderable_id).renderable.material_owners);
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
	auto& object = spawn_renderable_object(engine, renderable);
	RenderableID renderable_id = only_renderable_id(engine, object.get_id());
	renderable = {};
	diffuse_owner.reset();

	renderable_id = engine.set_renderable_specular_matte(renderable_id);

	const TexturedMatGroup group(
		engine.get_ecs().get_renderable(renderable_id).renderable.material_owners);
	ASSERT_TRUE(group.specular_mat);
	EXPECT_EQ(dynamic_cast<const TextureMaterial&>(
		MaterialSystem::get(*group.specular_mat)).source, "(matte)");
	engine.main_loop(0.1f);
	const auto frame =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	const TexturedMatGroup snapshot_group(
		find_renderable(*frame, renderable_id).definition->material_owners);
	EXPECT_EQ(snapshot_group.specular_mat, group.specular_mat);
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
	auto& object = spawn_renderable_object(engine, textured);
	const RenderableID renderable_id = only_renderable_id(engine, object.get_id());
	textured = {};
	material_owner.reset();
	const auto original = engine.get_ecs().get_renderable(renderable_id).renderable.get_material_ids();

	EXPECT_THROW(engine.replace_renderable_texture(
		RenderableID(999999), ETextureSemantic::BASE_COLOR, std::nullopt), std::runtime_error);
	EXPECT_THROW(engine.replace_renderable_texture(
		renderable_id, ETextureSemantic::BASE_COLOR,
		"does_not_exist.png"), ResourceLoadError);
	EXPECT_EQ(engine.get_ecs().get_renderable(renderable_id).renderable.get_material_ids(), original);

	Renderable colour = Renderable::make_default(
		MeshSystem::add(MeshFactory::cube(MeshFactory::EVertexType::COLOR)));
	auto& colour_object = spawn_renderable_object(engine, colour);
	EXPECT_THROW(engine.replace_renderable_texture(
		only_renderable_id(engine, colour_object.get_id()),
		ETextureSemantic::BASE_COLOR, std::nullopt), std::runtime_error);
}
