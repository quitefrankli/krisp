#include "billiards_ui.hpp"

#include <camera.hpp>
#include <config.hpp>
#include <entity_component_system/light_source.hpp>
#include <game_engine.hpp>
#include <iapplication.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>


namespace
{
constexpr float TABLE_LENGTH = 10.0f;
constexpr float TABLE_WIDTH = 5.0f;
constexpr float TABLE_SURFACE_Y = 0.65f;
constexpr float BALL_RADIUS = 0.16f;
constexpr float BALL_Y = TABLE_SURFACE_Y + BALL_RADIUS;
constexpr float CUE_LENGTH = 3.0f;
constexpr float CUE_GAP = 0.08f;
constexpr float MAX_PULLBACK = 1.2f;
constexpr glm::vec3 CUE_BALL_START{-2.5f, BALL_Y, 0.0f};
constexpr glm::vec3 RACK_APEX{2.0f, BALL_Y, 0.0f};

struct SpawnedObject
{
	ObjectID id;
	glm::vec3 initial_position;
};

ColorMaterial make_material(const glm::vec3& color, const float shininess = 24.0f)
{
	ColorMaterial material;
	material.data.ambient = color * 0.35f;
	material.data.diffuse = color;
	material.data.specular = glm::vec3(0.35f);
	material.data.shininess = shininess;
	return material;
}

class BilliardsApplication : public IApplication
{
public:
	void create_ui(GameEngine&, ApplicationUiManager& ui) override
	{
		ui.set_theme({
			.text = { 0.94f, 0.96f, 0.92f, 1.0f },
			.window_background = { 0.04f, 0.10f, 0.07f, 0.94f },
			.accent = { 0.16f, 0.52f, 0.30f, 1.0f },
			.window_rounding = 8.0f,
			.window_border_size = 1.0f,
		});
		ui.register_window<BilliardsControlsWindow>({
			.anchor = ApplicationUiAnchor::TOP_RIGHT,
			.offset = { -16.0f, 16.0f },
			.size = { 330.0f, 175.0f },
		}, ui_state);
	}

	bool allows_playerless_normal_mode() const override { return true; }

	void on_begin(GameEngine& engine) override
	{
		this->engine = &engine;
		create_resources();
		spawn_table();
		spawn_balls();
		spawn_cue();
		spawn_light();

		engine.spawn_cubemap();
		set_fixed_camera(engine);
		engine.set_camera_orbit_with_right_mouse(false);
		engine.set_game_mode(EGameMode::NORMAL);
		reset_rack();
	}

	void on_tick(GameEngine& engine, float) override
	{
		// NORMAL mode updates the camera from mouse motion before application ticks.
		// Restore the diagnostic view before projecting the cursor onto the table.
		set_fixed_camera(engine);
		if (ui_state.take_reset_request())
			reset_rack();

		const auto table_point = mouse_table_point(engine);
		if (!table_point)
			return;

		if (!charging)
		{
			const glm::vec3 direction = flattened_direction(CUE_BALL_START, *table_point);
			if (glm::length2(direction) > 0.0f)
				aim_direction = direction;
		}
		else
		{
			const float pullback = std::clamp(
				glm::dot(charge_origin - *table_point, aim_direction), 0.0f, MAX_PULLBACK);
			preview_power = pullback / MAX_PULLBACK;
			ui_state.publish_power(preview_power);
		}
		update_cue();
	}

	void on_mouse_button(GameEngine& engine, const MouseInput& input) override
	{
		if (input.button != EMouseButton::LEFT || input.modifier != EKeyModifier::NONE)
			return;

		if (input.action == EInputAction::PRESS)
		{
			const auto table_point = mouse_table_point(engine);
			if (!table_point)
				return;
			const glm::vec3 direction = flattened_direction(CUE_BALL_START, *table_point);
			if (glm::length2(direction) == 0.0f)
				return;
			aim_direction = direction;
			charge_origin = *table_point;
			charging = true;
		}
		else if (input.action == EInputAction::RELEASE && charging)
		{
			charging = false;
			preview_power = 0.0f;
			ui_state.publish_power(0.0f);
			update_cue();
		}
	}

	void on_click(GameEngine&, Object&) override {}
	void on_key_press(GameEngine&, const KeyInput&) override {}

private:
	void create_resources()
	{
		auto& ecs = engine->get_ecs();
		cube_mesh = ecs.get_mesh_system().add(MeshFactory::cube());
		sphere_mesh = ecs.get_mesh_system().add(MeshFactory::sphere());
		circle_mesh = ecs.get_mesh_system().add(MeshFactory::circle({}, 48));
		cylinder_mesh = ecs.get_mesh_system().add(MeshFactory::cylinder({}, 24));

		felt_material = ecs.get_material_system().add(
			std::make_unique<ColorMaterial>(make_material({ 0.03f, 0.34f, 0.16f }, 8.0f)));
		wood_material = ecs.get_material_system().add(
			std::make_unique<ColorMaterial>(make_material({ 0.24f, 0.09f, 0.035f })));
		pocket_material = ecs.get_material_system().add(
			std::make_unique<ColorMaterial>(make_material({ 0.008f, 0.008f, 0.01f }, 2.0f)));
		cue_material = ecs.get_material_system().add(
			std::make_unique<ColorMaterial>(make_material({ 0.72f, 0.52f, 0.25f })));
	}

	Object& spawn_primitive(
		const MeshHandle& mesh,
		const MaterialHandle& material,
		const glm::vec3& position,
		const glm::vec3& scale,
		const std::string_view name)
	{
		Renderable renderable;
		renderable.pipeline_render_type = ERenderType::COLOR;
		renderable.mesh_owner = mesh;
		renderable.material_owners = { material };
		auto& object = engine->spawn_object<Object>();
		object.set_name(name);
		engine->attach_renderable(object.get_id(), std::move(renderable));
		auto& transform = engine->get_ecs().get_transformation(object.get_id());
		transform.set_position(position);
		transform.set_scale(scale);
		return object;
	}

	void spawn_table()
	{
		spawn_primitive(cube_mesh, wood_material, { 0.0f, 0.2f, 0.0f },
			{ TABLE_LENGTH + 1.1f, 0.8f, TABLE_WIDTH + 1.1f }, "Table base");
		spawn_primitive(cube_mesh, felt_material, { 0.0f, 0.58f, 0.0f },
			{ TABLE_LENGTH, 0.14f, TABLE_WIDTH }, "Playing surface");

		constexpr float rail_height = 0.32f;
		constexpr float rail_thickness = 0.32f;
		constexpr float corner_gap = 0.62f;
		constexpr float side_gap = 0.72f;
		const float long_segment_length = (TABLE_LENGTH - 2.0f * corner_gap - side_gap) * 0.5f;
		const float long_segment_offset = side_gap * 0.5f + long_segment_length * 0.5f;
		for (const float z : { -TABLE_WIDTH * 0.5f, TABLE_WIDTH * 0.5f })
		{
			for (const float x : { -long_segment_offset, long_segment_offset })
				spawn_primitive(cube_mesh, wood_material,
					{ x, TABLE_SURFACE_Y + rail_height * 0.5f, z },
					{ long_segment_length, rail_height, rail_thickness }, "Long rail");
		}
		const float short_rail_length = TABLE_WIDTH - 2.0f * corner_gap;
		for (const float x : { -TABLE_LENGTH * 0.5f, TABLE_LENGTH * 0.5f })
			spawn_primitive(cube_mesh, wood_material,
				{ x, TABLE_SURFACE_Y + rail_height * 0.5f, 0.0f },
				{ rail_thickness, rail_height, short_rail_length }, "Short rail");

		constexpr float pocket_radius = 0.34f;
		const std::array<glm::vec3, 6> pockets{{
			{ -TABLE_LENGTH * 0.5f, TABLE_SURFACE_Y + 0.005f, -TABLE_WIDTH * 0.5f },
			{ -TABLE_LENGTH * 0.5f, TABLE_SURFACE_Y + 0.005f,  TABLE_WIDTH * 0.5f },
			{ 0.0f, TABLE_SURFACE_Y + 0.005f, -TABLE_WIDTH * 0.5f },
			{ 0.0f, TABLE_SURFACE_Y + 0.005f,  TABLE_WIDTH * 0.5f },
			{ TABLE_LENGTH * 0.5f, TABLE_SURFACE_Y + 0.005f, -TABLE_WIDTH * 0.5f },
			{ TABLE_LENGTH * 0.5f, TABLE_SURFACE_Y + 0.005f,  TABLE_WIDTH * 0.5f },
		}};
		for (const auto& position : pockets)
			spawn_primitive(circle_mesh, pocket_material, position,
				glm::vec3(pocket_radius * 2.0f), "Pocket");
	}

	void spawn_balls()
	{
		const std::array<glm::vec3, 15> colors{{
			{ 0.95f, 0.78f, 0.08f }, { 0.12f, 0.28f, 0.85f }, { 0.82f, 0.12f, 0.10f },
			{ 0.38f, 0.12f, 0.58f }, { 0.015f, 0.015f, 0.018f }, { 0.08f, 0.52f, 0.20f },
			{ 0.55f, 0.08f, 0.08f }, { 0.95f, 0.42f, 0.06f }, { 0.92f, 0.72f, 0.12f },
			{ 0.18f, 0.38f, 0.92f }, { 0.92f, 0.18f, 0.15f }, { 0.48f, 0.18f, 0.68f },
			{ 0.98f, 0.50f, 0.10f }, { 0.12f, 0.62f, 0.26f }, { 0.68f, 0.12f, 0.12f },
		}};
		ball_materials.reserve(colors.size() + 1);
		ball_materials.push_back(engine->get_ecs().get_material_system().add(
			std::make_unique<ColorMaterial>(make_material(glm::vec3(0.96f), 72.0f))));
		for (const auto& color : colors)
			ball_materials.push_back(engine->get_ecs().get_material_system().add(
				std::make_unique<ColorMaterial>(make_material(color, 72.0f))));

		auto& cue_ball = spawn_primitive(sphere_mesh, ball_materials[0], CUE_BALL_START,
			glm::vec3(BALL_RADIUS * 2.0f), "Cue ball");
		balls.push_back({ cue_ball.get_id(), CUE_BALL_START });

		constexpr float row_spacing = BALL_RADIUS * 1.82f;
		constexpr float column_spacing = BALL_RADIUS * 2.08f;
		std::size_t ball_index = 1;
		for (int row = 0; row < 5; ++row)
		{
			for (int column = 0; column <= row; ++column)
			{
				const glm::vec3 position = RACK_APEX + glm::vec3(
					row * row_spacing, 0.0f, (column - row * 0.5f) * column_spacing);
				auto& ball = spawn_primitive(sphere_mesh, ball_materials[ball_index], position,
					glm::vec3(BALL_RADIUS * 2.0f), "Object ball " + std::to_string(ball_index));
				balls.push_back({ ball.get_id(), position });
				++ball_index;
			}
		}
	}

	void spawn_cue()
	{
		auto& cue = spawn_primitive(cylinder_mesh, cue_material, CUE_BALL_START,
			{ 0.055f, CUE_LENGTH, 0.055f }, "Cue stick");
		cue_id = cue.get_id();
	}

	void spawn_light()
	{
		auto light_material = engine->get_ecs().get_material_system().add(
			std::make_unique<ColorMaterial>(make_material({ 1.0f, 0.92f, 0.76f })));
		auto& light = spawn_primitive(sphere_mesh, light_material, { -1.0f, 7.0f, -1.0f },
			glm::vec3(0.24f), "Overhead light");
		engine->get_ecs().add_light_source(light.get_id(), {
			.intensity = 1.6f,
			.color = { 1.0f, 0.92f, 0.78f },
		});
	}

	void reset_rack()
	{
		for (const auto& ball : balls)
			engine->get_ecs().get_transformation(ball.id).set_position(ball.initial_position);
		charging = false;
		preview_power = 0.0f;
		aim_direction = Maths::right_vec;
		ui_state.publish_power(0.0f);
		update_cue();
	}

	void update_cue()
	{
		if (!cue_id)
			return;
		const float pullback = preview_power * MAX_PULLBACK;
		const float distance = BALL_RADIUS + CUE_GAP + CUE_LENGTH * 0.5f + pullback;
		auto& transform = engine->get_ecs().get_transformation(*cue_id);
		transform.set_position(CUE_BALL_START - aim_direction * distance);
		transform.set_rotation(Maths::RotationBetweenVectors(Maths::up_vec, aim_direction));
	}

	static glm::vec3 flattened_direction(const glm::vec3& origin, const glm::vec3& target)
	{
		glm::vec3 direction = target - origin;
		direction.y = 0.0f;
		const float length_squared = glm::length2(direction);
		return length_squared > 0.000001f ? direction / std::sqrt(length_squared) : glm::vec3(0.0f);
	}

	static std::optional<glm::vec3> mouse_table_point(const GameEngine& engine)
	{
		const Maths::Ray ray = engine.get_mouse_ray();
		const Maths::Plane table_plane({ 0.0f, TABLE_SURFACE_Y, 0.0f }, Maths::up_vec);
		if (!Maths::check_ray_plane_intersection(ray, table_plane))
			return std::nullopt;
		return Maths::ray_plane_intersection(ray, table_plane);
	}

	static void set_fixed_camera(GameEngine& engine)
	{
		engine.get_camera().look_at(
			glm::vec3(0.0f, TABLE_SURFACE_Y, 0.0f),
			glm::vec3(-7.5f, 8.5f, -8.5f));
	}

	GameEngine* engine = nullptr;
	BilliardsUiState ui_state;
	MeshHandle cube_mesh;
	MeshHandle sphere_mesh;
	MeshHandle circle_mesh;
	MeshHandle cylinder_mesh;
	MaterialHandle felt_material;
	MaterialHandle wood_material;
	MaterialHandle pocket_material;
	MaterialHandle cue_material;
	std::vector<MaterialHandle> ball_materials;
	std::vector<SpawnedObject> balls;
	std::optional<ObjectID> cue_id;
	glm::vec3 aim_direction = Maths::right_vec;
	glm::vec3 charge_origin{};
	float preview_power = 0.0f;
	bool charging = false;
};
}

int main(int, char**)
{
	Config::init(PROJECT_NAME);
	auto engine = GameEngine::create<BilliardsApplication>();
	engine.run();
}
