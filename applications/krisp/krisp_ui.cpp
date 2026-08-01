#include "krisp_ui.hpp"

#include <imgui.h>

void KrispUiState::publish(const bool moving, const bool main_hand_equipped)
{
	const auto state = static_cast<std::uint8_t>(
		(moving ? MOVING : 0U) | (main_hand_equipped ? MAIN_HAND_EQUIPPED : 0U));
	current.store(state, std::memory_order_release);
}

KrispUiState::Snapshot KrispUiState::snapshot() const
{
	const auto state = current.load(std::memory_order_acquire);
	return {
		.moving = (state & MOVING) != 0,
		.main_hand_equipped = (state & MAIN_HAND_EQUIPPED) != 0,
	};
}

void KrispUiState::request_main_hand_toggle()
{
	main_hand_toggle_requested.store(true, std::memory_order_release);
}

bool KrispUiState::take_main_hand_toggle_request()
{
	return main_hand_toggle_requested.exchange(false, std::memory_order_acq_rel);
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
