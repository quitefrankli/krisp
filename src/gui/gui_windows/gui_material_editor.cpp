#include "gui_material_editor.hpp"

#include "entity_component_system/material_system.hpp"
#include "game_engine.hpp"
#include "gui_window_helpers.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"
#include "renderable/material.hpp"
#include "renderable/material_group.hpp"

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
			const auto texture_edit = [](const MaterialChange::TextureChange& texture)
			{
				if (!texture.changed)
					return PbrTextureEdit{};
				if (!texture.replacement)
					return PbrTextureEdit{ .action = PbrTextureEdit::Action::Clear };
				return PbrTextureEdit{
					.action = PbrTextureEdit::Action::Replace,
					.source = *texture.replacement,
				};
			};
			const RenderableID replacement_id = engine.set_renderable_pbr_material(
				change.renderable_id,
				PbrMaterialEdit{
					.base_color_factor = change.base_color_factor,
					.metallic_factor = change.metallic_factor,
					.roughness_factor = change.roughness_factor,
					.normal_scale = change.normal_scale,
					.alpha_mode = change.alpha_mode,
					.alpha_cutoff = change.alpha_cutoff,
					.double_sided = change.double_sided,
					.emissive_factor = change.emissive_factor,
					.base_color_texture = texture_edit(change.textures[0]),
					.metallic_roughness_texture = texture_edit(change.textures[1]),
					.normal_texture = texture_edit(change.textures[2]),
					.emissive_texture = texture_edit(change.textures[3]),
				});
			std::ranges::replace(renderable_ids, change.renderable_id, replacement_id);
			loaded_renderable.reset();
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
	{
		selected_renderable = 0;
		loaded_renderable.reset();
	}
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
	if (renderable.material_owners.empty())
	{
		target_status += " — selected mesh has no material";
		return;
	}
	const auto* material = dynamic_cast<const PbrMaterial*>(&renderable.material_owners.front()->get());
	if (!material)
	{
		target_status += " — selected mesh does not use a PBR material";
		return;
	}

	const PbrMatGroup materials(renderable.material_owners);
	if (loaded_renderable != renderable_ids[selected_renderable.value])
	{
		base_color_factor = material->data.base_color_factor;
		metallic_factor = material->data.metallic_factor;
		roughness_factor = material->data.roughness_factor;
		normal_scale = material->data.normal_scale;
		alpha_mode = material->properties.alpha_mode;
		alpha_cutoff = material->properties.alpha_cutoff;
		double_sided = material->properties.double_sided;
		emissive_factor = material->properties.emissive_factor;
		const std::array slots{
			material->textures.base_color,
			material->textures.metallic_roughness,
			material->textures.normal,
			material->textures.emissive,
		};
		for (size_t index = 0; index < slots.size(); ++index)
		{
			loaded_texture_names[index].clear();
			if (slots[index])
			{
				const auto* texture = dynamic_cast<const TextureMaterial*>(
					&materials.texture_owner(*slots[index])->get());
				if (texture)
					loaded_texture_names[index] = texture->source;
			}
			texture_names[index].fill('\0');
			std::copy_n(
				loaded_texture_names[index].begin(),
				std::min(loaded_texture_names[index].size(), texture_names[index].size() - 1),
				texture_names[index].begin());
		}
		loaded_renderable = renderable_ids[selected_renderable.value];
	}
	compatible = true;
	texture_compatible = renderable.pipeline_render_type == ERenderType::STANDARD
		|| renderable.pipeline_render_type == ERenderType::SKINNED;
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
		ImGui::ColorEdit4("Base color", &base_color_factor.x);
		ImGui::SliderFloat("Metallic", &metallic_factor, 0.0f, 1.0f);
		ImGui::SliderFloat("Roughness", &roughness_factor, 0.0f, 1.0f);
		ImGui::InputFloat("Normal scale", &normal_scale);
		constexpr std::array alpha_mode_labels{ "OPAQUE", "MASK", "BLEND" };
		int selected_alpha_mode = static_cast<int>(alpha_mode);
		if (ImGui::Combo(
			"Alpha mode", &selected_alpha_mode,
			alpha_mode_labels.data(), static_cast<int>(alpha_mode_labels.size())))
			alpha_mode = static_cast<EAlphaMode>(selected_alpha_mode);
		ImGui::BeginDisabled(alpha_mode != EAlphaMode::MASK);
		ImGui::InputFloat("Alpha cutoff", &alpha_cutoff);
		ImGui::EndDisabled();
		ImGui::Checkbox("Double-sided", &double_sided);
		ImGui::ColorEdit3("Emissive factor", &emissive_factor.x);
		ImGui::BeginDisabled(!texture_compatible);
		constexpr std::array labels{
			"Base-color texture", "Metallic-roughness texture", "Normal texture",
			"Emissive texture" };
		for (size_t index = 0; index < texture_names.size(); ++index)
		{
			ImGui::InputText(labels[index], texture_names[index].data(), texture_names[index].size());
			ImGui::SameLine();
			const auto clear_label = fmt::format("Clear##texture{}", index);
			if (ImGui::Button(clear_label.c_str()))
				texture_names[index][0] = '\0';
		}
		ImGui::EndDisabled();
		if (ImGui::Button("Apply"))
		{
			MaterialChange change{
				.renderable_id = renderable_ids.at(selected_renderable.value),
				.base_color_factor = base_color_factor,
				.metallic_factor = metallic_factor,
				.roughness_factor = roughness_factor,
				.normal_scale = normal_scale,
				.alpha_mode = alpha_mode,
				.alpha_cutoff = alpha_cutoff,
				.double_sided = double_sided,
				.emissive_factor = emissive_factor,
			};
			for (size_t index = 0; index < texture_names.size(); ++index)
			{
				const std::string value(texture_names[index].data());
				change.textures[index].changed = value != loaded_texture_names[index];
				if (!value.empty())
					change.textures[index].replacement = value;
			}
			pending_change = std::move(change);
		}
		ImGui::EndDisabled();
		GuiWindowDetail::draw_resource_load_error(load_error);
	}
	end();
}
