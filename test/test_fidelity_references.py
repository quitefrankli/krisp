import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from applications.rendering_fidelity.fidelity_references import (
	ReferenceError,
	_capture_spec,
	_case_input,
	_sha256,
	generate_references,
	verify_references,
)


class FidelityReferenceTests(unittest.TestCase):
	def setUp(self):
		self.temp = tempfile.TemporaryDirectory()
		self.root = Path(self.temp.name)
		self.assets = self.root / "assets"
		self.assets.mkdir()
		(self.assets / "Avocado.gltf").write_text('{"asset":{"version":"2.0"}}', encoding="utf-8")
		self.gltf_hash = _sha256(self.assets / "Avocado.gltf")
		(self.assets / "manifest.json").write_text(json.dumps({
			"assets": {
				"Avocado": {
					"gltf": "Avocado.gltf",
					"files": {"Avocado.gltf": self.gltf_hash},
					"source_commit": "abc123",
				}
			}
		}), encoding="utf-8")
		self.scenes = self.root / "scenes"
		self.scenes.mkdir()
		self.case_path = self.scenes / "avocado.json"
		self.case = {
			"case_id": "avocado",
			"version": 1,
			"asset": "Avocado",
			"reference_scene": {
				"resolution": [64, 48],
				"camera": {"from": [0, 1, -4], "target": [0, 0, 0], "vertical_fov_degrees": 45},
				"environment": {"type": "neutral", "radiance": [0.18, 0.18, 0.18]},
				"floor": {"enabled": True, "center": [0, -1, 0], "size": 4},
				"point_lights": [{"position": [0, 2, -2], "intensity": 50, "casts_shadow": True}],
				"instances": [{"translation": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]}],
			},
			"captures": {"default": {}},
		}
		self.case_path.write_text(json.dumps(self.case), encoding="utf-8")
		self.output = self.root / "references"

	def tearDown(self):
		self.temp.cleanup()

	@staticmethod
	def _write_cycles_image(_blender, _case, _gltf, _scene, output):
		Path(output).write_bytes(b"sample-render")
		return {"renderer_settings": {"engine": "Cycles CPU"}}

	def test_capture_poses_are_merged_without_losing_shared_scene(self):
		case = json.loads(self.case_path.read_text(encoding="utf-8"))
		case["captures"] = {"posed": {"poses": [{"type": "clip", "clip": 0, "time": 0.25}]}}
		scene = _capture_spec(case, case["captures"]["posed"])
		self.assertEqual(scene["poses"], [{"type": "clip", "clip": 0, "time": 0.25}])
		self.assertTrue(scene["floor"]["enabled"])
		self.assertTrue(scene["point_lights"][0]["casts_shadow"])

	def test_asset_checksum_mismatch_is_rejected_before_rendering(self):
		(self.assets / "Avocado.gltf").write_text("changed", encoding="utf-8")
		manifest = json.loads((self.assets / "manifest.json").read_text(encoding="utf-8"))
		with self.assertRaisesRegex(ReferenceError, "checksum mismatch"):
			_case_input(self.case_path, self.case, self.assets, manifest)

	@patch("applications.rendering_fidelity.fidelity_references._renderer_version", return_value="Blender 4.5.0")
	@patch("applications.rendering_fidelity.fidelity_references._run_cycles_capture")
	def test_render_records_asset_scene_and_image_provenance(self, run_capture, _version):
		run_capture.side_effect = self._write_cycles_image
		manifest = generate_references(self.assets, self.output, renderers=("cycles",), cases_dir=self.scenes, blender="blender")
		self.assertEqual(len(manifest["references"]), 1)
		item = manifest["references"][0]
		self.assertEqual(item["renderer_version"], "Blender 4.5.0")
		self.assertEqual(item["asset_source_commit"], "abc123")
		self.assertEqual(item["file_sha256"], _sha256(self.output / item["image"]))
		with patch("applications.rendering_fidelity.fidelity_references._renderer_version", return_value="Blender 4.5.0"):
			_, errors = verify_references(self.assets, self.output, cases_dir=self.scenes)
		self.assertEqual(errors, [])

	@patch("applications.rendering_fidelity.fidelity_references._renderer_version", return_value="Blender 4.5.0")
	@patch("applications.rendering_fidelity.fidelity_references._run_cycles_capture")
	def test_verify_marks_changed_scene_stale(self, run_capture, _version):
		run_capture.side_effect = self._write_cycles_image
		generate_references(self.assets, self.output, renderers=("cycles",), cases_dir=self.scenes, blender="blender")
		self.case["reference_scene"]["point_lights"][0]["intensity"] += 1
		self.case_path.write_text(json.dumps(self.case), encoding="utf-8")
		with patch("applications.rendering_fidelity.fidelity_references._renderer_version", return_value="Blender 4.5.0"):
			_, errors = verify_references(self.assets, self.output, cases_dir=self.scenes)
		self.assertTrue(any("stale" in error for error in errors))

	@patch("applications.rendering_fidelity.fidelity_references._renderer_version", return_value="Blender 4.5.0")
	@patch("applications.rendering_fidelity.fidelity_references._run_cycles_capture")
	def test_verify_rejects_image_paths_outside_reference_directory(self, run_capture, _version):
		run_capture.side_effect = self._write_cycles_image
		generate_references(self.assets, self.output, renderers=("cycles",), cases_dir=self.scenes, blender="blender")
		manifest_path = self.output / "manifest.json"
		manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
		manifest["references"][0]["image"] = "../../outside.png"
		manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
		_, errors = verify_references(self.assets, self.output, cases_dir=self.scenes)
		self.assertTrue(any("escapes references directory" in error for error in errors))

	@patch("applications.rendering_fidelity.fidelity_references._renderer_version", return_value="Blender 4.5.0")
	@patch("applications.rendering_fidelity.fidelity_references._run_cycles_capture")
	def test_verify_rejects_duplicate_reference_records(self, run_capture, _version):
		run_capture.side_effect = self._write_cycles_image
		generate_references(self.assets, self.output, renderers=("cycles",), cases_dir=self.scenes, blender="blender")
		manifest_path = self.output / "manifest.json"
		manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
		manifest["references"].append(manifest["references"][0])
		manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
		_, errors = verify_references(self.assets, self.output, cases_dir=self.scenes)
		self.assertTrue(any("duplicate reference" in error for error in errors))

	def test_khronos_is_not_silently_dropped_when_adapter_is_missing(self):
		with self.assertRaisesRegex(ReferenceError, "package is not installed"):
			generate_references(self.assets, self.output, renderers=("khronos",), cases_dir=self.scenes,
				khronos_renderer=self.root / "missing-renderer")

	@patch("applications.rendering_fidelity.fidelity_references._run_khronos_capture")
	def test_khronos_dispatches_to_renderer_and_records_capabilities(self, run_capture):
		renderer_root = self.root / "web"
		package = renderer_root / "node_modules/@khronosgroup/gltf-viewer/package.json"
		package.parent.mkdir(parents=True)
		package.write_text('{"version":"1.1.0"}', encoding="utf-8")
		def fake_render(_renderer, _case, _gltf, _scene, output):
			Path(output).write_bytes(b"khronos-image")
			return {"gpu_renderer": "WebGL test device", "reference_renderer": "Khronos glTF Sample Renderer"}
		run_capture.side_effect = fake_render
		manifest = generate_references(self.assets, self.output, renderers=("khronos",), cases_dir=self.scenes,
			khronos_renderer=renderer_root)
		self.assertEqual(run_capture.call_count, 1)
		item = manifest["references"][0]
		self.assertFalse(item["render_capabilities"]["cast_shadows"])
		self.assertIn("Khronos glTF Sample Renderer", item["reference_renderer"])


if __name__ == "__main__":
	unittest.main()
