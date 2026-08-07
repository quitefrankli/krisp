#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>


class RecordingSession
{
public:
	struct GameFrame
	{
		uint64_t session = 0;
		uint64_t sequence = 0;
		float delta_seconds = 0.0f;
	};

	struct CaptureTarget
	{
		uint64_t session = 0;
		uint64_t sequence = 0;
		uint64_t render_frame_number = 0;

		bool operator==(const CaptureTarget&) const = default;
	};

	RecordingSession() = default;
	RecordingSession(const RecordingSession&) = delete;
	RecordingSession& operator=(const RecordingSession&) = delete;

	void start(uint32_t frames_per_second);
	void stop();
	bool is_active() const;
	uint32_t get_frames_per_second() const;

	// Called by the game thread. The first frame is available immediately;
	// subsequent frames are paced from the start of the previous game frame.
	std::optional<GameFrame> begin_game_frame();
	// Announces the immutable frame produced for this ticket, then waits until
	// graphics has copied it or the recording is stopped.
	bool await_capture(const GameFrame& frame, uint64_t render_frame_number);

	// Called by the graphics thread. A target remains claimed until it is
	// completed or the session is stopped.
	std::optional<CaptureTarget> get_capture_target() const;
	bool complete_capture(const CaptureTarget& target);

private:
	using Clock = std::chrono::steady_clock;

	mutable std::mutex mutex;
	std::condition_variable state_changed;
	bool active = false;
	uint64_t session_number = 0;
	uint64_t next_sequence = 0;
	uint32_t frames_per_second = 0;
	Clock::time_point next_frame_not_before{};
	std::optional<CaptureTarget> capture_target;
	std::optional<uint64_t> completed_sequence;
};
