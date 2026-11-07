#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>


enum class FrameCaptureStatus
{
	Success,
	Cancelled,
	Failed,
};

enum class FrameCapturePixelFormat
{
	RGBA8,
};

struct FrameCaptureResult
{
	uint64_t request_id = 0;
	uint64_t frame_number = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	FrameCapturePixelFormat format = FrameCapturePixelFormat::RGBA8;
	uint32_t source_format = 0;
	uint32_t source_color_space = 0;
	std::vector<uint8_t> pixels;
	FrameCaptureStatus status = FrameCaptureStatus::Failed;
	std::string error;
};

// A single-slot rendezvous for an exact presentation-frame capture. Requests
// bind to the next immutable frame publication and hold simulation publication
// until graphics has resolved the request after its submission fence.
class FrameCaptureSession
{
public:
	using RequestId = uint64_t;
	using Clock = std::chrono::steady_clock;

	struct Target
	{
		RequestId request_id = 0;
		uint64_t frame_number = 0;
		Clock::time_point deadline{};
		bool bound = false;
		bool operator==(const Target &) const = default;
	};

	FrameCaptureSession() = default;
	FrameCaptureSession(const FrameCaptureSession &) = delete;
	FrameCaptureSession &operator=(const FrameCaptureSession &) = delete;

	RequestId request_next(std::chrono::milliseconds timeout = std::chrono::seconds(30));
	std::optional<Target> bind_and_wait(uint64_t frame_number);
	std::optional<Target> get_target() const;
	bool complete(const Target &target, FrameCaptureResult result);
	bool fail(const Target &target, std::string error);
	bool cancel(RequestId request_id);
	void cancel_all();
	void fail_all(std::string error);
	void set_suspended(bool suspended);
	std::optional<FrameCaptureResult> wait(RequestId request_id, std::chrono::milliseconds timeout);
	std::optional<FrameCaptureResult> try_get_result(RequestId request_id) const;
	std::optional<FrameCaptureResult> take_result(RequestId request_id);

private:
	mutable std::mutex mutex;
	std::condition_variable changed;
	RequestId next_request_id = 1;
	bool suspended = false;
	std::optional<Target> pending_target;
	std::optional<FrameCaptureResult> result;
};
