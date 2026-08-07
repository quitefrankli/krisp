#include "recording_session.hpp"

#include <stdexcept>


void RecordingSession::start(const uint32_t requested_frames_per_second)
{
	if (requested_frames_per_second == 0)
		throw std::invalid_argument("RecordingSession: frame rate must be positive");

	const std::lock_guard lock(mutex);
	if (active)
		throw std::logic_error("RecordingSession: recording is already active");

	active = true;
	++session_number;
	next_sequence = 0;
	frames_per_second = requested_frames_per_second;
	next_frame_not_before = Clock::now();
	capture_target.reset();
	completed_sequence.reset();
	state_changed.notify_all();
}

void RecordingSession::stop()
{
	const std::lock_guard lock(mutex);
	active = false;
	capture_target.reset();
	state_changed.notify_all();
}

bool RecordingSession::is_active() const
{
	const std::lock_guard lock(mutex);
	return active;
}

uint32_t RecordingSession::get_frames_per_second() const
{
	const std::lock_guard lock(mutex);
	return frames_per_second;
}

std::optional<RecordingSession::GameFrame> RecordingSession::begin_game_frame()
{
	std::unique_lock lock(mutex);
	if (!active)
		return std::nullopt;

	const uint64_t expected_session = session_number;
	state_changed.wait_until(lock, next_frame_not_before, [this, expected_session] {
		return !active || session_number != expected_session;
	});
	if (!active || session_number != expected_session)
		return std::nullopt;

	const auto now = Clock::now();
	const auto frame_period = std::chrono::duration_cast<Clock::duration>(
		std::chrono::duration<double>(1.0 / static_cast<double>(frames_per_second)));
	next_frame_not_before = now + frame_period;

	return GameFrame{
		.session = session_number,
		.sequence = next_sequence++,
		.delta_seconds = 1.0f / static_cast<float>(frames_per_second),
	};
}

bool RecordingSession::await_capture(
	const GameFrame& frame, const uint64_t render_frame_number)
{
	std::unique_lock lock(mutex);
	if (!active || frame.session != session_number)
		return false;
	if (capture_target)
		throw std::logic_error("RecordingSession: a capture is already pending");

	capture_target = CaptureTarget{
		.session = frame.session,
		.sequence = frame.sequence,
		.render_frame_number = render_frame_number,
	};
	state_changed.notify_all();

	state_changed.wait(lock, [this, &frame] {
		return !active || session_number != frame.session
			|| (completed_sequence && *completed_sequence == frame.sequence);
	});
	return active && session_number == frame.session
		&& completed_sequence && *completed_sequence == frame.sequence;
}

std::optional<RecordingSession::CaptureTarget> RecordingSession::get_capture_target() const
{
	const std::lock_guard lock(mutex);
	return active ? capture_target : std::nullopt;
}

bool RecordingSession::complete_capture(const CaptureTarget& target)
{
	const std::lock_guard lock(mutex);
	if (!active || !capture_target || *capture_target != target)
		return false;

	completed_sequence = target.sequence;
	capture_target.reset();
	state_changed.notify_all();
	return true;
}
