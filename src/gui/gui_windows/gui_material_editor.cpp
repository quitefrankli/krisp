#include "gui_material_editor.hpp"

#include "entity_component_system/material_system.hpp"
#include "game_engine.hpp"
#include "gui_window_helpers.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"
#include "renderable/material.hpp"

#include <fmt/core.h>
#include <imgui.h>

#include <algorithm>


GuiMaterialEditor::GuiMaterialEditor() :
	EngineUiWindow({ "material_editor", "Material Editor", GuiPanelDock::RIGHT, false })
{
}

void GuiMaterialEditor::process(GameEngine& engine)
{
	if (pending_change)
	{
		auto change = std::move(*pending_change);
		pending_change.reset();
		try
		{
			const RenderableID replacement_id = engine.set_renderable_pbr_material(
				change.renderable_id,
				change.base_color_factor,
				change.metallic_factor,
				change.roughness_factor);
			std::ranges::replace(renderable_ids, change.renderable_id, replacement_id);
			load_error.reset();
		}
		catch (const std::exception& error)
		{
			load_error = error.what();
		}
	}

	const auto previous_target = target_object;
	target_object.reset();
	renderable_labels.clear();
	renderable_ids.clear();
	compatible = false;

	const Object* object = engine.get_gizmo().get_selected_object();
	if (!object)
	{
		target_status = "Select an object with the gizmo";
		return;
	}

	target_object = object->get_id();
	if (previous_target != target_object)
		selected_renderable = 0;
	target_status = object->get_name().empty()
		? fmt::format("Object {}", object->get_id().get_underlying())
		: object->get_name();
	renderable_ids = engine.get_ecs().get_renderable_ids(object->get_id());
	for (size_t index = 0; index < renderable_ids.size(); ++index)
	{
		const auto& renderable =
			engine.get_ecs().get_renderable(renderable_ids[index]).renderable;
		renderable_labels.push_back(renderable.name.empty()
			? fmt::format("Mesh {} (ID {})", index + 1, renderable.get_mesh_id().get_underlying())
			: fmt::format("{} (ID {})", renderable.name, renderable.get_mesh_id().get_underlying()));
	}
	if (renderable_labels.empty())
	{
		target_status += " has no renderables";
		return;
	}

	selected_renderable.value = std::clamp(
		selected_renderable.value, 0, static_cast<int>(renderable_labels.size()) - 1);
	const auto& renderable = engine.get_ecs()
		.get_renderable(renderable_ids[selected_renderable.value]).renderable;
	if (renderable.material_owners.size() != 1)
	{
		target_status += " — selected mesh does not use one PBR material";
		return;
	}
	const auto* material = dynamic_cast<const PbrMaterial*>(&renderable.material_owners.front()->get());
	if (!material)
	{
		target_status += " — selected mesh does not use a PBR material";
		return;
	}

	base_color_factor = material->data.base_color_factor;
	metallic_factor = material->data.metallic_factor;
	roughness_factor = material->data.roughness_factor;
	compatible = true;
}

void GuiMaterialEditor::draw()
{
	if (begin())
	{
		ImGui::TextWrapped("%s", target_status.c_str());
		if (!renderable_labels.empty() && ImGui::BeginCombo(
			"Mesh", renderable_labels[selected_renderable.value].c_str()))
		{
			for (size_t index = 0; index < renderable_labels.size(); ++index)
			{
				if (ImGui::Selectable(
					renderable_labels[index].c_str(),
					selected_renderable.value == static_cast<int>(index)))
					selected_renderable = static_cast<int>(index);
			}
			ImGui::EndCombo();
		}

		ImGui::BeginDisabled(!compatible || !target_object);
		bool changed = ImGui::ColorEdit4("Base color", &base_color_factor.x);
		changed |= ImGui::SliderFloat("Metallic", &metallic_factor, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Roughness", &roughness_factor, 0.0f, 1.0f);
		if (changed)
		{
			pending_change = MaterialChange{
				.renderable_id = renderable_ids.at(selected_renderable.value),
				.base_color_factor = base_color_factor,
				.metallic_factor = metallic_factor,
				.roughness_factor = roughness_factor,
			};
		}
		ImGui::EndDisabled();
		GuiWindowDetail::draw_resource_load_error(load_error);
	}
	end();
}
