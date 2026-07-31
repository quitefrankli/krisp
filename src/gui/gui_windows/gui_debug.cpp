#include "gui_debug.hpp"

#include "camera.hpp"
#include "game_engine.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"

#include <imgui.h>

#include <unordered_set>

namespace
{
Object& get_or_spawn_collider_visual(
	GameEngine& engine,
	std::unordered_map<EntityID, ObjectID>& collider_visualiser_ids,
	const EntityID collider_entity,
	const Collider& collider)
{
	auto it = collider_visualiser_ids.find(collider_entity);
	if (it != collider_visualiser_ids.end())
	{
		if (auto* existing = engine.get_object(it->second))
			return *existing;
		collider_visualiser_ids.erase(it);
	}

	Object& visual = collider.spawn_debug_object(engine);
	collider_visualiser_ids.emplace(collider_entity, visual.get_id());
	return visual;
}
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
			clear_collider_visualisers(engine);
		}

		show_collider_visualisers.changed = false;
	}

	if (show_collider_visualisers.value)
	{
		sync_collider_visualisers(engine);
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

void GuiDebug::sync_collider_visualisers(GameEngine& engine)
{
	std::unordered_set<EntityID> seen;

	for (const auto& [entity_id, component] : engine.get_ecs().get_all_colliders())
	{
		if (component.persistence == ColliderPersistence::Transient)
			continue;
		seen.insert(entity_id);

		const Collider* collider = engine.get_ecs().get_collider(entity_id);
		if (!collider)
		{
			continue;
		}

		Object& visual = get_or_spawn_collider_visual(engine, collider_visualiser_ids, entity_id, *collider);
		collider->update_debug_object(engine, visual);
	}

	for (auto it = collider_visualiser_ids.begin(); it != collider_visualiser_ids.end(); )
	{
		if (seen.contains(it->first))
		{
			++it;
			continue;
		}

		engine.delete_object(it->second);
		it = collider_visualiser_ids.erase(it);
	}
}

void GuiDebug::clear_collider_visualisers(GameEngine& engine)
{
	for (const auto& [_, visual_id] : collider_visualiser_ids)
	{
		engine.delete_object(visual_id);
	}

	collider_visualiser_ids.clear();
}
