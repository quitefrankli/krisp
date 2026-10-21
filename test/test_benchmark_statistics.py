import unittest

from applications.stress_test.benchmark_statistics import (
	compare_compatibility,
	compare_repetitions,
	distribution,
	nearest_rank_percentile,
	summarize_repetitions,
	summarize_samples,
)


class BenchmarkStatisticsTests(unittest.TestCase):
	def test_nearest_rank_percentiles_and_empty_distribution(self):
		self.assertEqual(nearest_rank_percentile([4, 1, 3, 2], 50), 2)
		self.assertEqual(nearest_rank_percentile([1, 2, 3, 4], 95), 4)
		self.assertEqual(distribution([])["median"], None)
		self.assertEqual(distribution([])["reason"], "no_samples")

	def test_repetition_summary_uses_median_range_and_mad(self):
		self.assertEqual(summarize_repetitions([1, 2, 9]), {
			"count": 3, "median": 2, "range": [1, 9], "mad": 1,
			"values": [1, 2, 9], "available": True, "reason": None,
		})

	def test_repetition_comparison_is_deterministic_and_directional(self):
		baseline, candidate = [10, 11, 10, 9, 10], [14, 15, 14, 13, 14]
		first = compare_repetitions(baseline, candidate, lower_is_better=True)
		second = compare_repetitions(baseline, candidate, lower_is_better=True)
		self.assertEqual(first, second)
		self.assertEqual(first["label"], "regressed")
		self.assertEqual(first["difference"], 4)
		self.assertEqual(first["bootstrap_resamples"], 10_000)
		self.assertEqual(compare_repetitions([1], [2, 3], lower_is_better=True)["reason"], "insufficient_repetitions")

	def test_compatibility_fails_closed_on_unknown_and_reports_nested_mismatch(self):
		self.assertTrue(compare_compatibility({"machine": "A", "settings": {"fps": 60}}, {"machine": "A", "settings": {"fps": 60}})["compatible"])
		mismatch = compare_compatibility({"settings": {"fps": 60}}, {"settings": {"fps": 30}})
		self.assertEqual(mismatch["mismatches"][0]["field"], "settings.fps")
		unknown = compare_compatibility({"driver": "unknown"}, {"driver": "x"})
		self.assertFalse(unknown["compatible"])
		self.assertEqual(unknown["unknown_fields"], ["driver"])
		array_unknown = compare_compatibility({"drivers": ["a", "unknown"]}, {"drivers": ["a", "unknown"]})
		self.assertEqual(array_unknown["unknown_fields"], ["drivers[1]"])

	def test_sample_summary_excludes_window_boundaries_and_counts_only_successes(self):
		samples = [
			{"kind": "Simulation", "begin_ns": 0, "end_ns": 10},
			{"kind": "Simulation", "begin_ns": 10, "end_ns": 20},
			{"kind": "Simulation", "begin_ns": 20, "end_ns": 30},
			{"kind": "Submit", "begin_ns": 11, "end_ns": 12, "success": True, "submission_id": 1, "frame_number": 4, "publication_ns": 11},
			{"kind": "Submit", "begin_ns": 13, "end_ns": 14, "success": False, "submission_id": 2, "frame_number": 5, "publication_ns": 13},
			{"kind": "Present", "begin_ns": 14, "end_ns": 15, "success": True},
		]
		result = summarize_samples(samples, 10, 20, gpu_supported=False)
		self.assertEqual(result["counts"]["simulation_ticks"], 1)
		self.assertEqual(result["counts"]["successful_submissions"], 1)
		self.assertEqual(result["rates_per_second"]["successful_submissions_per_second"], 100_000_000)
		self.assertEqual(result["boundary_excluded"]["Simulation"]["before_window"], 1)
		self.assertEqual(result["boundary_excluded"]["Simulation"]["after_window"], 1)
		self.assertEqual(result["durations_ns"]["Gpu"]["reason"], "gpu_timestamps_unsupported")
		self.assertEqual(result["durations_ns"]["PresentCadence"]["reason"], "fewer_than_two_successful_presents")

	def test_gpu_matches_submission_id_and_measured_publication_phase(self):
		samples = [
			{"kind": "Submit", "begin_ns": 10, "end_ns": 11, "success": True, "submission_id": 7, "publication_ns": 15},
			{"kind": "Submit", "begin_ns": 12, "end_ns": 13, "success": True, "submission_id": 8, "publication_ns": 9},
			{"kind": "Gpu", "submission_id": 7, "duration_ns": 500, "available": True, "collected_ns": 99},
			{"kind": "Gpu", "submission_id": 8, "duration_ns": 900, "available": True, "collected_ns": 99},
		]
		metric = summarize_samples(samples, 10, 20, gpu_supported=True)["durations_ns"]["Gpu"]
		self.assertEqual(metric["count"], 1)
		self.assertEqual(metric["median"], 500)
		missing = summarize_samples(samples[:2], 10, 20, gpu_supported=True)["durations_ns"]["Gpu"]
		self.assertEqual(missing["median"], None)
		self.assertEqual(missing["reason"], "missing_gpu_samples_for_measured_submissions")

	def test_warmup_snapshot_is_excluded_from_graphics_metrics_even_inside_window(self):
		samples = [
			{"kind": "Graphics", "begin_ns": 11, "end_ns": 18, "publication_ns": 9},
			{"kind": "Acquire", "begin_ns": 12, "end_ns": 13, "success": True, "publication_ns": 9},
			{"kind": "Submit", "begin_ns": 14, "end_ns": 15, "success": True, "submission_id": 1, "frame_number": 2, "publication_ns": 9},
			{"kind": "Present", "begin_ns": 16, "end_ns": 17, "success": True, "publication_ns": 9},
			{"kind": "Gpu", "submission_id": 1, "duration_ns": 500, "available": True},
		]
		result = summarize_samples(samples, 10, 20, gpu_supported=True)
		self.assertEqual(result["counts"]["successful_submissions"], 0)
		self.assertEqual(result["counts"]["successful_presents"], 0)
		self.assertEqual(result["durations_ns"]["Gpu"]["median"], None)
		self.assertEqual(result["durations_ns"]["SnapshotAge"]["reason"], "no_measured_snapshot_age_samples")
		self.assertEqual(result["boundary_excluded"]["Submit"]["warmup_snapshot"], 1)

	def test_active_work_subtracts_union_of_child_waits_and_deadlines_count_slots(self):
		samples = [
			{"kind": "Graphics", "begin_ns": 10, "end_ns": 100},
			{"kind": "FrameFenceWait", "begin_ns": 20, "end_ns": 40},
			{"kind": "Acquire", "begin_ns": 30, "end_ns": 50},
			{"kind": "GraphicsSleep", "begin_ns": 70, "end_ns": 80},
			{"kind": "GameDeadline", "begin_ns": 40, "end_ns": 41, "missed_slots": 2},
			{"kind": "GameDeadline", "deadline_ns": 60, "completion_ns": 1_000_061},
		]
		result = summarize_samples(samples, 10, 100, gpu_supported=False)
		self.assertEqual(result["active_work_ns"]["Graphics"]["median"], 50)
		self.assertEqual(result["deadlines"]["GameDeadline"]["missed_deadline_count"], 3)
		self.assertEqual(result["deadlines"]["GameDeadline"]["skipped_schedule_slots"], 2)
		self.assertEqual(result["deadlines"]["GameDeadline"]["positive_lateness_count"], 2)
		self.assertEqual(result["deadlines"]["GameDeadline"]["tolerance_ns"], 1_000_000)

	def test_snapshot_repeat_skip_and_present_cadence_are_cpu_events(self):
		samples = []
		for index, frame in enumerate((1, 1, 4)):
			samples.append({"kind": "Submit", "begin_ns": 10 + index * 10, "end_ns": 11 + index * 10,
				"success": True, "submission_id": index, "frame_number": frame, "publication_ns": 10})
			samples.append({"kind": "Present", "begin_ns": 12 + index * 10, "end_ns": 13 + index * 10, "success": True})
		result = summarize_samples(samples, 10, 50, gpu_supported=False)
		self.assertEqual(result["snapshots"], {"repeated": 1, "skipped": 2})
		self.assertEqual(result["durations_ns"]["PresentCadence"]["median"], 10)
		self.assertIn("CPU", result["present_cadence_label"])


if __name__ == "__main__":
	unittest.main()
