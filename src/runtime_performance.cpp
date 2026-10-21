#include "runtime_performance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>


std::uint64_t runtime_now_ns() noexcept
{
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}

void validate_runtime_options(const RuntimeOptions &options)
{
	if (options.simulation_hz.has_value() &&
		(!std::isfinite(*options.simulation_hz) || *options.simulation_hz <= 0.0))
		throw std::invalid_argument("Simulation rate must be finite and greater than zero");
	if (options.render_hz.has_value() &&
		(!std::isfinite(*options.render_hz) || *options.render_hz < 0.0))
		throw std::invalid_argument("Render rate must be finite and non-negative");
	if (options.fixed_simulation_timestep && !options.simulation_hz.has_value())
		throw std::invalid_argument("A fixed simulation timestep requires a simulation rate");
	constexpr auto max_dimension = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
	if ((options.width.has_value() && (*options.width == 0 || *options.width > max_dimension)) ||
		(options.height.has_value() && (*options.height == 0 || *options.height > max_dimension)))
		throw std::invalid_argument("Runtime dimensions must fit a positive platform window dimension");
	const auto valid_buffer_capacity = [](const std::optional<std::uint64_t> capacity)
	{
		if (!capacity)
			return true;
		if (*capacity == 0 || *capacity % 4 != 0)
			return false;
		if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t))
			return *capacity <= std::numeric_limits<std::size_t>::max();
		return true;
	};
	if (!valid_buffer_capacity(options.vertex_buffer_bytes)
		|| !valid_buffer_capacity(options.index_buffer_bytes))
		throw std::invalid_argument("Runtime buffer capacities must be positive, four-byte aligned, and representable");
}

std::optional<std::uint64_t> timestamp_duration_ns(
	const std::uint64_t begin,
	const std::uint64_t end,
	const std::uint32_t valid_bits,
	const double timestamp_period_ns) noexcept
{
	if (valid_bits == 0 || valid_bits > 64 || !std::isfinite(timestamp_period_ns) || timestamp_period_ns <= 0.0)
		return std::nullopt;

	std::uint64_t ticks;
	if (valid_bits == 64)
		ticks = end - begin;
	else
	{
		const auto mask = (std::uint64_t{1} << valid_bits) - 1;
		ticks = ((end & mask) - (begin & mask)) & mask;
	}

	const long double duration = static_cast<long double>(ticks) * timestamp_period_ns;
	if (duration > static_cast<long double>(std::numeric_limits<std::uint64_t>::max()))
		return std::nullopt;
	return static_cast<std::uint64_t>(duration);
}

RuntimeObservation::Buffer::Buffer(const std::size_t capacity) : samples(capacity)
{
	if (capacity == 0)
		throw std::invalid_argument("Runtime observation buffer capacity must be greater than zero");
}

bool RuntimeObservation::Buffer::push(const RuntimeSample &sample) noexcept
{
	const auto write = write_index.load(std::memory_order_relaxed);
	const auto read = read_index.load(std::memory_order_acquire);
	if (write - read >= samples.size())
		return false;
	samples[write % samples.size()] = sample;
	write_index.store(write + 1, std::memory_order_release);
	return true;
}

void RuntimeObservation::Buffer::drain_into(std::vector<RuntimeSample> &output)
{
	const auto read = read_index.load(std::memory_order_relaxed);
	const auto write = write_index.load(std::memory_order_acquire);
	output.reserve(output.size() + static_cast<std::size_t>(write - read));
	for (auto index = read; index < write; ++index)
		output.push_back(samples[index % samples.size()]);
	read_index.store(write, std::memory_order_release);
}

RuntimeObservation::RuntimeObservation(const std::size_t capacity_per_thread) :
	game(capacity_per_thread),
	graphics(capacity_per_thread)
{
}

void RuntimeObservation::start() noexcept { collecting.store(true, std::memory_order_release); }
void RuntimeObservation::stop() noexcept { collecting.store(false, std::memory_order_release); }
bool RuntimeObservation::active() const noexcept { return collecting.load(std::memory_order_acquire); }

void RuntimeObservation::record_game(const RuntimeSample &sample) noexcept { record(game, sample); }
void RuntimeObservation::record_graphics(const RuntimeSample &sample) noexcept { record(graphics, sample); }

void RuntimeObservation::record(Buffer &buffer, const RuntimeSample &sample) noexcept
{
	if (!collecting.load(std::memory_order_acquire))
		return;
	if (!buffer.push(sample))
		lost.fetch_add(1, std::memory_order_relaxed);
}

std::vector<RuntimeSample> RuntimeObservation::drain()
{
	std::vector<RuntimeSample> samples;
	game.drain_into(samples);
	graphics.drain_into(samples);
	std::stable_sort(samples.begin(), samples.end(), [](const RuntimeSample &a, const RuntimeSample &b)
	{
		return a.begin_ns < b.begin_ns;
	});
	return samples;
}

std::uint64_t RuntimeObservation::lost_records() const noexcept { return lost.load(std::memory_order_relaxed); }

RuntimePacer::RuntimePacer(const double rate_hz) : rate_microhz(0)
{
	if (!std::isfinite(rate_hz) || rate_hz <= 0.0 ||
		rate_hz > static_cast<double>(std::numeric_limits<long long>::max()) / 1'000'000.0)
		throw std::invalid_argument("Runtime pacing rate must be finite and positive");
	rate_microhz = static_cast<std::uint64_t>(std::llround(rate_hz * 1'000'000.0));
	if (rate_microhz == 0)
		throw std::invalid_argument("Runtime pacing rate is below microhertz precision");
	reset(runtime_now_ns());
}

void RuntimePacer::reset(const std::uint64_t new_epoch_ns) noexcept
{
	epoch_ns = new_epoch_ns;
	next_slot = 1;
}

std::uint64_t RuntimePacer::deadline_for_slot(const std::uint64_t slot) const noexcept
{
	const auto offset = (static_cast<unsigned __int128>(slot) * 1'000'000'000'000'000ULL) / rate_microhz;
	if (offset > std::numeric_limits<std::uint64_t>::max() - epoch_ns)
		return std::numeric_limits<std::uint64_t>::max();
	return epoch_ns + static_cast<std::uint64_t>(offset);
}

RuntimePacingResult RuntimePacer::advance(const std::uint64_t now_ns) noexcept
{
	const auto begin = now_ns;
	auto deadline = deadline_for_slot(next_slot);
	std::uint64_t missed = 0;
	if (now_ns > deadline)
	{
		std::uint64_t low = next_slot;
		std::uint64_t high = next_slot;
		while (deadline_for_slot(high) <= now_ns && high < std::numeric_limits<std::uint64_t>::max() / 2)
			high *= 2;
		while (low + 1 < high)
		{
			const auto middle = low + (high - low) / 2;
			if (deadline_for_slot(middle) <= now_ns)
				low = middle;
			else
				high = middle;
		}
		missed = low - next_slot + 1;
		next_slot = high;
		deadline = deadline_for_slot(next_slot);
	}
	++next_slot;
	return RuntimePacingResult{begin, now_ns, deadline, missed};
}

RuntimePacingResult RuntimePacer::wait()
{
	const auto begin = runtime_now_ns();
	const auto schedule = advance(begin);
	const auto deadline = schedule.deadline_ns;
	const auto point = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(deadline));
	std::this_thread::sleep_until(point);
	return RuntimePacingResult{begin, runtime_now_ns(), deadline, schedule.missed_slots};
}

std::uint64_t RuntimePacer::next_deadline_ns() const noexcept { return deadline_for_slot(next_slot); }
