#include <game_engine.hpp>
#include <camera.hpp>
#include <iapplication.hpp>
#include <interface/gizmo.hpp>
#include <game_objects/player_character.hpp>
#include <gui/gui_windows/gui_model_spawner.hpp>

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
#include <fmt/core.h>
#include <nlohmann/json.hpp>

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
class TemporaryMutatedGltf
{
public:
	TemporaryMutatedGltf(
		const std::string_view source,
		const std::function<void(nlohmann::json&)>& mutate)
	{
		static uint32_t sequence = 0;
		path = Utility::get_top_level_path()/"test/data"
			/ fmt::format("krisp_game_engine_test_{}.gltf", sequence++);
		std::ifstream input(Utility::get_model(source));
		nlohmann::json document;
		input >> document;
		mutate(document);
		std::ofstream output(path);
		output << document;
	}

	~TemporaryMutatedGltf()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	std::string filename() const { return path.filename().string(); }

private:
	std::filesystem::path path;
};

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
	return Utility::get_saves_path() / std::string(name);
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

TEST_F(GameEngineTests, publishes_complete_render_view_state)
{
	auto& first = engine.spawn_object<Object>();
	auto& second = engine.spawn_object<Object>();

	engine.highlight_object(first);
	engine.highlight_object(second);
	engine.set_render_mode(ERenderMode::WIREFRAME);
	engine.main_loop(0.1f);

	const auto highlighted =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	EXPECT_EQ(highlighted->view.render_mode, ERenderMode::WIREFRAME);
	EXPECT_EQ(highlighted->view.stenciled_objects,
		(std::unordered_set<ObjectID>{ first.get_id(), second.get_id() }));

	engine.unhighlight_object(first);
	engine.main_loop(0.1f);

	const auto updated =
		engine.get_graphics_engine().load_latest_completed_render_frames()->current;
	EXPECT_EQ(updated->view.stenciled_objects,
		(std::unordered_set<ObjectID>{ second.get_id() }));
}

TEST_F(GameEngineTests, shutdown_requests_the_graphics_thread_to_stop)
{
	EXPECT_FALSE(get_mock_gfx().shutdown_requested);
	get_mock_gfx().get_recording_session().start(60);

	engine.shutdown();

	EXPECT_TRUE(get_mock_gfx().shutdown_requested);
	EXPECT_FALSE(get_mock_gfx().get_recording_session().is_active());
}

TEST_F(GameEngineTests, snapshots_object_hierarchy_and_reuses_unchanged_definitions)
{
	auto& parent = engine.spawn_object<Object>();
	auto& child = spawn_renderable_object(engine, Renderable::make_default(engine.get_ecs()));
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
	auto skinned = Renderable::make_default(engine.get_ecs());
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
	auto& object = spawn_renderable_object(engine, Renderable::make_default(engine.get_ecs()));
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

TEST_F(GameEngineTests, model_spawner_imports_each_mesh_node_as_clickable)
{
	auto& spawner = engine.get_gui_manager().model_spawner;
	spawner.queue_model_spawn("import_variants.gltf");
	spawner.process(engine);

	std::vector<ObjectID> spawned;
	for (const auto& [id, object] : engine.get_objects())
		if (!object->is_transient())
			spawned.push_back(id);
	ASSERT_EQ(spawned.size(), 2u);

	for (const ObjectID object_id : spawned)
	{
		const auto renderable_ids = engine.get_ecs().get_renderable_ids(object_id);
		ASSERT_EQ(renderable_ids.size(), 1u);
		const auto& renderable = engine.get_ecs()
			.get_renderable(renderable_ids.front()).renderable;
		const auto& pick_data = renderable.mesh_owner->get().get_pick_data();
		ASSERT_TRUE(pick_data.has_triangles());
		const auto& triangle = pick_data.get_triangles().front();
		const auto& positions = pick_data.get_positions();
		const glm::mat4 model_transform = renderable.get_model_transform(
			engine.get_ecs().get_maths_transform(object_id).get_mat4());
		const glm::vec3 a = glm::vec3(
			model_transform * glm::vec4(positions[triangle.vertices[0]], 1.0f));
		const glm::vec3 b = glm::vec3(
			model_transform * glm::vec4(positions[triangle.vertices[1]], 1.0f));
		const glm::vec3 c = glm::vec3(
			model_transform * glm::vec4(positions[triangle.vertices[2]], 1.0f));
		const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
		const glm::vec3 centre = (a + b + c) / 3.0f;

		const auto clicked = engine.get_ecs().check_any_entity_clicked(
			Maths::Ray(centre + normal, -normal));
		ASSERT_TRUE(clicked.bCollided);
		EXPECT_EQ(clicked.id, object_id);
	}
}

TEST_F(GameEngineTests, object_spawner_creates_a_clickable_textured_pbr_cube)
{
	auto& spawner = engine.get_gui_manager().object_spawner;
	ASSERT_TRUE(spawner.queue_object_spawn("textured cube"));
	spawner.process(engine);

	const auto object = std::ranges::find_if(engine.get_objects(), [](const auto& entry)
	{
		return !entry.second->is_transient();
	});
	ASSERT_NE(object, engine.get_objects().end());
	const auto renderable_ids = engine.get_ecs().get_renderable_ids(object->first);
	ASSERT_EQ(renderable_ids.size(), 1u);
	const auto& renderable = engine.get_ecs()
		.get_renderable(renderable_ids.front()).renderable;
	EXPECT_EQ(renderable.pipeline_render_type, ERenderType::STANDARD);
	EXPECT_NE(dynamic_cast<const TexMesh*>(&renderable.mesh_owner->get()), nullptr);
	const auto& material = dynamic_cast<const PbrMaterial&>(
		renderable.material_owners.front()->get());
	ASSERT_TRUE(material.textures.base_color.has_value());
	EXPECT_EQ(material.textures.base_color->texture,
		renderable.material_owners[1]->get_id());
	const auto& texture = dynamic_cast<const TextureMaterial&>(
		renderable.material_owners[1]->get());
	EXPECT_EQ(texture.semantic, ETextureSemantic::BASE_COLOR);

	const auto clicked = engine.get_ecs().check_any_entity_clicked(
		Maths::Ray(glm::vec3(0.0f, 0.0f, -2.0f), Maths::forward_vec));
	ASSERT_TRUE(clicked.bCollided);
	EXPECT_EQ(clicked.id, object->first);
}

TEST_F(GameEngineTests, scene_round_trips_generated_mesh_and_pbr_material)
{
	const glm::vec4 base_color(0.2f, 0.3f, 0.4f, 1.0f);
	constexpr float metallic = 0.6f;
	constexpr float roughness = 0.25f;
	auto original_owner = engine.get_ecs().get_material_system().add(
		std::make_unique<PbrMaterial>(base_color, metallic, roughness));
	auto mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::cube());
	Renderable renderable{ .pipeline_render_type = ERenderType::COLOR,
		.mesh_owner = mesh_owner, .material_owners = { original_owner } };
	auto& object = spawn_renderable_object(engine, renderable);
	engine.get_ecs().add_mesh_collider(object.get_id());
	const auto object_id = object.get_id();
	const std::string save_name = "krisp_scene_material_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	EXPECT_TRUE(std::filesystem::is_regular_file(path / "scene.yaml"));
	EXPECT_EQ(std::ranges::count_if(std::filesystem::directory_iterator(path), [](const auto& entry) {
		return entry.path().extension() == ".dat";
	}), 1);
	engine.load_scene(save_name);
	const auto& restored = engine.get_ecs().get_renderable(
		only_renderable_id(engine, object_id)).renderable;
	const auto& restored_mesh = dynamic_cast<const ColorMesh&>(restored.mesh_owner->get());
	const auto& restored_material = dynamic_cast<const PbrMaterial&>(restored.material_owners[0]->get());
	EXPECT_EQ(restored_mesh.get_indices(), dynamic_cast<const ColorMesh&>(mesh_owner->get()).get_indices());
	EXPECT_EQ(restored_material.data.base_color_factor, base_color);
	EXPECT_EQ(restored_material.data.metallic_factor, metallic);
	EXPECT_EQ(restored_material.data.roughness_factor, roughness);
	EXPECT_NE(engine.get_ecs().get_collider(object_id), nullptr);
	std::filesystem::remove_all(path);
}

TEST_F(GameEngineTests, replaces_a_pbr_material_as_an_immutable_renderable_update)
{
	auto renderable = Renderable::make_default(engine.get_ecs());
	auto& object = spawn_renderable_object(engine, std::move(renderable));
	const RenderableID original = only_renderable_id(engine, object.get_id());

	const glm::vec4 base_color(0.1f, 0.2f, 0.3f, 0.4f);
	const RenderableID replacement = engine.set_renderable_pbr_material(
		original, base_color, 0.6f, 0.25f);

	EXPECT_NE(replacement, original);
	EXPECT_FALSE(engine.get_ecs().has_renderable(original));
	const auto& updated = engine.get_ecs().get_renderable(replacement).renderable;
	ASSERT_EQ(updated.material_owners.size(), 1);
	const auto& material = dynamic_cast<const PbrMaterial&>(
		updated.material_owners.front()->get());
	EXPECT_EQ(material.data.base_color_factor, base_color);
	EXPECT_FLOAT_EQ(material.data.metallic_factor, 0.6f);
	EXPECT_FLOAT_EQ(material.data.roughness_factor, 0.25f);
}

TEST_F(GameEngineTests, scene_round_trips_flattened_imported_pbr_override)
{
	auto loaded = ResourceLoader::load_model(
		engine.get_ecs(), "normal_mapped_authored_tangents.gltf");
	auto& object = engine.spawn_object<Object>();
	const ObjectID object_id = object.get_id();
	const RenderableID imported = engine.attach_renderable(
		object_id, std::move(loaded.meshes.front().renderables.front()));
	const auto& imported_material = dynamic_cast<const PbrMaterial&>(
		engine.get_ecs().get_renderable(imported).renderable.material_owners.front()->get());

	const RenderableID edited = engine.set_renderable_pbr_material(imported, PbrMaterialEdit{
		.base_color_factor = imported_material.data.base_color_factor,
		.metallic_factor = imported_material.data.metallic_factor,
		.roughness_factor = 0.2f,
		.normal_scale = imported_material.data.normal_scale,
		.normal_texture = { .action = PbrTextureEdit::Action::Clear },
	});
	const auto* saved_override = ResourceProvenance::material_override(
		engine.get_ecs().get_renderable(edited).renderable.material_owners.front()->get_id());
	ASSERT_NE(saved_override, nullptr);
	EXPECT_FALSE(saved_override->base_color_factor.has_value());
	ASSERT_TRUE(saved_override->roughness_factor.has_value());
	EXPECT_FLOAT_EQ(*saved_override->roughness_factor, 0.2f);
	ASSERT_TRUE(saved_override->normal_texture.has_value());
	EXPECT_EQ(saved_override->normal_texture->mode, PbrTextureOverride::Mode::Cleared);

	const std::string save_name = "krisp_scene_imported_pbr_override_test";
	const auto path = save_path(save_name);
	engine.save_scene(save_name);
	engine.load_scene(save_name);
	const auto& restored_renderable = engine.get_ecs().get_renderable(
		only_renderable_id(engine, object_id)).renderable;
	const auto& restored = dynamic_cast<const PbrMaterial&>(
		restored_renderable.material_owners.front()->get());
	EXPECT_FLOAT_EQ(restored.data.roughness_factor, 0.2f);
	EXPECT_TRUE(restored.textures.base_color.has_value());
	EXPECT_FALSE(restored.textures.normal.has_value());
	ASSERT_NE(ResourceProvenance::material_override(
		restored_renderable.material_owners.front()->get_id()), nullptr);
	std::filesystem::remove_all(path);
}

TEST_F(GameEngineTests, reverting_an_imported_factor_removes_the_sparse_override)
{
	auto loaded = ResourceLoader::load_model(
		engine.get_ecs(), "normal_mapped_authored_tangents.gltf");
	auto& object = engine.spawn_object<Object>();
	const RenderableID imported = engine.attach_renderable(
		object.get_id(), std::move(loaded.meshes.front().renderables.front()));
	const auto& source = dynamic_cast<const PbrMaterial&>(
		engine.get_ecs().get_renderable(imported).renderable.material_owners.front()->get());
	const auto source_data = source.data;
	const RenderableID changed = engine.set_renderable_pbr_material(imported, PbrMaterialEdit{
		.base_color_factor = source_data.base_color_factor,
		.metallic_factor = source_data.metallic_factor,
		.roughness_factor = 0.2f,
		.normal_scale = source_data.normal_scale,
	});
	const RenderableID reverted = engine.set_renderable_pbr_material(changed, PbrMaterialEdit{
		.base_color_factor = source_data.base_color_factor,
		.metallic_factor = source_data.metallic_factor,
		.roughness_factor = source_data.roughness_factor,
		.normal_scale = source_data.normal_scale,
	});
	const auto* material_override = ResourceProvenance::material_override(
		engine.get_ecs().get_renderable(reverted).renderable.material_owners.front()->get_id());
	ASSERT_NE(material_override, nullptr);
	EXPECT_FALSE(material_override->roughness_factor.has_value());
}

TEST_F(GameEngineTests, scene_round_trips_distinct_semantics_for_one_gltf_texture_index)
{
	TemporaryMutatedGltf resource("normal_mapped_authored_tangents.gltf",
		[](nlohmann::json& document)
		{
			document["materials"][0]["pbrMetallicRoughness"]
				["metallicRoughnessTexture"] = { { "index", 0 } };
		});
	auto loaded = ResourceLoader::load_model(engine.get_ecs(), resource.filename());
	auto& object = engine.spawn_object<Object>();
	const ObjectID object_id = object.get_id();
	engine.attach_renderable(
		object_id, std::move(loaded.meshes.front().renderables.front()));
	const std::string save_name = "krisp_scene_shared_gltf_texture_semantics_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	engine.load_scene(save_name);
	const auto& restored = engine.get_ecs().get_renderable(
		only_renderable_id(engine, object_id)).renderable;
	const auto& pbr = dynamic_cast<const PbrMaterial&>(
		restored.material_owners.front()->get());
	ASSERT_TRUE(pbr.textures.base_color.has_value());
	ASSERT_TRUE(pbr.textures.metallic_roughness.has_value());
	EXPECT_NE(
		pbr.textures.base_color->texture,
		pbr.textures.metallic_roughness->texture);
	const auto find_texture = [&restored](const MaterialID id) -> const TextureMaterial*
	{
		const auto owner = std::ranges::find_if(
			restored.material_owners,
			[id](const MaterialHandle& candidate) { return candidate->get_id() == id; });
		return owner == restored.material_owners.end()
			? nullptr : dynamic_cast<const TextureMaterial*>(&(*owner)->get());
	};
	const auto* base_color = find_texture(pbr.textures.base_color->texture);
	const auto* metallic_roughness = find_texture(pbr.textures.metallic_roughness->texture);
	ASSERT_NE(base_color, nullptr);
	ASSERT_NE(metallic_roughness, nullptr);
	EXPECT_EQ(base_color->semantic, ETextureSemantic::BASE_COLOR);
	EXPECT_EQ(metallic_roughness->semantic, ETextureSemantic::METALLIC_ROUGHNESS);
	std::filesystem::remove_all(path);
}

TEST_F(GameEngineTests, scene_round_trips_replaced_imported_texture_override)
{
	auto loaded = ResourceLoader::load_model(
		engine.get_ecs(), "normal_mapped_authored_tangents.gltf");
	auto& object = engine.spawn_object<Object>();
	const ObjectID object_id = object.get_id();
	const RenderableID imported = engine.attach_renderable(
		object_id, std::move(loaded.meshes.front().renderables.front()));
	const auto& imported_material = dynamic_cast<const PbrMaterial&>(
		engine.get_ecs().get_renderable(imported).renderable.material_owners.front()->get());
	engine.set_renderable_pbr_material(imported, PbrMaterialEdit{
		.base_color_factor = imported_material.data.base_color_factor,
		.metallic_factor = imported_material.data.metallic_factor,
		.roughness_factor = imported_material.data.roughness_factor,
		.normal_scale = imported_material.data.normal_scale,
		.base_color_texture = {
			.action = PbrTextureEdit::Action::Replace,
			.source = "texture.jpg",
		},
	});
	const std::string save_name = "krisp_scene_replaced_imported_texture_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	engine.load_scene(save_name);
	const auto& restored = engine.get_ecs().get_renderable(
		only_renderable_id(engine, object_id)).renderable;
	const auto& pbr = dynamic_cast<const PbrMaterial&>(
		restored.material_owners.front()->get());
	ASSERT_TRUE(pbr.textures.base_color.has_value());
	const auto replacement = std::ranges::find_if(
		restored.material_owners,
		[&pbr](const MaterialHandle& owner)
		{
			return owner->get_id() == pbr.textures.base_color->texture;
		});
	ASSERT_NE(replacement, restored.material_owners.end());
	const auto* texture = dynamic_cast<const TextureMaterial*>(&(*replacement)->get());
	ASSERT_NE(texture, nullptr);
	EXPECT_EQ(texture->semantic, ETextureSemantic::BASE_COLOR);
	EXPECT_EQ(texture->source, "texture.jpg");
	EXPECT_NE(texture->data, nullptr);
	const auto* source = ResourceProvenance::material(texture->get_id());
	ASSERT_NE(source, nullptr);
	EXPECT_EQ(source->kind, EExternalResourceKind::Texture);
	EXPECT_EQ(source->source, "texture.jpg");
	const auto* material_override = ResourceProvenance::material_override(
		pbr.get_id());
	ASSERT_NE(material_override, nullptr);
	ASSERT_TRUE(material_override->base_color_texture.has_value());
	EXPECT_EQ(
		material_override->base_color_texture->mode,
		PbrTextureOverride::Mode::Replaced);
	EXPECT_EQ(
		material_override->base_color_texture->texture,
		texture->get_id());
	std::filesystem::remove_all(path);
}

TEST_F(GameEngineTests, scene_round_trips_manual_exposure)
{
	const std::string save_name = "krisp_scene_exposure_test";
	const auto path = save_path(save_name);
	EXPECT_FLOAT_EQ(engine.get_exposure_ev(), 0.0f);
	engine.set_exposure_ev(2.3f);

	engine.save_scene(save_name);
	engine.set_exposure_ev(-4.0f);
	engine.load_scene(save_name);

	EXPECT_FLOAT_EQ(engine.get_exposure_ev(), 2.3f);
	std::filesystem::remove_all(path);
}

TEST_F(GameEngineTests, scene_load_replaces_renderable_identity_when_resources_are_rebuilt)
{
	auto& object = spawn_renderable_object(engine, Renderable::make_default(engine.get_ecs()));
	const auto old_id = only_renderable_id(engine, object.get_id());
	const auto object_id = object.get_id();
	engine.main_loop(0.1f);
	const std::string save_name = "krisp_scene_renderable_identity_test";
	const auto path = save_path(save_name);
	engine.save_scene(save_name);

	engine.load_scene(save_name);
	const auto restored_ids = engine.get_ecs().get_renderable_ids(object_id);
	ASSERT_EQ(restored_ids.size(), 1);
	EXPECT_NE(restored_ids.front(), old_id);
	EXPECT_NO_THROW(engine.main_loop(0.1f));
	std::filesystem::remove_all(path);
}

TEST_F(GameEngineTests, scene_round_trips_generated_texture_payload_and_metadata)
{
	auto texture = std::make_unique<TextureMaterial>();
	texture->width = 2;
	texture->height = 1;
	texture->channels = 4;
	texture->data_len = 8;
	texture->mip_sizes = { 8 };
	texture->semantic = ETextureSemantic::NORMAL;
	texture->source = "generated noise";
	std::vector<std::byte> pixels{
		std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
		std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8} };
	texture->data = std::make_unique<OwnedTextureData>(pixels);
	auto texture_owner = engine.get_ecs().get_material_system().add(std::move(texture));
	PbrMaterial::TextureSlots texture_slots{
		.normal = PbrMaterial::TextureBinding{ texture_owner->get_id() },
	};
	auto pbr_owner = engine.get_ecs().get_material_system().add(
		std::make_unique<PbrMaterial>(
			glm::vec4(1.0f), 1.0f, 1.0f, texture_slots));
	auto mesh_owner = engine.get_ecs().get_mesh_system().add(
		MeshFactory::cube(MeshFactory::EVertexType::TEXTURE));
	Renderable renderable{ .pipeline_render_type = ERenderType::STANDARD,
		.mesh_owner = mesh_owner, .material_owners = { pbr_owner, texture_owner } };
	auto& object = spawn_renderable_object(engine, renderable);
	const auto object_id = object.get_id();
	const std::string save_name = "krisp_scene_generated_texture_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	EXPECT_EQ(std::ranges::count_if(std::filesystem::directory_iterator(path), [](const auto& entry) {
		return entry.path().extension() == ".dat";
	}), 2);
	engine.load_scene(save_name);
	const auto& restored = dynamic_cast<const TextureMaterial&>(
		engine.get_ecs().get_renderable(only_renderable_id(engine, object_id))
			.renderable.material_owners[1]->get());
	EXPECT_EQ(restored.width, 2u);
	EXPECT_EQ(restored.height, 1u);
	EXPECT_EQ(restored.semantic, ETextureSemantic::NORMAL);
	EXPECT_EQ(restored.source, "generated noise");
	ASSERT_EQ(restored.data_len, pixels.size());
	EXPECT_EQ(std::memcmp(restored.data->get(), pixels.data(), pixels.size()), 0);
	std::filesystem::remove_all(path);
}

TEST_F(GameEngineTests, scene_round_trips_generated_skinned_mesh)
{
	SkinnedVertices vertices(3);
	vertices[0].pos = { 0.0f, 0.0f, 0.0f };
	vertices[1].pos = { 1.0f, 0.0f, 0.0f };
	vertices[2].pos = { 0.0f, 1.0f, 0.0f };
	for (auto& vertex : vertices) {
		vertex.normal = { 0.0f, 0.0f, 1.0f };
		vertex.bone_ids = glm::vec4(0.0f);
		vertex.bone_weights = { 1.0f, 0.0f, 0.0f, 0.0f };
	}
	auto mesh = engine.get_ecs().get_mesh_system().add(
		std::make_unique<SkinnedMesh>(vertices, VertexIndices{ 0, 1, 2 }));
	auto material = engine.get_ecs().get_material_system().add(std::make_unique<PbrMaterial>());
	Bone bone;
	bone.name = "root";
	const auto skeleton = engine.get_ecs().add_skeleton({ bone });
	Renderable renderable{ .pipeline_render_type = ERenderType::SKINNED_COLOR,
		.mesh_owner = mesh, .material_owners = { material } };
	auto& object = spawn_renderable_object(engine, std::move(renderable), skeleton);
	const auto object_id = object.get_id();
	const std::string save_name = "krisp_scene_generated_skinned_mesh_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	engine.load_scene(save_name);
	const auto& restored = dynamic_cast<const SkinnedMesh&>(
		engine.get_ecs().get_renderable(only_renderable_id(engine, object_id))
			.renderable.mesh_owner->get());
	ASSERT_EQ(restored.get_vertices().size(), vertices.size());
	EXPECT_EQ(restored.get_vertices()[2].pos, vertices[2].pos);
	EXPECT_EQ(restored.get_indices(), (VertexIndices{ 0, 1, 2 }));
	std::filesystem::remove_all(path);
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
	std::filesystem::remove_all(path);

	EXPECT_NE(engine.get_ecs().get_collider(object_id), nullptr);
	EXPECT_TRUE(engine.get_ecs().get_renderable_ids(object_id).empty());
}

TEST_F(GameEngineTests, scene_round_trips_imported_factor_material_by_provenance)
{
	const auto model_path = "import_variants.gltf";
	auto model = ResourceLoader::load_model(engine.get_ecs(), model_path);
	ASSERT_FALSE(model.meshes.empty());
	auto& object = engine.spawn_object<Object>();
	engine.attach_renderables(object.get_id(), model.meshes.front().renderables);
	const ObjectID object_id = object.get_id();
	const std::string save_name = "krisp_scene_imported_resource_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	std::ifstream yaml(path / "scene.yaml");
	const std::string contents((std::istreambuf_iterator<char>(yaml)), {});
	EXPECT_NE(contents.find("kind: model"), std::string::npos);
	EXPECT_EQ(std::ranges::count_if(std::filesystem::directory_iterator(path), [](const auto& entry) {
		return entry.path().extension() == ".dat";
	}), 0);
	EXPECT_EQ(contents.find("source: image"), std::string::npos);

	EXPECT_NO_THROW(engine.load_scene(save_name));
	std::filesystem::remove_all(path);
	const auto* restored = engine.get_object(object_id);
	ASSERT_NE(restored, nullptr);
	const auto restored_ids = engine.get_ecs().get_renderable_ids(restored->get_id());
	ASSERT_EQ(restored_ids.size(), 1);
	const auto& restored_renderable = engine.get_ecs().get_renderable(restored_ids.front()).renderable;
	EXPECT_TRUE(engine.get_ecs().get_mesh_system().contains(restored_renderable.get_mesh_id()));
	EXPECT_TRUE(engine.get_ecs().get_material_system().contains(restored_renderable.get_material_id(0)));
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
	std::ifstream yaml(path / "scene.yaml");
	const std::string contents((std::istreambuf_iterator<char>(yaml)), {});
	EXPECT_NE(contents.find("imported_source:"), std::string::npos);
	EXPECT_EQ(contents.find("key_frames:"), std::string::npos);

	EXPECT_NO_THROW(engine.load_scene(save_name));
	std::filesystem::remove_all(path);
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
	std::filesystem::remove_all(path);

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
	std::filesystem::remove_all(path);
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
	std::filesystem::remove_all(path);

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
	std::filesystem::remove_all(path);

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
		EXPECT_TRUE(engine.get_ecs().get_mesh_system().contains(mesh_id));
	for (const MaterialID material_id : helper_materials)
		EXPECT_TRUE(engine.get_ecs().get_material_system().contains(material_id));
}

TEST_F(GameEngineTests, scene_serialization_omits_transient_helpers)
{
	engine.get_gizmo().init();
	engine.spawn_object<Object>();
	const std::string save_name = "krisp_scene_transient_helpers_test";
	const auto path = save_path(save_name);

	engine.save_scene(save_name);
	std::ifstream stream(path / "scene.yaml");
	std::ostringstream contents;
	contents << stream.rdbuf();
	const auto saved = Deserializer::parse(contents.str());
	std::filesystem::remove_all(path);

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
	engine.get_ecs().add_rigid_body(object.get_id(), RigidBodyDefinition{
		.motion = PhysicsMotionType::Dynamic,
	});
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
	EXPECT_FALSE(engine.get_ecs().has_rigid_body(object_id));
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
	engine.get_ecs().get_mesh_system().take_retired();
	engine.get_ecs().get_material_system().take_retired();
	auto mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::icosahedron());
	auto material_owner = engine.get_ecs().get_material_system().add(std::make_unique<PbrMaterial>());
	const MeshID mesh_id = mesh_owner->get_id();
	const MaterialID material_id = material_owner->get_id();
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

	EXPECT_EQ(engine.get_ecs().get_mesh_system().take_retired(), (std::vector<MeshID>{ mesh_id }));
	EXPECT_EQ(engine.get_ecs().get_material_system().take_retired(), (std::vector<MaterialID>{ material_id }));
}

TEST_F(GameEngineTests, dont_cleanup_resource_if_not_ready)
{
	engine.get_ecs().get_mesh_system().take_retired();
	engine.get_ecs().get_material_system().take_retired();
	auto mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::icosahedron());
	auto material_owner = engine.get_ecs().get_material_system().add(std::make_unique<PbrMaterial>());
	const MeshID mesh_id = mesh_owner->get_id();
	const MaterialID material_id = material_owner->get_id();
	auto external_material_owner = engine.get_ecs().get_material_system().acquire(material_id);
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

	EXPECT_EQ(engine.get_ecs().get_mesh_system().take_retired(), (std::vector<MeshID>{ mesh_id }));
	EXPECT_TRUE(engine.get_ecs().get_material_system().take_retired().empty());
	external_material_owner.reset();
	EXPECT_EQ(engine.get_ecs().get_material_system().take_retired(), (std::vector<MaterialID>{ material_id }));
}

TEST_F(GameEngineTests, shared_renderable_resources_are_retained_for_each_spawned_object)
{
	engine.get_ecs().get_mesh_system().take_retired();
	engine.get_ecs().get_material_system().take_retired();
	auto mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::icosahedron());
	auto material_owner = engine.get_ecs().get_material_system().add(std::make_unique<PbrMaterial>());
	const MeshID mesh_id = mesh_owner->get_id();
	const MaterialID material_id = material_owner->get_id();
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
	EXPECT_TRUE(engine.get_ecs().get_mesh_system().contains(mesh_id));
	EXPECT_TRUE(engine.get_ecs().get_material_system().contains(material_id));

	engine.delete_object(second_id);
	engine.main_loop(1.0f);
	EXPECT_TRUE(engine.get_ecs().get_mesh_system().contains(mesh_id));
	EXPECT_TRUE(engine.get_ecs().get_material_system().contains(material_id));
	EXPECT_FALSE(mesh_lifetime.expired());
	EXPECT_FALSE(material_lifetime.expired());

	// The last frame containing the object remains available as "previous".
	// Advancing once releases its immutable definition and retained assets.
	engine.main_loop(1.0f);
	EXPECT_FALSE(engine.get_ecs().get_mesh_system().contains(mesh_id));
	EXPECT_FALSE(engine.get_ecs().get_material_system().contains(material_id));
	EXPECT_TRUE(mesh_lifetime.expired());
	EXPECT_TRUE(material_lifetime.expired());

	EXPECT_EQ(engine.get_ecs().get_mesh_system().take_retired(), (std::vector<MeshID>{ mesh_id }));
	EXPECT_EQ(engine.get_ecs().get_material_system().take_retired(), (std::vector<MaterialID>{ material_id }));
}

TEST_F(GameEngineTests, deleting_multiple_objects_destroys_each_resource_once)
{
	engine.get_ecs().get_mesh_system().take_retired();
	engine.get_ecs().get_material_system().take_retired();
	std::vector<MeshID> mesh_ids;
	std::vector<MaterialID> material_ids;
	std::vector<ObjectID> object_ids;
	for (int i = 0; i < 2; ++i)
	{
		auto mesh_owner = engine.get_ecs().get_mesh_system().add(MeshFactory::icosahedron());
		auto material_owner = engine.get_ecs().get_material_system().add(std::make_unique<PbrMaterial>());
		mesh_ids.push_back(mesh_owner->get_id());
		material_ids.push_back(material_owner->get_id());
		Renderable renderable;
		renderable.mesh_owner = mesh_owner;
		renderable.material_owners = { material_owner };
		object_ids.push_back(spawn_renderable_object(engine, renderable).get_id());
	}

	for (const auto object_id : object_ids)
		engine.delete_object(object_id);
	engine.main_loop(1.0f);

	auto retired_meshes = engine.get_ecs().get_mesh_system().take_retired();
	auto retired_materials = engine.get_ecs().get_material_system().take_retired();
	std::ranges::sort(retired_meshes);
	std::ranges::sort(retired_materials);
	std::ranges::sort(mesh_ids);
	std::ranges::sort(material_ids);
	EXPECT_EQ(retired_meshes, mesh_ids);
	EXPECT_EQ(retired_materials, material_ids);
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
