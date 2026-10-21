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
	from .benchmark_statistics import compare_compatibility, compare_repetitions, summarize_repetitions, summarize_samples
	from .build_provenance import machine_identity
except ImportError:  # direct script execution
	import benchmark_assets
	from benchmark_statistics import compare_compatibility, compare_repetitions, summarize_repetitions, summarize_samples
	from build_provenance import machine_identity


RESULT_SCHEMA_VERSION = 1
SCENARIO_VERSION = 1
PROTOCOL_VERSION = 2
ASSET_LOADING_SCHEMA_VERSION = 2
ASSET_LOADING_BREAKDOWN_SCHEMA_VERSION = 1
PROFILE = benchmark_assets.PROFILE
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
		{"scenario": "moving-transforms", "count": 500, "view": None},
		{"scenario": "point-shadows", "count": 500, "view": None},
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
	if suite == "asset-loading":
		cases = [case for case in smoke_cases() if case["scenario"] in {"textured-objects", "textured-environment"}]
	else:
		cases = standard_cases(include_optional_views=workload is not None) if suite == "standard" else smoke_cases()
	if workload is None:
		return cases
	selected = [case for case in cases if case["case_id"] == workload]
	if not selected:
		raise ValueError(f"Workload {workload!r} is not in suite {suite!r}; choose: " + ", ".join(case["case_id"] for case in cases))
	return selected


def suite_defaults(suite: str) -> tuple[float, float, int]:
	return {"standard": (2.0, 10.0, 2), "smoke": (0.2, 0.5, 1), "asset-loading": (0.0, 0.0, 2)}[suite]


def case_modes(suite: str, case: Mapping[str, Any]) -> tuple[str, ...]:
	if suite == "asset-loading":
		return ("asset-loading",)
	if suite == "standard" and case["scenario"] != "textured-environment":
		return ("throughput",)
	return ("throughput", "paced")


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
			"instrumentation_version": 2 if protocol["suite"] == "asset-loading" else 1,
			"warmup_seconds": protocol["warmup_seconds"], "measurement_seconds": protocol["measurement_seconds"],
			"repetitions": protocol["repetitions"], "deadline_tolerance_ns": protocol["deadline_tolerance_ns"],
			**({"asset_loading_schema_version": ASSET_LOADING_SCHEMA_VERSION,
				"cache_policy": "fresh-process-os-cache-uncontrolled-preverified",
				"readiness_boundary": "first-scene-presentation-observed"} if protocol["suite"] == "asset-loading" else {}),
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
		worker_timeout = SHUTDOWN_MARGIN_SECONDS if mode == "asset-loading" else warmup + measurement + DRAIN_TIMEOUT_SECONDS + SHUTDOWN_MARGIN_SECONDS
		worker_deadline = time.monotonic() + worker_timeout
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
			if mode == "asset-loading":
				result["summary"] = _asset_loading_summary(worker)
				result["status"] = "invalid" if worker.get("errors") or worker.get("lost_records", 0) else worker.get("status")
				if result["status"] not in {"complete", "partial"}:
					result["status"] = "failed"
			else:
				result["summary"] = _summary_for_worker(worker_dir, worker, gpu_supported)
				result["status"] = _status_for_worker(worker, result["summary"])
		else:
			result["status"] = worker["status"]
	except (OSError, ValueError, KeyError, TypeError) as exc:
		result["status"] = "failed"
		result["error"] = str(exc)
	return result


_ASSET_LOADING_DURATIONS = (
	"engine_initialization_ns", "asset_registration_ns", "asset_instantiation_ns",
	"scene_setup_ns", "presentation_ready_ns", "asset_to_ready_ns", "engine_to_ready_ns",
)
_ASSET_LOADING_TIMESTAMPS = (
	"engine_begin_ns", "engine_end_ns", "asset_begin_ns", "registration_end_ns",
	"instantiation_end_ns", "scene_end_ns", "ready_ns",
)
_ASSET_LOADING_INSTANTIATION_BREAKDOWN = (
	"dependency_read_ns", "image_decode_ns", "document_processing_ns", "mesh_conversion_ns",
	"material_construction_ns", "ecs_adoption_ns", "other_ns",
)


def _asset_loading_summary(worker: Mapping[str, Any]) -> dict[str, Any]:
	loading = worker.get("asset_loading")
	if not isinstance(loading, dict):
		raise ValueError("worker completion is missing asset_loading timings")
	if type(loading.get("schema_version")) is not int or loading["schema_version"] != ASSET_LOADING_SCHEMA_VERSION:
		raise ValueError(f"asset_loading schema_version must be {ASSET_LOADING_SCHEMA_VERSION}")
	if loading.get("units") != "nanoseconds":
		raise ValueError("asset_loading units must be nanoseconds")
	if loading.get("cache_policy") != "fresh-process-os-cache-uncontrolled-preverified":
		raise ValueError("asset_loading cache_policy is missing or unsupported")
	if loading.get("readiness_boundary") != "first-scene-presentation-observed":
		raise ValueError("asset_loading readiness_boundary is missing or unsupported")
	durations = loading.get("durations_ns")
	timestamps = loading.get("timestamps_ns")
	if not isinstance(durations, dict) or not isinstance(timestamps, dict):
		raise ValueError("asset_loading must include durations_ns and timestamps_ns objects")
	for key in _ASSET_LOADING_DURATIONS:
		value = durations.get(key)
		if type(value) is not int or value < 0:
			raise ValueError(f"asset_loading duration {key} must be a non-negative integer")
	for key in _ASSET_LOADING_TIMESTAMPS:
		value = timestamps.get(key)
		if type(value) is not int or value < 0:
			raise ValueError(f"asset_loading timestamp {key} must be a non-negative integer")
	values = [timestamps[key] for key in _ASSET_LOADING_TIMESTAMPS]
	if any(right < left for left, right in zip(values, values[1:])):
		raise ValueError("asset_loading timestamps must be ordered")
	if durations["asset_to_ready_ns"] <= 0 or durations["engine_to_ready_ns"] <= 0:
		raise ValueError("asset_loading aggregate durations must be positive")
	phase_sum = sum(durations[key] for key in _ASSET_LOADING_DURATIONS[1:5])
	if durations["asset_to_ready_ns"] != phase_sum:
		raise ValueError("asset_loading asset_to_ready_ns must equal registration through presentation-ready durations")
	phase_deltas = (
		timestamps["engine_end_ns"] - timestamps["engine_begin_ns"],
		timestamps["registration_end_ns"] - timestamps["asset_begin_ns"],
		timestamps["instantiation_end_ns"] - timestamps["registration_end_ns"],
		timestamps["scene_end_ns"] - timestamps["instantiation_end_ns"],
		timestamps["ready_ns"] - timestamps["scene_end_ns"],
	)
	for key, actual in zip(_ASSET_LOADING_DURATIONS[:5], phase_deltas):
		if durations[key] != actual:
			raise ValueError(f"asset_loading duration {key} does not match timestamps_ns")
	if durations["asset_to_ready_ns"] != timestamps["ready_ns"] - timestamps["asset_begin_ns"]:
		raise ValueError("asset_loading asset_to_ready_ns does not match timestamps_ns")
	if durations["engine_to_ready_ns"] != timestamps["ready_ns"] - timestamps["engine_begin_ns"]:
		raise ValueError("asset_loading engine_to_ready_ns does not match timestamps_ns")
	breakdown = loading.get("instantiation_breakdown")
	if not isinstance(breakdown, dict):
		raise ValueError("asset_loading is missing instantiation_breakdown")
	if type(breakdown.get("schema_version")) is not int or breakdown["schema_version"] != ASSET_LOADING_BREAKDOWN_SCHEMA_VERSION:
		raise ValueError("asset_loading instantiation_breakdown schema_version must be 1")
	if breakdown.get("units") != "nanoseconds":
		raise ValueError("asset_loading instantiation_breakdown units must be nanoseconds")
	breakdown_durations = breakdown.get("durations_ns")
	if not isinstance(breakdown_durations, dict) or set(breakdown_durations) != set(_ASSET_LOADING_INSTANTIATION_BREAKDOWN):
		raise ValueError("asset_loading instantiation_breakdown durations_ns must contain the fixed phase keys")
	for key in _ASSET_LOADING_INSTANTIATION_BREAKDOWN:
		value = breakdown_durations[key]
		if type(value) is not int or value < 0:
			raise ValueError(f"asset_loading instantiation_breakdown duration {key} must be a non-negative integer")
	if sum(breakdown_durations.values()) != durations["asset_instantiation_ns"]:
		raise ValueError("asset_loading instantiation_breakdown durations must sum to asset_instantiation_ns")
	return {
		"asset_loading": {"durations_ns": {key: durations[key] for key in _ASSET_LOADING_DURATIONS},
			"timestamps_ns": {key: timestamps[key] for key in _ASSET_LOADING_TIMESTAMPS},
			"instantiation_breakdown": {"schema_version": ASSET_LOADING_BREAKDOWN_SCHEMA_VERSION,
				"units": "nanoseconds", "durations_ns": {key: breakdown_durations[key]
					for key in _ASSET_LOADING_INSTANTIATION_BREAKDOWN}}},
	}


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
	for name in _ASSET_LOADING_DURATIONS:
		values = []
		for repetition in repetitions:
			if repetition.get("status") not in {"complete", "partial"}:
				continue
			value = repetition.get("summary", {}).get("asset_loading", {}).get("durations_ns", {}).get(name)
			if type(value) is int:
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
		metrics = (
			("simulation_ticks_per_second", False),
			("successful_submissions_per_second", False),
			("simulation_active_median_ns", True),
			("graphics_active_median_ns", True),
			("gpu_median_ns", True),
		)
		if manifest.get("protocol", {}).get("suite") == "asset-loading":
			metrics = tuple((metric, True) for metric in _ASSET_LOADING_DURATIONS)
		for metric, lower_is_better in metrics:
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
	asset_loading_suite = manifest.get("protocol", {}).get("suite") == "asset-loading"
	if asset_loading_suite:
		lines.extend(["", "Asset loading durations are in milliseconds. Readiness means the first scene presentation was observed; OS cache state is uncontrolled.", "", "## Asset loading", ""])
	else:
		lines.extend(["", "Each duration is in nanoseconds. Present-call cadence is measured on the CPU and does not represent display scanout.", "", "## Cases", ""])
	for case in manifest.get("cases", []):
		lines.append(f"### {case['case_id']}")
		lines.append("")
		for mode, repetitions in case.get("repetitions_by_mode", {}).items():
			lines.append(f"**{mode}**")
			if asset_loading_suite:
				for repetition in repetitions:
					status = repetition.get("status", "failed")
					loading = repetition.get("summary", {}).get("asset_loading", {})
					if not loading:
						lines.append(f"- repetition {repetition.get('repetition')}: **{status}** — {repetition.get('error') or '; '.join(repetition.get('worker', {}).get('errors', []))}")
						continue
					lines.append(f"- repetition {repetition['repetition']}: **{status}**")
					for metric_name in _ASSET_LOADING_DURATIONS:
						lines.append(f"  - {metric_name}: {loading['durations_ns'][metric_name] / 1_000_000:.3f} ms")
					breakdown = loading["instantiation_breakdown"]["durations_ns"]
					instantiation_ns = loading["durations_ns"]["asset_instantiation_ns"]
					lines.append("  - Instantiation breakdown (exclusive spans):")
					for metric_name in _ASSET_LOADING_INSTANTIATION_BREAKDOWN:
						phase_name = metric_name.removesuffix("_ns").replace("_", " ")
						percentage = f"{breakdown[metric_name] / instantiation_ns * 100:.1f}%" if instantiation_ns else "n/a"
						lines.append(
							f"    - {phase_name}: {breakdown[metric_name] / 1_000_000:.3f} ms "
							f"({percentage} of asset instantiation)"
						)
				for metric_name in _ASSET_LOADING_DURATIONS:
					values = _primary_metrics(repetitions)[metric_name]
					distribution = summarize_repetitions(values)
					if distribution["available"]:
						low, high = distribution["range"]
						lines.append(f"  - {metric_name} across repetitions: median {distribution['median'] / 1_000_000:.3f} ms; range {low / 1_000_000:.3f}–{high / 1_000_000:.3f} ms; MAD {distribution['mad'] / 1_000_000:.3f} ms")
					else:
						lines.append(f"  - {metric_name} across repetitions: unavailable ({distribution['reason']})")
				breakdown_repetitions = [
					repetition.get("summary", {}).get("asset_loading", {})
					for repetition in repetitions
					if repetition.get("status") in {"complete", "partial"}
				]
				for metric_name in _ASSET_LOADING_INSTANTIATION_BREAKDOWN:
					phase_name = metric_name.removesuffix("_ns").replace("_", " ")
					durations = [value["instantiation_breakdown"]["durations_ns"][metric_name] for value in breakdown_repetitions]
					percentages = [
						value["instantiation_breakdown"]["durations_ns"][metric_name]
						/ value["durations_ns"]["asset_instantiation_ns"] * 100
						for value in breakdown_repetitions if value["durations_ns"]["asset_instantiation_ns"]
					]
					distribution = summarize_repetitions(durations)
					percentage_distribution = summarize_repetitions(percentages)
					if distribution["available"]:
						percentage = f"{percentage_distribution['median']:.1f}%" if percentage_distribution["available"] else "n/a"
						lines.append(
							f"  - {phase_name} across repetitions: median {distribution['median'] / 1_000_000:.3f} ms "
							f"({percentage} of asset instantiation)"
						)
					else:
						lines.append(f"  - {phase_name} across repetitions: unavailable ({distribution['reason']})")
				continue
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
	if asset_loading_suite:
		lines.extend([
			"## Interpretation", "",
			"Each repetition starts a fresh worker process. Asset files are verified before the run, while operating-system cache state is uncontrolled. Repetition ranges and baseline bootstrap intervals describe observed variation.", "",
		])
	else:
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
	if args.suite == "asset-loading" and (args.warmup is not None or args.measure is not None):
		print("--suite asset-loading fixes warmup and measurement at zero; do not pass --warmup or --measure", file=sys.stderr)
		return 2
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
	try:
		if benchmark_assets.needs_preparation(cache):
			print(f"Preparing missing or outdated asset cache: {cache} (downloads may take several minutes)", flush=True)
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
	if warmup < 0 or (measurement <= 0 and args.suite != "asset-loading") or repetitions < 1:
		manifest["errors"].append("warmup must be non-negative, measurement positive, and repetitions at least one")
		return _preflight_failure(manifest, output)
	schedule = make_schedule(args.suite, repetitions, workload=args.workload)
	protocol = {
		"suite": args.suite, "protocol_version": PROTOCOL_VERSION,
		"warmup_seconds": warmup, "measurement_seconds": measurement,
		"repetitions": repetitions, "deadline_tolerance_ns": DEADLINE_TOLERANCE_NS,
		"order_seed": ORDER_SEED, "schedule": schedule, "selected_workload": args.workload,
	}
	if args.suite == "asset-loading":
		protocol.update({
			"asset_loading_schema_version": ASSET_LOADING_SCHEMA_VERSION,
			"cache_policy": "fresh-process-os-cache-uncontrolled-preverified",
			"readiness_boundary": "first-scene-presentation-observed",
		})
	manifest["protocol"] = protocol
	manifest["scheduled_workers"] = sum(len(case_modes(args.suite, entry)) for entry in schedule)
	manifest["estimated_measurement_seconds"] = manifest["scheduled_workers"] * (warmup + measurement)
	manifest["cases"] = [
		{
			"case_id": case["case_id"], "scenario": case["scenario"], "count": case["count"], "view": case.get("view"),
			"repetitions_by_mode": {mode: [] for mode in case_modes(args.suite, case)},
			"comparisons": {},
		}
		for case in cases
	]
	by_id = {case["case_id"]: case for case in manifest["cases"]}
	_save(manifest, output)
	if args.suite == "asset-loading":
		print(f"Scheduled {manifest['scheduled_workers']} loading workers; duration depends on asset loading, with no warm-up or steady-state measurement.", flush=True)
	else:
		print(f"Scheduled {manifest['scheduled_workers']} workers; measured time estimate {manifest['estimated_measurement_seconds'] / 60:.1f} minutes plus startup and collection.", flush=True)
	operational_failure = False
	for index, entry in enumerate(schedule, 1):
		for mode in case_modes(args.suite, entry):
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
	run_parser.add_argument("--suite", choices=("standard", "smoke", "asset-loading"), default="standard")
	run_parser.add_argument("--workload", choices=sorted({case["case_id"] for case in standard_cases(include_optional_views=True) + smoke_cases()}),
		help="run only this workload from the selected suite and configured repetitions")
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
