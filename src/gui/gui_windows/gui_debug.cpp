#include "gui_debug.hpp"

#include "camera.hpp"
#include "game_engine.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"
#include "renderable/mesh.hpp"

#include <imgui.h>

#include <array>
#include <unordered_set>

namespace
{
constexpr std::array<glm::vec3, 10> collider_colors{{
	{0.15f, 0.85f, 1.00f}, {1.00f, 0.35f, 0.25f}, {0.35f, 1.00f, 0.40f},
	{1.00f, 0.80f, 0.20f}, {0.75f, 0.40f, 1.00f}, {1.00f, 0.35f, 0.75f},
	{0.25f, 0.55f, 1.00f}, {0.95f, 0.60f, 0.20f}, {0.30f, 1.00f, 0.75f},
	{0.80f, 0.85f, 0.95f},
}};
constexpr float collider_opacity = 0.35f;
}

GuiDebug::GuiDebug() :
	EngineUiWindow({ "debug", "Debug", GuiPanelDock::RIGHT })
{
}

void GuiDebug::process(GameEngine& engine)
{
	is_paused = engine.is_paused();
	if (should_toggle_pause)
	{
		engine.toggle_paused();
		is_paused = engine.is_paused();
		should_toggle_pause = false;
	}

	// TODO: fix bone visualisers
	// if (show_bone_visualisers.changed)
	// {
	// 	for (const auto entity : engine.get_ecs().get_all_skinned_entities())
	// 	{
	// 		engine.get_object(entity)->set_visibility(!show_bone_visualisers);
	// 		for (const auto visualiser : engine.get_ecs().get_skeletal_component(entity).get_visualisers())
	// 		{
	// 			engine.get_object(visualiser)->set_visibility(show_bone_visualisers);
	// 		}
	// 	}

	// 	show_bone_visualisers.changed = false;
	// }

	if (selected_object.changed)
	{
		const auto pos = engine.get_ecs().get_transformation(selected_object).get_position();
		engine.get_camera().look_at(pos);
		engine.get_gizmo().select_object(engine.get_object(selected_object.value));
		selected_object.changed = false;
	}

	if (show_collider_visualisers.changed)
	{
		if (!show_collider_visualisers.value)
		{
			clear_physics_visualiser(engine);
		}

		show_collider_visualisers.changed = false;
	}

	if (show_collider_visualisers.value)
	{
		refresh_physics_visualiser(engine);
	}

	if (should_refresh_objects_list)
	{
		object_ids.clear();
		object_ids_strs.clear();
		for (auto& [id, object] : engine.get_objects())
		{
			if (object->is_transient())
				continue;
			object_ids.push_back(id);
			const auto& name = object->get_name();
			object_ids_strs.push_back(name.empty() ? std::to_string(id.get_underlying()) : name);
		}

		should_refresh_objects_list = false;
	}

}

void GuiDebug::draw()
{
	if (begin())
	{

	if (ImGui::Button(is_paused ? "Resume" : "Pause"))
	{
		should_toggle_pause = true;
	}

	if (ImGui::Button("screenshot"))
	{
		should_take_screenshot = true;
	}

	if (ImGui::Button(is_recording ? "Stop Recording" : "Start Recording"))
	{
		if (is_recording)
			should_stop_recording = true;
		else
			should_start_recording = true;
	}
	if (is_recording)
	{
		ImGui::SameLine();
		ImGui::TextColored({1.0f, 0.0f, 0.0f, 1.0f}, "REC");
	}

	if (ImGui::Checkbox("Show Bone Visualisers", &show_bone_visualisers.value))
	{
		show_bone_visualisers.changed = true;
	}

	if (ImGui::Checkbox("Show Collider Visualisers", &show_collider_visualisers.value))
	{
		show_collider_visualisers.changed = true;
	}

	if (ImGui::Button("Refresh Objects List"))
	{
		should_refresh_objects_list = true;
	}

	ImGui::InputText("Filter", filter_text.data(), filter_text.size());

	if (ImGui::BeginCombo("Objects", "Select Object"))
	{
		const std::string search_text = filter_text.data();
		for (int i = 0; i < object_ids.size(); i++)
		{
			if (!search_text.empty() && object_ids_strs[i].find(search_text) == std::string::npos)
			{
				continue;
			}

			// bool is_selected = (current_item == items[n]); // You can store your selection however you want, outside or inside your objects
			if (ImGui::Selectable(object_ids_strs[i].data(), selected_object == object_ids[i]))
			{
				selected_object = object_ids[i];
				selected_object.changed = true;
			}
				// current_item = items[n];

			// if (is_selected)
			// {
			// 	ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
			// }
		}
		ImGui::EndCombo();
	}

	}
	end();
}

void GuiDebug::refresh_physics_visualiser(GameEngine& engine)
{
	auto& ecs = engine.get_ecs();
	if (physics_visual_materials.empty()) {
		physics_visual_materials.reserve(collider_colors.size());
		for (const glm::vec3 color : collider_colors) {
			auto material = std::make_unique<PbrMaterial>(glm::vec4(color, 1.0f), 0.0f, 1.0f);
			physics_visual_materials.push_back(ecs.get_material_system().add(std::move(material)));
		}
	}

	std::unordered_set<EntityID> seen;
	for (const auto& body : ecs.get_debug_bodies()) {
		seen.insert(body.entity);
		auto found = physics_visuals.find(body.entity);
		if (found != physics_visuals.end() && found->second.body_id != body.body_id) {
			engine.delete_object(found->second.object);
			physics_visuals.erase(found);
			suppressed_physics_visuals.insert(body.entity);
			continue;
		}
		if (suppressed_physics_visuals.contains(body.entity)) continue;
		if (found == physics_visuals.end()) {
			const auto triangles = ecs.get_debug_shape_triangles(body.entity);
			if (triangles.empty()) continue;
			ColorVertices vertices;
			VertexIndices indices;
			vertices.reserve(triangles.size() * 3);
			indices.reserve(triangles.size() * 3);
			for (const auto& triangle : triangles) {
				const glm::vec3 normal = glm::normalize(glm::cross(
					triangle.vertices[1] - triangle.vertices[0], triangle.vertices[2] - triangle.vertices[0]));
				for (const glm::vec3 vertex : triangle.vertices) {
					indices.push_back(static_cast<uint32_t>(vertices.size()));
					vertices.push_back({vertex, normal});
				}
			}
			auto& object = engine.spawn_object<Object>();
			object.set_name("Jolt Collision Shape");
			object.set_transient(true);
			Renderable renderable;
			renderable.pipeline_render_type = ERenderType::COLOR;
			renderable.alpha_mode = EAlphaMode::BLEND;
			renderable.opacity = collider_opacity;
			renderable.casts_shadow = false;
			renderable.render_on_top = true;
			renderable.mesh_owner = ecs.get_mesh_system().add(
				std::make_unique<ColorMesh>(std::move(vertices), std::move(indices)));
			renderable.material_owners.push_back(physics_visual_materials[
				std::hash<EntityID>{}(body.entity) % physics_visual_materials.size()]);
			engine.attach_renderable(object.get_id(), std::move(renderable));
			found = physics_visuals.emplace(body.entity,
				PhysicsVisual{object.get_id(), body.body_id}).first;
		}
		auto& transform = ecs.get_transformation(found->second.object);
		transform.set_position(body.position);
		transform.set_rotation(body.rotation);
	}

	for (auto it = physics_visuals.begin(); it != physics_visuals.end(); ) {
		if (seen.contains(it->first)) {
			++it;
		} else {
			engine.delete_object(it->second.object);
			it = physics_visuals.erase(it);
		}
	}
}

void GuiDebug::clear_physics_visualiser(GameEngine& engine)
{
	for (const auto& [_, visual] : physics_visuals)
		engine.delete_object(visual.object);
	physics_visuals.clear();
	suppressed_physics_visuals.clear();
	physics_visual_materials.clear();
}
