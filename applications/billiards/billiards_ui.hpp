#pragma once

#include <gui/application_ui_manager.hpp>

#include <mutex>


class BilliardsUiState
{
public:
	void publish_power(float value);
	float power() const;
	void request_reset();
	bool take_reset_request();

private:
	mutable std::mutex mutex;
	float preview_power = 0.0f;
	bool reset_requested = false;
};

class BilliardsControlsWindow : public ApplicationUiWindow
{
public:
	explicit BilliardsControlsWindow(BilliardsUiState& state);

private:
	void draw_contents() override;

	BilliardsUiState& state;
};
