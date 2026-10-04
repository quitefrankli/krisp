#include "gui_material_editor.hpp"

#include "entity_component_system/material_system.hpp"
#include "game_engine.hpp"
#include "gui_window_helpers.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"
#include "renderable/material.hpp"
#include "renderable/material_group.hpp"
#include "utility.hpp"

#include <fmt/core.h>
#include <imgui.h>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace
{
std::string get_overlay_disabled_reason(
	const bool texture_compatible,
	const bool has_base_color_texture,
	const size_t overlay_count,
	const TextureCompositionOverlay& overlay)
{
	if (!texture_compatible)
		return "Selected mesh does not support texture sampling";
	if (!has_base_color_texture)
		return "A base-color texture is required";
	if (overlay_count >= CSTS::MAX_TEXTURE_COMPOSITION_LAYERS - 1)
		return "The overlay layer limit has been reached";
	if (overlay.texture_filename.empty())
		return "Choose an overlay texture";
	if (!std::isfinite(overlay.centre.x) || !std::isfinite(overlay.centre.y)
		|| !std::isfinite(overlay.scale.x) || !std::isfinite(overlay.scale.y)
		|| overlay.scale.x <= 0.0f || overlay.scale.y <= 0.0f
		|| !std::isfinite(overlay.rotation_radians)
		|| !std::isfinite(overlay.tint.r) || !std::isfinite(overlay.tint.g)
		|| !std::isfinite(overlay.tint.b) || overlay.tint.r < 0.0f
		|| overlay.tint.r > 1.0f || overlay.tint.g < 0.0f || overlay.tint.g > 1.0f
		|| overlay.tint.b < 0.0f || overlay.tint.b > 1.0f
		|| !std::isfinite(overlay.opacity) || overlay.opacity < 0.0f || overlay.opacity > 1.0f)
		return "Overlay parameters must be finite; scale must be positive; tint and opacity must be between 0 and 1";
	return {};
}
}


GuiMaterialEditor::GuiMaterialEditor() :
	EngineUiWindow({ "material_editor", "Material Editor", GuiPanelDock::RIGHT, false })
{
}

void GuiMaterialEditor::reset_overlay_draft()
{
	overlay_draft = TextureCompositionOverlay{};
}

void GuiMaterialEditor::process(GameEngine& engine)
{
	if (should_refresh_textures)
	{
		should_refresh_textures = false;
		texture_paths = Utility::get_all_textures();
		std::ranges::sort(texture_paths);
		texture_tree = GuiWindowDetail::build_resource_tree(texture_paths);
	}

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
			if (pending_overlay && pending_overlay->renderable_id == change.renderable_id)
				pending_overlay->renderable_id = replacement_id;
			loaded_renderable.reset();
			load_error.reset();
		}
		catch (const std::exception& error)
		{
			load_error = error.what();
		}
	}
	if (pending_overlay)
	{
		auto change = std::move(*pending_overlay);
		pending_overlay.reset();
		try
		{
			const RenderableID replacement_id = engine.composite_renderable_base_color(
				change.renderable_id, std::vector<TextureCompositionOverlay>{ change.overlay });
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
	texture_compatible = false;
	base_color_texture_available = false;

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
		composition_overlay_count = 0;
		for (size_t index = 0; index < slots.size(); ++index)
		{
			loaded_texture_names[index].clear();
			if (slots[index])
			{
				const auto& slot_material = materials.texture_owner(*slots[index])->get();
				const auto* texture = dynamic_cast<const TextureMaterial*>(&slot_material);
				if (texture)
					loaded_texture_names[index] = texture->source;
				if (index == 0)
				{
					if (const auto* composition =
						dynamic_cast<const CompositedTextureMaterial*>(&slot_material))
					{
						composition_overlay_count = composition->layers.size() - 1;
						const auto& bottom = static_cast<const TextureMaterial&>(
							composition->layers.front().source->get());
						loaded_texture_names[index] = fmt::format("{} + {} overlay{}",
							bottom.source, composition_overlay_count, composition_overlay_count == 1 ? "" : "s");
					}
				}
			}
			texture_names[index] = loaded_texture_names[index];
		}
		reset_overlay_draft();
		loaded_renderable = renderable_ids[selected_renderable.value];
	}
	compatible = true;
	texture_compatible = renderable.pipeline_render_type == ERenderType::STANDARD
		|| renderable.pipeline_render_type == ERenderType::SKINNED;
	base_color_texture_available = material->textures.base_color.has_value();
}

void GuiMaterialEditor::draw_texture_picker(std::string& name, bool& was_open)
{
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	const bool open = ImGui::BeginCombo(
		"##texture", name.empty() ? "(none)" : name.c_str(), ImGuiComboFlags_HeightLargest);
	if (open && !was_open)
		should_refresh_textures = true;
	was_open = open;
	if (!name.empty() && ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", name.c_str());
	if (open)
	{
		const auto current = std::ranges::find(texture_paths, name);
		const std::optional<size_t> selected = current == texture_paths.end()
			? std::nullopt
			: std::optional<size_t>(std::distance(texture_paths.begin(), current));
		if (ImGui::Selectable("(none)", name.empty()))
			name.clear();
		if (const auto texture = GuiWindowDetail::draw_resource_tree(texture_tree, texture_paths, selected))
			name = texture_paths[*texture];
		ImGui::EndCombo();
	}
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
					selected_renderable.value == static_cast<int>(index))
					&& selected_renderable.value != static_cast<int>(index))
				{
					selected_renderable = static_cast<int>(index);
					reset_overlay_draft();
				}
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
			ImGui::PushID(static_cast<int>(index));
			ImGui::TextWrapped("%s", labels[index]);
			draw_texture_picker(texture_names[index], texture_dropdown_open[index]);
			if (ImGui::Button("Clear"))
				texture_names[index].clear();
			ImGui::PopID();
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
				const std::string& value = texture_names[index];
				change.textures[index].changed = value != loaded_texture_names[index];
				if (!value.empty())
					change.textures[index].replacement = value;
			}
			pending_change = std::move(change);
		}
		ImGui::Separator();
		ImGui::TextUnformatted("Base-color overlays");
		ImGui::Text("Applied overlays: %zu", composition_overlay_count);
		ImGui::TextWrapped("Replacing or clearing the base-color texture removes its overlays.");
		ImGui::BeginDisabled(!texture_compatible);
		ImGui::PushID("overlay");
		ImGui::TextUnformatted("Overlay texture");
		draw_texture_picker(overlay_draft.texture_filename, overlay_dropdown_open);
		ImGui::InputFloat2("Centre (UV)", &overlay_draft.centre.x);
		ImGui::InputFloat2("Scale (UV)", &overlay_draft.scale.x);
		float rotation_degrees = glm::degrees(overlay_draft.rotation_radians);
		if (ImGui::InputFloat("Rotation (degrees)", &rotation_degrees))
			overlay_draft.rotation_radians = glm::radians(rotation_degrees);
		ImGui::ColorEdit3("Tint", &overlay_draft.tint.x);
		ImGui::SliderFloat("Opacity", &overlay_draft.opacity, 0.0f, 1.0f);
		ImGui::EndDisabled();
		const auto overlay_disabled_reason = get_overlay_disabled_reason(
			texture_compatible, base_color_texture_available, composition_overlay_count, overlay_draft);
		ImGui::BeginDisabled(!overlay_disabled_reason.empty());
		if (ImGui::Button("Apply Overlay"))
		{
			pending_overlay = OverlayChange{
				.renderable_id = renderable_ids.at(selected_renderable.value),
				.overlay = overlay_draft,
			};
		}
		ImGui::EndDisabled();
		if (!overlay_disabled_reason.empty())
			ImGui::TextWrapped("%s", overlay_disabled_reason.c_str());
		ImGui::PopID();
		ImGui::EndDisabled();
		GuiWindowDetail::draw_resource_load_error(load_error);
	}
	end();
}
