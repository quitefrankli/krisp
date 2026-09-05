#pragma once

#include <gui/application_ui_manager.hpp>

#include <atomic>
#include <cstdint>

class RpgUiState
{
public:
	struct Snapshot
	{
		bool moving = false;
		bool main_hand_equipped = false;
	};

	void publish(bool moving, bool main_hand_equipped);
	Snapshot snapshot() const;
	void request_main_hand_toggle();
	bool take_main_hand_toggle_request();

private:
	static constexpr std::uint8_t MOVING = 1U << 0U;
	static constexpr std::uint8_t MAIN_HAND_EQUIPPED = 1U << 1U;
	std::atomic<std::uint8_t> current = 0;
	std::atomic<bool> main_hand_toggle_requested = false;
};

class RpgEquipmentWindow : public ApplicationUiWindow
{
public:
	explicit RpgEquipmentWindow(RpgUiState& state);

private:
	void draw_contents() override;
	RpgUiState& state;
};

class RpgStatusOverlay : public ApplicationUiOverlay
{
public:
	explicit RpgStatusOverlay(RpgUiState& state);

private:
	void draw_contents() override;
	RpgUiState& state;
};
