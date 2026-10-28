"""Pure-Python aggregation for performance benchmark observations.

Durations and timestamps are integer nanoseconds. This module has no engine or
application dependencies so saved observations can be analyzed consistently.
"""

from __future__ import annotations

import math
import random
from bisect import bisect_right
from collections import defaultdict
from collections.abc import Iterable, Mapping, Sequence
from statistics import median
from typing import Any


_CPU_INTERVAL_KINDS = (
	"Simulation", "Graphics", "UniformUpload", "CommandRecording", "ResourceUpload",
	"SnapshotConstruction", "FrameFenceWait", "ImageFenceWait", "Acquire", "Submit",
	"Present", "GameSleep", "GraphicsSleep",
)
_GRAPHICS_PHASE_KINDS = {
	"Graphics", "UniformUpload", "CommandRecording", "FrameFenceWait", "ImageFenceWait",
	"Acquire", "Submit", "Present", "GraphicsSleep",
}
_GPU_KINDS = ("Gpu", "GpuShadow", "GpuMain", "GpuUi")


def nearest_rank_percentile(values: Iterable[float], percentile: float) -> float | None:
	"""Return the nearest-rank percentile (rank ceil(p*n)), or None if empty."""
	if not 0.0 <= percentile <= 100.0:
		raise ValueError("percentile must be between 0 and 100")
	ordered = sorted(values)
	if not ordered:
		return None
	if percentile == 0:
		return ordered[0]
	return ordered[math.ceil(percentile / 100.0 * len(ordered)) - 1]


def distribution(values: Iterable[float], *, reason: str | None = None) -> dict[str, Any]:
	"""Summarize durations, marking empty metrics unavailable instead of zero."""
	ordered = sorted(values)
	if not ordered:
		return {"count": 0, "median": None, "p95": None, "p99": None, "available": False, "reason": reason or "no_samples"}
	return {
		"count": len(ordered), "median": nearest_rank_percentile(ordered, 50),
		"p95": nearest_rank_percentile(ordered, 95), "p99": nearest_rank_percentile(ordered, 99),
		"available": True, "reason": None,
	}


def summarize_repetitions(values: Sequence[float]) -> dict[str, Any]:
	"""Summarize independent repetition-level metric values."""
	ordered = sorted(values)
	if not ordered:
		return {"count": 0, "median": None, "range": None, "mad": None, "values": [], "available": False, "reason": "no_repetitions"}
	median_value = median(ordered)
	return {
		"count": len(ordered), "median": median_value, "range": [ordered[0], ordered[-1]],
		"mad": median(abs(value - median_value) for value in ordered),
		"values": list(values), "available": True, "reason": None,
	}


def _is_unknown(value: Any) -> bool:
	return value is None or (isinstance(value, str) and value.strip().lower() in {"", "unknown", "unspecified", "unavailable"})


def compare_compatibility(baseline: Mapping[str, Any], candidate: Mapping[str, Any]) -> dict[str, Any]:
	"""Compare compatibility keys exactly and fail closed on unknown values.

	Pass only the compatibility key here; source revision and fingerprint belong
	to provenance and are intentionally outside this comparison.
	"""
	unknown: list[str] = []
	mismatches: list[dict[str, Any]] = []

	def visit(left: Any, right: Any, path: str) -> None:
		if isinstance(left, Mapping) and isinstance(right, Mapping):
			for key in sorted(set(left) | set(right), key=str):
				child_path = f"{path}.{key}" if path else str(key)
				if key not in left or key not in right:
					mismatches.append({"field": child_path, "baseline": left.get(key), "candidate": right.get(key), "reason": "field_missing"})
				else:
					visit(left[key], right[key], child_path)
		elif isinstance(left, (list, tuple)) and isinstance(right, (list, tuple)):
			for index in range(max(len(left), len(right))):
				child_path = f"{path}[{index}]"
				if index >= len(left) or index >= len(right):
					mismatches.append({"field": child_path, "baseline": left[index] if index < len(left) else None, "candidate": right[index] if index < len(right) else None, "reason": "field_missing"})
				else:
					visit(left[index], right[index], child_path)
		elif _is_unknown(left) or _is_unknown(right):
			unknown.append(path or "<root>")
		elif type(left) is not type(right) or left != right:
			mismatches.append({"field": path or "<root>", "baseline": left, "candidate": right, "reason": "value_mismatch"})

	visit(baseline, candidate, "")
	return {"compatible": not unknown and not mismatches, "mismatches": mismatches, "unknown_fields": sorted(set(unknown))}


def compare_repetitions(
	baseline: Sequence[float], candidate: Sequence[float], *, lower_is_better: bool, seed: int = 0,
) -> dict[str, Any]:
	"""Compare repetitions using a deterministic independent-sample bootstrap.

	The 95% interval contains the 2.5th and 97.5th nearest-rank percentiles of
	10,000 resampled median(candidate) - median(baseline) differences.
	"""
	base = summarize_repetitions(baseline)
	current = summarize_repetitions(candidate)
	result: dict[str, Any] = {
		"baseline": base, "candidate": current, "difference": None,
		"relative_change_percent": None, "difference_95_interval": None,
		"label": "inconclusive", "bootstrap_resamples": 10_000, "seed": seed,
	}
	if base["count"] < 2 or current["count"] < 2:
		result["reason"] = "insufficient_repetitions"
		return result
	base_median, current_median = base["median"], current["median"]
	difference = current_median - base_median
	result["difference"] = difference
	if base_median != 0:
		result["relative_change_percent"] = difference / base_median * 100.0

	rng = random.Random(seed)
	differences = []
	for _ in range(10_000):
		base_draw = [baseline[rng.randrange(len(baseline))] for _ in baseline]
		candidate_draw = [candidate[rng.randrange(len(candidate))] for _ in candidate]
		differences.append(median(candidate_draw) - median(base_draw))
	interval = [nearest_rank_percentile(differences, 2.5), nearest_rank_percentile(differences, 97.5)]
	result["difference_95_interval"] = interval
	if interval[0] > 0:
		result["label"] = "regressed" if lower_is_better else "improved"
	elif interval[1] < 0:
		result["label"] = "improved" if lower_is_better else "regressed"
	result["reason"] = None if result["label"] != "inconclusive" else "interval_includes_zero"
	return result


def _interval(record: Mapping[str, Any]) -> tuple[int, int] | None:
	start, end = record.get("begin_ns"), record.get("end_ns")
	if isinstance(start, int) and isinstance(end, int) and end >= start:
		return start, end
	return None


class _IntervalUnion:
	"""Index union coverage once so arbitrary parent intervals cost O(log n)."""

	def __init__(self, intervals: Iterable[tuple[int, int]]) -> None:
		self.starts: list[int] = []
		self.ends: list[int] = []
		for start, end in sorted(intervals):
			if end <= start:
				continue
			if self.ends and start <= self.ends[-1]:
				self.ends[-1] = max(self.ends[-1], end)
			else:
				self.starts.append(start)
				self.ends.append(end)
		self.prefix_ns = [0]
		for start, end in zip(self.starts, self.ends):
			self.prefix_ns.append(self.prefix_ns[-1] + end - start)

	def _coverage_before(self, timestamp: int) -> int:
		index = bisect_right(self.starts, timestamp) - 1
		if index < 0:
			return 0
		return self.prefix_ns[index] + min(timestamp, self.ends[index]) - self.starts[index]

	def overlap_ns(self, parent: tuple[int, int]) -> int:
		return self._coverage_before(parent[1]) - self._coverage_before(parent[0])


def summarize_samples(
	samples: Iterable[Mapping[str, Any]], window_start_ns: int, window_end_ns: int,
	gpu_supported: bool, tolerance_ns: int = 1_000_000,
) -> dict[str, Any]:
	"""Summarize one repetition's raw records for a monotonic measurement window.

	CPU intervals count only when wholly inside the window. GPU records are
	assigned to measured submissions by ID and publication phase, independent of
	when delayed results were collected.
	"""
	if window_end_ns <= window_start_ns:
		raise ValueError("measurement window must have positive duration")
	if tolerance_ns < 0:
		raise ValueError("tolerance_ns must be non-negative")
	records = [dict(sample) for sample in samples]
	window_ns = window_end_ns - window_start_ns
	by_kind: dict[str, list[dict[str, Any]]] = defaultdict(list)
	for record in records:
		by_kind[str(record.get("kind", "Unknown"))].append(record)

	in_window: dict[str, list[dict[str, Any]]] = defaultdict(list)
	boundary_excluded: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
	for kind in _CPU_INTERVAL_KINDS:
		for record in by_kind[kind]:
			publication = record.get("publication_ns")
			if kind in _GRAPHICS_PHASE_KINDS and isinstance(publication, int) and publication > 0 and not window_start_ns <= publication < window_end_ns:
				reason = "warmup_snapshot" if publication < window_start_ns else "after_window_snapshot"
				boundary_excluded[kind][reason] += 1
				continue
			interval = _interval(record)
			if interval is None:
				boundary_excluded[kind]["invalid_interval"] += 1
			elif interval[0] >= window_start_ns and interval[1] <= window_end_ns:
				in_window[kind].append(record)
			else:
				reason = "before_window" if interval[1] <= window_start_ns else "after_window" if interval[0] >= window_end_ns else "overlaps_window"
				boundary_excluded[kind][reason] += 1

	seconds = window_ns / 1_000_000_000
	counts = {
		"simulation_ticks": len(in_window["Simulation"]),
		"successful_submissions": sum(
			record.get("success") is True
			and isinstance(record.get("publication_ns"), int)
			and window_start_ns <= record["publication_ns"] < window_end_ns
			for record in in_window["Submit"]
		),
		"successful_presents": sum(record.get("success") is True for record in in_window["Present"]),
		"successful_acquires": sum(record.get("success") is True for record in in_window["Acquire"]),
	}
	rates = {key + "_per_second": value / seconds for key, value in counts.items()}
	interval_metrics = {
		kind: distribution((record["end_ns"] - record["begin_ns"] for record in in_window[kind]), reason="no_fully_contained_samples")
		for kind in _CPU_INTERVAL_KINDS
	}
	for kind in _GPU_KINDS:
		interval_metrics[kind] = _gpu_distribution(
			records, in_window["Submit"], window_start_ns, window_end_ns, gpu_supported, kind=kind,
		)

	child_kinds = {"FrameFenceWait", "ImageFenceWait", "Acquire", "Submit", "Present", "GraphicsSleep"}
	active: dict[str, dict[str, Any]] = {}
	for parent_kind, excluded_kinds in (("Simulation", {"GameSleep"}), ("Graphics", child_kinds)):
		children = [_interval(record) for kind in excluded_kinds for record in in_window[kind]]
		child_union = _IntervalUnion(child for child in children if child is not None)
		active_values = []
		for record in in_window[parent_kind]:
			parent = _interval(record)
			if parent is not None:
				active_values.append(max(0, parent[1] - parent[0] - child_union.overlap_ns(parent)))
		active[parent_kind] = distribution(active_values, reason="no_fully_contained_samples")

	present_endpoints = sorted(record["end_ns"] for record in in_window["Present"] if record.get("success") is True)
	interval_metrics["PresentCadence"] = distribution(
		(right - left for left, right in zip(present_endpoints, present_endpoints[1:])),
		reason="fewer_than_two_successful_presents",
	)
	snapshot_ages = []
	for record in in_window["Submit"]:
		publication = record.get("publication_ns")
		begin = record.get("begin_ns")
		if record.get("success") is True and isinstance(publication, int) and isinstance(begin, int) and publication <= begin:
			snapshot_ages.append(begin - publication)
	interval_metrics["SnapshotAge"] = distribution(snapshot_ages, reason="no_measured_snapshot_age_samples")
	submissions = sorted(
		(
			record for record in in_window["Submit"]
			if record.get("success") is True
			and isinstance(record.get("publication_ns"), int)
			and window_start_ns <= record["publication_ns"] < window_end_ns
		),
		key=lambda record: (record.get("end_ns", record.get("begin_ns", 0)), record.get("submission_id", -1)),
	)
	repeats = skipped = 0
	previous_frame = None
	for record in submissions:
		frame = record.get("frame_number")
		if not isinstance(frame, int):
			continue
		if previous_frame is not None:
			if frame == previous_frame:
				repeats += 1
			elif frame > previous_frame + 1:
				skipped += frame - previous_frame - 1
		previous_frame = frame

	deadlines: dict[str, Any] = {}
	for kind in ("GameDeadline", "GraphicsDeadline"):
		entries = []
		for record in by_kind[kind]:
			deadline = record.get("deadline_ns", record.get("begin_ns"))
			completion = record.get("completion_ns", record.get("end_ns"))
			if not isinstance(deadline, int) or not isinstance(completion, int) or not window_start_ns <= deadline < window_end_ns:
				continue
			lateness = max(0, completion - deadline)
			slots = record.get("missed_slots", 0)
			if not isinstance(slots, int) or slots < 0:
				slots = 0
			entries.append((lateness, slots))
		deadlines[kind] = {
			"count": len(entries),
			"lateness": distribution((item[0] for item in entries), reason="no_deadline_samples"),
			"positive_lateness_count": sum(item[0] > 0 for item in entries),
			"positive_lateness": distribution((item[0] for item in entries if item[0] > 0), reason="no_late_deadlines"),
			"missed_deadline_count": sum(item[0] > tolerance_ns for item in entries) + sum(item[1] for item in entries),
			"skipped_schedule_slots": sum(item[1] for item in entries),
			"tolerance_ns": tolerance_ns,
		}

	return {
		"window": {"start_ns": window_start_ns, "end_ns": window_end_ns, "duration_ns": window_ns},
		"counts": counts, "rates_per_second": rates, "durations_ns": interval_metrics,
		"active_work_ns": active, "deadlines": deadlines,
		"snapshots": {"repeated": repeats, "skipped": skipped},
		"boundary_excluded": {kind: dict(reasons) for kind, reasons in boundary_excluded.items()},
		"present_cadence_label": "CPU present-call completion intervals; not display cadence",
	}


def _gpu_distribution(
	records: Sequence[Mapping[str, Any]], submissions: Sequence[Mapping[str, Any]],
	window_start_ns: int, window_end_ns: int, gpu_supported: bool, *, kind: str = "Gpu",
) -> dict[str, Any]:
	if not gpu_supported:
		return distribution((), reason="gpu_timestamps_unsupported")
	measured_ids = {
		record.get("submission_id") for record in submissions
		if isinstance(record.get("submission_id"), int)
		and isinstance(record.get("publication_ns"), int)
		and window_start_ns <= record["publication_ns"] < window_end_ns
	}
	values = []
	observed_ids = set()
	for record in records:
		if record.get("kind") != kind or record.get("submission_id") not in measured_ids:
			continue
		if record.get("success") is False or record.get("available") is False:
			continue
		if isinstance(record.get("duration_ns"), (int, float)) and record["duration_ns"] >= 0:
			values.append(record["duration_ns"])
			observed_ids.add(record["submission_id"])
	if measured_ids - observed_ids:
		return distribution((), reason="missing_gpu_samples_for_measured_submissions")
	return distribution(values, reason="missing_gpu_samples_for_measured_submissions")
