#include "krisp_ui.hpp"

#include <imgui.h>

#include <utility>

void KrispUiState::publish(const bool moving, const bool main_hand_equipped)
{
	const std::lock_guard lock(mutex);
	current = { moving, main_hand_equipped };
}

KrispUiState::Snapshot KrispUiState::snapshot() const
{
	const std::lock_guard lock(mutex);
	return current;
}

void KrispUiState::request_main_hand_toggle()
{
	const std::lock_guard lock(mutex);
	main_hand_toggle_requested = true;
}

bool KrispUiState::take_main_hand_toggle_request()
{
	const std::lock_guard lock(mutex);
	return std::exchange(main_hand_toggle_requested, false);
}

KrispEquipmentWindow::KrispEquipmentWindow(KrispUiState& state) :
	ApplicationUiWindow({ "krisp_equipment", "Equipment" }),
	state(state)
{
}

void KrispEquipmentWindow::draw_contents()
{
	const auto current = state.snapshot();
	ImGui::TextUnformatted("Main Hand");
	ImGui::SameLine();
	if (ImGui::Button(current.main_hand_equipped ? "Iron Longsword" : "Empty"))
		state.request_main_hand_toggle();
	ImGui::TextUnformatted("Off Hand: Empty");
	ImGui::TextUnformatted("Head: Empty");
	ImGui::TextUnformatted("Chest: Empty");
}

KrispStatusOverlay::KrispStatusOverlay(KrispUiState& state) :
	ApplicationUiOverlay({ "krisp_status", "Status" }),
	state(state)
{
}

void KrispStatusOverlay::draw_contents()
{
	const auto current = state.snapshot();
	ImGui::Text("%s", current.moving ? "Moving" : "Idle");
	ImGui::Text("Main Hand: %s", current.main_hand_equipped ? "Iron Longsword" : "Empty");
}
