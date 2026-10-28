import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from PIL import Image

from applications.rendering_fidelity import fidelity_images as images
from applications.rendering_fidelity import fidelity_runner as runner


def write_png(path, pixel=(10, 20, 30, 255), size=(2, 2), compress_level=6):
	image = Image.new("RGBA", size, pixel)
	image.save(path, format="PNG", compress_level=compress_level)


def worker_manifest(image_path, case_id="fixture", capture_id="view", environment=None, settings=None):
	return {
		"schema_version": 1, "status": "complete", "case_id": case_id, "capture_id": capture_id,
		"frame_id": 42, "case_version": 1, "fixture_hashes": {"fixture.json": "fixture-sha"},
		"fixture_files": ["fixture.json"],
		"image": {"file": image_path.name, "width": 2, "height": 2, "pixel_format": "RGBA8",
			"sha256": runner.sha256_file(image_path)},
		"environment": environment or {"gpu": "GPU A", "driver": "1", "build_class": "debug",
			"source_revision": "old", "dirty_content_fingerprint": "old-content"},
		"effective_settings": settings or {"extent": [2, 2], "exposure": 1.0},
	}


def make_run_evidence(root, capture_ids=("view",), status="approval-required", reason="no baseline",
		environment=None, fixture_text="fixture input"):
	(root / "fixture.json").write_text(fixture_text)
	environment = environment or {"gpu": "GPU A", "driver": "1", "build_class": "debug"}
	profile_id = runner.digest_json(environment)
	captures = []
	for capture_id in capture_ids:
		repetitions = []
		for repetition in range(2):
			worker_dir = root / "workers" / capture_id / str(repetition)
			worker_dir.mkdir(parents=True, exist_ok=True)
			image_path = worker_dir / f"{capture_id}.png"
			write_png(image_path, pixel=(10, 20, 30 + repetition * 0, 255))
			worker = worker_manifest(image_path, capture_id=capture_id, environment=environment)
			worker["fixture_hashes"] = {"fixture.json": runner.sha256_file(root / "fixture.json")}
			(worker_dir / f"{capture_id}.worker.json").write_text(json.dumps(worker))
			worker["_decoded_pixel_sha256"] = images.pixel_sha256(images.decode_rgba(image_path)[0], images.decode_rgba(image_path)[1])
			repetitions.append({"repetition": repetition + 1, "worker_manifest": worker, "image_path": str(image_path)})
		captures.append({
			"case_id": "fixture", "capture_id": capture_id, "status": status, "reasons": [reason],
			"image_path": repetitions[0]["image_path"], "compatibility": runner._compatibility(repetitions[0]["worker_manifest"]),
			"environment": environment,
			"effective_settings": repetitions[0]["worker_manifest"]["effective_settings"], "case_version": 1,
			"fixture_hashes": repetitions[0]["worker_manifest"]["fixture_hashes"], "repeatability": {"equal": True},
			"repetitions": repetitions,
		})
	manifest = {"schema_version": runner.SCHEMA_VERSION, "profile_id": profile_id, "source_root": str(root.resolve()),
		"approved_baseline_set_digest": None,
		"cases": [{"case_id": "fixture", "captures": captures}]}
	runner.atomic_json(root / "run_manifest.json", manifest)
	return manifest


def write_case_definitions(root, cases):
	path = root / "applications" / "rendering_fidelity" / "fixtures" / "case_definitions.json"
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_text(json.dumps({"schema_version": 1, "cases": {
		case_id: {"version": 1, "captures": {capture_id: {} for capture_id in capture_ids}}
		for case_id, capture_ids in cases.items()
	}}))


class RenderingFidelityImageTests(unittest.TestCase):
	def test_png_encoding_does_not_affect_decoded_equality(self):
		with tempfile.TemporaryDirectory() as temporary:
			first, second = Path(temporary) / "a.png", Path(temporary) / "b.png"
			write_png(first, compress_level=0)
			write_png(second, compress_level=9)
			self.assertEqual(images.compare_pngs(first, second)["equal"], True)

	def test_one_channel_and_alpha_changes_are_exact_and_report_bounds(self):
		with tempfile.TemporaryDirectory() as temporary:
			first, second = Path(temporary) / "a.png", Path(temporary) / "b.png"
			write_png(first)
			write_png(second)
			with Image.open(second) as image:
				image.putpixel((1, 0), (10, 20, 31, 255))
				image.putpixel((0, 1), (10, 20, 30, 254))
				image.save(second)
			comparison = images.compare_pngs(first, second)
			self.assertFalse(comparison["equal"])
			self.assertEqual(comparison["changed_pixels"], 2)
			self.assertEqual(comparison["changed_channels"], 2)
			self.assertEqual(comparison["bounds"], [0, 0, 1, 1])

	def test_dimension_change_is_not_resized_or_normalized(self):
		with tempfile.TemporaryDirectory() as temporary:
			first, second = Path(temporary) / "a.png", Path(temporary) / "b.png"
			write_png(first, size=(2, 2))
			write_png(second, size=(1, 2))
			comparison = images.compare_pngs(first, second)
			self.assertFalse(comparison["equal"])
			self.assertEqual(comparison["reason"], "dimensions-differ")

	def test_amplified_diagnostic_shows_alpha_only_changes(self):
		with tempfile.TemporaryDirectory() as temporary:
			first, second = Path(temporary) / "a.png", Path(temporary) / "b.png"
			write_png(first)
			write_png(second, pixel=(10, 20, 30, 254))
			payload = images.diagnostic_difference(first, second)
			self.assertIsNotNone(payload)
			with Image.open(io.BytesIO(payload)) as difference:
				self.assertEqual(difference.mode, "RGB")
				self.assertNotEqual(difference.getpixel((0, 0)), (0, 0, 0))


class RenderingFidelityCoordinatorTests(unittest.TestCase):
	def test_virtual_display_command_uses_private_screen_and_hidden_reentry(self):
		ambient_display = runner.os.environ.get("DISPLAY")
		command = runner._virtual_display_command("/usr/bin/xvfb-run", [
			"--executable", "engine", "--source-root", "/source", "run", "--case", "grid"])
		self.assertEqual(command[:4], ["/usr/bin/xvfb-run", "-a", "-s", "-screen 0 640x480x24 -nolisten tcp"])
		self.assertIn("--case", command)
		self.assertEqual(command[-1], runner.VIRTUAL_DISPLAY_FLAG)
		self.assertNotIn(ambient_display, command)
		self.assertEqual(runner.os.environ.get("DISPLAY"), ambient_display)

	def test_missing_virtual_display_dependency_creates_nonpassing_evidence(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			args = SimpleNamespace(source_root=Path(__file__).resolve().parents[1],
				output_root=root / "runs", timeout=10)
			with patch.object(runner.shutil, "which", return_value=None), \
				redirect_stdout(io.StringIO()) as stdout, redirect_stderr(io.StringIO()) as stderr:
				self.assertEqual(runner._run_in_virtual_display(args, ["run"]), 1)
			run_path = next((root / "runs").iterdir())
			manifest = runner._read_json(run_path / "run_manifest.json")
			self.assertEqual(manifest["status"], "error")
			self.assertIn("xvfb-run, Xvfb, xauth", manifest["errors"][0])
			self.assertIn("Evidence:", stdout.getvalue())
			self.assertIn("private virtual display is unavailable", stderr.getvalue())

	def test_default_inventory_matches_versioned_case_definitions_without_duplicates(self):
		root = Path(__file__).resolve().parents[1]
		definition = json.loads((root / "applications/rendering_fidelity/fixtures/case_definitions.json").read_text())
		expected = {case_id: tuple(case["captures"]) for case_id, case in definition["cases"].items()}
		for path in (root / "applications/rendering_fidelity/fixtures/reference_cases").glob("*.json"):
			case = json.loads(path.read_text())
			self.assertNotIn(case["case_id"], expected)
			expected[case["case_id"]] = tuple(case["captures"])
		self.assertEqual(runner.CASES, expected)
		selected = runner._selected_cases(None)
		self.assertEqual(len(selected), len(set(selected)))
		self.assertEqual(set(selected), {(case_id, capture_id) for case_id, captures in expected.items() for capture_id in captures})

	def test_reference_changes_invalidate_baseline_review(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			run = make_run_evidence(root)
			evidence = {"assets": str(root / "assets"), "directory": str(root / "references"), "manifest_sha256": "old"}
			run["reference_evidence"] = evidence
			runner.atomic_json(root / "run_manifest.json", run)
			with patch.object(runner, "_reference_evidence", return_value={**evidence, "manifest_sha256": "new"}):
				with self.assertRaisesRegex(ValueError, "reference evidence changed"):
					runner._capture_candidates(root)

	def test_invalid_optional_references_block_baseline_review(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			run = make_run_evidence(root)
			run["reference_errors"] = ["stale reference"]
			runner.atomic_json(root / "run_manifest.json", run)
			with self.assertRaisesRegex(ValueError, "reference evidence is invalid"):
				runner._capture_candidates(root)

	def test_missing_reference_assets_fail_before_starting_workers(self):
		self.assertEqual(runner._aggregate_status([{"status": "error"}, {"status": "incompatible"}]), "error")
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			write_case_definitions(root, {})
			case_dir = root / "applications/rendering_fidelity/fixtures/reference_cases"
			case_dir.mkdir()
			(case_dir / "reference-fixture.json").write_text(json.dumps({
				"case_id": "reference-fixture", "version": 1, "captures": {"default": {}}}))
			args = SimpleNamespace(executable="must-not-run", source_root=root, output_root=root / "runs",
				baselines=root / "baselines", asset_cache=None, case=None, timeout=2, report=False)
			tools = SimpleNamespace(verify_cache=lambda cache: ({}, ["missing pinned asset"]))
			with patch.object(runner, "_asset_tools", return_value=tools), \
				patch.object(runner, "_run_worker") as worker, redirect_stdout(io.StringIO()):
				self.assertEqual(runner._run(args), 1)
				worker.assert_not_called()
			run = runner._read_json(next((root / "runs").iterdir()) / "run_manifest.json")
			self.assertEqual(run["status"], "error")
			self.assertIn("prepare-assets", run["cases"][0]["captures"][0]["repetitions"][0]["error"])

	def test_timeout_and_missing_completion_are_recorded_without_losing_other_cases(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			output_root = root / "runs"
			baseline_root = root / "baselines"
			(root / "fixture.json").write_text("fixture input")
			original_cases = runner.CASES
			try:
				runner.CASES = {"good": ("view",), "timeout": ("view",), "malformed": ("view",)}
				write_case_definitions(root, runner.CASES)
				args = SimpleNamespace(executable="fake-engine", source_root=root, output_root=output_root,
					baselines=baseline_root, asset_cache=None, case=None, timeout=2, report=True)

				def fake_worker(command, worker_dir, timeout):
					case_id = command[command.index("--case") + 1]
					capture_id = command[command.index("--capture") + 1]
					if case_id == "timeout":
						return {"status": "timeout", "return_code": None, "stdout_tail": "timed out",
							"stderr_tail": "deadline", "timeout_seconds": timeout}
					if case_id == "malformed":
						return {"status": "exited", "return_code": 0, "stdout_tail": "", "stderr_tail": ""}
					image_path = worker_dir / f"{capture_id}.png"
					write_png(image_path)
					manifest = worker_manifest(image_path, case_id, capture_id)
					(worker_dir / f"{capture_id}.worker.json").write_text(json.dumps(manifest))
					return {"status": "exited", "return_code": 0, "stdout_tail": "ok", "stderr_tail": ""}

				with patch.object(runner, "_run_worker", side_effect=fake_worker), \
					patch.object(runner, "_source_provenance", return_value={"revision": "x"}), \
					patch.object(runner, "write_report", side_effect=OSError("disk full")), \
					redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
					self.assertEqual(runner._run(args), 1)
			finally:
				runner.CASES = original_cases
			run_path = next(output_root.iterdir())
			manifest = runner._read_json(run_path / "run_manifest.json")
			by_case = {case["case_id"]: case["captures"][0] for case in manifest["cases"]}
			self.assertEqual(by_case["good"]["status"], "approval-required")
			self.assertEqual(by_case["timeout"]["status"], "error")
			self.assertEqual(by_case["malformed"]["status"], "error")
			self.assertEqual(by_case["timeout"]["repetitions"][0]["worker"]["status"], "timeout")
			self.assertIsNotNone(by_case["good"]["repetitions"][0]["worker_manifest"])
			self.assertIn("disk full", manifest["report_error"])
			self.assertEqual(manifest["status"], "error")
			self.assertFalse(baseline_root.exists(), "ordinary runs must not create or change approved baselines")

	def test_missing_required_case_manifest_is_error_instead_of_fallback_inventory(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			output_root = root / "runs"
			args = SimpleNamespace(executable="must-not-run", source_root=root, output_root=output_root,
				baselines=root / "baselines", asset_cache=None, case=None, timeout=2, report=False)
			with patch.object(runner, "CASES", {"embedded-fallback": ("view",)}), \
				patch.object(runner, "_run_worker", side_effect=AssertionError("fallback worker must not run")), \
				redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
				self.assertEqual(runner._run(args), 1)
			run_path = next(output_root.iterdir())
			manifest = runner._read_json(run_path / "run_manifest.json")
			self.assertEqual(manifest["status"], "error")
			self.assertIn("required rendering-fidelity case definitions", manifest["errors"][0])

	def test_corrupt_approved_pointer_is_retained_as_nonpassing_evidence(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			write_case_definitions(root, {"fixture": ("view",)})
			(root / "fixture.json").write_text("fixture input")
			baseline_root = root / "baselines"
			pointer = baseline_root / "unknown" / "approved.json"
			pointer.parent.mkdir(parents=True)
			pointer.write_text("not-json")
			args = SimpleNamespace(executable="fixture", source_root=root, output_root=root / "runs",
				baselines=baseline_root, asset_cache=None, case=None, timeout=1, report=False)
			with patch.object(runner, "_run_worker", return_value={"status": "timeout", "return_code": None}), \
				patch.object(runner, "_source_provenance", return_value={"revision": "test"}), \
				redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
				self.assertEqual(runner._run(args), 1)
			run_path = next((root / "runs").iterdir())
			manifest = runner._read_json(run_path / "run_manifest.json")
			self.assertEqual(manifest["status"], "error")
			self.assertTrue(any("corrupt-baseline" in reason for reason in manifest["cases"][0]["captures"][0]["reasons"]))

	def test_real_worker_deadline_terminates_the_process_group(self):
		with tempfile.TemporaryDirectory() as temporary:
			worker = runner._run_worker([sys.executable, "-c", "import time; time.sleep(5)"], Path(temporary), 0.1)
			self.assertEqual(worker["status"], "timeout")
			self.assertEqual(worker["timeout_seconds"], 0.1)
			self.assertTrue(Path(worker["stdout_log"]).is_file())

	def test_worker_cancellation_terminates_and_reaps_detached_process_group(self):
		class InterruptedWorker:
			pid = 12345
			def __init__(self):
				self.wait_count = 0
			def wait(self, timeout=None):
				self.wait_count += 1
				if self.wait_count == 1:
					raise KeyboardInterrupt("cancel")
				return 0

		with tempfile.TemporaryDirectory() as temporary:
			process = InterruptedWorker()
			with patch.object(runner.subprocess, "Popen", return_value=process), \
				patch.object(runner.os, "killpg") as killpg, self.assertRaises(KeyboardInterrupt):
				runner._run_worker(["worker"], Path(temporary), 10)
			killpg.assert_called_once_with(process.pid, runner.signal.SIGTERM)
			self.assertEqual(process.wait_count, 2)

	def test_repeatability_is_checked_before_baseline_match_and_source_changes_are_ignored(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			paths = [root / "first.png", root / "second.png"]
			for path in paths:
				write_png(path)
			manifest_a = worker_manifest(paths[0])
			manifest_b = worker_manifest(paths[1], environment={"gpu": "GPU A", "driver": "1",
				"build_class": "debug", "source_revision": "new", "dirty_content_fingerprint": "new-content"})
			runs = [{"worker_manifest": manifest_a, "image_path": str(paths[0])},
				{"worker_manifest": manifest_b, "image_path": str(paths[1])}]
			compatibility = runner._compatibility(manifest_a)
			baseline = {"captures": [{"case_id": "fixture", "capture_id": "view", "image": "base.png",
				"compatibility": compatibility, "file_sha256": runner.sha256_file(paths[0])}]}
			(root / "base.png").write_bytes(paths[0].read_bytes())
			result = runner._compare_capture("fixture", "view", runs, baseline, root, None, "profile")
			self.assertEqual(result["status"], "pass")
			with Image.open(paths[1]) as image:
				image.putpixel((0, 0), (11, 20, 30, 255))
				image.save(paths[1])
			manifest_b = worker_manifest(paths[1], environment=manifest_a["environment"])
			runs[1]["worker_manifest"] = manifest_b
			result = runner._compare_capture("fixture", "view", runs, baseline, root, None, "profile")
			self.assertEqual(result["status"], "nondeterministic")

	def test_changed_device_and_corrupt_baseline_are_incompatible(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			paths = [root / "a.png", root / "b.png", root / "base.png"]
			for path in paths:
				write_png(path)
			manifest = worker_manifest(paths[0])
			runs = [{"worker_manifest": manifest, "image_path": str(paths[0])}, {"worker_manifest": manifest, "image_path": str(paths[1])}]
			baseline_compatibility = runner._compatibility(manifest)
			baseline_compatibility["environment"] = {"gpu": "GPU B", "driver": "1", "build_class": "debug"}
			baseline = {"captures": [{"case_id": "fixture", "capture_id": "view", "image": "base.png",
				"compatibility": baseline_compatibility, "file_sha256": runner.sha256_file(paths[2])}]}
			result = runner._compare_capture("fixture", "view", runs, baseline, root, None, "profile")
			self.assertEqual(result["status"], "incompatible")
			paths[2].write_bytes(b"corrupt")
			result = runner._compare_capture("fixture", "view", [
				{"worker_manifest": manifest, "image_path": str(paths[0])}, {"worker_manifest": manifest, "image_path": str(paths[1])}],
				baseline, root, None, "profile")
			self.assertEqual(result["status"], "incompatible")
			with Image.open(paths[1]) as image:
				image.putpixel((0, 0), (11, 20, 30, 255))
				image.save(paths[1])
			result = runner._compare_capture("fixture", "view", [
				{"worker_manifest": manifest, "image_path": str(paths[0])}, {"worker_manifest": manifest, "image_path": str(paths[1])}],
				baseline, root, None, "profile")
			self.assertEqual(result["status"], "nondeterministic")
			self.assertTrue(any("differ" in reason for reason in result["reasons"]))
			self.assertTrue(any("corrupt" in reason for reason in result["reasons"]))

	def test_all_selected_capture_passes_and_report_is_self_contained(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			image_path = root / "capture.png"
			write_png(image_path)
			second = root / "second.png"
			write_png(second, pixel=(10, 20, 31, 255))
			results = {"status": "nondeterministic", "cases": [{"case_id": "fixture", "captures": [{
				"capture_id": "view", "status": "approval-required", "reasons": ["missing baseline"],
				"image_path": str(image_path), "baseline_path": None, "environment": {"gpu": "GPU"},
				"effective_settings": {"exposure": 1}, "repeatability": {"equal": False, "changed_pixels": 4, "bounds": [0, 0, 1, 1]},
				"repetitions": [{"image_path": str(image_path)}, {"image_path": str(second)}],
				"comparison": None,
			}]}]}
			from applications.rendering_fidelity.fidelity_report import write_report
			report = root / "report.html"
			write_report(results, report)
			body = report.read_text()
			self.assertIn("Image unavailable", body)
			self.assertIn("Amplified difference (diagnostic only)", body)
			self.assertIn("Repeatability evidence (diagnostic only)", body)
			self.assertIn("data:image/png;base64,", body)
			self.assertEqual(runner._aggregate_status([{"status": "pass"}, {"status": "pass"}]), "pass")

	def test_baseline_compatibility_checks_device_driver_build_assets_and_settings(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			first, second, baseline_path = (root / name for name in ("first.png", "second.png", "baseline.png"))
			for path in (first, second, baseline_path):
				write_png(path)
			manifest = worker_manifest(first)
			runs = [{"worker_manifest": manifest, "image_path": str(first)}, {"worker_manifest": manifest, "image_path": str(second)}]
			base_compatibility = runner._compatibility(manifest)
			changes = [
				("environment", "gpu", "Other GPU"), ("environment", "driver", "other driver"),
				("environment", "build_class", "release"), ("effective_settings", "extent", [640, 480]),
				("fixture_hashes", "fixture.json", "changed fixture"),
			]
			for section, field, value in changes:
				compatibility = json.loads(json.dumps(base_compatibility))
				compatibility[section][field] = value
				baseline = {"captures": [{"case_id": "fixture", "capture_id": "view", "image": "baseline.png",
					"compatibility": compatibility, "file_sha256": runner.sha256_file(baseline_path)}]}
				with self.subTest(field=field):
					result = runner._compare_capture("fixture", "view", runs, baseline, root, None, "profile")
					self.assertEqual(result["status"], "incompatible")

	def test_repeatable_candidate_with_one_pixel_difference_from_baseline_is_mismatch(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			first, second, baseline_path = (root / name for name in ("first.png", "second.png", "baseline.png"))
			write_png(first)
			write_png(second)
			write_png(baseline_path, pixel=(10, 20, 31, 255))
			manifest = worker_manifest(first)
			runs = [{"worker_manifest": manifest, "image_path": str(first)}, {"worker_manifest": manifest, "image_path": str(second)}]
			baseline = {"captures": [{"case_id": "fixture", "capture_id": "view", "image": "baseline.png",
				"compatibility": runner._compatibility(manifest), "file_sha256": runner.sha256_file(baseline_path)}]}
			result = runner._compare_capture("fixture", "view", runs, baseline, root, None, "profile")
			self.assertEqual(result["status"], "mismatch")
			self.assertEqual(result["comparison"]["changed_pixels"], 4)

	def test_baseline_candidates_recheck_both_process_images_and_reject_nondeterminism(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			make_run_evidence(root)
			_, candidates = runner._capture_candidates(root)
			self.assertEqual(len(candidates), 1)
			second = root / "workers" / "view" / "1" / "view.png"
			with Image.open(second) as image:
				image.putpixel((0, 0), (99, 20, 30, 255))
				image.save(second)
			manifest = runner._read_json(root / "run_manifest.json")
			capture = manifest["cases"][0]["captures"][0]
			updated = worker_manifest(second, capture_id="view", environment=capture["environment"])
			updated["fixture_hashes"] = {"fixture.json": runner.sha256_file(root / "fixture.json")}
			updated["_decoded_pixel_sha256"] = images.pixel_sha256(*images.decode_rgba(second))
			(second.parent / "view.worker.json").write_text(json.dumps({key: value for key, value in updated.items() if not key.startswith("_")}))
			capture["repetitions"][1]["worker_manifest"] = updated
			runner.atomic_json(root / "run_manifest.json", manifest)
			with self.assertRaisesRegex(ValueError, "nondeterministic"):
				runner._capture_candidates(root)

	def test_new_environment_and_changed_fixture_incompatibilities_are_proposable(self):
		for label, environment, fixture_text, reason in (
			("new-device", {"gpu": "GPU B", "driver": "2", "build_class": "debug"}, "fixture input",
				"environment-profile-has-no-approved-baseline"),
			("changed-fixture", {"gpu": "GPU A", "driver": "1", "build_class": "debug"}, "updated fixture input",
				"environment-or-case-configuration-changed"),
		):
			with self.subTest(candidate=label), tempfile.TemporaryDirectory() as temporary:
				root = Path(temporary)
				make_run_evidence(root, status="incompatible", reason=reason,
					environment=environment, fixture_text=fixture_text)
				_, candidates = runner._capture_candidates(root)
				self.assertEqual(len(candidates), 1)
				self.assertEqual(candidates[0]["compatibility"]["environment"]["gpu"], environment["gpu"])
				self.assertEqual(candidates[0]["fixture_hashes"]["fixture.json"], runner.sha256_file(root / "fixture.json"))
				if label == "new-device":
					args = SimpleNamespace(run_dir=root, baselines=root / "baselines", source_root=root,
						proposal=root / "proposal.json")
					with redirect_stdout(io.StringIO()):
						self.assertEqual(runner._make_proposal(args), 0)
					proposal = runner._read_json(args.proposal)
					self.assertEqual(proposal["profile_id"], runner.digest_json(environment))
					self.assertEqual(proposal["existing_baseline_digests"]["fixture/view"]["set_digest"], None)
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			make_run_evidence(root, status="incompatible", reason="corrupt-approved-baseline-image")
			with self.assertRaisesRegex(ValueError, "not eligible"):
				runner._capture_candidates(root)

	def test_promotion_is_content_addressed_preserves_set_and_rejects_stale_evidence(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			run_root, baseline_root = root / "run", root / "baselines"
			run_root.mkdir()
			make_run_evidence(run_root, ("view", "detail"))
			run, candidates = runner._capture_candidates(run_root)
			proposal = {
				"schema_version": 1, "kind": "rendering-fidelity-baseline-proposal",
				"run_manifest_sha256": runner.sha256_file(run_root / "run_manifest.json"),
				"run_directory": str(run_root.resolve()), "profile_id": run["profile_id"],
				"baseline_root": str(baseline_root.resolve()), "expected_current_set_digest": None,
				"candidates": candidates,
			}
			proposal["manifest_digest"] = runner.digest_json(proposal)
			with redirect_stdout(io.StringIO()):
				self.assertEqual(runner._publish_proposal(proposal), 0)
			approved_path = baseline_root / run["profile_id"] / "approved.json"
			approved = runner._read_json(approved_path)
			baseline_dir = baseline_root / run["profile_id"] / "sets" / approved["set_digest"]
			baseline = runner._read_json(baseline_dir / "manifest.json")
			self.assertEqual({entry["capture_id"] for entry in baseline["captures"]}, {"view", "detail"})

			# A stale proposal is rejected before publication and leaves the active set untouched.
			stale = dict(proposal)
			stale["expected_current_set_digest"] = None
			stale["manifest_digest"] = runner.digest_json({key: value for key, value in stale.items() if key != "manifest_digest"})
			with self.assertRaisesRegex(ValueError, "approved baseline changed"):
				runner._publish_proposal(stale)
			self.assertEqual(runner._read_json(approved_path)["set_digest"], approved["set_digest"])

	def test_failed_publication_never_activates_a_partial_set(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			run_root, baseline_root = root / "run", root / "baselines"
			run_root.mkdir()
			make_run_evidence(run_root, ("view", "detail"))
			run, candidates = runner._capture_candidates(run_root)
			proposal = {"schema_version": 1, "kind": "rendering-fidelity-baseline-proposal",
				"run_manifest_sha256": runner.sha256_file(run_root / "run_manifest.json"),
				"run_directory": str(run_root.resolve()), "profile_id": run["profile_id"],
				"baseline_root": str(baseline_root.resolve()), "expected_current_set_digest": None,
				"candidates": candidates}
			proposal["manifest_digest"] = runner.digest_json(proposal)
			copy = runner.shutil.copyfile
			calls = 0
			def fail_second_copy(source, destination):
				nonlocal calls
				calls += 1
				if calls == 2:
					raise OSError("simulated staging failure")
				return copy(source, destination)
			with patch.object(runner.shutil, "copyfile", side_effect=fail_second_copy):
				with self.assertRaisesRegex(OSError, "simulated staging failure"):
					runner._publish_proposal(proposal)
			self.assertFalse((baseline_root / run["profile_id"] / "approved.json").exists())
			self.assertEqual(list((baseline_root / run["profile_id"] / "sets").iterdir()), [])


if __name__ == "__main__":
	unittest.main()
