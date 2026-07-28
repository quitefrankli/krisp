#pragma once

#include <gui/application_ui_manager.hpp>

#include <array>
#include <mutex>

class KrispUiState
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
	mutable std::mutex mutex;
	Snapshot current;
	bool main_hand_toggle_requested = false;
};

class KrispEquipmentWindow : public ApplicationUiWindow
{
public:
	explicit KrispEquipmentWindow(KrispUiState& state);

private:
	void draw_contents() override;
	KrispUiState& state;
};

class KrispStatusOverlay : public ApplicationUiOverlay
{
public:
	explicit KrispStatusOverlay(KrispUiState& state);

private:
	void draw_contents() override;
	KrispUiState& state;
};
