#include <camera.hpp>
#include <config.hpp>
#include <entity_component_system/light_source.hpp>
#include <entity_component_system/physics/physics.hpp>
#include <game_engine.hpp>
#include <gui/gui_manager.hpp>
#include <iapplication.hpp>
#include <objects/object.hpp>
#include <resource_paths.hpp>
#include <renderable/material.hpp>
#include <renderable/mesh_factory.hpp>
#include <utility.hpp>

#include <imgui.h>
#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


namespace
{
struct LightUiState
{
	ObjectID id{ 0 };
	std::string name;
	glm::vec3 position{ 0.0f };
	glm::vec3 color{ 1.0f };
	float intensity = 1.0f;
	bool enabled = true;
	bool casts_shadow = true;
};

struct LightUiRequest
{
	enum class Type { ADD, UPDATE, REMOVE } type;
	LightUiState light;
};

class LightPanel final : public EngineUiWindow
{
public:
	explicit LightPanel(std::function<void(GameEngine&)> process_lighting) :
		EngineUiWindow({ "sandbox-lights", "Lighting", GuiPanelDock::RIGHT, true }),
		process_lighting(std::move(process_lighting))
	{
	}

	void process(GameEngine& engine) override { process_lighting(engine); }

	void set_lights(std::vector<LightUiState> value, const bool discard_requests = false)
	{
		const std::lock_guard lock(mutex);
		lights = std::move(value);
		if (discard_requests)
			requests.clear();
	}

	std::vector<LightUiRequest> take_requests()
	{
		const std::lock_guard lock(mutex);
		return std::exchange(requests, {});
	}

private:
	void draw() override
	{
		ImGui::SetNextWindowSize(ImVec2(340.0f, 650.0f), ImGuiCond_FirstUseEver);
		if (!begin())
		{
			end();
			return;
		}
		const std::lock_guard lock(mutex);
		ImGui::Text("Point lights: %zu / %d", lights.size(), SDS::MAX_POINT_LIGHTS);
		ImGui::BeginDisabled(lights.size() >= SDS::MAX_POINT_LIGHTS);
		if (ImGui::Button("Add point light"))
			requests.push_back({ LightUiRequest::Type::ADD, {} });
		ImGui::EndDisabled();
		ImGui::TextWrapped("Edit positions here, or Shift-click a light marker to select its gizmo.");

		for (auto& light : lights)
		{
			const auto id = std::to_string(light.id.get_underlying());
			ImGui::PushID(id.c_str());
			ImGui::Separator();
			ImGui::TextUnformatted(light.name.c_str());
			bool removed = false;
			if (ImGui::Button("Remove"))
			{
				requests.push_back({ LightUiRequest::Type::REMOVE, light });
				removed = true;
			}

			bool changed = false;
			changed |= ImGui::Checkbox("Enabled", &light.enabled);
			changed |= ImGui::Checkbox("Casts shadow", &light.casts_shadow);
			changed |= ImGui::DragFloat3("Position", &light.position.x, 0.05f);
			changed |= ImGui::ColorEdit3("Linear RGB", &light.color.x);
			changed |= ImGui::DragFloat("Intensity", &light.intensity, 1.0f,
				0.0f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			if (changed && !removed)
				requests.push_back({ LightUiRequest::Type::UPDATE, light });
			ImGui::PopID();
		}
		end();
	}

	std::function<void(GameEngine&)> process_lighting;
	std::mutex mutex;
	std::vector<LightUiState> lights;
	std::vector<LightUiRequest> requests;
};

class SandboxApplication final : public DummyApplication
{
public:
	void create_ui(GameEngine& engine, ApplicationUiManager&) override
	{
		panel = &engine.get_gui_manager().spawn_gui<LightPanel>(
			[this](GameEngine& game) { process_lighting(game); });
	}

	void on_begin(GameEngine& engine) override
	{
		engine.set_exposure_ev(0.0f);
		engine.get_camera().look_at(
			glm::vec3(0.0f, 2.4f, 0.8f),
			glm::vec3(0.0f, 5.5f, -15.0f));

		spawn_factor_grid(engine);
		spawn_floor(engine);
		spawn_light(engine, "White key", glm::vec3(0.0f, 6.5f, -7.0f),
			glm::vec3(1.0f), 225.0f, true);
		spawn_light(engine, "Warm fill", glm::vec3(-4.0f, 3.5f, -1.0f),
			glm::vec3(1.0f, 0.55f, 0.28f), 55.0f, false);
		spawn_light(engine, "Cool side", glm::vec3(4.0f, 4.5f, 1.0f),
			glm::vec3(0.35f, 0.58f, 1.0f), 80.0f, true);
		refresh_lights(engine, true);
	}

	void on_scene_loaded(GameEngine& engine) override
	{
		engine.set_game_mode(EGameMode::EDITOR);
		marker_mesh.reset();
		marker_renderables.clear();
		refresh_lights(engine, true);
	}

private:
	void process_lighting(GameEngine& engine)
	{
		for (const auto& request : panel->take_requests())
		{
			switch (request.type)
			{
				case LightUiRequest::Type::ADD:
					if (engine.get_ecs().get_light_source_ids().size() < SDS::MAX_POINT_LIGHTS)
						spawn_light(engine, "Point light", glm::vec3(0.0f, 4.0f, 0.0f),
							glm::vec3(1.0f), 50.0f, true);
					break;
				case LightUiRequest::Type::UPDATE:
					update_light(engine, request.light);
					break;
				case LightUiRequest::Type::REMOVE:
					if (engine.get_ecs().get_light_component(request.light.id))
						engine.delete_object(request.light.id);
					marker_renderables.erase(request.light.id);
					break;
			}
		}
		refresh_lights(engine);
	}

	static void spawn_floor(GameEngine& engine)
	{
		Renderable renderable{
			.name = "PBR shadow receiver",
			.pipeline_render_type = ERenderType::COLOR,
			.mesh_owner = engine.get_ecs().get_mesh_system().add(
				MeshFactory::cube(MeshFactory::EVertexType::COLOR)),
			.material_owners = {
				engine.get_ecs().get_material_system().add(
					std::make_unique<PbrMaterial>(
						glm::vec4(0.45f, 0.45f, 0.45f, 1.0f), 0.0f, 0.9f)),
			},
		};
		auto& floor = engine.spawn_object<Object>();
		floor.set_name("PBR shadow receiver");
		engine.attach_renderable(floor.get_id(), std::move(renderable));
		auto& transform = engine.get_ecs().get_transformation(floor.get_id());
		transform.set_position(glm::vec3(0.0f, -0.2f, 0.5f));
		transform.set_scale(glm::vec3(8.5f, 0.1f, 7.0f));
	}

	void spawn_light(
		GameEngine& engine, std::string name, const glm::vec3 position,
		const glm::vec3 color, const float intensity, const bool casts_shadow)
	{
		auto& light = engine.spawn_object<Object>();
		light.set_name(std::move(name));
		const auto id = light.get_id();
		engine.get_ecs().get_transformation(id).set_position(position);
		engine.get_ecs().add_light_source(id, LightComponent{
			.intensity = intensity,
			.color = color,
			.enabled = true,
			.casts_shadow = casts_shadow,
		});

		Renderable marker{
			.name = "Point-light handle",
			.pipeline_render_type = ERenderType::COLOR,
			.shading_mode = EShadingMode::UNLIT,
			.casts_shadow = false,
			.mesh_owner = get_marker_mesh(engine),
			.material_owners = {
				engine.get_ecs().get_material_system().add(
					std::make_unique<PbrMaterial>(
						glm::vec4(glm::clamp(color, 0.0f, 1.0f), 1.0f), 0.0f, 0.25f)),
			},
		};
		marker.local_transform.set_scale(glm::vec3(1.0f / 3.0f));
		marker_renderables[id] = engine.attach_renderable(id, std::move(marker));
		engine.get_ecs().add_rigid_body(id, RigidBodyDefinition{
			.shape = SpherePhysicsShape{1.2f},
			.participation = PhysicsParticipation::QueryOnly,
		});
		engine.get_ecs().add_clickable_entity(id);
	}

	MeshHandle get_marker_mesh(GameEngine& engine)
	{
		if (!marker_mesh)
			marker_mesh = engine.get_ecs().get_mesh_system().add(MeshFactory::sphere(
				MeshFactory::EVertexType::COLOR,
				MeshFactory::GenerationMethod::ICO_SPHERE,
				100));
		return marker_mesh;
	}

	void update_light(GameEngine& engine, const LightUiState& state)
	{
		auto& ecs = engine.get_ecs();
		auto* component = ecs.get_light_component(state.id);
		if (!component)
			return;
		const bool color_changed = component->color != state.color;
		LightComponent updated{
			.intensity = state.intensity,
			.color = state.color,
			.enabled = state.enabled,
			.casts_shadow = state.casts_shadow,
		};
		validate_light_component(updated);
		ecs.get_transformation(state.id).set_position(state.position);
		*component = updated;

		const auto marker = marker_renderables.find(state.id);
		if (color_changed && marker != marker_renderables.end() && ecs.has_renderable(marker->second))
			marker->second = engine.set_renderable_pbr_material(marker->second,
				glm::vec4(glm::clamp(state.color, 0.0f, 1.0f), 1.0f), 0.0f, 0.25f);
	}

	void refresh_lights(GameEngine& engine, const bool discard_requests = false)
	{
		std::vector<LightUiState> states;
		auto& ecs = engine.get_ecs();
		const auto light_ids = ecs.get_light_source_ids();
		states.reserve(light_ids.size());
		for (const auto id : light_ids)
		{
			const auto* component = ecs.get_light_component(id);
			if (!component)
				continue;
			const auto& object = ecs.get_object(id);
			states.push_back({
				.id = id,
				.name = object.get_name(),
				.position = ecs.get_position(id),
				.color = component->color,
				.intensity = component->intensity,
				.enabled = component->enabled,
				.casts_shadow = component->casts_shadow,
			});

			bool found_marker = false;
			for (const auto renderable_id : ecs.get_renderable_ids(id))
			{
				if (ecs.get_renderable(renderable_id).renderable.name != "Point-light handle")
					continue;
				if (!found_marker)
				{
					marker_renderables[id] = renderable_id;
					found_marker = true;
				}
			}
		}
		std::erase_if(marker_renderables, [&ecs](const auto& entry) {
			return !ecs.has_object(entry.first) || !ecs.has_renderable(entry.second);
		});
		panel->set_lights(std::move(states), discard_requests);
	}

	static void spawn_factor_grid(GameEngine& engine)
	{
		const auto sphere = engine.get_ecs().get_mesh_system().add(MeshFactory::sphere(
			MeshFactory::EVertexType::COLOR,
			MeshFactory::GenerationMethod::ICO_SPHERE,
			400));
		constexpr std::array roughness{ 0.04f, 0.25f, 0.5f, 0.75f, 1.0f };
		constexpr std::array metallic{ 0.0f, 0.5f, 1.0f };
		constexpr glm::vec4 base_color(0.8f, 0.15f, 0.05f, 1.0f);

		for (size_t row = 0; row < metallic.size(); ++row)
		{
			for (size_t column = 0; column < roughness.size(); ++column)
			{
				Renderable renderable{
					.name = "PBR factor reference",
					.pipeline_render_type = ERenderType::COLOR,
					.mesh_owner = sphere,
					.material_owners = {
						engine.get_ecs().get_material_system().add(
							std::make_unique<PbrMaterial>(
								base_color, metallic[row], roughness[column])),
					},
				};
				auto& object = engine.spawn_object<Object>();
				object.set_name("PBR factor reference");
				engine.attach_renderable(object.get_id(), std::move(renderable));

				auto& transform = engine.get_ecs().get_transformation(object.get_id());
				transform.set_position(glm::vec3(
					(static_cast<float>(column) - 2.0f) * 1.7f,
					0.8f + static_cast<float>(row) * 1.7f,
					0.0f));
				transform.set_scale(glm::vec3(0.85f));
			}
		}
	}

	LightPanel* panel = nullptr;
	MeshHandle marker_mesh;
	std::unordered_map<ObjectID, RenderableID> marker_renderables;
};
}

int main()
{
	auto runtime_paths = ResourcePaths::paths_for_executable(PROJECT_NAME);
	runtime_paths.app_resources = std::filesystem::path(KRISP_SOURCE_DIR) / "resources/sandbox";
	Config::init(PROJECT_NAME, std::move(runtime_paths));
	auto engine = GameEngine::create<SandboxApplication>();
	engine.spawn_cubemap();
	engine.run();
}
