#include "gui_mesh_editor.hpp"

#include "entity_component_system/ecs.hpp"
#include "game_engine.hpp"
#include "interface/gizmo.hpp"
#include "objects/objects.hpp"

#include <imgui.h>
#include <fmt/core.h>

#include <exception>
#include <stdexcept>
#include <utility>


namespace
{
bool draw_visibility_button(const bool visible)
{
	const ImVec2 size(26.0f, 20.0f);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const bool clicked = ImGui::InvisibleButton("##visibility", size);
	const bool hovered = ImGui::IsItemHovered();
	auto* draw_list = ImGui::GetWindowDrawList();
	const auto background = ImGui::GetColorU32(
		hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	const auto foreground = ImGui::GetColorU32(ImGuiCol_Text);
	draw_list->AddRectFilled(
		origin, ImVec2(origin.x + size.x, origin.y + size.y), background, 3.0f);

	const ImVec2 eye[] = {
		{ origin.x + 5.0f, origin.y + 10.0f },
		{ origin.x + 13.0f, origin.y + 5.0f },
		{ origin.x + 21.0f, origin.y + 10.0f },
		{ origin.x + 13.0f, origin.y + 15.0f },
	};
	draw_list->AddPolyline(eye, 4, foreground, ImDrawFlags_Closed, 1.5f);
	draw_list->AddCircleFilled({ origin.x + 13.0f, origin.y + 10.0f }, 2.5f, foreground);
	if (!visible)
		draw_list->AddLine(
			{ origin.x + 4.0f, origin.y + 16.0f },
			{ origin.x + 22.0f, origin.y + 4.0f }, foreground, 2.0f);
	if (hovered)
		ImGui::SetTooltip("%s renderable", visible ? "Hide" : "Show");
	return clicked;
}
}

GuiMeshEditor::GuiMeshEditor() :
	EngineUiWindow({ "mesh_editor", "Mesh Editor", GuiPanelDock::RIGHT, false })
{
}

void GuiMeshEditor::queue(Request request)
{
	if (!pending_request)
		pending_request = std::move(request);
}

void GuiMeshEditor::process(GameEngine& engine)
{
	if (edit_requested)
	{
		edit_requested = false;
		if (const auto* selected = engine.get_gizmo().get_selected_object())
		{
			target_object = selected->get_id();
			engine.get_gizmo().deselect();
			error.reset();
		}
	}

	if (pending_request)
	{
		const auto request = *pending_request;
		pending_request.reset();
		try
		{
			auto& ecs = engine.get_ecs();
			if (!ecs.has_object(request.object_id))
				throw std::out_of_range("Edited object no longer exists");
			if (request.action != Action::ADD)
			{
				if (!request.target_id || !ecs.has_renderable(*request.target_id)
					|| ecs.get_renderable(*request.target_id).object_id != request.object_id)
					throw std::out_of_range("Target renderable no longer belongs to the edited object");
			}
			if ((request.action == Action::ADD || request.action == Action::REPLACE)
				&& (!request.source_id || !ecs.has_renderable(*request.source_id)))
				throw std::out_of_range("Source renderable does not exist");

			switch (request.action)
			{
			case Action::ADD:
				ecs.clone_renderable(*request.source_id, request.object_id);
				break;
			case Action::REMOVE:
				ecs.remove_renderable(*request.target_id);
				break;
			case Action::REPLACE:
				ecs.replace_renderable_with_clone(*request.target_id, *request.source_id);
				break;
			case Action::SET_VISIBILITY:
				ecs.set_renderable_visibility(*request.target_id, request.visible);
				break;
			}
			error.reset();
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
		}
	}

	refresh(engine);
}

void GuiMeshEditor::refresh(GameEngine& engine)
{
	object_selected = engine.get_gizmo().get_selected_object() != nullptr;
	auto& ecs = engine.get_ecs();
	source_id_valid = ecs.has_renderable(RenderableID(source_id_value));
	rows.clear();
	if (!target_object)
	{
		target_status = "Select an object with the gizmo, then click Edit Object";
		return;
	}
	const auto* object = engine.get_object(*target_object);
	if (!object || !ecs.has_object(*target_object))
	{
		target_object.reset();
		target_status = "Edited object no longer exists";
		return;
	}
	target_status = object->get_name().empty()
		? fmt::format("Editing object {}", target_object->get_underlying())
		: fmt::format("Editing {} (Object ID {})", object->get_name(), target_object->get_underlying());
	for (const RenderableID id : ecs.get_renderable_ids(*target_object))
	{
		const auto& attachment = ecs.get_renderable(id);
		rows.push_back({ id, attachment.renderable.name, attachment.visible });
	}
}

void GuiMeshEditor::draw()
{
	if (begin())
	{
		ImGui::BeginDisabled(!object_selected);
		if (ImGui::Button("Edit Object"))
			edit_requested = true;
		ImGui::EndDisabled();
		ImGui::TextWrapped("%s", target_status.c_str());

		if (target_object)
		{
			ImGui::InputScalar("RenderableID", ImGuiDataType_U64, &source_id_value);
			ImGui::BeginDisabled(!source_id_valid);
			if (ImGui::Button("Add"))
				queue({ Action::ADD, *target_object, {}, RenderableID(source_id_value) });
			ImGui::EndDisabled();
			if (!source_id_valid)
				ImGui::TextDisabled("Enter an existing RenderableID to add or replace");

			if (ImGui::BeginTable("renderables", 5,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Replace", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableHeadersRow();
				for (const auto& row : rows)
				{
					const auto row_id = std::to_string(row.id.get_underlying());
					ImGui::PushID(row_id.c_str());
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (draw_visibility_button(row.visible))
						queue({ Action::SET_VISIBILITY, *target_object, row.id, {}, !row.visible });
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(row.name.empty() ? "(unnamed)" : row.name.c_str());
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%llu", static_cast<unsigned long long>(row.id.get_underlying()));
					ImGui::TableSetColumnIndex(3);
					ImGui::BeginDisabled(!source_id_valid);
					if (ImGui::SmallButton("Replace"))
						queue({ Action::REPLACE, *target_object, row.id,
							RenderableID(source_id_value) });
					ImGui::EndDisabled();
					ImGui::TableSetColumnIndex(4);
					if (ImGui::SmallButton("Remove"))
						queue({ Action::REMOVE, *target_object, row.id, {} });
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
		}

		if (error)
			ImGui::TextColored(
				ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", error->c_str());
	}
	end();
}
