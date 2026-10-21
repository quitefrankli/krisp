#pragma once

#include <runtime_performance.hpp>

namespace BenchmarkMeasurement
{
struct Membership
{
	bool included;
	const char* phase;
	const char* exclusion_reason;
};

inline Membership classify(const RuntimeSample& sample, uint64_t start, uint64_t end,
	bool measured_gpu_submission = false)
{
	if (!start || end <= start) return {false, "startup", "measurement_window_not_started"};
	if (sample.begin_ns < start)
		return {false, sample.end_ns > start ? "boundary" : "warmup", "before_measurement_start"};
	if (sample.begin_ns >= end) return {false, "drain", "after_measurement_end"};
	const bool deadline = sample.kind == RuntimeSampleKind::GameDeadline || sample.kind == RuntimeSampleKind::GraphicsDeadline;
	if (is_gpu_sample(sample.kind))
		return measured_gpu_submission ? Membership{true, "measurement", nullptr}
			: Membership{false, "boundary", "submission_not_measured"};
	if (!deadline && sample.end_ns > end) return {false, "boundary", "overlaps_measurement_end"};
	const bool frame = sample.kind == RuntimeSampleKind::Graphics || sample.kind == RuntimeSampleKind::FrameFenceWait
		|| sample.kind == RuntimeSampleKind::ImageFenceWait || sample.kind == RuntimeSampleKind::Acquire
		|| sample.kind == RuntimeSampleKind::Submit || sample.kind == RuntimeSampleKind::Present
		|| sample.kind == RuntimeSampleKind::UniformUpload || sample.kind == RuntimeSampleKind::CommandRecording;
	if (frame && (sample.publication_ns < start || sample.publication_ns >= end))
		return {false, "boundary", "snapshot_not_measured"};
	return {true, "measurement", nullptr};
}
}
