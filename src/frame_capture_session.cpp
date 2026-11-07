#include "frame_capture_session.hpp"

#include <stdexcept>
#include <utility>
#include <limits>


FrameCaptureSession::RequestId FrameCaptureSession::request_next(const std::chrono::milliseconds timeout)
{
	if (timeout <= std::chrono::milliseconds::zero())
		throw std::invalid_argument("FrameCaptureSession: request timeout must be positive");
	const std::lock_guard lock(mutex);
	if (pending_target || result)
		throw std::logic_error("FrameCaptureSession: a capture request is already pending");
	const auto request_id = next_request_id++;
	if (suspended)
	{
		result = FrameCaptureResult{
			.request_id = request_id,
			.status = FrameCaptureStatus::Failed,
			.error = "capture unavailable while rendering is suspended",
		};
		changed.notify_all();
		return request_id;
	}
	pending_target = Target{
		.request_id = request_id,
		.deadline = Clock::now() + timeout,
	};
	changed.notify_all();
	return request_id;
}

std::optional<FrameCaptureSession::Target> FrameCaptureSession::bind_and_wait(const uint64_t frame_number)
{
	std::unique_lock lock(mutex);
	if (!pending_target)
		return std::nullopt;
	if (pending_target->bound)
		throw std::logic_error("FrameCaptureSession: request was already bound to a frame");
	pending_target->frame_number = frame_number;
	pending_target->bound = true;
	const auto target = *pending_target;
	changed.notify_all();
	if (!changed.wait_until(lock, target.deadline,
	                        [this, &target] { return !pending_target || *pending_target != target; }))
	{
		result = FrameCaptureResult{
			.request_id = target.request_id,
			.frame_number = target.frame_number,
			.status = FrameCaptureStatus::Failed,
			.error = "capture completion deadline expired",
		};
		pending_target.reset();
		changed.notify_all();
	}
	return target;
}

std::optional<FrameCaptureSession::Target> FrameCaptureSession::get_target() const
{
	const std::lock_guard lock(mutex);
	if (!pending_target || !pending_target->bound)
		return std::nullopt;
	return pending_target;
}

bool FrameCaptureSession::complete(const Target &target, FrameCaptureResult completed)
{
	const std::lock_guard lock(mutex);
	if (!pending_target || *pending_target != target || completed.request_id != target.request_id ||
	    completed.frame_number != target.frame_number || completed.status != FrameCaptureStatus::Success)
		return false;
	const auto area = static_cast<uint64_t>(completed.width) * static_cast<uint64_t>(completed.height);
	const bool size_overflow = area > std::numeric_limits<uint64_t>::max() / 4;
	const auto expected_bytes = size_overflow ? 0 : area * 4;
	if (completed.width == 0 || completed.height == 0 || size_overflow ||
	    expected_bytes > std::numeric_limits<size_t>::max() || completed.pixels.size() != expected_bytes)
	{
		result = FrameCaptureResult{
			.request_id = target.request_id,
			.frame_number = target.frame_number,
			.status = FrameCaptureStatus::Failed,
			.error = "capture result has invalid dimensions or pixel payload",
		};
		pending_target.reset();
		changed.notify_all();
		return false;
	}
	result = std::move(completed);
	pending_target.reset();
	changed.notify_all();
	return true;
}

bool FrameCaptureSession::fail(const Target &target, std::string error)
{
	FrameCaptureResult failed{
		.request_id = target.request_id,
		.frame_number = target.frame_number,
		.status = FrameCaptureStatus::Failed,
		.error = std::move(error),
	};
	const std::lock_guard lock(mutex);
	if (!pending_target || *pending_target != target)
		return false;
	result = std::move(failed);
	pending_target.reset();
	changed.notify_all();
	return true;
}

bool FrameCaptureSession::cancel(const RequestId request_id)
{
	const std::lock_guard lock(mutex);
	if (!pending_target || pending_target->request_id != request_id)
		return false;
	result = FrameCaptureResult{
		.request_id = request_id,
		.frame_number = pending_target->frame_number,
		.status = FrameCaptureStatus::Cancelled,
		.error = "capture request cancelled",
	};
	pending_target.reset();
	changed.notify_all();
	return true;
}

void FrameCaptureSession::cancel_all()
{
	const std::lock_guard lock(mutex);
	if (pending_target)
	{
		result = FrameCaptureResult{
			.request_id = pending_target->request_id,
			.frame_number = pending_target->frame_number,
			.status = FrameCaptureStatus::Cancelled,
			.error = "capture session cancelled",
		};
		pending_target.reset();
	}
	changed.notify_all();
}

void FrameCaptureSession::fail_all(std::string error)
{
	const std::lock_guard lock(mutex);
	if (pending_target)
	{
		result = FrameCaptureResult{
			.request_id = pending_target->request_id,
			.frame_number = pending_target->frame_number,
			.status = FrameCaptureStatus::Failed,
			.error = std::move(error),
		};
		pending_target.reset();
	}
	changed.notify_all();
}

void FrameCaptureSession::set_suspended(const bool should_suspend)
{
	const std::lock_guard lock(mutex);
	if (suspended == should_suspend)
		return;
	suspended = should_suspend;
	if (suspended && pending_target)
	{
		result = FrameCaptureResult{
			.request_id = pending_target->request_id,
			.frame_number = pending_target->frame_number,
			.status = FrameCaptureStatus::Failed,
			.error = "capture failed because rendering was suspended",
		};
		pending_target.reset();
	}
	changed.notify_all();
}

std::optional<FrameCaptureResult> FrameCaptureSession::wait(const RequestId request_id,
                                                            const std::chrono::milliseconds timeout)
{
	std::unique_lock lock(mutex);
	if (!changed.wait_for(lock, timeout, [this, request_id] { return result && result->request_id == request_id; }))
		return std::nullopt;
	return result;
}

std::optional<FrameCaptureResult> FrameCaptureSession::try_get_result(const RequestId request_id) const
{
	const std::lock_guard lock(mutex);
	if (!result || result->request_id != request_id)
		return std::nullopt;
	return result;
}

std::optional<FrameCaptureResult> FrameCaptureSession::take_result(const RequestId request_id)
{
	const std::lock_guard lock(mutex);
	if (result && result->request_id == request_id)
	{
		auto completed = std::move(result);
		result.reset();
		return completed;
	}
	return std::nullopt;
}
