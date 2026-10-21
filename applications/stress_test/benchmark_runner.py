"""Coordinator and report writer for the stress-test benchmark."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import random
import signal
import subprocess
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence

try:
	from . import benchmark_assets
	from .benchmark_statistics import compare_compatibility, compare_repetitions, summarize_samples
	from .build_provenance import machine_identity
except ImportError:  # direct script execution
	import benchmark_assets
	from benchmark_statistics import compare_compatibility, compare_repetitions, summarize_samples
	from build_provenance import machine_identity


RESULT_SCHEMA_VERSION = 1
SCENARIO_VERSION = 1
PROTOCOL_VERSION = 2
PROFILE = "engine-pbr-opaque-v1"
RUNTIME_TOOL_NAMES = (
	"benchmark_runner.py", "benchmark_statistics.py", "benchmark_assets.py",
	"build_provenance.py", "assets.lock.json",
)
ORDER_SEED = 0x4B52495350
DEADLINE_TOLERANCE_NS = 1_000_000
READY_TIMEOUT_SECONDS = 120
ENVIRONMENT_READY_TIMEOUT_SECONDS = 300
DRAIN_TIMEOUT_SECONDS = 30
SHUTDOWN_MARGIN_SECONDS = 10


def standard_cases(*, include_optional_views: bool = False) -> list[dict[str, Any]]:
	cases = [
		{"scenario": "static-objects", "count": 500, "view": None},
		{"scenario": "moving-transforms", "count": 500, "view": None},
		{"scenario": "point-shadows", "count": 500, "view": None},
		{"scenario": "textured-objects", "count": 16, "view": None},
		{"scenario": "textured-environment", "count": 1, "view": "atrium"},
	]
	if include_optional_views:
		cases.extend({"scenario": "textured-environment", "count": 1, "view": view}
			for view in ("corridor", "material-closeup"))
	for case in cases:
		case["case_id"] = _case_id(case)
	return cases


def smoke_cases() -> list[dict[str, Any]]:
	cases = [
		{"scenario": "static-objects", "count": 100, "view": None},
		{"scenario": "textured-objects", "count": 1, "view": None},
		{"scenario": "textured-environment", "count": 1, "view": "atrium"},
	]
	for case in cases:
		case["case_id"] = _case_id(case)
	return cases


def _case_id(case: Mapping[str, Any]) -> str:
	parts = [str(case["scenario"])]
	if case.get("view"):
		parts.append(str(case["view"]))
	else:
		parts.append(str(case["count"]))
	return "-".join(parts)


def suite_cases(suite: str, workload: str | None = None) -> list[dict[str, Any]]:
	cases = standard_cases(include_optional_views=workload is not None) if suite == "standard" else smoke_cases()
	if workload is None:
		return cases
	selected = [case for case in cases if case["case_id"] == workload]
	if not selected:
		raise ValueError(f"Workload {workload!r} is not in suite {suite!r}; choose: " + ", ".join(case["case_id"] for case in cases))
	return selected


def suite_defaults(suite: str) -> tuple[float, float, int]:
	return {"standard": (2.0, 5.0, 2), "smoke": (0.2, 0.5, 1)}[suite]


def make_schedule(suite: str, repetitions: int, seed: int = ORDER_SEED, *, workload: str | None = None) -> list[dict[str, Any]]:
	"""Shuffle each repetition round deterministically, preserving round identity."""
	if repetitions < 1:
		raise ValueError("repetitions must be positive")
	cases = suite_cases(suite, workload)
	rng = random.Random(seed)
	schedule = []
	for repetition in range(1, repetitions + 1):
		round_cases = list(cases)
		rng.shuffle(round_cases)
		for order, case in enumerate(round_cases):
			schedule.append({**case, "repetition": repetition, "round_order": order})
	return schedule


def verify_asset_cache(cache_dir: Path) -> tuple[dict[str, Any], list[str]]:
	"""Verify pinned source provenance and prepared files without network access."""
	try:
		lock_hash = benchmark_assets.sha256_file(benchmark_assets.LOCK_PATH)
	except OSError as exc:
		return {}, [f"cannot read checked-in asset lock {benchmark_assets.LOCK_PATH}: {exc}"]
	prepared, errors = benchmark_assets.verify_prepared_cache(
		cache_dir, expected_lock_hash=lock_hash, lock_path=benchmark_assets.LOCK_PATH,
	)
	if errors:
		return prepared, errors
	files = prepared["files"]
	canonical_identity = {
		"profile": PROFILE,
		"lock_sha256": prepared["lock_sha256"],
		"converter_version": prepared["converter_version"],
		"files": dict(files),
	}
	canonical = json.dumps(canonical_identity, sort_keys=True, separators=(",", ":")).encode()
	identity = hashlib.sha256(canonical).hexdigest()
	return {"profile": PROFILE, "manifest_sha256": identity, "files": dict(files)}, []


def benchmark_build_errors(suite: str, build: Mapping[str, Any]) -> list[str]:
	if suite == "smoke":
		return []
	if (build.get("build_type") == "release" and build.get("optimised") is True
		and build.get("debug_macro") is False and build.get("assertions") is False):
		return []
	return ["Performance benchmarks require an optimized Release build with assertions disabled "
		"(--buildtype=release -Db_ndebug=true). Debug builds support --suite smoke for correctness checks only."]


def _sha256(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def verify_runtime_tools(tool_dir: Path, expected_hashes: Any) -> list[str]:
	"""Fail closed unless every runtime tool matches the build-time fingerprint."""
	if not isinstance(expected_hashes, dict):
		return ["build manifest is missing runtime_tools_sha256; rebuild the stress-test executable"]
	if set(expected_hashes) != set(RUNTIME_TOOL_NAMES):
		return ["build manifest has incomplete runtime_tools_sha256; rebuild the stress-test executable"]
	errors = []
	for name in RUNTIME_TOOL_NAMES:
		path = tool_dir / name
		if not path.is_file():
			errors.append(f"runtime tool is missing: {path}")
			continue
		try:
			actual = _sha256(path)
		except OSError as exc:
			errors.append(f"cannot hash runtime tool {path}: {exc}")
			continue
		if not isinstance(expected_hashes[name], str) or actual != expected_hashes[name]:
			errors.append(f"runtime tool changed since build: {name}; rebuild the stress-test executable")
	return errors


def _shader_spirv_hashes(directory: Path) -> dict[str, str]:
	"""Hash compiled shader contents for provenance, independently of source compatibility."""
	if not directory.is_dir():
		raise ValueError(f"shader directory does not exist: {directory}")
	files = sorted(path for path in directory.rglob("*.spv") if path.is_file())
	if not files:
		raise ValueError(f"shader directory contains no .spv files: {directory}")
	return {path.relative_to(directory).as_posix(): _sha256(path) for path in files}


def _load_json(path: Path) -> dict[str, Any]:
	value = json.loads(path.read_text(encoding="utf-8"))
	if not isinstance(value, dict):
		raise ValueError(f"expected a JSON object in {path}")
	return value


def _atomic_json(path: Path, value: Any) -> None:
	temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
	try:
		with temporary.open("x", encoding="utf-8") as output:
			json.dump(value, output, indent=2, sort_keys=True)
			output.write("\n")
			output.flush()
			os.fsync(output.fileno())
		temporary.replace(path)
	finally:
		if temporary.exists():
			temporary.unlink()


def _new_output_dir() -> Path:
	base = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share")) / "krisp-engine" / "stress-test"
	base.mkdir(parents=True, exist_ok=True)
	path = base / f"run-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}-{uuid.uuid4().hex[:8]}"
	path.mkdir()
	return path


def _resolve_baseline(path_text: str) -> Path:
	path = Path(path_text).expanduser()
	if path.is_dir():
		path = path / "run_manifest.json"
	return path.resolve()


def _terminate_group(process: subprocess.Popen[Any], grace_seconds: float = 2.0) -> None:
	if process.poll() is not None:
		return
	try:
		os.killpg(process.pid, signal.SIGTERM)
	except ProcessLookupError:
		return
	try:
		process.wait(timeout=grace_seconds)
	except subprocess.TimeoutExpired:
		try:
			os.killpg(process.pid, signal.SIGKILL)
		except ProcessLookupError:
			pass
		process.wait()


def _early_worker_failure(completion_path: Path, reason: str) -> dict[str, Any]:
	"""Preserve worker diagnostics when initialization fails before ready.json."""
	worker: dict[str, Any] = {"status": "failed", "errors": []}
	try:
		loaded = _load_json(completion_path)
		worker.update(loaded)
	except (OSError, ValueError, json.JSONDecodeError):
		pass
	errors = worker.get("errors")
	if not isinstance(errors, list):
		errors = []
		worker["errors"] = errors
	if reason not in errors:
		errors.append(reason)
	worker["status"] = "failed"
	return worker


def _worker_command(
	executable: str, case: Mapping[str, Any], mode: str, warmup: float,
	measurement: float, cache: Path, output: Path,
) -> list[str]:
	command = [
		executable, "_worker", "--scenario", str(case["scenario"]),
		"--count", str(case["count"]), "--mode", mode,
		"--warmup", str(warmup), "--measure", str(measurement),
		"--asset-cache", str(cache), "--output", str(output),
	]
	if case.get("view"):
		command.extend(("--view", str(case["view"])))
	return command


def _read_raw_samples(path: Path) -> list[dict[str, Any]]:
	result = []
	with path.open(encoding="utf-8") as raw:
		for line_number, line in enumerate(raw, 1):
			if not line.strip():
				continue
			try:
				value = json.loads(line)
			except json.JSONDecodeError as exc:
				raise ValueError(f"invalid raw JSONL at {path}:{line_number}: {exc}") from exc
			if not isinstance(value, dict):
				raise ValueError(f"raw record at {path}:{line_number} must be an object")
			result.append(value)
	return result


def _case_compatibility(
	case: Mapping[str, Any], mode: str, protocol: Mapping[str, Any], assets: Mapping[str, Any],
	build: Mapping[str, Any], worker: Mapping[str, Any], engine_resources: Mapping[str, Any] | None = None,
) -> dict[str, Any]:
	graphics = worker.get("graphics")
	if not isinstance(graphics, dict):
		graphics = {}
	scenario = worker.get("scenario")
	if not isinstance(scenario, dict):
		scenario = {}
	machine = machine_identity()
	worker_machine = graphics.get("machine")
	if isinstance(worker_machine, dict):
		machine.update(worker_machine)
	selected_gpu = graphics.get("selected_gpu", graphics.get("selected_device"))
	if selected_gpu is None and any(key in graphics for key in ("vendor_id", "device_id", "device_name", "api_version")):
		selected_gpu = {
			key: graphics.get(key) for key in ("vendor_id", "device_id", "device_name", "api_version")
		}
	selected_driver = graphics.get("driver", graphics.get("driver_version", graphics.get("driver_name")))
	if selected_gpu is not None:
		machine["gpu"] = selected_gpu
	if selected_driver is not None:
		machine["driver"] = selected_driver
	effective_settings = graphics.get("effective_settings")
	if not isinstance(effective_settings, dict):
		setting_keys = (
			"width", "height", "requested_width", "requested_height", "presentation_mode",
			"requested_presentation_mode", "validation_enabled", "timestamp_valid_bits",
			"timestamp_period_ns", "swapchain_images", "simulation_hz", "fixed_simulation_timestep",
			"render_path", "exposure_ev", "ui", "performance_overlay", "interactive", "vertex_buffer_bytes",
			"index_buffer_bytes", "render_hz", "msaa_samples", "shadow_resolution", "configuration_note",
		)
		effective_settings = {key: graphics.get(key) for key in setting_keys}
		if effective_settings.get("configuration_note") in (None, ""):
			effective_settings["configuration_note"] = "none"
	workload = {
		"scenario": case["scenario"], "scenario_version": scenario.get("version", SCENARIO_VERSION),
		"count": case["count"],
		"instances": scenario.get("instances"), "renderables": scenario.get("renderables"),
		"asset_profile": scenario.get("asset_profile", assets.get("profile")),
		"asset_manifest_sha256": assets.get("manifest_sha256"),
	}
	if case.get("view") is not None:
		workload["view"] = case["view"]
	if engine_resources is not None:
		workload["engine_resources"] = {
			"environment_lighting_sha256": engine_resources.get("environment_lighting_sha256"),
		}
	return {
		"machine": machine,
		"build": {
			"build_class": build.get("build_class"),
			"build_type": build.get("build_type"),
			"optimised": build.get("optimised"),
			"debug_macro": build.get("debug_macro"),
			"assertions": build.get("assertions"),
			"validation": graphics.get("validation_enabled", build.get("validation")),
			"compiler": build.get("compiler"),
			"compiler_version": build.get("compiler_version"),
			"optimization_level": build.get("optimization_level"),
			"dependency_versions": build.get("dependency_versions"),
			"conan_profile_sha256": build.get("conan_profile_sha256"),
			"conan_native_file_sha256": build.get("conan_native_file_sha256"),
			"configuration_fingerprint": build.get("configuration_fingerprint"),
			"runtime_tools_sha256": build.get("runtime_tools_sha256"),
		},
		"workload": workload,
		"mode": mode,
		"protocol": {
			"suite": protocol["suite"], "protocol_version": protocol["protocol_version"],
			"instrumentation_version": 1,
			"warmup_seconds": protocol["warmup_seconds"], "measurement_seconds": protocol["measurement_seconds"],
			"repetitions": protocol["repetitions"], "deadline_tolerance_ns": protocol["deadline_tolerance_ns"],
		},
		"effective_settings": effective_settings,
	}


def _summary_for_worker(worker_dir: Path, worker: dict[str, Any], gpu_supported: bool) -> dict[str, Any]:
	start, end = worker.get("window_start_ns"), worker.get("window_end_ns")
	if not isinstance(start, int) or not isinstance(end, int) or end <= start:
		raise ValueError("worker completion must include a positive measurement window")
	raw_path = worker_dir / "raw.jsonl"
	if not raw_path.is_file():
		raise ValueError("worker did not produce raw.jsonl")
	samples = _read_raw_samples(raw_path)
	return summarize_samples(samples, start, end, gpu_supported, DEADLINE_TOLERANCE_NS)


def _status_for_worker(worker: Mapping[str, Any], summary: Mapping[str, Any]) -> str:
	status = worker.get("status")
	if status in {"failed", "invalid"}:
		return str(status)
	if worker.get("errors") or worker.get("lost_records", 0):
		return "invalid"
	if status not in {"complete", "partial"}:
		return "failed"
	if worker.get("gpu_supported") is True and not summary["durations_ns"]["Gpu"]["available"]:
		return "invalid"
	if status == "partial" or worker.get("gpu_supported") is not True:
		return "partial"
	return "complete"


def _run_worker(
	executable: str, case: Mapping[str, Any], mode: str, repetition: int,
	warmup: float, measurement: float, cache: Path, worker_dir: Path,
) -> tuple[dict[str, Any], int | None, str | None]:
	worker_dir.mkdir(parents=True, exist_ok=False)
	ready_path = worker_dir / "ready.json"
	completion_path = worker_dir / "worker.json"
	stdout_path = (worker_dir / "stdout.log").open("wb")
	stderr_path = (worker_dir / "stderr.log").open("wb")
	command = _worker_command(executable, case, mode, warmup, measurement, cache, worker_dir)
	process = None
	try:
		process = subprocess.Popen(command, stdout=stdout_path, stderr=stderr_path, start_new_session=True)
		ready_timeout = ENVIRONMENT_READY_TIMEOUT_SECONDS if case["scenario"] == "textured-environment" else READY_TIMEOUT_SECONDS
		ready_deadline = time.monotonic() + ready_timeout
		while not ready_path.is_file() and time.monotonic() < ready_deadline:
			if process.poll() is not None:
				break
			time.sleep(0.05)
		if not ready_path.is_file():
			_terminate_group(process)
			worker = _early_worker_failure(completion_path, "worker readiness timeout or early exit")
			return worker, process.returncode, "readiness_timeout"
		try:
			ready = _load_json(ready_path)
		except (OSError, ValueError, json.JSONDecodeError) as exc:
			_terminate_group(process)
			return {"status": "failed", "errors": [f"invalid ready.json: {exc}"]}, process.returncode, "invalid_readiness"
		if ready.get("status") != "ready":
			_terminate_group(process)
			return {"status": "failed", "errors": ["ready.json status is not ready"]}, process.returncode, "invalid_readiness"
		worker_deadline = time.monotonic() + warmup + measurement + DRAIN_TIMEOUT_SECONDS + SHUTDOWN_MARGIN_SECONDS
		try:
			process.wait(timeout=max(0.0, worker_deadline - time.monotonic()))
		except subprocess.TimeoutExpired:
			_terminate_group(process)
			worker = _early_worker_failure(completion_path, "worker exceeded bounded run/drain timeout")
			return worker, process.returncode, "worker_timeout"
		if not completion_path.is_file():
			return {"status": "failed", "errors": ["worker exited without worker.json"]}, process.returncode, "missing_completion"
		try:
			worker = _load_json(completion_path)
		except (OSError, ValueError, json.JSONDecodeError) as exc:
			return {"status": "failed", "errors": [f"invalid worker.json: {exc}"]}, process.returncode, "invalid_completion"
		if process.returncode != 0 and not (process.returncode == 1 and worker.get("status") == "invalid"):
			errors = worker.get("errors")
			if not isinstance(errors, list):
				errors = []
				worker["errors"] = errors
			errors.append(f"worker exited with status {process.returncode}")
			worker["status"] = "failed"
		return worker, process.returncode, None
	except OSError as exc:
		if process is not None:
			_terminate_group(process)
		return {"status": "failed", "errors": [f"could not start worker: {exc}"]}, process.returncode if process else None, "worker_spawn_error"
	except KeyboardInterrupt:
		if process is not None:
			_terminate_group(process)
		return {"status": "failed", "errors": ["interrupted; active worker terminated"]}, process.returncode if process else None, "interrupted"
	finally:
		stdout_path.close()
		stderr_path.close()


def _case_result(
	worker: dict[str, Any], worker_dir: Path, case: Mapping[str, Any], mode: str,
	repetition: int, compatibility: Mapping[str, Any], gpu_supported: bool,
) -> dict[str, Any]:
	result: dict[str, Any] = {
		"repetition": repetition, "mode": mode, "status": "failed",
		"compatibility": compatibility, "worker_directory": str(worker_dir),
		"worker": worker,
	}
	try:
		if worker.get("status") not in {"failed", "invalid"}:
			result["summary"] = _summary_for_worker(worker_dir, worker, gpu_supported)
			result["status"] = _status_for_worker(worker, result["summary"])
		else:
			result["status"] = worker["status"]
	except (OSError, ValueError, KeyError, TypeError) as exc:
		result["status"] = "failed"
		result["error"] = str(exc)
	return result


def _primary_metrics(repetitions: Sequence[Mapping[str, Any]]) -> dict[str, list[float]]:
	metrics: dict[str, list[float]] = {}
	paths = {
		"simulation_ticks_per_second": ("rates_per_second", "simulation_ticks_per_second"),
		"successful_submissions_per_second": ("rates_per_second", "successful_submissions_per_second"),
		"simulation_active_median_ns": ("active_work_ns", "Simulation", "median"),
		"graphics_active_median_ns": ("active_work_ns", "Graphics", "median"),
		"gpu_median_ns": ("durations_ns", "Gpu", "median"),
	}
	for name, path in paths.items():
		values = []
		for repetition in repetitions:
			if repetition.get("status") not in {"complete", "partial"}:
				continue
			value: Any = repetition.get("summary", {})
			for part in path:
				value = value.get(part) if isinstance(value, dict) else None
			if isinstance(value, (int, float)):
				values.append(value)
		metrics[name] = values
	return metrics


def _compare_run(manifest: dict[str, Any], baseline: Mapping[str, Any]) -> None:
	baseline_cases = {case.get("case_id"): case for case in baseline.get("cases", []) if isinstance(case, dict)}
	for case in manifest.get("cases", []):
		prior = baseline_cases.get(case["case_id"])
		case["comparisons"] = {}
		if prior is None:
			for mode in case["repetitions_by_mode"]:
				case["comparisons"][mode] = {
					"compatibility": {
						"compatible": False,
						"mismatches": [{"field": "case_id", "reason": "case_missing_from_baseline"}],
						"unknown_fields": [],
					}
				}
			continue
		for metric, lower_is_better in (
			("simulation_ticks_per_second", False),
			("successful_submissions_per_second", False),
			("simulation_active_median_ns", True),
			("graphics_active_median_ns", True),
			("gpu_median_ns", True),
		):
			for mode, reps in case["repetitions_by_mode"].items():
				baseline_reps = prior.get("repetitions_by_mode", {}).get(mode, [])
				current_compat = next((r.get("compatibility") for r in reps if r.get("compatibility")), {})
				baseline_compat = next((r.get("compatibility") for r in baseline_reps if r.get("compatibility")), {})
				compatibility = compare_compatibility(baseline_compat, current_compat)
				case["comparisons"].setdefault(mode, {})["compatibility"] = compatibility
				if not compatibility["compatible"]:
					continue
				base_values = _primary_metrics(baseline_reps).get(metric, [])
				candidate_values = _primary_metrics(reps).get(metric, [])
				case["comparisons"][mode][metric] = compare_repetitions(
					base_values, candidate_values, lower_is_better=lower_is_better,
					seed=int(hashlib.sha256(f"{case['case_id']}:{mode}:{metric}".encode()).hexdigest()[:8], 16),
				)


def _report(manifest: Mapping[str, Any]) -> str:
	lines = [
		"# Stress-test benchmark report", "",
		f"Run: `{manifest.get('run_id', 'unknown')}`  ",
		f"Suite: `{manifest.get('protocol', {}).get('suite', 'unknown')}`  ",
		f"Status: **{manifest.get('status', 'failed')}**  ",
		f"Workers: {manifest.get('completed_workers', 0)}/{manifest.get('scheduled_workers', 0)} completed", "",
	]
	if "wall_time_seconds" in manifest:
		lines.append(f"Elapsed: {manifest['wall_time_seconds']:.1f} seconds including preflight and worker startup.\n")
	build = manifest.get("build", {})
	validation_states = {
		repetition.get("worker", {}).get("graphics", {}).get("validation_enabled")
		for case in manifest.get("cases", [])
		for repetitions in case.get("repetitions_by_mode", {}).values()
		for repetition in repetitions
		if isinstance(repetition.get("worker", {}).get("graphics"), dict)
	}
	validation_label = ", ".join(str(value) for value in sorted(validation_states, key=str)) if validation_states else build.get("validation", "unknown")
	lines.append(
		f"Build class: `{build.get('build_class', 'unknown')}`; type `{build.get('build_type', 'unknown')}`; "
		f"validation enabled: `{validation_label}`; source `{manifest.get('build', {}).get('source_revision', 'unknown')}` "
		f"(dirty: `{manifest.get('build', {}).get('dirty', 'unknown')}`)"
	)
	lines.extend(["", "Each duration is in nanoseconds. Present-call cadence is measured on the CPU and does not represent display scanout.", "", "## Cases", ""])
	for case in manifest.get("cases", []):
		lines.append(f"### {case['case_id']}")
		lines.append("")
		for mode, repetitions in case.get("repetitions_by_mode", {}).items():
			lines.append(f"**{mode}**")
			for repetition in repetitions:
				status = repetition.get("status", "failed")
				if status not in {"complete", "partial"} and "summary" not in repetition:
					lines.append(f"- repetition {repetition.get('repetition')}: **{status}** — {repetition.get('error') or '; '.join(repetition.get('worker', {}).get('errors', []))}")
					continue
				summary = repetition.get("summary", {})
				counts = summary.get("counts", {})
				rates = summary.get("rates_per_second", {})
				lines.append(f"- repetition {repetition['repetition']}: **{status}**")
				lines.append(
					f"  - Counts: ticks {counts.get('simulation_ticks', 0)}, submissions {counts.get('successful_submissions', 0)}, "
					f"presents {counts.get('successful_presents', 0)}; achieved rates: "
					f"{rates.get('simulation_ticks_per_second', 0):.2f} ticks/s, "
					f"{rates.get('successful_submissions_per_second', 0):.2f} submissions/s, "
					f"{rates.get('successful_presents_per_second', 0):.2f} presents/s"
				)
				for metric_name in (
					"Simulation", "Graphics", "FrameFenceWait", "ImageFenceWait", "Acquire", "Submit", "Present",
					"GameSleep", "GraphicsSleep", "Gpu", "PresentCadence", "SnapshotAge",
				):
					metric = summary.get("durations_ns", {}).get(metric_name, {})
					if metric.get("available"):
						lines.append(f"  - {metric_name} ns (n={metric['count']}): median {metric['median']}, p95 {metric['p95']}, p99 {metric['p99']}")
					elif metric:
						lines.append(f"  - {metric_name}: unavailable ({metric.get('reason')})")
				for active_name, metric in summary.get("active_work_ns", {}).items():
					if metric.get("available"):
						lines.append(f"  - {active_name} active work ns (n={metric['count']}): median {metric['median']}, p95 {metric['p95']}, p99 {metric['p99']}")
				for loop, deadline in summary.get("deadlines", {}).items():
					lines.append(
						f"  - {loop}: {deadline['missed_deadline_count']} missed deadlines, "
						f"{deadline['skipped_schedule_slots']} skipped slots, tolerance {deadline['tolerance_ns']} ns"
					)
				snapshots = summary.get("snapshots", {})
				lines.append(f"  - Snapshots: {snapshots.get('repeated', 0)} repeats, {snapshots.get('skipped', 0)} skipped IDs")
				graphics = repetition.get("worker", {}).get("graphics", {})
				if isinstance(graphics, dict):
					effective = repetition.get("compatibility", {}).get("effective_settings", graphics.get("effective_settings"))
					presentation = graphics.get("effective_presentation_mode") or graphics.get("presentation_mode")
					if presentation:
						lines.append(f"  - Effective presentation mode: `{presentation}`")
					if effective is not None:
						lines.append(f"  - Effective settings: `{json.dumps(effective, sort_keys=True)}`")
				if status == "invalid":
					lines.append("  - Invalid repetitions are excluded from comparisons.")
				if status == "partial":
					lines.append("  - CPU results are retained; GPU timestamp coverage is unavailable or partial.")
			comparison = case.get("comparisons", {}).get(mode, {})
			compat = comparison.get("compatibility")
			if compat:
				lines.append(f"- Baseline compatibility: {'compatible' if compat['compatible'] else 'incompatible'}")
				for mismatch in compat.get("mismatches", []):
					lines.append(f"  - {mismatch['field']}: {mismatch.get('baseline')} → {mismatch.get('candidate')} ({mismatch['reason']})")
				for unknown in compat.get("unknown_fields", []):
					lines.append(f"  - {unknown}: unknown compatibility value")
				for metric, value in comparison.items():
					if metric != "compatibility" and isinstance(value, dict):
						base_summary = value.get("baseline", {})
						candidate_summary = value.get("candidate", {})
						lines.append(
							f"- {metric}: **{value['label']}**; delta {value.get('difference')}, "
							f"relative {value.get('relative_change_percent')}%; 95% interval {value.get('difference_95_interval')}; "
							f"baseline median/range/MAD {base_summary.get('median')}/{base_summary.get('range')}/{base_summary.get('mad')}; "
							f"values baseline/candidate {base_summary.get('values')}/{candidate_summary.get('values')}; "
							f"candidate median/range/MAD {candidate_summary.get('median')}/{candidate_summary.get('range')}/{candidate_summary.get('mad')}; "
							f"{value.get('reason') or 'directional evidence'}"
						)
			lines.append("")
	lines.extend([
		"## Interpretation", "",
		"The report keeps simulation, graphics, GPU and CPU present-call measurements separate. A slowdown is advisory and does not change the command exit status. Repetition ranges and bootstrap intervals describe observed variation; they do not remove machine-load or thermal effects.", "",
	])
	return "\n".join(lines)


def _save(manifest: dict[str, Any], output: Path) -> None:
	_atomic_json(output / "run_manifest.json", manifest)
	report_tmp = output / ".report.md.tmp"
	report_tmp.write_text(_report(manifest), encoding="utf-8")
	report_tmp.replace(output / "report.md")


def _preflight_failure(manifest: dict[str, Any], output: Path) -> int:
	_save(manifest, output)
	for error in manifest["errors"]:
		print(f"Preflight failed: {error}", file=sys.stderr, flush=True)
	print(f"Report: {output / 'report.md'}", file=sys.stderr, flush=True)
	return 2


def run(args: argparse.Namespace) -> int:
	started = time.monotonic()
	try:
		output = _new_output_dir()
	except OSError as exc:
		print(f"cannot create output directory: {exc}", file=sys.stderr)
		return 2
	cache = Path(args.default_asset_cache).expanduser().resolve()
	build_path = Path(args.build_manifest).expanduser().resolve()
	manifest: dict[str, Any] = {
		"schema_version": RESULT_SCHEMA_VERSION,
		"run_id": output.name,
		"created_utc": datetime.now(timezone.utc).isoformat(),
		"suite": args.suite,
		"status": "failed",
		"output_directory": str(output),
		"asset_cache": str(cache),
		"completed_workers": 0,
		"scheduled_workers": 0,
		"errors": [],
		"cases": [],
	}
	try:
		cases = suite_cases(args.suite, args.workload)
	except ValueError as exc:
		manifest["errors"].append(str(exc))
		return _preflight_failure(manifest, output)
	try:
		manifest["runtime_binary_sha256"] = _sha256(Path(args.executable))
	except OSError as exc:
		manifest["errors"].append(f"cannot fingerprint benchmark executable: {exc}")
		return _preflight_failure(manifest, output)
	engine_resources: dict[str, Any] = {}
	try:
		lighting_path = Path(args.environment_lighting).expanduser().resolve()
		engine_resources["environment_lighting_sha256"] = _sha256(lighting_path)
		engine_resources["environment_lighting_file"] = lighting_path.name
	except OSError as exc:
		manifest["errors"].append(f"cannot fingerprint environment lighting file: {exc}")
	try:
		shader_directory = Path(args.shader_directory).expanduser().resolve()
		shader_hashes = _shader_spirv_hashes(shader_directory)
		manifest["runtime_shader_spirv_sha256"] = shader_hashes
		manifest["shader_directory"] = str(shader_directory)
	except (OSError, ValueError) as exc:
		manifest["errors"].append(f"cannot fingerprint compiled shaders: {exc}")
	if manifest["errors"]:
		manifest["engine_resources"] = engine_resources
		return _preflight_failure(manifest, output)
	manifest["engine_resources"] = engine_resources
	try:
		build = _load_json(build_path)
		manifest["build"] = build
	except (OSError, ValueError, json.JSONDecodeError) as exc:
		manifest["errors"].append(f"cannot load build manifest {build_path}: {exc}")
		return _preflight_failure(manifest, output)
	build_errors = benchmark_build_errors(args.suite, build)
	if build_errors:
		manifest["errors"].extend(build_errors)
		return _preflight_failure(manifest, output)
	runtime_tool_errors = verify_runtime_tools(Path(__file__).resolve().parent, build.get("runtime_tools_sha256"))
	if runtime_tool_errors:
		manifest["errors"].extend(runtime_tool_errors)
		return _preflight_failure(manifest, output)
	if not cache.exists():
		print(f"Preparing missing asset cache: {cache} (downloads may take several minutes)", flush=True)
		try:
			benchmark_assets.prepare_assets(cache, source_directory=args.source_directory)
		except (benchmark_assets.AssetPreparationError, OSError) as exc:
			manifest["errors"].append(f"asset preparation failed: {exc}")
			return _preflight_failure(manifest, output)
	print(f"Verifying asset cache: {cache}", flush=True)
	assets, asset_errors = verify_asset_cache(cache)
	manifest["assets"] = assets
	if asset_errors:
		manifest["errors"].extend(asset_errors)
		manifest["errors"].append(f"prepare or repair assets with: stress_test prepare-assets")
		return _preflight_failure(manifest, output)
	if args.baseline:
		try:
			baseline_path = _resolve_baseline(args.baseline)
			baseline = _load_json(baseline_path)
		except (OSError, ValueError, json.JSONDecodeError) as exc:
			manifest["errors"].append(f"cannot load baseline: {exc}")
			return _preflight_failure(manifest, output)
	else:
		baseline = None

	default_warmup, default_measurement, default_repetitions = suite_defaults(args.suite)
	warmup = default_warmup if args.warmup is None else args.warmup
	measurement = default_measurement if args.measure is None else args.measure
	repetitions = default_repetitions if args.repetitions is None else args.repetitions
	if warmup < 0 or measurement <= 0 or repetitions < 1:
		manifest["errors"].append("warmup must be non-negative, measurement positive, and repetitions at least one")
		return _preflight_failure(manifest, output)
	schedule = make_schedule(args.suite, repetitions, workload=args.workload)
	protocol = {
		"suite": args.suite, "protocol_version": PROTOCOL_VERSION,
		"warmup_seconds": warmup, "measurement_seconds": measurement,
		"repetitions": repetitions, "deadline_tolerance_ns": DEADLINE_TOLERANCE_NS,
		"order_seed": ORDER_SEED, "schedule": schedule, "selected_workload": args.workload,
	}
	manifest["protocol"] = protocol
	manifest["scheduled_workers"] = len(schedule) * 2
	manifest["estimated_measurement_seconds"] = manifest["scheduled_workers"] * (warmup + measurement)
	manifest["cases"] = [
		{
			"case_id": case["case_id"], "scenario": case["scenario"], "count": case["count"], "view": case.get("view"),
			"repetitions_by_mode": {"throughput": [], "paced": []},
			"comparisons": {},
		}
		for case in cases
	]
	by_id = {case["case_id"]: case for case in manifest["cases"]}
	_save(manifest, output)
	print(f"Scheduled {manifest['scheduled_workers']} workers; measured time estimate {manifest['estimated_measurement_seconds'] / 60:.1f} minutes plus startup and collection.", flush=True)
	operational_failure = False
	for index, entry in enumerate(schedule, 1):
		for mode in ("throughput", "paced"):
			case_manifest = by_id[entry["case_id"]]
			worker_dir = output / "workers" / entry["case_id"] / mode / f"rep-{entry['repetition']:02d}"
			worker_dir.parent.mkdir(parents=True, exist_ok=True)
			print(f"[{manifest['completed_workers'] + 1}/{manifest['scheduled_workers']}] {entry['case_id']} {mode} repetition {entry['repetition']}", flush=True)
			worker, return_code, error = _run_worker(args.executable, entry, mode, entry["repetition"], warmup, measurement, cache, worker_dir)
			gpu_supported = worker.get("gpu_supported") is True
			compatibility = _case_compatibility(entry, mode, protocol, assets, build, worker, engine_resources)
			result = _case_result(worker, worker_dir, entry, mode, entry["repetition"], compatibility, gpu_supported)
			if error:
				result["error"] = error
			operational_failure |= error is not None or return_code != 0 or result["status"] == "failed"
			case_manifest["repetitions_by_mode"][mode].append(result)
			manifest["completed_workers"] += 1
			_save(manifest, output)
			if error == "interrupted":
				manifest["errors"].append("run interrupted; completed worker results preserved")
				operational_failure = True
				break
		if error == "interrupted":
			break
	if baseline is not None:
		_compare_run(manifest, baseline)
		manifest["baseline"] = str(baseline_path)
	failed_count = sum(
		rep["status"] in {"failed", "invalid"}
		for case in manifest["cases"] for repetitions_for_mode in case["repetitions_by_mode"].values() for rep in repetitions_for_mode
	)
	manifest["failed_or_invalid_workers"] = failed_count
	manifest["status"] = "failed" if operational_failure else "partial" if any(
		rep["status"] == "partial" for case in manifest["cases"] for reps in case["repetitions_by_mode"].values() for rep in reps
	) else "complete"
	manifest["wall_time_seconds"] = time.monotonic() - started
	_save(manifest, output)
	print(f"Wrote {output / 'report.md'}", flush=True)
	return 1 if operational_failure else 0


def _parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--default-asset-cache", type=Path, default=benchmark_assets.default_cache_path(), help=argparse.SUPPRESS)
	parser.add_argument("--executable", required=True, help=argparse.SUPPRESS)
	parser.add_argument("--build-manifest", required=True, help=argparse.SUPPRESS)
	parser.add_argument("--environment-lighting", required=True, help=argparse.SUPPRESS)
	parser.add_argument("--shader-directory", required=True, help=argparse.SUPPRESS)
	subparsers = parser.add_subparsers(dest="command", required=True)
	run_parser = subparsers.add_parser("run", help="run the routine benchmark (default) or smoke checks")
	run_parser.add_argument("--suite", choices=("standard", "smoke"), default="standard")
	run_parser.add_argument("--workload", choices=sorted({case["case_id"] for case in standard_cases(include_optional_views=True) + smoke_cases()}),
		help="run only this workload from the selected suite (both modes and configured repetitions)")
	run_parser.add_argument("--baseline")
	run_parser.add_argument("--source-directory", type=Path, help="use locally supplied pinned inputs when preparing a missing asset cache")
	run_parser.add_argument("--warmup", type=float)
	run_parser.add_argument("--measure", type=float)
	run_parser.add_argument("--repetitions", type=int)
	return parser


def main(argv: Sequence[str] | None = None) -> int:
	args = _parser().parse_args(argv)
	if args.command == "run":
		return run(args)
	return 2


if __name__ == "__main__":
	raise SystemExit(main())
