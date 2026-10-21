#include "runtime_performance.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>


TEST(RuntimeOptions, accepts_legacy_defaults_and_uncapped_rendering)
{
	RuntimeOptions defaults;
	EXPECT_NO_THROW(validate_runtime_options(defaults));

	RuntimeOptions uncapped;
	uncapped.simulation_hz = 60.0;
	uncapped.render_hz = 0.0;
	uncapped.fixed_simulation_timestep = true;
	EXPECT_NO_THROW(validate_runtime_options(uncapped));
}

TEST(RuntimeOptions, rejects_invalid_rates_fixed_step_without_rate_and_bad_dimensions)
{
	RuntimeOptions options;
	options.simulation_hz = 0.0;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.simulation_hz = std::numeric_limits<double>::infinity();
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.simulation_hz.reset();
	options.fixed_simulation_timestep = true;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.fixed_simulation_timestep = false;
	options.render_hz = -1.0;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.render_hz = std::numeric_limits<double>::quiet_NaN();
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.render_hz.reset();
	options.width = 0;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.width = static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + 1U;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.width.reset();
	options.height = 0;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
}

TEST(RuntimeOptions, requires_explicit_buffer_capacities_to_be_positive_and_aligned)
{
	RuntimeOptions options;
	options.vertex_buffer_bytes = 4;
	options.index_buffer_bytes = 8;
	EXPECT_NO_THROW(validate_runtime_options(options));

	options.vertex_buffer_bytes = 0;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.vertex_buffer_bytes = 6;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
	options.vertex_buffer_bytes.reset();
	options.index_buffer_bytes = 3;
	EXPECT_THROW(validate_runtime_options(options), std::invalid_argument);
}

TEST(RuntimeObservation, records_into_bounded_thread_buffers_and_reports_overflow)
{
	RuntimeObservation observation(2);
	RuntimeSample first;
	first.begin_ns = 20;
	first.frame_number = 4;
	RuntimeSample second;
	second.begin_ns = 10;
	second.kind = RuntimeSampleKind::Graphics;
	RuntimeSample dropped;
	dropped.begin_ns = 30;

	observation.record_game(first);
	EXPECT_TRUE(observation.drain().empty());
	observation.start();
	observation.record_game(first);
	observation.record_game(dropped);
	observation.record_game(first);
	observation.record_graphics(second);
	EXPECT_EQ(observation.lost_records(), 1);

	const auto records = observation.drain();
	ASSERT_EQ(records.size(), 3);
	EXPECT_EQ(records[0].begin_ns, 10);
	EXPECT_EQ(records[1].begin_ns, 20);
	EXPECT_EQ(records[2].begin_ns, 30);
	EXPECT_EQ(records[1].frame_number, 4);

	observation.stop();
	observation.record_graphics(second);
	EXPECT_TRUE(observation.drain().empty());
}

TEST(RuntimePacer, distributes_sixty_hz_deadlines_without_integer_period_drift)
{
	RuntimePacer pacer(60.0);
	pacer.reset(1'000'000'000);
	EXPECT_EQ(pacer.next_deadline_ns(), 1'016'666'666);

	const auto first = pacer.advance(1'000'000'000);
	EXPECT_EQ(first.deadline_ns, 1'016'666'666);
	EXPECT_EQ(first.missed_slots, 0);
	EXPECT_EQ(pacer.next_deadline_ns(), 1'033'333'333);

	for (int i = 0; i < 58; ++i)
		pacer.advance(1'000'000'000);
	EXPECT_EQ(pacer.next_deadline_ns(), 2'000'000'000);
}

TEST(RuntimePacer, skips_overdue_slots_and_reports_the_count)
{
	RuntimePacer pacer(60.0);
	pacer.reset(0);
	const auto result = pacer.advance(55'000'000);
	EXPECT_EQ(result.missed_slots, 3);
	EXPECT_EQ(result.deadline_ns, 66'666'666);
	EXPECT_EQ(pacer.next_deadline_ns(), 83'333'333);
}

TEST(RuntimePacer, consumes_returned_deadline_after_a_small_oversleep)
{
	RuntimePacer pacer(60.0);
	pacer.reset(0);

	const auto late_first = pacer.advance(16'766'666);
	EXPECT_EQ(late_first.missed_slots, 1);
	EXPECT_EQ(late_first.deadline_ns, 33'333'333);

	const auto late_after_wait = pacer.advance(33'433'333);
	EXPECT_EQ(late_after_wait.missed_slots, 0);
	EXPECT_EQ(late_after_wait.deadline_ns, 50'000'000);
}

TEST(RuntimePacer, validates_rate)
{
	EXPECT_THROW(RuntimePacer{0.0}, std::invalid_argument);
	EXPECT_THROW(RuntimePacer{std::numeric_limits<double>::infinity()}, std::invalid_argument);
}

TEST(RuntimeTimestamp, converts_wrapping_counters_and_rejects_unavailable_timing)
{
	EXPECT_EQ(timestamp_duration_ns(250, 5, 8, 2.0), 22);
	EXPECT_EQ(timestamp_duration_ns(std::numeric_limits<std::uint64_t>::max() - 3, 2, 64, 1.0), 6);
	EXPECT_FALSE(timestamp_duration_ns(0, 1, 0, 1.0).has_value());
	EXPECT_FALSE(timestamp_duration_ns(0, 1, 65, 1.0).has_value());
	EXPECT_FALSE(timestamp_duration_ns(0, 1, 8, 0.0).has_value());
}
