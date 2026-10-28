"""Fresh-process rendering fidelity coordinator and baseline lifecycle."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import signal
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Sequence

from PIL import UnidentifiedImageError

if __package__ in (None, ""):
	from fidelity_images import compare_pngs, decode_rgba, pixel_sha256
	from fidelity_report import write_report
else:
	from .fidelity_images import compare_pngs, decode_rgba, pixel_sha256
	from .fidelity_report import write_report


SCHEMA_VERSION = 1
WORKER_SCHEMA_VERSION = 1
REPETITIONS = 2
WORKER_TIMEOUT_SECONDS = 180
VIRTUAL_DISPLAY_FLAG = "--virtual-display-ready"
SOURCE_FIELDS = {"source_revision", "source_fingerprint", "dirty", "dirty_content_fingerprint"}
ACTIVE_RUN_OUTPUT: Path | None = None


def canonical_json(value: Any) -> bytes:
	return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def digest_json(value: Any) -> str:
	return hashlib.sha256(canonical_json(value)).hexdigest()


def sha256_file(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def _sha256_or_none(path: Path) -> str | None:
	try:
		return sha256_file(path)
	except OSError:
		return None


def atomic_json(path: Path, value: Any) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)
	temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
	try:
		with temporary.open("x", encoding="utf-8") as output:
			json.dump(value, output, indent=2, sort_keys=True)
			output.write("\n")
			output.flush()
			os.fsync(output.fileno())
		temporary.replace(path)
	finally:
		temporary.unlink(missing_ok=True)


def _read_json(path: Path) -> dict[str, Any]:
	value = json.loads(path.read_text(encoding="utf-8"))
	if not isinstance(value, dict):
		raise ValueError(f"expected a JSON object in {path}")
	return value


def _output_root() -> Path:
	base = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
	return base / "krisp-engine" / "rendering-fidelity"


def _new_run_dir(base: Path) -> Path:
	base.mkdir(parents=True, exist_ok=True)
	path = base / f"run-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}-{uuid.uuid4().hex[:10]}"
	path.mkdir()
	return path


def _virtual_display_command(xvfb_run: str, arguments: Sequence[str]) -> list[str]:
	return [xvfb_run, "-a", "-s", "-screen 0 640x480x24 -nolisten tcp",
		sys.executable, str(Path(__file__).resolve()), *arguments, VIRTUAL_DISPLAY_FLAG]


def _terminate_process_group(process: subprocess.Popen[Any], grace_seconds: float = 5) -> None:
	try:
		os.killpg(process.pid, signal.SIGTERM)
	except ProcessLookupError:
		pass
	try:
		process.wait(timeout=grace_seconds)
	except subprocess.TimeoutExpired:
		try:
			os.killpg(process.pid, signal.SIGKILL)
		except ProcessLookupError:
			pass
		process.wait()


def _suite_timeout_seconds(args: argparse.Namespace) -> int:
	try:
		selected = _selected_cases(args.case, _case_inventory(args.source_root))
	except (OSError, ValueError, json.JSONDecodeError):
		return 60
	return max(60, len(selected) * REPETITIONS * args.timeout + 60)


def _write_virtual_display_error(args: argparse.Namespace, message: str,
		stdout_tail: str = "", stderr_tail: str = "", timeout: bool = False) -> int:
	output = _new_run_dir(args.output_root or _output_root())
	manifest = {
		"schema_version": SCHEMA_VERSION, "suite": "rendering-fidelity", "status": "error",
		"profile_id": "unknown", "source_root": str(args.source_root.resolve()),
		"source_provenance": _source_provenance(args.source_root),
		"started_utc": datetime.now(timezone.utc).isoformat(),
		"completed_utc": datetime.now(timezone.utc).isoformat(),
		"repetitions_per_capture": REPETITIONS, "worker_timeout_seconds": args.timeout,
		"cases": [], "errors": [message],
		"virtual_display": {"backend": "xvfb", "stdout_tail": stdout_tail, "stderr_tail": stderr_tail},
	}
	if timeout:
		manifest["timeout_seconds"] = _suite_timeout_seconds(args)
	atomic_json(output / "run_manifest.json", manifest)
	print(message, file=sys.stderr)
	if stderr_tail:
		print(stderr_tail, file=sys.stderr, end="" if stderr_tail.endswith("\n") else "\n")
	print(f"Evidence: {output}")
	return 1


def _write_cancellation_error(args: argparse.Namespace, message: str) -> int:
	output = ACTIVE_RUN_OUTPUT or _new_run_dir(args.output_root or _output_root())
	manifest_path = output / "run_manifest.json"
	try:
		manifest = _read_json(manifest_path)
		manifest["status"] = "error"
		manifest.setdefault("errors", []).append(message)
		manifest["completed_utc"] = datetime.now(timezone.utc).isoformat()
	except (OSError, ValueError, json.JSONDecodeError):
		manifest = {
			"schema_version": SCHEMA_VERSION, "suite": "rendering-fidelity", "status": "error",
			"profile_id": "unknown", "source_root": str(args.source_root.resolve()),
			"source_provenance": _source_provenance(args.source_root),
			"started_utc": datetime.now(timezone.utc).isoformat(),
			"completed_utc": datetime.now(timezone.utc).isoformat(),
			"repetitions_per_capture": REPETITIONS, "worker_timeout_seconds": args.timeout,
			"cases": [], "errors": [message],
		}
	atomic_json(manifest_path, manifest)
	print(message, file=sys.stderr)
	print(f"Evidence: {output}")
	return 130


def _run_in_virtual_display(args: argparse.Namespace, arguments: Sequence[str]) -> int:
	missing = [name for name in ("xvfb-run", "Xvfb", "xauth") if shutil.which(name) is None]
	if missing:
		return _write_virtual_display_error(args,
			f"private virtual display is unavailable; install the required executable(s): {', '.join(missing)}")
	command = _virtual_display_command(shutil.which("xvfb-run") or "xvfb-run", arguments)
	with tempfile.TemporaryDirectory(prefix="krisp-fidelity-xvfb-") as temporary:
		stdout_path = Path(temporary) / "stdout.log"
		stderr_path = Path(temporary) / "stderr.log"
		environment = os.environ.copy()
		environment["KRISP_FIDELITY_VIRTUAL_DISPLAY"] = "1"
		environment.pop("WAYLAND_DISPLAY", None)
		process: subprocess.Popen[bytes] | None = None
		suite_timeout = _suite_timeout_seconds(args)
		timed_out = False
		try:
			with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
				process = subprocess.Popen(command, stdout=stdout, stderr=stderr, env=environment,
					start_new_session=True)
				try:
					return_code = process.wait(timeout=suite_timeout)
				except subprocess.TimeoutExpired:
					timed_out = True
					_terminate_process_group(process)
					return_code = process.returncode if process.returncode is not None else -signal.SIGKILL
		except KeyboardInterrupt:
			if process is not None and process.poll() is None:
				_terminate_process_group(process)
			return_code = 130
		except OSError as error:
			return _write_virtual_display_error(args, f"could not start xvfb-run: {error}")
		stdout_tail = _read_file_tail(stdout_path)
		stderr_tail = _read_file_tail(stderr_path)
		if timed_out:
			return _write_virtual_display_error(args,
				f"rendering-fidelity suite exceeded its {suite_timeout}-second total deadline; the private display process group was stopped",
				stdout_tail, stderr_tail, timeout=True)
		if stdout_tail:
			print(stdout_tail, end="" if stdout_tail.endswith("\n") else "\n")
		if stderr_tail:
			print(stderr_tail, file=sys.stderr, end="" if stderr_tail.endswith("\n") else "\n")
		if "Evidence:" not in stdout_tail:
			return _write_virtual_display_error(args,
				f"private virtual display failed before the suite produced evidence (exit {return_code}); verify xvfb-run, Xvfb, and xauth",
				stdout_tail, stderr_tail)
		return return_code


def _read_file_tail(path: Path, maximum: int = 65536) -> str:
	try:
		with path.open("rb") as source:
			source.seek(0, os.SEEK_END)
			length = source.tell()
			source.seek(max(0, length - maximum))
			return source.read(maximum).decode("utf-8", errors="replace")
	except OSError:
		return ""


def _case_definitions(source_root: Path) -> dict[str, dict[str, Any]]:
	path = source_root / "applications" / "rendering_fidelity" / "fixtures" / "case_definitions.json"
	definition = _read_json(path)
	if definition.get("schema_version") != 1 or not isinstance(definition.get("cases"), dict):
		raise ValueError(f"unsupported case definition schema in {path}")
	definitions = dict(definition["cases"])
	for reference_path in sorted((path.parent / "reference_cases").glob("*.json")):
		reference = _read_json(reference_path)
		case_id = reference.get("case_id")
		if case_id != reference_path.stem or case_id in definitions:
			raise ValueError(f"duplicate or invalid reference case identity in {reference_path}")
		definitions[case_id] = reference
	return definitions


def _case_inventory(source_root: Path) -> dict[str, tuple[str, ...]]:
	cases: dict[str, tuple[str, ...]] = {}
	for case_id, case in _case_definitions(source_root).items():
		captures = case.get("captures") if isinstance(case, dict) else None
		if not isinstance(captures, dict) or not captures or not all(isinstance(capture, str) for capture in captures):
			raise ValueError(f"case {case_id!r} must define capture IDs")
		cases[case_id] = tuple(captures)
	return cases


try:
	CASES = _case_inventory(Path(__file__).resolve().parents[2])
except (OSError, ValueError, json.JSONDecodeError):
	CASES: dict[str, tuple[str, ...]] = {}


def _selected_cases(selection: str | None, cases: dict[str, tuple[str, ...]] | None = None) -> list[tuple[str, str]]:
	cases = CASES if cases is None else cases
	if selection:
		if selection in cases:
			return [(selection, capture) for capture in cases[selection]]
		for case_id, captures in cases.items():
			if selection in captures:
				return [(case_id, selection)]
		raise ValueError(f"unknown case or capture ID: {selection}")
	return [(case_id, capture) for case_id, captures in cases.items() for capture in captures]


def _worker_command(executable: str, case_id: str, capture_id: str, output: Path,
		asset_cache: Path | None, reference_assets: Path | None = None) -> list[str]:
	command = [executable, "_fidelity-worker", "--case", case_id, "--capture", capture_id, "--output", str(output)]
	if asset_cache is not None:
		command.extend(("--asset-cache", str(asset_cache)))
	if reference_assets is not None:
		command.extend(("--reference-assets", str(reference_assets)))
	return command


def _run_worker(command: list[str], output: Path, timeout: int) -> dict[str, Any]:
	started = time.monotonic()
	stdout_path, stderr_path = output / "worker.stdout.log", output / "worker.stderr.log"
	try:
		with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
			process = subprocess.Popen(command, stdout=stdout, stderr=stderr, start_new_session=True)
			try:
				return_code = process.wait(timeout=timeout)
				status = "exited"
			except subprocess.TimeoutExpired:
				status = "timeout"
				_terminate_process_group(process, grace_seconds=2)
				return_code = None
			except KeyboardInterrupt:
				_terminate_process_group(process, grace_seconds=2)
				raise
		return {"status": status, "return_code": return_code,
			"stdout_log": str(stdout_path), "stderr_log": str(stderr_path),
			"stdout_tail": _read_tail(stdout_path), "stderr_tail": _read_tail(stderr_path),
			"duration_seconds": time.monotonic() - started,
			**({"timeout_seconds": timeout} if status == "timeout" else {})}
	except OSError as error:
		return {"status": "launch-error", "return_code": None, "stdout_log": str(stdout_path),
			"stderr_log": str(stderr_path), "stderr_tail": str(error),
			"duration_seconds": time.monotonic() - started}


def _read_tail(path: Path, limit: int = 16_384) -> str:
	try:
		with path.open("rb") as source:
			source.seek(max(0, path.stat().st_size - limit))
			return source.read().decode("utf-8", errors="replace")
	except OSError:
		return ""


def _worker_evidence(
	worker: dict[str, Any], output: Path, case_id: str, capture_id: str, source_root: Path,
) -> tuple[dict[str, Any] | None, Path | None, str | None]:
	manifest_path = output / f"{capture_id}.worker.json"
	image_path = output / f"{capture_id}.png"
	if worker["status"] != "exited" or worker.get("return_code") != 0:
		return None, image_path if image_path.is_file() else None, "worker-did-not-exit-successfully"
	try:
		manifest = _read_json(manifest_path)
		if manifest.get("schema_version") != WORKER_SCHEMA_VERSION:
			raise ValueError("unsupported worker manifest schema")
		if manifest.get("status") != "complete":
			raise ValueError(f"worker status is {manifest.get('status')!r}")
		if manifest.get("case_id") != case_id or manifest.get("capture_id") != capture_id:
			raise ValueError("worker case/capture identity does not match request")
		if not isinstance(manifest.get("frame_id"), int) or not isinstance(manifest.get("case_version"), int):
			raise ValueError("worker frame identity and case version are required")
		image = manifest.get("image")
		if not isinstance(image, dict) or image.get("file") != image_path.name:
			raise ValueError("worker image entry is missing or names an unexpected file")
		if image.get("pixel_format") != "RGBA8" or not image_path.is_file():
			raise ValueError("RGBA8 capture PNG is missing")
		size, pixels = decode_rgba(image_path)
		if list(size) != [image.get("width"), image.get("height")]:
			raise ValueError("PNG dimensions disagree with worker manifest")
		image_digest = sha256_file(image_path)
		if image.get("sha256") not in (None, image_digest):
			raise ValueError("PNG file digest disagrees with worker manifest")
		image["sha256"] = image_digest
		if not isinstance(manifest.get("environment"), dict) or not manifest["environment"] or not isinstance(manifest.get("effective_settings"), dict) or not manifest["effective_settings"]:
			raise ValueError("environment and effective_settings objects are required")
		fixture_files = manifest.get("fixture_files")
		if not isinstance(fixture_files, list) or not fixture_files or not all(isinstance(item, str) and item for item in fixture_files):
			raise ValueError("fixture_files must list the actual case and asset inputs")
		if len(fixture_files) != len(set(fixture_files)):
			raise ValueError("fixture_files contains duplicate inputs")
		fixture_hashes: dict[str, str] = {}
		for name in fixture_files:
			fixture_path = Path(name)
			if not fixture_path.is_absolute():
				fixture_path = source_root / fixture_path
			if not fixture_path.is_file():
				raise ValueError(f"declared fixture input is missing: {fixture_path}")
			fixture_hashes[name] = sha256_file(fixture_path)
		manifest["fixture_hashes"] = fixture_hashes
		manifest["_decoded_pixel_sha256"] = pixel_sha256(size, pixels)
		return manifest, image_path, None
	except (OSError, ValueError, KeyError, json.JSONDecodeError, UnidentifiedImageError) as error:
		return None, image_path if image_path.is_file() else None, f"invalid-worker-evidence: {error}"


def _compatibility(worker_manifest: dict[str, Any]) -> dict[str, Any]:
	return {
		"environment": {key: value for key, value in worker_manifest["environment"].items() if key not in SOURCE_FIELDS},
		"effective_settings": worker_manifest["effective_settings"],
		"case_version": worker_manifest.get("case_version"),
		"fixture_hashes": worker_manifest.get("fixture_hashes"),
	}


def _profile_fields(environment: dict[str, Any]) -> dict[str, Any]:
	return {key: value for key, value in environment.items() if key not in SOURCE_FIELDS}


def _load_baseline(baseline_root: Path, profile_id: str) -> tuple[dict[str, Any] | None, Path | None, str | None]:
	profile_dir = baseline_root / profile_id
	approval_path = profile_dir / "approved.json"
	if not approval_path.exists():
		return None, None, None
	try:
		approval = _read_json(approval_path)
		set_digest = approval.get("set_digest")
		if not isinstance(set_digest, str):
			raise ValueError("approved set pointer has no digest")
		manifest_path = profile_dir / "sets" / set_digest / "manifest.json"
		manifest = _read_json(manifest_path)
		if digest_json(manifest) != set_digest:
			raise ValueError("approved baseline manifest digest is corrupt")
		if manifest.get("profile_id") != profile_id:
			raise ValueError("baseline profile identity mismatch")
		return manifest, manifest_path.parent, None
	except (OSError, ValueError, json.JSONDecodeError) as error:
		return None, None, f"corrupt-baseline: {error}"


def _baseline_entry(manifest: dict[str, Any], case_id: str, capture_id: str) -> dict[str, Any] | None:
	for entry in manifest.get("captures", []):
		if entry.get("case_id") == case_id and entry.get("capture_id") == capture_id:
			return entry
	return None


def _compare_capture(
	case_id: str, capture_id: str, repetitions: list[dict[str, Any]], baseline: dict[str, Any] | None,
	baseline_dir: Path | None, baseline_error: str | None, profile_id: str,
	has_approved_profiles: bool = False,
) -> dict[str, Any]:
	valid = [run for run in repetitions if run.get("worker_manifest") is not None and run.get("image_path")]
	first_manifest = valid[0]["worker_manifest"] if valid else None
	image_path = Path(valid[0]["image_path"]) if valid else None
	reasons = [run["error"] for run in repetitions if run.get("error")]
	repeatability: dict[str, Any] | None = None
	if len(valid) >= 2:
		repeatability = compare_pngs(Path(valid[0]["image_path"]), Path(valid[1]["image_path"]))
	if len(valid) != REPETITIONS:
		status = "error"
		reasons.append(f"expected-{REPETITIONS}-valid-fresh-process-captures-got-{len(valid)}")
	elif repeatability is None or not repeatability["equal"]:
		status = "nondeterministic"
		reasons.append("fresh-process-captures-differ")
	elif first_manifest is None:
		status = "error"
		reasons.append("worker-manifest-unavailable")
	elif any(_compatibility(run["worker_manifest"]) != _compatibility(first_manifest) for run in valid[1:]):
		status = "nondeterministic"
		reasons.append("effective-environment-or-settings-differ-between-processes")
	elif baseline_error:
		status = "incompatible"
		reasons.append(baseline_error)
	elif baseline is None:
		status = "incompatible" if has_approved_profiles else "approval-required"
		reasons.append("environment-profile-has-no-approved-baseline" if has_approved_profiles else "no-approved-baseline-for-compatible-profile")
	else:
		entry = _baseline_entry(baseline, case_id, capture_id)
		if entry is None:
			status = "approval-required"
			reasons.append("approved-profile-does-not-contain-this-capture")
		else:
			relative = entry.get("image")
			base_image = baseline_dir / relative if baseline_dir and isinstance(relative, str) else None
			baseline_compatibility = entry.get("compatibility")
			if not base_image or not base_image.is_file():
				status = "incompatible"
				reasons.append("approved-baseline-image-missing")
			elif entry.get("file_sha256") != _sha256_or_none(base_image):
				status = "incompatible"
				reasons.append("approved-baseline-image-digest-is-corrupt")
			elif baseline_compatibility != _compatibility(first_manifest):
				status = "incompatible"
				reasons.append("environment-or-case-configuration-changed")
			else:
				comparison = compare_pngs(base_image, image_path)
				status = "pass" if comparison["equal"] else "mismatch"
				if not comparison["equal"]:
					reasons.append(comparison.get("reason", "pixel-mismatch"))
				return _capture_result(case_id, capture_id, repetitions, first_manifest, image_path,
					status, reasons, repeatability, comparison, base_image)
	if baseline_error and baseline_error not in reasons:
		reasons.append(baseline_error)
	elif baseline is None:
		missing_reason = "environment-profile-has-no-approved-baseline" if has_approved_profiles else "no-approved-baseline-for-compatible-profile"
		if missing_reason not in reasons:
			reasons.append(missing_reason)
	elif first_manifest is not None:
		entry = _baseline_entry(baseline, case_id, capture_id)
		if entry is None:
			missing_reason = "approved-profile-does-not-contain-this-capture"
			if missing_reason not in reasons:
				reasons.append(missing_reason)
		else:
			base_image = baseline_dir / entry["image"] if baseline_dir and isinstance(entry.get("image"), str) else None
			if not base_image or not base_image.is_file():
				baseline_reason = "approved-baseline-image-missing"
			elif entry.get("file_sha256") != _sha256_or_none(base_image):
				baseline_reason = "approved-baseline-image-digest-is-corrupt"
			elif entry.get("compatibility") != _compatibility(first_manifest):
				baseline_reason = "environment-or-case-configuration-changed"
			else:
				baseline_comparison = compare_pngs(base_image, Path(valid[0]["image_path"]))
				baseline_reason = None if baseline_comparison["equal"] else baseline_comparison.get("reason")
			if baseline_reason and baseline_reason not in reasons:
				reasons.append(baseline_reason)
	return _capture_result(case_id, capture_id, repetitions, first_manifest, image_path,
		status, reasons, repeatability, None, None)


def _capture_result(
	case_id: str, capture_id: str, repetitions: list[dict[str, Any]], manifest: dict[str, Any] | None,
	image_path: Path | None, status: str, reasons: list[str], repeatability: dict[str, Any] | None,
	comparison: dict[str, Any] | None, baseline_path: Path | None,
) -> dict[str, Any]:
	return {
		"case_id": case_id, "capture_id": capture_id, "status": status, "reasons": reasons,
		"image_path": str(image_path) if image_path else None,
		"worker_manifest_path": str(Path(repetitions[0]["worker_manifest_path"])) if repetitions and repetitions[0].get("worker_manifest_path") else None,
		"baseline_path": str(baseline_path) if baseline_path else None,
		"repeatability": repeatability, "comparison": comparison,
		"environment": manifest.get("environment") if manifest else None,
		"effective_settings": manifest.get("effective_settings") if manifest else None,
		"compatibility": _compatibility(manifest) if manifest else None,
		"case_version": manifest.get("case_version") if manifest else None,
		"fixture_hashes": manifest.get("fixture_hashes") if manifest else None,
		"repetitions": repetitions,
	}


def _aggregate_status(captures: list[dict[str, Any]]) -> str:
	statuses = {capture["status"] for capture in captures}
	for status in ("error", "nondeterministic", "incompatible", "mismatch", "approval-required"):
		if status in statuses:
			return status
	return "pass"


def _ensure_source_root(source_root: Path) -> None:
	resolved = str(source_root.resolve())
	if resolved not in sys.path:
		sys.path.insert(0, resolved)


def _source_provenance(source_root: Path) -> dict[str, Any]:
	provenance: dict[str, Any] = {"revision": None, "dirty": None, "dirty_content_fingerprint": None}
	try:
		_ensure_source_root(source_root)
		from applications.benchmarks.performance import build_provenance
		provenance["revision"], provenance["dirty"] = build_provenance.source_revision(source_root)
		provenance["dirty_content_fingerprint"] = build_provenance.source_fingerprint(source_root)
	except (ImportError, OSError, ValueError):
		provenance["platform"] = platform.platform()
	return provenance


def _asset_tools() -> Any:
	if __package__ in (None, ""):
		import fidelity_assets
	else:
		from . import fidelity_assets
	return fidelity_assets


def _reference_tools() -> Any:
	if __package__ in (None, ""):
		import fidelity_references
	else:
		from . import fidelity_references
	return fidelity_references


def _reference_assets_path(args: argparse.Namespace) -> Path:
	return getattr(args, "reference_assets", None) or args.source_root / "build" / "fidelity-assets"


def _reference_evidence(assets: Path, references: Path, source_root: Path) -> dict[str, Any]:
	manifest, errors = _reference_tools().verify_references(assets, references,
		cases_dir=source_root / "applications/rendering_fidelity/fixtures/reference_cases")
	if errors:
		raise ValueError("; ".join(errors))
	return {"directory": str(references.resolve()), "assets": str(assets.resolve()),
		"manifest_sha256": sha256_file(references / "manifest.json"), "manifest": manifest}


def _run(args: argparse.Namespace) -> int:
	global ACTIVE_RUN_OUTPUT
	started_utc = datetime.now(timezone.utc)
	output = _new_run_dir(args.output_root or _output_root())
	ACTIVE_RUN_OUTPUT = output
	_ensure_source_root(args.source_root)
	try:
		cases = _case_inventory(args.source_root)
	except (OSError, ValueError, json.JSONDecodeError) as error:
		message = f"required rendering-fidelity case definitions are unavailable or invalid: {error}; restore {args.source_root / 'applications/rendering_fidelity/fixtures/case_definitions.json'}"
		manifest = {
			"schema_version": SCHEMA_VERSION, "suite": "rendering-fidelity", "status": "error",
			"profile_id": "unknown", "source_root": str(args.source_root.resolve()),
			"source_provenance": _source_provenance(args.source_root),
			"started_utc": started_utc.isoformat(), "completed_utc": datetime.now(timezone.utc).isoformat(),
			"repetitions_per_capture": REPETITIONS, "worker_timeout_seconds": args.timeout,
			"cases": [], "errors": [message],
		}
		atomic_json(output / "run_manifest.json", manifest)
		print(message, file=sys.stderr)
		print(f"Evidence: {output}")
		return 1
	try:
		selected = _selected_cases(args.case, cases)
	except ValueError as error:
		print(str(error), file=sys.stderr)
		return 2
	baseline_root = args.baselines or args.source_root / "test" / "rendering_fidelity" / "baselines"
	asset_cache = None
	asset_preflight_error = None
	reference_assets = _reference_assets_path(args)
	reference_cases = {
		path.stem for path in (args.source_root / "applications/rendering_fidelity/fixtures/reference_cases").glob("*.json")
	}
	reference_asset_error = None
	if any(case_id in reference_cases for case_id, _ in selected):
		try:
			_, errors = _asset_tools().verify_cache(reference_assets)
			if errors:
				reference_asset_error = "; ".join(errors)
		except (ImportError, OSError, ValueError) as error:
			reference_asset_error = str(error)
	if any(case_id == "textured-environment" for case_id, _ in selected):
		try:
			from applications.benchmarks.performance import benchmark_assets
			asset_cache = args.asset_cache or benchmark_assets.default_cache_path()
			lock_hash = benchmark_assets.sha256_file(benchmark_assets.LOCK_PATH)
			_, asset_errors = benchmark_assets.verify_prepared_cache(asset_cache, lock_hash, benchmark_assets.LOCK_PATH)
			if asset_errors:
				asset_preflight_error = "; ".join(asset_errors)
		except (ImportError, OSError, ValueError) as error:
			asset_preflight_error = str(error)
	first_manifests: list[dict[str, Any]] = []
	case_runs: dict[tuple[str, str], list[dict[str, Any]]] = {}
	for case_id, capture_id in selected:
		runs: list[dict[str, Any]] = []
		for repetition in range(1, REPETITIONS + 1):
			worker_dir = output / "workers" / case_id / capture_id / f"process-{repetition}"
			worker_dir.mkdir(parents=True, exist_ok=True)
			command = _worker_command(args.executable, case_id, capture_id, worker_dir,
				asset_cache if case_id == "textured-environment" else None,
				reference_assets if case_id in reference_cases else None)
			preflight_error = reference_asset_error if case_id in reference_cases else asset_preflight_error if case_id == "textured-environment" else None
			if preflight_error:
				worker = {"status": "preflight-error", "return_code": None,
					"stderr_tail": preflight_error, "duration_seconds": 0.0}
				prepare_command = "./run benchmarks -- fidelity prepare-assets" if case_id in reference_cases else "./run benchmarks -- performance prepare-assets"
				error = f"prepared assets are unavailable or invalid: {preflight_error}; prepare them with `{prepare_command}`"
				manifest, image_path = None, None
			else:
				worker = _run_worker(command, worker_dir, args.timeout)
				manifest, image_path, error = _worker_evidence(worker, worker_dir, case_id, capture_id, args.source_root)
			record = {"repetition": repetition, "command": command, "worker": worker,
				"worker_manifest": manifest, "image_path": str(image_path) if image_path else None,
				"worker_manifest_path": str(worker_dir / f"{capture_id}.worker.json"),
				"error": error}
			runs.append(record)
			if manifest is not None:
				first_manifests.append(manifest)
		case_runs[(case_id, capture_id)] = runs

	environments = [	_profile_fields(manifest["environment"]) for manifest in first_manifests]
	if environments:
		profile_ids = {digest_json(environment) for environment in environments}
		profile_id = next(iter(profile_ids)) if len(profile_ids) == 1 else digest_json(sorted(environments, key=canonical_json))
	else:
		profile_id = "unknown"
	baseline, baseline_dir, baseline_error = _load_baseline(baseline_root, profile_id)
	try:
		baseline_set_digest = _current_set_digest(baseline_root, profile_id)
	except ValueError as error:
		baseline_set_digest = None
		baseline_error = baseline_error or str(error)
	has_approved_profiles = any(baseline_root.glob("*/approved.json"))
	results: list[dict[str, Any]] = []
	reference_evidence = None
	reference_errors: list[str] = []
	if getattr(args, "references", None):
		try:
			reference_evidence = _reference_evidence(reference_assets, args.references, args.source_root)
		except (OSError, ValueError, RuntimeError) as error:
			reference_errors.append(str(error))
	case_results: dict[str, list[dict[str, Any]]] = {}
	for (case_id, capture_id), repetitions in case_runs.items():
		capture_result = _compare_capture(case_id, capture_id, repetitions, baseline,
			baseline_dir, baseline_error, profile_id, has_approved_profiles)
		if reference_evidence:
			capture_result["external_references"] = [
				{"renderer": item["renderer"], "image_path": str(args.references.resolve() / item["image"]),
				 "metadata": item, "limitations": item.get("limitations", [])}
				for item in reference_evidence["manifest"]["references"]
				if item["case_id"] == case_id and item["capture_id"] == capture_id]
		results.append(capture_result)
		case_results.setdefault(case_id, []).append(capture_result)
	cases = [{"case_id": case_id, "captures": captures} for case_id, captures in case_results.items()]
	manifest = {
		"schema_version": SCHEMA_VERSION, "suite": "rendering-fidelity", "status": _aggregate_status(results),
		"profile_id": profile_id, "source_root": str(args.source_root.resolve()),
		"approved_baseline_set_digest": baseline_set_digest,
		"source_provenance": _source_provenance(args.source_root),
		"started_utc": started_utc.isoformat(), "completed_utc": datetime.now(timezone.utc).isoformat(),
		"repetitions_per_capture": REPETITIONS,
		"worker_timeout_seconds": args.timeout, "baseline_root": str(baseline_root.resolve()),
		"cases": cases,
	}
	if reference_evidence:
		manifest["reference_evidence"] = reference_evidence
	if reference_errors:
		manifest["reference_errors"] = reference_errors
	manifest_path = output / "run_manifest.json"
	atomic_json(manifest_path, manifest)
	if args.report:
		try:
			write_report(manifest, output / "report.html")
		except (OSError, ValueError) as error:
			manifest["report_error"] = str(error)
			atomic_json(manifest_path, manifest)
			print(f"Report generation failed: {error}", file=sys.stderr)
	print(f"Rendering fidelity: {manifest['status']} ({len(results)} captures)")
	print(f"Evidence: {output}")
	if args.report and "report_error" not in manifest:
		print(f"Report: {output / 'report.html'}")
	return 0 if manifest["status"] == "pass" and "report_error" not in manifest and not reference_errors else 1


def _capture_candidates(run_path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
	run = _read_json(run_path / "run_manifest.json")
	if run.get("reference_errors"):
		raise ValueError("independent reference evidence is invalid; regenerate the report before baseline review")
	if run.get("reference_evidence"):
		evidence = run["reference_evidence"]
		current = _reference_evidence(Path(evidence["assets"]), Path(evidence["directory"]), Path(run["source_root"]))
		if current != evidence:
			raise ValueError("independent reference evidence changed since the run completed")
	profile_id = run.get("profile_id")
	if run.get("schema_version") != SCHEMA_VERSION or not isinstance(profile_id, str):
		raise ValueError("unsupported or incomplete run evidence")
	candidates: list[dict[str, Any]] = []
	profile_environments: set[str] = set()
	seen: set[tuple[str, str]] = set()
	for case in run.get("cases", []):
		for capture in case.get("captures", []):
			key = (case.get("case_id"), capture.get("capture_id"))
			if key in seen:
				raise ValueError("run evidence contains duplicate case/capture IDs")
			seen.add(key)
			status = capture.get("status")
			reasons = set(capture.get("reasons", []))
			reviewable_incompatibilities = {
				"environment-profile-has-no-approved-baseline",
				"environment-or-case-configuration-changed",
			}
			approved_incompatibility = (
				status == "incompatible"
				and bool(reasons & reviewable_incompatibilities)
				and reasons <= reviewable_incompatibilities
			)
			if status not in {"pass", "mismatch", "approval-required"} and not approved_incompatibility:
				raise ValueError(f"{case.get('case_id')}/{capture.get('capture_id')} is not eligible for baseline review: {', '.join(sorted(reasons)) or status}")
			repetitions = capture.get("repetitions", [])
			if len(repetitions) != REPETITIONS or any(not item.get("worker_manifest") or not item.get("image_path") for item in repetitions):
				raise ValueError("candidate does not contain complete captures from every fresh process")
			verified: list[dict[str, Any]] = []
			for repetition in repetitions:
				image = Path(repetition["image_path"])
				if not image.is_file():
					raise ValueError(f"candidate capture is missing: {image}")
				worker, verified_image, error = _worker_evidence(
					{"status": "exited", "return_code": 0}, image.parent, case["case_id"], capture["capture_id"],
					Path(run.get("source_root", run_path.parent)))
				if error or worker is None or verified_image != image:
					raise ValueError(f"candidate worker evidence is invalid: {error or image}")
				recorded = repetition["worker_manifest"]
				if ({name: value for name, value in worker.items() if name != "_decoded_pixel_sha256"}
						!= {name: value for name, value in recorded.items() if name != "_decoded_pixel_sha256"}):
					raise ValueError("worker manifest changed since the run completed")
				if worker["_decoded_pixel_sha256"] != recorded.get("_decoded_pixel_sha256"):
					raise ValueError("decoded-pixel digest changed since the run completed")
				verified.append({"image": image, "worker": worker})
			if not compare_pngs(verified[0]["image"], verified[1]["image"])["equal"]:
				raise ValueError("nondeterministic evidence cannot be proposed")
			if any(_compatibility(item["worker"]) != _compatibility(verified[0]["worker"]) for item in verified[1:]):
				raise ValueError("worker compatibility fields changed between repetitions")
			profile_environments.add(digest_json(_profile_fields(verified[0]["worker"]["environment"])))
			image = verified[0]["image"]
			if Path(capture["image_path"]) != image:
				raise ValueError("recorded candidate is not the first verified repetition")
			candidates.append({
				"case_id": case["case_id"], "capture_id": capture["capture_id"],
				"source_image": str(image.resolve()), "file_sha256": sha256_file(image),
				"pixel_sha256": verified[0]["worker"]["_decoded_pixel_sha256"],
				"compatibility": capture["compatibility"], "effective_settings": capture["effective_settings"],
				"case_version": capture["case_version"], "fixture_hashes": capture["fixture_hashes"],
				"repeatability": capture["repeatability"],
			})
	if not candidates:
		raise ValueError("run contains no baseline candidates")
	if len(profile_environments) != 1 or next(iter(profile_environments)) != profile_id:
		raise ValueError("candidate captures do not share the declared compatibility profile")
	return run, candidates


def _current_set_digest(baseline_root: Path, profile_id: str) -> str | None:
	path = baseline_root / profile_id / "approved.json"
	try:
		path.stat()
	except FileNotFoundError:
		return None
	except OSError as error:
		raise ValueError(f"cannot inspect approved baseline pointer {path}: {error}") from error
	try:
		approval = _read_json(path)
		set_digest = approval.get("set_digest")
		if not isinstance(set_digest, str) or not set_digest:
			raise ValueError("approved set pointer has no digest")
		return set_digest
	except (OSError, KeyError, ValueError, json.JSONDecodeError) as error:
		raise ValueError(f"corrupt approved baseline pointer {path}: {error}") from error


def _make_proposal(args: argparse.Namespace) -> int:
	try:
		run, candidates = _capture_candidates(args.run_dir)
		baseline_root = args.baselines or args.source_root / "test" / "rendering_fidelity" / "baselines"
		if _current_set_digest(baseline_root, run["profile_id"]) != run.get("approved_baseline_set_digest"):
			raise ValueError("approved baseline changed since this run; rerun comparisons before proposing")
		baseline, baseline_dir, baseline_error = _load_baseline(baseline_root, run["profile_id"])
		if baseline_error:
			raise ValueError(baseline_error)
		existing_baselines: dict[str, Any] = {}
		for candidate in candidates:
			key = f"{candidate['case_id']}/{candidate['capture_id']}"
			entry = _baseline_entry(baseline, candidate["case_id"], candidate["capture_id"]) if baseline else None
			if entry:
				existing_baselines[key] = {
					"set_digest": run["approved_baseline_set_digest"],
					"file_sha256": entry["file_sha256"], "pixel_sha256": entry["pixel_sha256"],
				}
			else:
				existing_baselines[key] = {"set_digest": run["approved_baseline_set_digest"], "image": None}
		proposal: dict[str, Any] = {
			"schema_version": 1, "kind": "rendering-fidelity-baseline-proposal",
			"run_manifest_sha256": sha256_file(args.run_dir / "run_manifest.json"),
			"run_directory": str(args.run_dir.resolve()), "profile_id": run["profile_id"],
			"baseline_root": str(baseline_root.resolve()),
			"expected_current_set_digest": run.get("approved_baseline_set_digest"),
			"existing_baseline_digests": existing_baselines, "candidates": candidates,
		}
		proposal["manifest_digest"] = digest_json(proposal)
		path = args.proposal or args.run_dir / "baseline-proposal.json"
		atomic_json(path, proposal)
		print(f"Proposal: {path}")
		print(f"Exact approval digest: {proposal['manifest_digest']}")
		return 0
	except (OSError, KeyError, ValueError, json.JSONDecodeError) as error:
		print(f"Cannot propose baselines: {error}", file=sys.stderr)
		return 2


def _promote(args: argparse.Namespace) -> int:
	if not sys.stdin.isatty():
		print("Promotion requires an interactive terminal to approve this exact manifest digest.", file=sys.stderr)
		return 2
	try:
		proposal = _read_json(args.proposal)
		expected_digest = proposal.pop("manifest_digest", None)
		if expected_digest != digest_json(proposal):
			raise ValueError("proposal manifest digest is invalid")
		proposal["manifest_digest"] = expected_digest
		print(f"Promote these {len(proposal['candidates'])} captures to profile {proposal['profile_id']}?")
		print(f"Manifest digest: {expected_digest}")
		if input("Type the full manifest digest to approve: ").strip() != expected_digest:
			print("Approval did not match; no baselines were changed.", file=sys.stderr)
			return 2
		return _publish_proposal(proposal)
	except (OSError, KeyError, ValueError, json.JSONDecodeError) as error:
		print(f"Cannot promote baselines: {error}", file=sys.stderr)
		return 2


def _publish_proposal(proposal: dict[str, Any]) -> int:
	proposed_digest = proposal.get("manifest_digest")
	unsigned = {key: value for key, value in proposal.items() if key != "manifest_digest"}
	if proposed_digest != digest_json(unsigned):
		raise ValueError("proposal manifest digest is invalid")
	run_path = Path(proposal["run_directory"])
	if sha256_file(run_path / "run_manifest.json") != proposal["run_manifest_sha256"]:
		raise ValueError("run evidence changed after proposal")
	baseline_root = Path(proposal["baseline_root"])
	profile_id = proposal["profile_id"]
	if _current_set_digest(baseline_root, profile_id) != proposal.get("expected_current_set_digest"):
		raise ValueError("approved baseline changed after proposal; prepare a fresh proposal")
	run, current_candidates = _capture_candidates(run_path)
	by_key = {(item["case_id"], item["capture_id"]): item for item in current_candidates}
	if run["profile_id"] != profile_id:
		raise ValueError("candidate environment no longer matches the proposed profile")
	profile = _profile_fields(run["cases"][0]["captures"][0]["environment"])
	if digest_json(profile) != profile_id:
		raise ValueError("candidate profile digest does not match its environment")
	parent = baseline_root / profile_id
	sets = parent / "sets"
	sets.mkdir(parents=True, exist_ok=True)
	existing, existing_dir, existing_error = _load_baseline(baseline_root, profile_id)
	if existing_error:
		raise ValueError(existing_error)
	stage = sets / f".stage-{uuid.uuid4().hex}"
	stage.mkdir()
	try:
		entries_by_key: dict[tuple[str, str], dict[str, Any]] = {}
		if existing and existing_dir:
			for previous in existing.get("captures", []):
				previous_image = existing_dir / previous["image"]
				if not previous_image.is_file() or previous.get("file_sha256") != sha256_file(previous_image):
					raise ValueError("existing approved baseline is corrupt")
				name = previous_image.name
				shutil.copyfile(previous_image, stage / name)
				entries_by_key[(previous["case_id"], previous["capture_id"])] = {**previous, "image": name}
		for candidate in proposal["candidates"]:
			key = (candidate["case_id"], candidate["capture_id"])
			current = by_key.get(key)
			if current is None or current != candidate:
				raise ValueError(f"candidate changed since proposal: {key[0]}/{key[1]}")
			image = Path(candidate["source_image"])
			if not image.is_file() or sha256_file(image) != candidate["file_sha256"]:
				raise ValueError(f"candidate file digest changed: {image}")
			name = f"{key[0]}--{key[1]}.png"
			shutil.copyfile(image, stage / name)
			entries_by_key[key] = {key_name: candidate[key_name] for key_name in ("case_id", "capture_id", "compatibility", "effective_settings", "case_version", "fixture_hashes")} | {
				"image": name, "file_sha256": candidate["file_sha256"], "pixel_sha256": candidate["pixel_sha256"],
			}
		baseline_manifest = {"schema_version": 1, "profile_id": profile_id, "environment": profile,
			"captures": list(entries_by_key.values()),
			"approval": {"proposal_digest": proposal["manifest_digest"], "approved_utc": datetime.now(timezone.utc).isoformat()}}
		set_digest = digest_json(baseline_manifest)
		atomic_json(stage / "manifest.json", baseline_manifest)
		published = sets / set_digest
		if published.exists():
			shutil.rmtree(stage)
		else:
			stage.replace(published)
		if _current_set_digest(baseline_root, profile_id) != proposal.get("expected_current_set_digest"):
			raise ValueError("approved baseline changed during promotion; new set is retained but was not activated")
		atomic_json(parent / "approved.json", {"set_digest": set_digest, "manifest_sha256": set_digest})
		print(f"Promoted approved baseline set {set_digest} for profile {profile_id}")
		return 0
	finally:
		if stage.exists():
			shutil.rmtree(stage)


def _parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--executable", help=argparse.SUPPRESS)
	parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2], help=argparse.SUPPRESS)
	parser.add_argument("--output-root", type=Path, help=argparse.SUPPRESS)
	parser.add_argument("--baselines", type=Path, help=argparse.SUPPRESS)
	parser.add_argument("--asset-cache", type=Path, help=argparse.SUPPRESS)
	parser.add_argument("--reference-assets", type=Path, help=argparse.SUPPRESS)
	subparsers = parser.add_subparsers(dest="command", required=True)
	run_parser = subparsers.add_parser("run", help="run exact rendering comparisons")
	run_parser.add_argument("--case", help="select one case ID or capture ID")
	run_parser.add_argument("--report", action="store_true", help="write a self-contained offline HTML report")
	run_parser.add_argument("--references", type=Path, help="include verified independent renderer references in the report")
	run_parser.add_argument("--timeout", type=int, default=WORKER_TIMEOUT_SECONDS, help=argparse.SUPPRESS)
	run_parser.add_argument(VIRTUAL_DISPLAY_FLAG, action="store_true", help=argparse.SUPPRESS)
	proposal_parser = subparsers.add_parser("propose", help="prepare an exact-digest baseline proposal from run evidence")
	proposal_parser.add_argument("run_dir", type=Path)
	proposal_parser.add_argument("--proposal", type=Path)
	promote_parser = subparsers.add_parser("promote", help="interactively approve and publish one exact proposal")
	promote_parser.add_argument("proposal", type=Path)
	prepare_parser = subparsers.add_parser("prepare-assets", help="download and verify pinned external reference models")
	prepare_parser.add_argument("--cache", type=Path, help=argparse.SUPPRESS)
	reference_parser = subparsers.add_parser("render-references", help="generate independent renderer review images")
	reference_parser.add_argument("--assets", type=Path)
	reference_parser.add_argument("--output", type=Path)
	reference_parser.add_argument("--case")
	reference_parser.add_argument("--renderer", choices=("khronos", "cycles"), action="append")
	return parser


def main(argv: Sequence[str] | None = None) -> int:
	arguments = list(sys.argv[1:] if argv is None else argv)
	args = _parser().parse_args(arguments)
	if args.command == "prepare-assets":
		try:
			cache = args.cache or _reference_assets_path(args)
			_asset_tools().prepare_assets(cache)
			print(f"Prepared reference assets: {cache}")
			return 0
		except (OSError, ValueError, RuntimeError) as error:
			print(f"Could not prepare reference assets: {error}", file=sys.stderr)
			return 1
	if args.command == "render-references":
		try:
			assets = args.assets or _reference_assets_path(args)
			_, errors = _asset_tools().verify_cache(assets)
			if errors:
				raise ValueError("; ".join(errors))
			output = args.output or _new_run_dir(_output_root() / "references")
			return _reference_tools().main([
				"render", "--assets", str(assets),
				"--output", str(output),
				"--cases-dir", str(args.source_root / "applications/rendering_fidelity/fixtures/reference_cases"),
				"--khronos-renderer", str(args.source_root / "applications/rendering_fidelity/reference_web"),
				*(["--case", args.case] if args.case else []),
				*[part for renderer in args.renderer or ("khronos", "cycles") for part in ("--renderer", renderer)],
			])
		except (OSError, ValueError, RuntimeError) as error:
			print(f"Could not render independent references: {error}", file=sys.stderr)
			return 1
	if args.command == "run":
		if not args.executable:
			print("the launcher must provide --executable", file=sys.stderr)
			return 2
		if args.timeout <= 0:
			print("--timeout must be positive", file=sys.stderr)
			return 2
		if not args.virtual_display_ready:
			previous_sigterm = signal.getsignal(signal.SIGTERM)
			def cancel_wrapper_on_sigterm(_signum: int, _frame: Any) -> None:
				raise KeyboardInterrupt("received SIGTERM")
			signal.signal(signal.SIGTERM, cancel_wrapper_on_sigterm)
			try:
				return _run_in_virtual_display(args, arguments)
			finally:
				signal.signal(signal.SIGTERM, previous_sigterm)
		previous_sigterm = signal.getsignal(signal.SIGTERM)
		def cancel_on_sigterm(_signum: int, _frame: Any) -> None:
			raise KeyboardInterrupt("received SIGTERM")
		signal.signal(signal.SIGTERM, cancel_on_sigterm)
		try:
			return _run(args)
		except KeyboardInterrupt as error:
			return _write_cancellation_error(args, f"rendering-fidelity suite was cancelled: {error}")
		finally:
			signal.signal(signal.SIGTERM, previous_sigterm)
	if args.command == "propose":
		return _make_proposal(args)
	if args.command == "promote":
		return _promote(args)
	return 2


if __name__ == "__main__":
	raise SystemExit(main())
