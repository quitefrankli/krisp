import hashlib
import io
from contextlib import redirect_stderr, redirect_stdout
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from applications.stress_test import benchmark_assets, benchmark_runner as runner
from applications.stress_test import build_provenance


class BenchmarkRunnerTests(unittest.TestCase):
	def test_protocol_version_and_report_include_instrumented_phases(self):
		self.assertEqual(runner.PROTOCOL_VERSION, 3)
		phase_names = (
			"UniformUpload", "CommandRecording", "ResourceUpload", "SnapshotConstruction",
			"GpuShadow", "GpuMain", "GpuUi",
		)
		report = runner._report({
			"protocol": {"suite": "smoke"}, "status": "complete", "completed_workers": 1,
			"scheduled_workers": 1, "cases": [{"case_id": "fixture", "repetitions_by_mode": {
				"throughput": [{"repetition": 1, "status": "complete", "summary": {
					"durations_ns": {name: {"available": True, "count": 1, "median": 10, "p95": 10, "p99": 10}
						for name in phase_names},
				}}],
			}}],
		})
		for name in phase_names:
			self.assertIn(f"{name} ns (n=1): p50 10, p95 10, p99 10", report)

	def test_supported_gpu_requires_every_gpu_metric(self):
		worker = {"status": "complete", "gpu_supported": True, "errors": [], "lost_records": 0}
		metrics = {kind: {"available": True} for kind in runner._GPU_DURATION_KINDS}
		summary = {"durations_ns": metrics}
		self.assertEqual(runner._status_for_worker(worker, summary), "complete")
		metrics["GpuMain"] = {"available": False, "reason": "missing_gpu_samples_for_measured_submissions"}
		self.assertEqual(runner._status_for_worker(worker, summary), "invalid")

	def test_workload_filter_runs_only_selected_case_in_both_repetition_rounds(self):
		for suite, workload, repetitions in [("standard", "point-shadows-500", 2), ("smoke", "static-objects-100", 1)]:
			with self.subTest(suite=suite):
				cases = runner.suite_cases(suite, workload)
				self.assertEqual([case["case_id"] for case in cases], [workload])
				schedule = runner.make_schedule(suite, repetitions, workload=workload)
				self.assertEqual(len(schedule), repetitions)
				self.assertEqual({case["case_id"] for case in schedule}, {workload})
				self.assertEqual([case["repetition"] for case in schedule], list(range(1, repetitions + 1)))
		with self.assertRaisesRegex(ValueError, "not in suite"):
			runner.suite_cases("standard", "static-objects-100")

	def test_sponza_views_are_selectable_and_atrium_is_in_the_default_suite(self):
		for view, workload, shadow_lights in (
			("atrium", "textured-environment-atrium", 0),
			("corridor", "textured-environment-corridor", 0),
			("material-closeup", "textured-environment-material-closeup", 0),
			("atrium", "textured-environment-atrium-shadows", 1),
			("atrium", "textured-environment-atrium-shadows-4-lights", 4),
			("atrium", "textured-environment-atrium-shadows-8-lights", 8),
			("atrium", "textured-environment-atrium-shadows-environment", 1),
		):
			with self.subTest(view=view):
				cases = runner.suite_cases("standard", workload)
				self.assertEqual(len(cases), 1)
				self.assertEqual(cases[0]["scenario"], "textured-environment")
				self.assertEqual(cases[0]["view"], view)
				self.assertEqual(cases[0]["count"], 1)
				self.assertEqual(cases[0].get("shadow_lights", 0), shadow_lights)
				self.assertEqual(cases[0].get("environment_lighting", False), workload.endswith("-environment"))
				self.assertEqual(len(runner.make_schedule("standard", 2, workload=workload)) * 2, 4)
				args = runner._parser().parse_args(["--executable", "fixture", "--build-manifest", "build.json",
					"--environment-lighting", "lighting", "--shader-directory", "shaders", "run", "--workload", workload])
				self.assertEqual(args.suite, "standard")
		self.assertEqual([case["case_id"] for case in runner.standard_cases()], [
			"moving-transforms-500", "point-shadows-500", "textured-environment-atrium-shadows",
		])
		optional = {case["case_id"]: case for case in runner.standard_cases(include_optional_views=True)}
		self.assertEqual(optional["textured-environment-atrium"].get("shadow_lights", 0), 0)
		self.assertEqual(len(optional), 9)
		self.assertEqual(len({case["case_id"] for case in optional.values()}), len(optional))
		self.assertEqual(optional["textured-environment-atrium-shadows-environment"]["environment_lighting"], True)
		self.assertEqual(len(runner.suite_cases("standard")), 3)

	def test_environment_lighting_workload_flag_and_comparison_metadata(self):
		workload = "textured-environment-atrium-shadows-environment"
		case, = runner.suite_cases("standard", workload)
		command = runner._worker_command("engine", case, "paced", 0.2, 0.5, Path("cache"), Path("out"))
		self.assertIn("--environment-lighting", command)
		self.assertEqual(command[command.index("--shadow-lights") + 1], "1")
		plain = runner._worker_command("engine", runner.standard_cases()[2], "paced", 0.2, 0.5,
			Path("cache"), Path("out"))
		self.assertNotIn("--environment-lighting", plain)

		worker = {"scenario": {"shadow_casting_lights": 1, "lighting": []}}
		protocol = {"protocol_version": 1, "suite": "standard", "warmup_seconds": 0.2,
			"measurement_seconds": 0.5, "repetitions": 1, "deadline_tolerance_ns": 1_000_000}
		baseline = runner._case_compatibility({**case, "environment_lighting": False}, "paced",
			protocol, {}, {}, worker)
		candidate = runner._case_compatibility(case, "paced", protocol, {}, {}, worker)
		self.assertFalse(baseline["workload"]["environment_lighting"])
		self.assertTrue(candidate["workload"]["environment_lighting"])
		comparison = runner.compare_compatibility(baseline, candidate)
		self.assertFalse(comparison["compatible"])
		self.assertIn("workload.environment_lighting", [item["field"] for item in comparison["mismatches"]])

	def test_shadow_variant_is_optional_in_smoke_and_worker_flag_is_explicit(self):
		self.assertEqual(len(runner.smoke_cases()), 3)
		self.assertEqual(len(runner.make_schedule("smoke", 1)), 3)
		workload = "textured-environment-atrium-shadows"
		case, = runner.suite_cases("smoke", workload)
		self.assertEqual(case["shadow_lights"], 1)
		self.assertEqual(len(runner.make_schedule("smoke", 1, workload=workload)), 1)
		command = runner._worker_command("engine", case, "paced", 0.2, 0.5, Path("cache"), Path("out"))
		self.assertEqual(command[command.index("--shadow-lights") + 1], "1")
		plain = runner._worker_command("engine", runner.smoke_cases()[2], "paced", 0.2, 0.5, Path("cache"), Path("out"))
		self.assertNotIn("--shadow-lights", plain)
		for count in (4, 8):
			variant = f"textured-environment-atrium-shadows-{count}-lights"
			for suite in ("standard", "smoke"):
				selected, = runner.suite_cases(suite, variant)
				self.assertEqual(selected["shadow_lights"], count)
				self.assertEqual(len(runner.make_schedule(suite, 2 if suite == "standard" else 1,
					workload=variant)), 2 if suite == "standard" else 1)
				command = runner._worker_command("engine", selected, "paced", 0.2, 0.5, Path("cache"), Path("out"))
				self.assertEqual(command[command.index("--shadow-lights") + 1], str(count))
		self.assertEqual([case["case_id"] for case in runner.suite_cases("asset-loading")], [
			"textured-objects-1", "textured-environment-atrium",
		])

	def test_public_cli_rejects_removed_path_overrides(self):
		base = ["--executable", "fixture", "--build-manifest", "build.json",
			"--environment-lighting", "lighting", "--shader-directory", "shaders", "run"]
		for option in ("--output", "--asset-cache"):
			with self.subTest(option=option), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
				runner._parser().parse_args([*base, option, "fixture"])
			self.assertEqual(error.exception.code, 2)
		with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
			benchmark_assets.build_parser().parse_args(["--asset-cache", "fixture"])
		self.assertEqual(error.exception.code, 2)

	def test_unchanged_build_manifest_preserves_timestamp_to_avoid_relinking(self):
		with tempfile.TemporaryDirectory() as temporary:
			path = Path(temporary) / "build.json"
			build_provenance._write_atomic(path, {"source_fingerprint": "original"})
			os.utime(path, ns=(1_000_000_000, 1_000_000_000))
			build_provenance._write_atomic(path, {"source_fingerprint": "original"})
			self.assertEqual(path.stat().st_mtime_ns, 1_000_000_000)
			build_provenance._write_atomic(path, {"source_fingerprint": "changed"})
			self.assertNotEqual(path.stat().st_mtime_ns, 1_000_000_000)
			self.assertEqual(json.loads(path.read_text()), {"source_fingerprint": "changed"})

	def test_all_benchmark_suites_require_known_optimized_release_configuration(self):
		release = {"build_type": "release", "optimised": True, "debug_macro": False, "assertions": False}
		invalid_builds = [
			{**release, "build_type": "debug"},
			{**release, "build_type": "debugoptimized"},
			{**release, "build_type": "unknown"},
			{**release, "optimised": False},
			{**release, "optimised": None},
			{**release, "assertions": True},
			{**release, "debug_macro": True},
			{},
		]
		for suite in ("standard", "smoke", "asset-loading"):
			with self.subTest(suite=suite, build="release"):
				self.assertEqual(runner.benchmark_build_errors(release), [])
			for build in invalid_builds:
				with self.subTest(suite=suite, build=build):
					self.assertTrue(runner.benchmark_build_errors(build))

	def test_suites_have_expected_coverage_duration_and_repeatable_order(self):
		for suite, case_count, worker_count, timed_seconds in [
			("standard", 3, 8, 96), ("smoke", 3, 6, 4.2)]:
			with self.subTest(suite=suite):
				warmup, measure, repetitions = runner.suite_defaults(suite)
				schedule = runner.make_schedule(suite, repetitions)
				self.assertEqual(sum(len(runner.case_modes(suite, case)) for case in schedule), worker_count)
				self.assertEqual(len({case["case_id"] for case in schedule}), case_count)
				self.assertAlmostEqual(worker_count * (warmup + measure), timed_seconds)
				self.assertEqual(schedule, runner.make_schedule(suite, repetitions))
		self.assertEqual({case["case_id"] for case in runner.standard_cases()}, {
			"moving-transforms-500", "point-shadows-500", "textured-environment-atrium-shadows"})
		args = runner._parser().parse_args(["--executable", "fixture", "--build-manifest", "build.json",
			"--environment-lighting", "lighting", "--shader-directory", "shaders", "run"])
		self.assertEqual(args.suite, "standard")
		with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
			runner._parser().parse_args(["--executable", "fixture", "--build-manifest", "build.json",
				"--environment-lighting", "lighting", "--shader-directory", "shaders", "run", "--suite", "extended"])
		self.assertEqual(error.exception.code, 2)

	def test_standard_modes_keep_pacing_only_for_environment_workloads(self):
		for case in runner.standard_cases(include_optional_views=True):
			expected = ("throughput", "paced") if case["scenario"] == "textured-environment" else ("throughput",)
			self.assertEqual(runner.case_modes("standard", case), expected)
		for workload in ("static-objects-500", "textured-objects-16"):
			with self.assertRaisesRegex(ValueError, "not in suite"):
				runner.suite_cases("standard", workload)

	def test_asset_loading_suite_has_only_two_fresh_process_cases(self):
		cases = runner.suite_cases("asset-loading")
		self.assertEqual([case["case_id"] for case in cases], ["textured-objects-1", "textured-environment-atrium"])
		self.assertEqual(runner.suite_defaults("asset-loading"), (0.0, 0.0, 2))
		self.assertEqual(len(runner.make_schedule("asset-loading", 2)), 4)
		for case in cases:
			self.assertEqual(runner.suite_cases("asset-loading", case["case_id"]), [case])
		args = runner._parser().parse_args(["--executable", "fixture", "--build-manifest", "build.json",
			"--environment-lighting", "lighting", "--shader-directory", "shaders", "run",
			"--suite", "asset-loading", "--workload", "textured-objects-1"])
		self.assertEqual(args.suite, "asset-loading")

	def test_asset_loading_rejects_explicit_timing_options_before_preflight(self):
		for option in ("--warmup", "--measure"):
			args = runner._parser().parse_args(["--executable", "fixture", "--build-manifest", "build.json",
				"--environment-lighting", "lighting", "--shader-directory", "shaders", "run",
				"--suite", "asset-loading", option, "0"])
			with self.subTest(option=option), patch.object(runner, "_new_output_dir") as new_output, redirect_stderr(io.StringIO()):
				self.assertEqual(runner.run(args), 2)
				new_output.assert_not_called()

	def test_asset_loading_result_validates_schema_timing_and_timestamp_consistency(self):
		loading = {
			"schema_version": runner.ASSET_LOADING_SCHEMA_VERSION,
			"units": "nanoseconds",
			"cache_policy": "fresh-process-os-cache-uncontrolled-preverified",
			"readiness_boundary": "first-scene-presentation-observed",
			"durations_ns": {
				"engine_initialization_ns": 50, "asset_registration_ns": 20, "asset_instantiation_ns": 30,
				"scene_setup_ns": 30, "presentation_ready_ns": 20, "asset_to_ready_ns": 100,
				"engine_to_ready_ns": 160,
			},
			"timestamps_ns": {
				"engine_begin_ns": 100, "engine_end_ns": 150, "asset_begin_ns": 160,
				"registration_end_ns": 180, "instantiation_end_ns": 210,
				"scene_end_ns": 240, "ready_ns": 260,
			},
			"instantiation_breakdown": {
				"schema_version": 1, "units": "nanoseconds",
				"durations_ns": {
					"dependency_read_ns": 10, "image_decode_ns": 5, "document_processing_ns": 5,
					"mesh_conversion_ns": 5, "material_construction_ns": 5,
					"ecs_adoption_ns": 0, "other_ns": 0,
				},
			},
		}
		worker = {"status": "complete", "asset_loading": loading, "graphics": {}, "scenario": {}, "errors": []}
		result = runner._case_result(worker, Path("unused"), {}, "asset-loading", 1, {}, False)
		self.assertEqual(result["status"], "complete")
		self.assertEqual(result["summary"]["asset_loading"]["durations_ns"]["asset_to_ready_ns"], 100)
		self.assertEqual(result["summary"]["asset_loading"]["instantiation_breakdown"]["durations_ns"]["dependency_read_ns"], 10)
		for invalid in (
			{**loading, "durations_ns": {**loading["durations_ns"], "engine_initialization_ns": True}},
			{**loading, "durations_ns": {**loading["durations_ns"], "asset_to_ready_ns": 0}},
			{**loading, "timestamps_ns": {**loading["timestamps_ns"], "ready_ns": 90}},
			{**loading, "schema_version": 1},
			{**loading, "schema_version": 2.0},
			{key: value for key, value in loading.items() if key != "instantiation_breakdown"},
			{**loading, "instantiation_breakdown": {**loading["instantiation_breakdown"], "durations_ns": {
				**loading["instantiation_breakdown"]["durations_ns"], "image_decode_ns": -1}}},
			{**loading, "instantiation_breakdown": {**loading["instantiation_breakdown"], "durations_ns": {
				**loading["instantiation_breakdown"]["durations_ns"], "image_decode_ns": 99}}},
			{**loading, "instantiation_breakdown": {**loading["instantiation_breakdown"], "durations_ns": {
				key: value for key, value in loading["instantiation_breakdown"]["durations_ns"].items()
				if key != "other_ns"}}},
		):
			with self.subTest(invalid=invalid):
				bad = runner._case_result({**worker, "asset_loading": invalid}, Path("unused"), {}, "asset-loading", 1, {}, False)
				self.assertEqual(bad["status"], "failed")
				self.assertIn("asset_loading", bad["error"])

		missing = runner._case_result({"status": "complete"}, Path("unused"), {}, "asset-loading", 1, {}, False)
		self.assertEqual(missing["status"], "failed")
		self.assertIn("missing asset_loading", missing["error"])

	def test_asset_loading_report_uses_milliseconds_and_repetition_distributions(self):
		duration_names = (
			"engine_initialization_ns", "asset_registration_ns", "asset_instantiation_ns",
			"scene_setup_ns", "presentation_ready_ns", "asset_to_ready_ns", "engine_to_ready_ns",
		)
		repetitions = []
		for repetition, offset in ((1, 0), (2, 2)):
			durations = {name: (index + 1 + offset) * 1_000_000 for index, name in enumerate(duration_names)}
			instantiation = durations["asset_instantiation_ns"]
			repetitions.append({"repetition": repetition, "mode": "asset-loading", "status": "complete",
				"summary": {"asset_loading": {"durations_ns": durations,
					"instantiation_breakdown": {"durations_ns": {
						"dependency_read_ns": instantiation, "image_decode_ns": 0, "document_processing_ns": 0,
						"mesh_conversion_ns": 0, "material_construction_ns": 0, "ecs_adoption_ns": 0, "other_ns": 0,
					}}}}})
		report = runner._report({
			"protocol": {"suite": "asset-loading"}, "status": "complete", "completed_workers": 2,
			"scheduled_workers": 2, "cases": [{"case_id": "textured-objects-1",
				"repetitions_by_mode": {"asset-loading": repetitions}}],
		})
		self.assertIn("Asset loading durations are in milliseconds", report)
		self.assertIn("engine_initialization_ns: 1.000 ms", report)
		self.assertIn("engine_initialization_ns across repetitions: median 2.000 ms; range 1.000–3.000 ms; MAD 1.000 ms", report)
		self.assertIn("dependency read: 3.000 ms (100.0% of asset instantiation)", report)
		self.assertIn("dependency read across repetitions: median 4.000 ms (100.0% of asset instantiation)", report)
		for name in duration_names:
			self.assertIn(name, report)
		self.assertNotIn("Present-call cadence", report)
		self.assertNotIn("ticks/s", report)
		self.assertNotIn("Gpu", report)

	def test_asset_loading_baseline_compares_all_metrics_and_rejects_policy_mismatch(self):
		duration_names = (
			"engine_initialization_ns", "asset_registration_ns", "asset_instantiation_ns",
			"scene_setup_ns", "presentation_ready_ns", "asset_to_ready_ns", "engine_to_ready_ns",
		)
		compatibility = {"mode": "asset-loading", "protocol": {
			"suite": "asset-loading", "asset_loading_schema_version": runner.ASSET_LOADING_SCHEMA_VERSION,
			"cache_policy": "fresh-process-os-cache-uncontrolled-preverified",
			"readiness_boundary": "first-scene-presentation-observed",
		}}
		def reps(offset):
			return [{"status": "complete", "compatibility": compatibility,
				"summary": {"asset_loading": {"durations_ns": {
					name: (index + offset + repetition) * 1_000_000
					for index, name in enumerate(duration_names)
				}}}} for repetition in (0, 1)]
		baseline_case = {"case_id": "textured-objects-1", "repetitions_by_mode": {"asset-loading": reps(1)}}
		candidate_case = {"case_id": "textured-objects-1", "repetitions_by_mode": {"asset-loading": reps(3)}}
		manifest = {"protocol": {"suite": "asset-loading"}, "cases": [candidate_case]}
		runner._compare_run(manifest, {"cases": [baseline_case]})
		comparisons = candidate_case["comparisons"]["asset-loading"]
		self.assertTrue(comparisons["compatibility"]["compatible"])
		self.assertEqual({name for name in comparisons if name != "compatibility"}, set(duration_names))
		self.assertEqual(comparisons["asset_to_ready_ns"]["label"], "regressed")
		self.assertEqual(comparisons["asset_to_ready_ns"]["difference"], 2_000_000)
		self.assertEqual(comparisons["asset_to_ready_ns"]["candidate"]["range"], [8_000_000, 9_000_000])

		changed_policy = {**baseline_case, "repetitions_by_mode": {"asset-loading": [
			{**rep, "compatibility": {**compatibility, "protocol": {
				**compatibility["protocol"], "cache_policy": "different-policy"}}}
			for rep in baseline_case["repetitions_by_mode"]["asset-loading"]
		]}}
		incompatible_manifest = {"protocol": {"suite": "asset-loading"}, "cases": [candidate_case.copy()]}
		runner._compare_run(incompatible_manifest, {"cases": [changed_policy]})
		incompatible = incompatible_manifest["cases"][0]["comparisons"]["asset-loading"]
		self.assertFalse(incompatible["compatibility"]["compatible"])
		self.assertTrue(any(item["field"] == "protocol.cache_policy" for item in incompatible["compatibility"]["mismatches"]))
		self.assertNotIn("asset_to_ready_ns", incompatible)

	def test_asset_loading_run_uses_one_mode_and_accepts_zero_measurement(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			cache = root / "cache"
			cache.mkdir()
			lighting = root / "environment.krisp-ibl"
			lighting.write_bytes(b"lighting")
			shaders = root / "shaders"
			shaders.mkdir()
			(shaders / "test.spv").write_bytes(b"shader")
			build = root / "build.json"
			build.write_text(json.dumps({
				"build_type": "release", "optimised": True, "debug_macro": False,
				"assertions": False, "runtime_tools_sha256": build_provenance.runtime_tools_sha256(Path.cwd()),
			}))
			output = root / "run"
			args = runner._parser().parse_args([
				"--executable", os.fspath(Path(os.sys.executable)), "--build-manifest", os.fspath(build),
				"--default-asset-cache", os.fspath(cache), "--environment-lighting", os.fspath(lighting),
				"--shader-directory", os.fspath(shaders), "run", "--suite", "asset-loading", "--repetitions", "1",
			])
			loading = {
				"schema_version": runner.ASSET_LOADING_SCHEMA_VERSION, "units": "nanoseconds",
				"cache_policy": "fresh-process-os-cache-uncontrolled-preverified",
				"readiness_boundary": "first-scene-presentation-observed",
				"durations_ns": {
					"engine_initialization_ns": 50, "asset_registration_ns": 20, "asset_instantiation_ns": 30,
					"scene_setup_ns": 30, "presentation_ready_ns": 20, "asset_to_ready_ns": 100,
					"engine_to_ready_ns": 160,
				},
				"timestamps_ns": {
					"engine_begin_ns": 100, "engine_end_ns": 150, "asset_begin_ns": 160,
					"registration_end_ns": 180, "instantiation_end_ns": 210,
					"scene_end_ns": 240, "ready_ns": 260,
				},
				"instantiation_breakdown": {
					"schema_version": 1, "units": "nanoseconds",
					"durations_ns": {
						"dependency_read_ns": 10, "image_decode_ns": 5, "document_processing_ns": 5,
						"mesh_conversion_ns": 5, "material_construction_ns": 5,
						"ecs_adoption_ns": 0, "other_ns": 0,
					},
				},
			}
			worker_result = {"status": "complete", "asset_loading": loading, "graphics": {},
				"scenario": {"version": 1, "instances": 1, "renderables": 1}, "errors": []}
			output.mkdir()
			with patch.object(runner, "_new_output_dir", return_value=output), \
				patch.object(benchmark_assets, "needs_preparation", return_value=False), \
				patch.object(runner, "verify_asset_cache", return_value=({"profile": runner.PROFILE, "manifest_sha256": "assets", "files": {}}, [])), \
				patch.object(runner, "_run_worker", return_value=(worker_result, 0, None)) as run_worker, \
				redirect_stdout(io.StringIO()):
				self.assertEqual(runner.run(args), 0)
			self.assertEqual(run_worker.call_count, 2)
			self.assertEqual({call.args[2] for call in run_worker.call_args_list}, {"asset-loading"})
			manifest = json.loads((output / "run_manifest.json").read_text())
			self.assertEqual(manifest["protocol"]["warmup_seconds"], 0.0)
			self.assertEqual(manifest["protocol"]["measurement_seconds"], 0.0)
			self.assertEqual(manifest["protocol"]["asset_loading_schema_version"], 2)
			self.assertEqual(manifest["scheduled_workers"], 2)
			self.assertEqual(manifest["status"], "complete")
			compatibility = manifest["cases"][0]["repetitions_by_mode"]["asset-loading"][0]["compatibility"]["protocol"]
			self.assertEqual(compatibility["instrumentation_version"], 2)
			self.assertEqual(compatibility["asset_loading_schema_version"], 2)
			self.assertEqual((output / "report.md").read_text().count("**asset-loading**"), 2)


	def test_asset_cache_verifies_all_declared_files_and_profile(self):
		with tempfile.TemporaryDirectory() as temporary:
			cache = Path(temporary)
			(cache / "meshes" / "flight_helmet").mkdir(parents=True)
			(cache / "meshes" / "sponza").mkdir(parents=True)
			for name in ("helmet.gltf", "sponza.gltf", "buffer.bin"):
				asset_dir = "flight_helmet" if name == "helmet.gltf" else "sponza"
				(cache / "meshes" / asset_dir / name).write_text(name)
			files = {
				f"meshes/{'flight_helmet' if name == 'helmet.gltf' else 'sponza'}/{name}": hashlib.sha256(
					(cache / "meshes" / ("flight_helmet" if name == "helmet.gltf" else "sponza") / name).read_bytes()
				).hexdigest()
				for name in ("helmet.gltf", "sponza.gltf")
			}
			files["meshes/sponza/buffer.bin"] = hashlib.sha256(b"buffer.bin").hexdigest()
			expected = {
				"flight_helmet_renderables_per_instance": 1,
				"flight_helmet_mesh_count": 1,
				"flight_helmet_material_count": 1,
				"flight_helmet_texture_count": 1,
				"flight_helmet_vertex_count": 3,
				"flight_helmet_index_count": 3,
				"flight_helmet_import_scale": 1.0,
				"sponza_renderables_per_instance": 1,
				"sponza_mesh_count": 1,
				"sponza_material_count": 1,
				"sponza_texture_count": 1,
				"sponza_vertex_count": 3,
				"sponza_index_count": 3,
				"sponza_triangle_count": 1,
				"sponza_import_scale": 1.0,
				"sponza_light_position": [0.0, 12.0, 2.0],
				"sponza_light_intensity": 800.0,
				"sponza_views": {"atrium": {"from": [0, 0, 1], "focus": [0, 0, 0]}},
				"sponza_normalized_emissive_factor": {"material": "light_bulb", "normalized_component": 2,
					"source_emissive_factor": [1.0, 0.99999994, 1.00000012],
					"prepared_emissive_factor": [1.0, 0.99999994, 1.0]},
			}
			lock = {"schema_version": 1, "profile": runner.PROFILE, "expected": expected}
			lock_path = cache / "synthetic-lock.json"
			lock_path.write_text(json.dumps(lock))
			helmet = {key.removeprefix("flight_helmet_"): value for key, value in expected.items()
				if key.startswith("flight_helmet_")}
			sponza = {key.removeprefix("sponza_"): value for key, value in expected.items()
				if key.startswith("sponza_") and key != "sponza_normalized_emissive_factor"}
			sponza["views"] = expected["sponza_views"]
			sponza["deltas"] = {"normalized_emissive_factor": expected["sponza_normalized_emissive_factor"]}
			prepared = {
				"schema_version": 1, "profile": runner.PROFILE,
				"converter_version": benchmark_assets.CONVERTER_VERSION,
				"lock_sha256": benchmark_assets.sha256_file(lock_path),
				"assets": {"flight_helmet": {"gltf": "meshes/flight_helmet/helmet.gltf", **helmet},
					"sponza": {"gltf": "meshes/sponza/sponza.gltf", **sponza}},
				"files": files,
			}
			(cache / "prepared.json").write_text(json.dumps(prepared))
			with patch.object(benchmark_assets, "LOCK_PATH", lock_path):
				identity, errors = runner.verify_asset_cache(cache)
			self.assertFalse(errors)
			self.assertEqual(identity["profile"], runner.PROFILE)
			prepared["lock_sha256"] = "0" * 64
			(cache / "prepared.json").write_text(json.dumps(prepared))
			with patch.object(benchmark_assets, "LOCK_PATH", lock_path):
				_, errors = runner.verify_asset_cache(cache)
			self.assertTrue(any("lock fingerprint" in error for error in errors))
			prepared["lock_sha256"] = benchmark_assets.sha256_file(lock_path)
			(cache / "prepared.json").write_text(json.dumps(prepared))
			(cache / "meshes" / "sponza" / "buffer.bin").write_text("changed")
			with patch.object(benchmark_assets, "LOCK_PATH", lock_path):
				_, errors = runner.verify_asset_cache(cache)
			self.assertTrue(any("checksum mismatch" in error for error in errors))

	def _fake_executable(self, directory: Path, source: str) -> Path:
		path = directory / "worker"
		path.write_text("#!/usr/bin/env python3\n" + source)
		path.chmod(0o755)
		return path

	def test_worker_requires_ready_completion_and_zero_exit(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			worker = self._fake_executable(root, """
import json, pathlib, sys
args = sys.argv
out = pathlib.Path(args[args.index('--output') + 1])
(out / 'ready.json').write_text(json.dumps({'status': 'ready'}))
(out / 'raw.jsonl').write_text(json.dumps({'kind':'Simulation','begin_ns':10,'end_ns':20}) + '\\n')
(out / 'worker.json').write_text(json.dumps({'status':'complete','window_start_ns':10,'window_end_ns':20,'gpu_supported':False,'graphics':{},'scenario':{},'errors':[]}))
""")
			output = root / "worker-output"
			case = {"scenario": "static-objects", "count": 100, "view": None}
			completed, return_code, error = runner._run_worker(str(worker), case, "paced", 1, 0, 0.1, root, output)
			self.assertEqual(return_code, 0)
			self.assertIsNone(error)
			result = runner._case_result(completed, output, case, "paced", 1, {}, False)
			self.assertEqual(result["status"], "partial")
			self.assertEqual(result["summary"]["counts"]["simulation_ticks"], 1)

			bad_worker = self._fake_executable(root, """
import json, pathlib, sys
args = sys.argv
out = pathlib.Path(args[args.index('--output') + 1])
(out / 'worker.json').write_text(json.dumps({'status':'failed','errors':['graphics initialization failed']}))
raise SystemExit(4)
""")
			failed, exit_code, error = runner._run_worker(str(bad_worker), case, "paced", 1, 0, 0.1, root, root / "bad-output")
			self.assertEqual(exit_code, 4)
			self.assertEqual(error, "readiness_timeout")
			self.assertEqual(failed["status"], "failed")
			self.assertIn("graphics initialization failed", failed["errors"])

	def test_interactive_worker_waits_for_normal_exit_and_forwards_flag(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			worker = self._fake_executable(root, """
import json, pathlib, sys, time
args = sys.argv
out = pathlib.Path(args[args.index('--output') + 1])
assert '--interactive' in args
(out / 'ready.json').write_text(json.dumps({'status': 'ready'}))
time.sleep(0.2)
(out / 'worker.json').write_text(json.dumps({'status':'complete','interactive':True}))
""")
			case = {"scenario": "textured-environment", "count": 1, "view": "closeup"}
			command = runner._worker_command(str(worker), case, "paced", 0, 0, root, root / "command-output", interactive=True)
			self.assertIn("--interactive", command)
			with patch.object(runner, "DRAIN_TIMEOUT_SECONDS", 0.01), patch.object(runner, "SHUTDOWN_MARGIN_SECONDS", 0.01):
				started = runner.time.monotonic()
				result, code, error = runner._run_worker(
					str(worker), case, "paced", 1, 0, 0, root, root / "interactive-output", interactive=True,
				)
			self.assertGreaterEqual(runner.time.monotonic() - started, 0.15)
			self.assertEqual(code, 0)
			self.assertIsNone(error)
			self.assertEqual(result, {"status": "complete", "interactive": True})

	def test_interactive_run_launches_one_paced_worker_without_benchmark_summary(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			build = root / "build.json"
			build.write_text(json.dumps({
				"build_type": "release", "optimised": True, "debug_macro": False,
				"assertions": False, "runtime_tools_sha256": build_provenance.runtime_tools_sha256(Path.cwd()),
			}))
			lighting = root / "lighting.krisp-ibl"
			lighting.write_bytes(b"lighting")
			shaders = root / "shaders"
			shaders.mkdir()
			(shaders / "test.spv").write_bytes(b"spirv")
			output = root / "interactive-run"
			output.mkdir()
			args = runner._parser().parse_args([
				"--executable", os.fspath(Path(os.sys.executable)), "--build-manifest", os.fspath(build),
				"--default-asset-cache", os.fspath(root / "cache"),
				"--environment-lighting", os.fspath(lighting), "--shader-directory", os.fspath(shaders),
				"run", "--suite", "standard", "--workload", "textured-environment-material-closeup",
				"--interactive", "--warmup", "99", "--measure", "99", "--repetitions", "8",
			])
			worker_result = {"status": "complete", "interactive": True, "scenario": {}}
			with patch.object(runner, "_new_output_dir", return_value=output), \
				patch.object(benchmark_assets, "needs_preparation", return_value=False), \
				patch.object(runner, "verify_asset_cache", return_value=({"profile": runner.PROFILE, "manifest_sha256": "assets", "files": {}}, [])), \
				patch.object(runner, "_run_worker", return_value=(worker_result, 0, None)) as run_worker, \
				redirect_stdout(io.StringIO()):
				self.assertEqual(runner.run(args), 0)
			self.assertEqual(run_worker.call_count, 1)
			call = run_worker.call_args
			self.assertEqual(call.args[2:6], ("paced", 1, 0, 0))
			self.assertTrue(call.kwargs["interactive"])
			manifest = json.loads((output / "run_manifest.json").read_text())
			self.assertTrue(manifest["interactive"])
			self.assertEqual(manifest["scheduled_workers"], 1)
			self.assertEqual(manifest["completed_workers"], 1)
			self.assertEqual(manifest["interactive_session"]["worker"]["status"], "complete")
			self.assertEqual(manifest["status"], "complete")
			self.assertNotIn("cases", manifest)
			self.assertNotIn("summary", manifest["interactive_session"])

			invalid_output = root / "invalid-interactive-run"
			invalid_output.mkdir()
			with patch.object(runner, "_new_output_dir", return_value=invalid_output), \
				patch.object(benchmark_assets, "needs_preparation", return_value=False), \
				patch.object(runner, "verify_asset_cache", return_value=({"profile": runner.PROFILE, "manifest_sha256": "assets", "files": {}}, [])), \
				patch.object(runner, "_run_worker", return_value=({"status": "invalid", "errors": ["fixture invalid"]}, 0, None)), \
				redirect_stdout(io.StringIO()):
				self.assertEqual(runner.run(args), 1)
			invalid_manifest = json.loads((invalid_output / "run_manifest.json").read_text())
			self.assertEqual(invalid_manifest["status"], "failed")
			self.assertEqual(invalid_manifest["interactive_session"]["worker_status"], "invalid")

	def test_interactive_requires_a_workload(self):
		args = runner._parser().parse_args([
			"--executable", "worker", "--build-manifest", "build.json",
			"--environment-lighting", "lighting", "--shader-directory", "shaders",
			"run", "--interactive",
		])
		stderr = io.StringIO()
		with redirect_stderr(stderr), patch.object(runner, "_new_output_dir") as new_output:
			self.assertEqual(runner.run(args), 2)
		new_output.assert_not_called()
		self.assertIn("--interactive requires --workload", stderr.getvalue())

	def test_invalid_worker_retains_validity_reason_with_expected_nonzero_exit(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			worker = self._fake_executable(root, """
import json, pathlib, sys
out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])
(out / 'ready.json').write_text(json.dumps({'status': 'ready'}))
(out / 'worker.json').write_text(json.dumps({'status': 'invalid', 'errors': ['Window lost focus']}))
raise SystemExit(1)
""")
			case = {"scenario": "static-objects", "count": 100, "view": None}
			result, code, error = runner._run_worker(str(worker), case, "paced", 1, 0, .1, root, root / "output")
			self.assertEqual(code, 1)
			self.assertIsNone(error)
			self.assertEqual(result, {"status": "invalid", "errors": ["Window lost focus"]})

	def test_environment_readiness_has_a_separate_bounded_watchdog(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			worker = self._fake_executable(root, "import time\ntime.sleep(10)\n")
			with patch.object(runner, "ENVIRONMENT_READY_TIMEOUT_SECONDS", 0.05), \
				patch.object(runner, "READY_TIMEOUT_SECONDS", 5):
				started = runner.time.monotonic()
				result, code, error = runner._run_worker(str(worker),
					{"scenario": "textured-environment", "count": 1, "view": "atrium"},
					"throughput", 1, 0, .1, root, root / "worker-output")
				self.assertLess(runner.time.monotonic() - started, 2)
			self.assertEqual(error, "readiness_timeout")
			self.assertEqual(result["status"], "failed")
			self.assertIsNotNone(code)

	def test_worker_watchdog_terminates_and_reaps_process_group(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			worker = self._fake_executable(root, """
import json, pathlib, sys, time
args = sys.argv
out = pathlib.Path(args[args.index('--output') + 1])
(out / 'ready.json').write_text(json.dumps({'status':'ready'}))
time.sleep(5)
""")
			with patch.object(runner, "DRAIN_TIMEOUT_SECONDS", 0.05), patch.object(runner, "SHUTDOWN_MARGIN_SECONDS", 0.05):
				result, exit_code, error = runner._run_worker(
					str(worker), {"scenario": "static-objects", "count": 1}, "throughput", 1,
					0, 0.01, root, root / "slow-output",
				)
			self.assertEqual(error, "worker_timeout")
			self.assertEqual(result["status"], "failed")
			self.assertIsNotNone(exit_code)

	def test_asset_loading_worker_has_a_bounded_ready_to_exit_timeout(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			worker = self._fake_executable(root, """
import json, pathlib, sys, time
out = pathlib.Path(sys.argv[sys.argv.index('--output') + 1])
(out / 'ready.json').write_text(json.dumps({'status': 'ready'}))
time.sleep(5)
""")
			with patch.object(runner, "SHUTDOWN_MARGIN_SECONDS", 0.05), patch.object(runner, "DRAIN_TIMEOUT_SECONDS", 30):
				started = runner.time.monotonic()
				result, exit_code, error = runner._run_worker(str(worker),
					{"scenario": "textured-objects", "count": 1}, "asset-loading", 1,
					0, 0, root, root / "loading-output")
				self.assertLess(runner.time.monotonic() - started, 2)
			self.assertEqual(error, "worker_timeout")
			self.assertEqual(result["status"], "failed")
			self.assertIsNotNone(exit_code)

	def test_preflight_failure_writes_report_and_launches_no_workers(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			build = root / "build.json"
			release_build = {"schema_version": 1, "build_class": "optimized", "build_type": "release",
				"optimised": True, "debug_macro": False, "assertions": False, "validation": True,
				"runtime_tools_sha256": build_provenance.runtime_tools_sha256(Path.cwd())}
			build.write_text(json.dumps(release_build))
			lighting = root / "default_environment.krisp-ibl"
			lighting.write_bytes(b"synthetic lighting")
			shaders = root / "shaders"
			shaders.mkdir()
			(shaders / "test.spv").write_bytes(b"spirv")
			output = root / "result"
			args = runner._parser().parse_args([
				"--executable", os.fspath(Path(os.sys.executable)),
				"--build-manifest", os.fspath(build),
				"--default-asset-cache", os.fspath(root / "missing-cache"),
				"--environment-lighting", os.fspath(lighting), "--shader-directory", os.fspath(shaders),
				"run", "--suite", "smoke",

			])
			def invoke():
				output.mkdir()
				with patch.object(runner, "_new_output_dir", return_value=output):
					return runner.run(args)
			for suite in ("standard", "smoke", "asset-loading"):
				build.write_text(json.dumps({**release_build, "build_class": "debug", "build_type": "debug",
					"optimised": False, "debug_macro": True, "assertions": True}))
				args.suite = suite
				output = root / f"debug-{suite}"
				stderr = io.StringIO()
				with redirect_stderr(stderr), patch.object(benchmark_assets, "prepare_assets") as prepare, \
					patch.object(runner, "_run_worker") as worker:
					self.assertEqual(invoke(), 2)
					prepare.assert_not_called()
					worker.assert_not_called()
				self.assertIn("including smoke", stderr.getvalue())
				manifest = json.loads((output / "run_manifest.json").read_text())
				self.assertIn("including smoke", " ".join(manifest["errors"]))
			build.write_text(json.dumps(release_build))
			args.suite = "smoke"
			output = root / "result"
			stderr = io.StringIO()
			with redirect_stderr(stderr), patch.object(benchmark_assets, "prepare_assets",
				side_effect=benchmark_assets.AssetPreparationError("fixture prepared.json unavailable")) as prepare:
				self.assertEqual(invoke(), 2)
				prepare.assert_called_once()
			self.assertIn("prepared.json", stderr.getvalue())
			self.assertIn(str(output / "report.md"), stderr.getvalue())
			manifest = json.loads((output / "run_manifest.json").read_text())
			self.assertEqual(manifest["completed_workers"], 0)
			self.assertIn("prepared.json", " ".join(manifest["errors"]))
			self.assertEqual(manifest["engine_resources"]["environment_lighting_sha256"],
				hashlib.sha256(b"synthetic lighting").hexdigest())
			self.assertEqual(manifest["runtime_shader_spirv_sha256"]["test.spv"],
				hashlib.sha256(b"spirv").hexdigest())
			self.assertTrue((output / "report.md").is_file())
			args.workload = "static-objects-100"
			args.suite = "standard"
			output = root / "wrong-suite"
			with patch.object(benchmark_assets, "prepare_assets") as prepare, patch.object(runner, "_run_worker") as worker:
				self.assertEqual(invoke(), 2)
				prepare.assert_not_called()
				worker.assert_not_called()
			args.workload = None

			args.suite = "smoke"
			args.default_asset_cache = root / "build" / "benchmark-assets"
			args.source_directory = root / "pinned-inputs"
			args.default_asset_cache.mkdir(parents=True)
			prepared_path = args.default_asset_cache / "prepared.json"
			prepared_path.write_text(json.dumps({
				"profile": "engine-pbr-opaque-bc3-v1",
				"converter_version": "krisp-benchmark-assets/3",
			}))
			def prepare_missing(cache, *, source_directory):
				self.assertEqual(source_directory, args.source_directory)
				prepared_path.write_text(json.dumps({
					"profile": benchmark_assets.PROFILE,
					"converter_version": benchmark_assets.CONVERTER_VERSION,
				}))
			def verify_existing(cache):
				self.assertEqual(json.loads(prepared_path.read_text()), {
					"profile": benchmark_assets.PROFILE,
					"converter_version": benchmark_assets.CONVERTER_VERSION,
				}, "Preparation must finish before verification")
				return {"profile": "fixture"}, []
			output = root / "automatic-outdated"
			with patch.object(benchmark_assets, "prepare_assets", side_effect=prepare_missing) as prepare, \
				patch.object(runner, "verify_asset_cache", side_effect=verify_existing) as verify, \
				patch.object(runner, "make_schedule", return_value=[]):
				self.assertEqual(invoke(), 0)
				prepare.assert_called_once_with(args.default_asset_cache, source_directory=args.source_directory)
				verify.assert_called_once_with(args.default_asset_cache)
			output = root / "reuse-current"
			with patch.object(benchmark_assets, "prepare_assets") as prepare, \
				patch.object(runner, "verify_asset_cache", side_effect=verify_existing) as verify, \
				patch.object(runner, "make_schedule", return_value=[]):
				self.assertEqual(invoke(), 0)
				prepare.assert_not_called()
				verify.assert_called_once_with(args.default_asset_cache)
			output = root / "corrupt-existing"
			with patch.object(benchmark_assets, "prepare_assets") as prepare, \
				patch.object(runner, "verify_asset_cache", return_value=({}, ["fixture checksum mismatch"])), \
				patch.object(runner, "_run_worker") as worker:
				self.assertEqual(invoke(), 2)
				prepare.assert_not_called()
				worker.assert_not_called()



	def test_shader_spirv_hashes_are_recursive_and_ignore_non_spirv_files(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "nested").mkdir()
			(root / "nested" / "shader.spv").write_bytes(b"compiled")
			(root / "source.glsl").write_text("source")
			hashes = runner._shader_spirv_hashes(root)
			self.assertEqual(hashes, {"nested/shader.spv": hashlib.sha256(b"compiled").hexdigest()})

	def test_default_output_directories_are_unique_under_local_data(self):
		with tempfile.TemporaryDirectory() as temporary, patch.dict(os.environ, {"XDG_DATA_HOME": temporary}):
			first = runner._new_output_dir()
			(first / "keep.txt").write_text("preserve")
			second = runner._new_output_dir()
			self.assertNotEqual(first, second)
			self.assertEqual(first.parent, Path(temporary) / "krisp-engine" / "stress-test")
			self.assertEqual((first / "keep.txt").read_text(), "preserve")

	def test_source_fingerprint_tracks_inputs_but_ignores_build_artifacts(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			(root / "src").mkdir()
			(root / "build" / "debug").mkdir(parents=True)
			source = root / "src" / "unit.cpp"
			source.write_text("int value = 1;\n")
			artifact = root / "build" / "debug" / "generated.cpp"
			artifact.write_text("generated one\n")
			first = build_provenance.source_fingerprint(root)
			artifact.write_text("generated two\n")
			self.assertEqual(build_provenance.source_fingerprint(root), first)
			source.write_text("int value = 2;\n")
			self.assertNotEqual(build_provenance.source_fingerprint(root), first)

	def test_build_configuration_labels_assertions_and_validation(self):
		options = [
			{"name": "buildtype", "value": "debug"},
			{"name": "b_ndebug", "value": False},
			{"name": "vulkan_validation", "value": True},
		]
		with patch.object(build_provenance, "_meson_data", side_effect=[options, {"cpp": {"id": "clang", "version": "18.1"}}, []]):
			configuration = build_provenance._build_configuration(Path("."), Path("build"), None, None)
		self.assertEqual(configuration["build_class"], "debug")
		self.assertTrue(configuration["assertions"])
		self.assertIsNone(configuration["validation"])
		self.assertEqual(configuration["compiler_version"], "18.1")

	def test_mesons_internal_dependency_ids_and_missing_optional_fields_are_canonical(self):
		first = [
			{"name": "vulkan", "type": "pkgconfig", "version": "1.3"},
			{"name": "dep136783", "type": "internal", "version": None},
			{"name": "glm", "type": "pkgconfig", "version": "0.9.9.8", "found": True},
		]
		second = [
			{"name": "glm", "type": "pkgconfig", "version": "0.9.9.8"},
			{"name": "dep987654", "type": "internal"},
			{"name": "vulkan", "type": "pkgconfig", "version": "1.3", "found": True},
		]
		canonical = build_provenance._canonical_dependency_versions(first)
		self.assertEqual(canonical, build_provenance._canonical_dependency_versions(second))
		self.assertEqual(canonical, [
			{"name": "glm", "type": "pkgconfig", "version": "0.9.9.8"},
			{"name": "vulkan", "type": "pkgconfig", "version": "1.3"},
		])

	def test_runtime_tool_hashes_are_required_and_verified(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			hashes = {}
			for name in runner.RUNTIME_TOOL_NAMES:
				(root / name).write_text(name)
				hashes[name] = hashlib.sha256(name.encode()).hexdigest()
			self.assertEqual(runner.verify_runtime_tools(root, hashes), [])
			(root / "benchmark_statistics.py").write_text("changed")
			errors = runner.verify_runtime_tools(root, hashes)
			self.assertTrue(any("changed since build" in error for error in errors))
			self.assertTrue(any("rebuild" in error for error in errors))
			self.assertTrue(any("missing runtime_tools_sha256" in error
				for error in runner.verify_runtime_tools(root, None)))

	def test_build_manifest_captures_runtime_tool_hashes(self):
		with patch.object(build_provenance, "source_revision", return_value=("revision", False)), \
			patch.object(build_provenance, "_build_configuration", return_value={"build_class": "debug"}):
			manifest = build_provenance.create_build_manifest(Path.cwd(), Path("unused-build"))
		self.assertEqual(set(manifest["runtime_tools_sha256"]), set(runner.RUNTIME_TOOL_NAMES))
		for name, expected in manifest["runtime_tools_sha256"].items():
			path = Path("applications/stress_test") / name
			self.assertEqual(expected, hashlib.sha256(path.read_bytes()).hexdigest())

	def test_worker_metadata_populates_fail_closed_compatibility_key(self):
		worker = {"status": "complete", "window_start_ns": 10, "window_end_ns": 20,
			"gpu_supported": True, "errors": [], "phases": {"ready_ns": 1, "completed_ns": 20},
			"scenario": {"version": 1, "instances": 100, "renderables": 101}, "graphics": {
			"width": 1280, "height": 720, "requested_width": 1280, "requested_height": 720,
			"vendor_id": 1, "device_id": 2, "device_name": "test GPU", "driver_version": "1.0",
			"api_version": "1.3", "validation_enabled": False, "timestamp_valid_bits": 64,
			"timestamp_period_ns": 1.0, "swapchain_images": 3, "presentation_mode": "immediate",
			"requested_presentation_mode": "uncapped", "simulation_hz": 60, "configuration_note": "none",
			"vertex_buffer_bytes": 134217728, "index_buffer_bytes": 67108864,
			"render_hz": 60.0, "msaa_samples": 4, "shadow_resolution": 2048,
			"fixed_simulation_timestep": True, "render_path": "rasterized-pbr",
			"exposure_ev": 0.0, "ui": False, "performance_overlay": True, "interactive": False,
		}}
		build_configuration = {
			"build_class": "debug", "build_type": "debug", "optimised": False, "debug_macro": True,
			"assertions": True, "validation": None, "compiler": "clang", "compiler_version": "18",
			"optimization_level": "0", "optimization_level_known": True,
			"dependency_versions": [{"name": "vulkan", "type": "pkgconfig", "version": "1.3"}],
			"conan_profile_sha256": "a", "conan_native_file_sha256": "b",
			"dependency_options": {}, "configuration_fingerprint": "build-config-hash",
		}
		with patch.object(build_provenance, "source_revision", return_value=("revision", False)), \
			patch.object(build_provenance, "source_fingerprint", return_value="source-hash"), \
			patch.object(build_provenance, "_build_configuration", return_value=build_configuration):
			build = build_provenance.create_build_manifest(Path.cwd(), Path("unused-build"))
		protocol = {"suite": "smoke", "protocol_version": 1, "warmup_seconds": 0.2,
			"measurement_seconds": 0.5, "repetitions": 1, "deadline_tolerance_ns": 1_000_000}
		case = {"scenario": "static-objects", "count": 100, "view": None}
		compatibility = runner._case_compatibility(case, "paced", protocol, {"profile": runner.PROFILE,
			"manifest_sha256": "asset"}, build, worker,
			{"environment_lighting_sha256": "lighting-hash", "shader_spirv_sha256": {"shader.spv": "provenance-only"}})
		comparison = runner.compare_compatibility(compatibility, compatibility)
		self.assertTrue(comparison["compatible"], comparison)
		self.assertEqual(compatibility["protocol"]["instrumentation_version"], 2)
		self.assertEqual(compatibility["machine"]["gpu"]["device_id"], 2)
		self.assertEqual(compatibility["effective_settings"]["width"], 1280)
		self.assertTrue(compatibility["effective_settings"]["performance_overlay"])
		without_overlay = {**compatibility, "effective_settings": {
			**compatibility["effective_settings"], "performance_overlay": False}}
		comparison = runner.compare_compatibility(without_overlay, compatibility)
		self.assertFalse(comparison["compatible"])
		self.assertIn("effective_settings.performance_overlay", [item["field"] for item in comparison["mismatches"]])

		self.assertEqual(compatibility["effective_settings"]["vertex_buffer_bytes"], 134217728)
		self.assertEqual(compatibility["effective_settings"]["render_hz"], 60.0)
		self.assertEqual(compatibility["effective_settings"]["msaa_samples"], 4)
		self.assertEqual(compatibility["effective_settings"]["shadow_resolution"], 2048)
		self.assertEqual(compatibility["workload"]["engine_resources"]["environment_lighting_sha256"],
			"lighting-hash")
		self.assertEqual(compatibility["build"]["runtime_tools_sha256"], build["runtime_tools_sha256"])
		lighting = [{"position": [0, 1, 2], "intensity": 3.0, "color": [1, 1, 1], "enabled": True, "casts_shadow": True}]
		shadowed_case = {"scenario": "textured-environment", "count": 1, "view": "atrium", "shadow_lights": 1}
		shadowed_worker = {**worker, "scenario": {**worker["scenario"], "shadow_casting_lights": 1, "lighting": lighting}}
		shadowed = runner._case_compatibility(shadowed_case, "paced", protocol, {}, build, shadowed_worker)
		self.assertEqual(shadowed["workload"]["shadow_lights"], 1)
		self.assertFalse(shadowed["workload"]["environment_lighting"])
		self.assertEqual(shadowed["workload"]["shadow_casting_lights"], 1)
		self.assertEqual(shadowed["workload"]["lighting"], lighting)
		unshadowed_case = {**shadowed_case, "shadow_lights": 0}
		unshadowed_worker = {**worker, "scenario": {**worker["scenario"], "shadow_casting_lights": 0, "lighting": []}}
		unshadowed = runner._case_compatibility(unshadowed_case, "paced", protocol, {}, build, unshadowed_worker)
		comparison = runner.compare_compatibility(shadowed, unshadowed)
		self.assertFalse(comparison["compatible"])
		mismatch_fields = {item["field"] for item in comparison["mismatches"]}
		self.assertTrue({"workload.shadow_lights", "workload.shadow_casting_lights"}.issubset(mismatch_fields))
		self.assertTrue(any(field.startswith("workload.lighting") for field in mismatch_fields))
		multi_case = {**shadowed_case, "shadow_lights": 4}
		multi_worker = {**worker, "scenario": {**worker["scenario"], "shadow_casting_lights": 4,
			"lighting": lighting * 4}}
		multi = runner._case_compatibility(multi_case, "paced", protocol, {}, build, multi_worker)
		self.assertEqual(multi["workload"]["shadow_lights"], 4)
		self.assertEqual(multi["workload"]["shadow_casting_lights"], 4)
		self.assertEqual(multi["workload"]["lighting"], lighting * 4)
		unknown = runner._case_compatibility(shadowed_case, "paced", protocol, {}, build,
			{**worker, "scenario": {**worker["scenario"], "shadow_casting_lights": 1}})
		comparison = runner.compare_compatibility(shadowed, unknown)
		self.assertFalse(comparison["compatible"])
		self.assertIn("workload.lighting", comparison["unknown_fields"])

	def test_environment_rendering_requires_observed_shadow_light_count(self):
		case = {"scenario": "textured-environment", "count": 1, "view": "atrium", "shadow_lights": 4}
		for scenario in ({}, {"shadow_casting_lights": 0}, {"shadow_casting_lights": 1}):
			with self.subTest(scenario=scenario):
				result = runner._case_result({"status": "complete", "scenario": scenario}, Path("unused"),
					case, "paced", 1, {}, False)
				self.assertEqual(result["status"], "invalid")
				self.assertIn("shadow-casting lights", result["error"])

	def test_environment_lighting_worker_result_must_match_selected_mode(self):
		case = {"scenario": "textured-environment", "count": 1, "view": "atrium", "shadow_lights": 1,
			"environment_lighting": True}
		for scenario in ({"shadow_casting_lights": 1},
			{"shadow_casting_lights": 1, "environment_lighting": False}):
			with self.subTest(scenario=scenario):
				result = runner._case_result({"status": "complete", "scenario": scenario}, Path("unused"),
					case, "paced", 1, {}, False)
				self.assertEqual(result["status"], "invalid")
				self.assertIn("environment_lighting", result["error"])


if __name__ == "__main__":
	unittest.main()
