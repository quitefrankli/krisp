#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>


enum class PresentationPreference
{
	Default,
	Uncapped,
	Paced
};

class RuntimeObservation;
class FrameCaptureSession;

struct RuntimeOptions
{
	std::optional<double> simulation_hz;
	std::optional<double> render_hz;
	bool fixed_simulation_timestep = false;
	std::optional<std::uint32_t> width;
	std::optional<std::uint32_t> height;
	std::optional<std::uint64_t> vertex_buffer_bytes;
	std::optional<std::uint64_t> index_buffer_bytes;
	bool interactive = true;
	bool ui = true;
	bool performance_overlay = false;
	PresentationPreference presentation = PresentationPreference::Default;
	std::shared_ptr<RuntimeObservation> observation;
	std::shared_ptr<FrameCaptureSession> frame_capture;
};

struct RuntimeGraphicsInfo
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t requested_width = 0;
	std::uint32_t requested_height = 0;
	std::uint32_t vendor_id = 0;
	std::uint32_t device_id = 0;
	std::uint32_t driver_version = 0;
	std::uint32_t api_version = 0;
	std::uint32_t timestamp_valid_bits = 0;
	std::uint32_t swapchain_images = 0;
	std::uint64_t vertex_buffer_bytes = 0;
	std::uint64_t index_buffer_bytes = 0;
	double timestamp_period_ns = 0.0;
	bool validation_enabled = false;
	std::string device_name;
	std::string presentation_mode;
	std::string requested_presentation_mode;
	std::string configuration_note;
	std::string gpu_unavailable_reason;
};

void validate_runtime_options(const RuntimeOptions &options);

enum class RuntimeSampleKind
{
	Simulation,
	Graphics,
	UniformUpload,
	CommandRecording,
	ResourceUpload,
	SnapshotConstruction,
	FrameFenceWait,
	ImageFenceWait,
	Acquire,
	Submit,
	Present,
	GameSleep,
	GraphicsSleep,
	GameDeadline,
	GraphicsDeadline,
	Gpu,
	GpuShadow,
	GpuMain,
	GpuUi
};

constexpr bool is_gpu_sample(RuntimeSampleKind kind) noexcept
{
	return kind == RuntimeSampleKind::Gpu || kind == RuntimeSampleKind::GpuShadow
		|| kind == RuntimeSampleKind::GpuMain || kind == RuntimeSampleKind::GpuUi;
}

struct RuntimeSample
{
	RuntimeSampleKind kind = RuntimeSampleKind::Simulation;
	std::uint64_t begin_ns = 0;
	std::uint64_t end_ns = 0;
	// GPU samples carry their GPU-clock duration here; CPU samples use end_ns - begin_ns.
	std::uint64_t duration_ns = 0;
	std::uint64_t submission_id = 0;
	std::uint64_t frame_number = 0;
	std::uint64_t tick_index = 0;
	std::uint64_t publication_ns = 0;
	std::uint64_t missed_slots = 0;
	bool success = true;
};

std::uint64_t runtime_now_ns() noexcept;

// Converts a timestamp delta using the queue's counter width and device period.
// Returns empty when the counter width or period cannot produce a valid result.
std::optional<std::uint64_t> timestamp_duration_ns(
	std::uint64_t begin,
	std::uint64_t end,
	std::uint32_t valid_bits,
	double timestamp_period_ns) noexcept;

// Bounded single-producer/single-consumer queues for the independent game and
// graphics threads. Constructing the observer allocates fixed storage; recording
// never allocates or waits for the consumer. Call start/stop around a collection.
class RuntimeObservation
{
public:
	explicit RuntimeObservation(std::size_t capacity_per_thread = 4096);
	RuntimeObservation(const RuntimeObservation &) = delete;
	RuntimeObservation &operator=(const RuntimeObservation &) = delete;

	void start() noexcept;
	void stop() noexcept;
	bool active() const noexcept;
	void record_game(const RuntimeSample &sample) noexcept;
	void record_graphics(const RuntimeSample &sample) noexcept;
	std::vector<RuntimeSample> drain();
	std::uint64_t lost_records() const noexcept;

private:
	struct Buffer
	{
		explicit Buffer(std::size_t capacity);
		bool push(const RuntimeSample &sample) noexcept;
		void drain_into(std::vector<RuntimeSample> &output);

		std::vector<RuntimeSample> samples;
		std::atomic<std::uint64_t> read_index{0};
		std::atomic<std::uint64_t> write_index{0};
	};

	void record(Buffer &buffer, const RuntimeSample &sample) noexcept;

	Buffer game;
	Buffer graphics;
	std::atomic<bool> collecting{false};
	std::atomic<std::uint64_t> lost{0};
};

struct RuntimePacingResult
{
	std::uint64_t begin_ns = 0;
	std::uint64_t end_ns = 0;
	std::uint64_t deadline_ns = 0;
	std::uint64_t missed_slots = 0;
};

// Absolute-deadline pacer. advance() is the deterministic, clock-injected path;
// wait() uses steady_clock and sleeps until an absolute deadline. The rate is
// represented to microhertz precision, keeping integer-Hz schedules (including
// 60 Hz) free of cumulative period-rounding drift.
class RuntimePacer
{
public:
	explicit RuntimePacer(double rate_hz);
	void reset(std::uint64_t epoch_ns) noexcept;
	RuntimePacingResult advance(std::uint64_t now_ns) noexcept;
	RuntimePacingResult wait();
	std::uint64_t next_deadline_ns() const noexcept;

private:
	std::uint64_t deadline_for_slot(std::uint64_t slot) const noexcept;
	std::uint64_t rate_microhz;
	std::uint64_t epoch_ns = 0;
	std::uint64_t next_slot = 1;
};
