#include "gui_material_editor.hpp"

#include "gui_window_helpers.hpp"

#include "entity_component_system/material_system.hpp"
#include "game_engine.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"
#include "renderable/material_factory.hpp"
#include "utility.hpp"

#include <imgui.h>
#include <fmt/core.h>

#include <algorithm>

using GuiWindowDetail::draw_resource_load_error;
using GuiWindowDetail::report_resource_load_error;

GuiMaterialEditor::GuiMaterialEditor() :
	EngineUiWindow({ "material_editor", "Material Editor", GuiPanelDock::RIGHT, false })
{
	refresh_textures();
}

void GuiMaterialEditor::refresh_textures()
{
	texture_paths = Utility::get_all_textures();
	std::ranges::sort(texture_paths);
	texture_names = texture_paths;
}

void GuiMaterialEditor::process(GameEngine& engine)
{
	if (should_refresh_textures)
	{
		should_refresh_textures = false;
		refresh_textures();
	}

	if (pending_change)
	{
		auto change = std::move(*pending_change);
		pending_change.reset();
		try
		{
			RenderableID replacement_id = change.renderable_id;
			if (change.matte)
				replacement_id = engine.set_renderable_specular_matte(change.renderable_id);
			else
				replacement_id = engine.replace_renderable_texture(
					change.renderable_id, change.semantic, std::move(change.path));
			std::ranges::replace(renderable_ids, change.renderable_id, replacement_id);
			load_error.reset();
		}
		catch (const ResourceLoadError& error)
		{
			load_error = report_resource_load_error("Material Editor failed to load texture", error);
		}
		catch (const std::runtime_error& error)
		{
			load_error = report_resource_load_error(
				"Material Editor failed to update material", ResourceLoadError(error.what()));
		}
	}
	const auto previous_target = target_object;
	target_object.reset();
	renderable_labels.clear();
	renderable_ids.clear();
	compatible = false;
	diffuse_label = "(none)";
	normal_label = "(none)";
	specular_label = "(glossy)";
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
		renderable_labels.push_back(fmt::format(
			"Mesh {} (ID {})", index + 1, renderable.get_mesh_id().get_underlying()));
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
	compatible = renderable.pipeline_render_type == ERenderType::STANDARD
		|| renderable.pipeline_render_type == ERenderType::SKINNED;
	if (!compatible)
	{
		target_status += " — selected mesh does not support textures";
		return;
	}
	if (renderable.material_owners.empty())
	{
		compatible = false;
		target_status += " — selected mesh has no material";
		return;
	}

	const auto material_label = [](const MaterialID id)
	{
		if (!MaterialSystem::contains(id))
			return std::string("(missing material)");
		const auto* texture = dynamic_cast<const TextureMaterial*>(&MaterialSystem::get(id));
		if (!texture || texture->source.empty())
			return fmt::format("Material {}", id.get_underlying());
		if (texture->source == "(matte)" || texture->source == "(none)")
			return texture->source;
		const std::filesystem::path source(texture->source);
		return source.filename().empty() ? texture->source : source.filename().string();
	};
	const TexturedMatGroup materials(renderable.material_owners);
	diffuse_label = material_label(materials.base_color_mat);
	if (materials.normal_mat)
		normal_label = material_label(*materials.normal_mat);
	if (materials.specular_mat)
		specular_label = material_label(*materials.specular_mat);
}

void GuiMaterialEditor::draw_texture_section(
	const char* title,
	const ETextureSemantic semantic,
	const std::string& current_label,
	bool& dropdown_was_open)
{
	if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
		return;
	ImGui::PushID(title);
	const bool dropdown_open = ImGui::BeginCombo("##texture", current_label.c_str());
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", current_label.c_str());
	if (dropdown_open && !dropdown_was_open)
		should_refresh_textures = true;
	dropdown_was_open = dropdown_open;
	if (dropdown_open)
	{
		const char* glossy_label = semantic == ETextureSemantic::SPECULAR ? "(glossy)" : "(none)";
		if (ImGui::Selectable(glossy_label, current_label == glossy_label))
			pending_change = TextureChange{
				renderable_ids.at(selected_renderable.value), semantic, std::nullopt };
		if (semantic == ETextureSemantic::SPECULAR
			&& ImGui::Selectable("(matte)", current_label == "(matte)"))
			pending_change = TextureChange{
				renderable_ids.at(selected_renderable.value), semantic, std::nullopt, true };
		for (size_t index = 0; index < texture_paths.size(); ++index)
		{
			if (ImGui::Selectable(texture_names[index].c_str(), current_label == texture_names[index]))
				pending_change = TextureChange{
					renderable_ids.at(selected_renderable.value), semantic, texture_paths[index] };
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", texture_names[index].c_str());
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
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
					renderable_labels[index].c_str(), selected_renderable.value == static_cast<int>(index)))
					selected_renderable = static_cast<int>(index);
			}
			ImGui::EndCombo();
		}

		ImGui::BeginDisabled(!compatible || !target_object);
		draw_texture_section(
			"Diffuse Texture", ETextureSemantic::BASE_COLOR, diffuse_label, diffuse_dropdown_open);
		draw_texture_section(
			"Normal Map", ETextureSemantic::NORMAL, normal_label, normal_dropdown_open);
		draw_texture_section(
			"Specular Map",
			ETextureSemantic::SPECULAR,
			specular_label,
			specular_dropdown_open);
		ImGui::EndDisabled();
		draw_resource_load_error(load_error);
	}
	end();
}
