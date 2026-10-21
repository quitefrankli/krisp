#include "../benchmark_measurement.hpp"

#include <gtest/gtest.h>

TEST(MeasurementMembership, excludes_boundary_work_and_warmup_snapshots)
{
	RuntimeSample sample{.kind = RuntimeSampleKind::Simulation, .begin_ns = 90, .end_ns = 110};
	EXPECT_FALSE(BenchmarkMeasurement::classify(sample, 100, 200).included);
	sample.begin_ns = 150;
	sample.end_ns = 210;
	EXPECT_FALSE(BenchmarkMeasurement::classify(sample, 100, 200).included);
	sample.end_ns = 190;
	EXPECT_TRUE(BenchmarkMeasurement::classify(sample, 100, 200).included);
	sample.kind = RuntimeSampleKind::Submit;
	sample.publication_ns = 99;
	EXPECT_FALSE(BenchmarkMeasurement::classify(sample, 100, 200).included);
	sample.publication_ns = 120;
	EXPECT_TRUE(BenchmarkMeasurement::classify(sample, 100, 200).included);
}

TEST(MeasurementMembership, attributes_late_gpu_results_to_original_submission)
{
	RuntimeSample sample{.kind = RuntimeSampleKind::Gpu, .begin_ns = 150, .end_ns = 250};
	EXPECT_TRUE(BenchmarkMeasurement::classify(sample, 100, 200, true).included);
	EXPECT_FALSE(BenchmarkMeasurement::classify(sample, 100, 200, false).included);
	sample.begin_ns = 90;
	EXPECT_FALSE(BenchmarkMeasurement::classify(sample, 100, 200, true).included);
}

TEST(MeasurementMembership, includes_deadline_lateness_but_not_unstarted_measurement)
{
	RuntimeSample sample{.kind = RuntimeSampleKind::GameDeadline, .begin_ns = 150, .end_ns = 250};
	EXPECT_TRUE(BenchmarkMeasurement::classify(sample, 100, 200).included);
	EXPECT_FALSE(BenchmarkMeasurement::classify(sample, 0, 0).included);
	sample.kind = RuntimeSampleKind::GraphicsSleep;
	sample.end_ns = 190;
	EXPECT_TRUE(BenchmarkMeasurement::classify(sample, 100, 200).included);
}
