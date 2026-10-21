#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

namespace BenchmarkLoading
{
struct Span
{
	const char* name;
	std::uint64_t begin_ns;
	std::uint64_t end_ns;
	std::uint64_t duration_ns() const { return end_ns - begin_ns; }
};

// Wall-clock boundaries on the application thread. Construction and initial
// loading-screen setup precede asset registration; the four asset phases are
// contiguous. Readiness is the engine's observation of scene presentation.
struct Timeline
{
	std::uint64_t engine_begin_ns = 0;
	std::uint64_t engine_end_ns = 0;
	std::uint64_t asset_begin_ns = 0;
	std::uint64_t registration_end_ns = 0;
	std::uint64_t instantiation_end_ns = 0;
	std::uint64_t scene_end_ns = 0;
	std::uint64_t ready_ns = 0;

	std::array<Span, 7> spans() const
	{
		if (!engine_begin_ns || engine_end_ns < engine_begin_ns || asset_begin_ns < engine_end_ns
			|| registration_end_ns < asset_begin_ns || instantiation_end_ns < registration_end_ns
			|| scene_end_ns < instantiation_end_ns || ready_ns < scene_end_ns || ready_ns <= asset_begin_ns)
			throw std::invalid_argument("Asset-loading boundaries are missing or out of order");
		return {{
			{"engine_initialization_ns", engine_begin_ns, engine_end_ns},
			{"asset_registration_ns", asset_begin_ns, registration_end_ns},
			{"asset_instantiation_ns", registration_end_ns, instantiation_end_ns},
			{"scene_setup_ns", instantiation_end_ns, scene_end_ns},
			{"presentation_ready_ns", scene_end_ns, ready_ns},
			{"asset_to_ready_ns", asset_begin_ns, ready_ns},
			{"engine_to_ready_ns", engine_begin_ns, ready_ns}
		}};
	}
};
}
