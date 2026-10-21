import hashlib
import json
import stat
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

from applications.stress_test import benchmark_assets as assets


class BenchmarkAssetPreparationTests(unittest.TestCase):
	def test_default_cache_is_shared_under_checkout_build_directory(self):
		expected = Path(assets.__file__).resolve().parents[2] / "build" / "benchmark-assets"
		self.assertEqual(assets.default_cache_path(), expected)
		self.assertEqual(assets.build_parser().parse_args([]).default_asset_cache, expected)

	def test_tangent_repair_changes_only_pinned_bytes_and_rejects_other_sources(self):
		before = [1.0, 0.0, 0.0, -1.0]
		after = [0.0, 1.0, 0.0, -1.0]
		buffer = b"prefix!!" + struct.pack("<4f", *before) + b"suffix"
		model = {
			"buffers": [{"uri": "fixture.bin"}],
			"bufferViews": [{"byteOffset": 8, "byteLength": 16}],
			"accessors": [{"bufferView": 0, "componentType": 5126, "type": "VEC4", "count": 1}],
			"meshes": [{"primitives": [{"attributes": {"TANGENT": 0}}]}],
		}
		repairs = [{"mesh": 0, "accessor": 0, "vertex": 0, "before": before, "after": after}]
		with tempfile.TemporaryDirectory() as temporary:
			out = Path(temporary)
			self.assertEqual(assets._repair_pinned_tangents(model, [buffer], out, repairs), 1)
			self.assertEqual((out / "fixture.bin").read_bytes(), b"prefix!!" + struct.pack("<4f", *after) + b"suffix")
			with self.assertRaisesRegex(assets.AssetPreparationError, "approved source value"):
				assets._repair_pinned_tangents(model, [b"prefix!!" + struct.pack("<4f", *after) + b"suffix"], out, repairs)

	def test_source_hash_mismatch_fails_closed(self):
		with tempfile.TemporaryDirectory() as temporary:
			path = Path(temporary) / "pinned.bin"
			path.write_bytes(b"fixture")
			with self.assertRaisesRegex(assets.AssetPreparationError, "SHA256 mismatch"):
				assets._checked_source_file(path, hashlib.sha256(b"other").hexdigest(), "fixture")

	def test_asset_paths_reject_traversal_and_encoded_traversal(self):
		for path in ("../outside", "%2e%2e/outside", "/absolute/file", "folder\\file"):
			with self.subTest(path=path), self.assertRaises(assets.AssetPreparationError):
				assets._safe_relative_path(path)
		self.assertEqual(assets._safe_relative_path("textures/base.png"), Path("textures/base.png"))

	def test_zip_index_rejects_traversal_and_symlinks(self):
		for member in ("../../outside", "linked-file"):
			with tempfile.TemporaryDirectory() as temporary:
				path = Path(temporary) / "fixture.zip"
				with zipfile.ZipFile(path, "w") as archive:
					info = zipfile.ZipInfo(member)
					if member == "linked-file":
						info.external_attr = (stat.S_IFLNK | 0o777) << 16
					archive.writestr(info, "target")
				with zipfile.ZipFile(path) as archive, self.subTest(member=member):
					with self.assertRaises(assets.AssetPreparationError):
						assets._safe_zip_index(archive)

	def test_flight_recipe_changes_only_allowed_material_fields(self):
		model = {
			"asset": {"version": "2.0"},
			"extensionsUsed": ["KHR_materials_transmission"],
			"extensionsRequired": ["KHR_materials_transmission"],
			"materials": [
				{
					"name": "LensesMat", "alphaMode": "BLEND", "occlusionTexture": {"index": 1},
					"extensions": {"KHR_materials_transmission": {"transmissionFactor": 1.0}},
					"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}},
				},
			],
			"textures": [{"source": 0}, {"source": 0}],
			"images": [{}],
			"meshes": [],
		}
		assets._validate_gltf_basics(model, "fixture")
		deltas = assets._apply_flight_recipe(model)
		self.assertEqual(model["materials"][0]["alphaMode"], "OPAQUE")
		self.assertNotIn("occlusionTexture", model["materials"][0])
		self.assertEqual(model["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"], {"index": 0})
		self.assertNotIn("KHR_materials_transmission", model.get("extensionsUsed", []))
		self.assertNotIn("KHR_materials_transmission", model.get("extensionsRequired", []))
		self.assertEqual(deltas, {
			"removed_occlusion_bindings": 1,
			"opaque_materials": ["LensesMat"],
			"removed_transmission_extensions": 1,
		})

	def test_unknown_material_extension_is_rejected(self):
		model = {
			"asset": {"version": "2.0"}, "materials": [{
				"extensions": {"KHR_materials_pbrSpecularGlossiness": {}},
			}], "meshes": [],
		}
		with self.assertRaisesRegex(assets.AssetPreparationError, "has unsupported extensions"):
			assets._validate_gltf_basics(model, "fixture")

	def test_white_vertex_colours_must_remain_exactly_white(self):
		buffer = struct.pack("<8f", *(1.0 for _ in range(8)))
		model = {
			"accessors": [{"bufferView": 0, "componentType": 5126, "count": 2, "type": "VEC4"}],
			"bufferViews": [{"buffer": 0, "byteLength": len(buffer)}],
			"meshes": [{"primitives": [{"attributes": {"COLOR_0": 0}}]}],
		}
		self.assertEqual(assets._validate_white_vertex_colours(model, [buffer]), 2)
		changed = struct.pack("<8f", 1.0, 1.0, 0.5, 1.0, 1.0, 1.0, 1.0, 1.0)
		with self.assertRaisesRegex(assets.AssetPreparationError, "non-white"):
			assets._validate_white_vertex_colours(model, [changed])

	def test_sponza_light_bulb_emissive_roundoff_is_exactly_pinned(self):
		model = {
			"asset": {"version": "2.0"},
			"materials": [{
				"name": "light_bulb",
				"emissiveFactor": [1.0, 0.99999994, 1.00000012],
			}],
			"meshes": [],
		}
		with self.assertRaisesRegex(assets.AssetPreparationError, "invalid emissiveFactor"):
			assets._validate_gltf_basics(model, "fixture")
		assets._validate_gltf_basics(model, "fixture", allow_sponza_light_bulb_roundoff=True)
		delta = assets._normalize_sponza_light_bulb_emissive(model)
		self.assertEqual(model["materials"][0]["emissiveFactor"], [1.0, 0.99999994, 1.0])
		self.assertEqual(delta["normalized_component"], 2)
		assets._validate_gltf_basics(model, "prepared fixture")
		model["materials"][0]["emissiveFactor"][2] = 1.0000001
		with self.assertRaisesRegex(assets.AssetPreparationError, "differs from the approved source vector"):
			assets._normalize_sponza_light_bulb_emissive(model)

	def test_prepared_file_map_is_checked_against_lock_not_self_claimed(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			cache = root / "cache"
			(cache / "meshes").mkdir(parents=True)
			gltf = cache / "meshes" / "model.gltf"
			gltf.write_text("{}\n", encoding="utf-8")
			expected = hashlib.sha256(gltf.read_bytes()).hexdigest()
			lock = root / "assets.lock.json"
			lock.write_text(json.dumps({
				"schema_version": 1,
				"profile": assets.PROFILE,
				"expected": {},
				"expected_runtime_files": {"meshes/model.gltf": expected},
			}), encoding="utf-8")
			manifest = {
				"schema_version": 1,
				"profile": assets.PROFILE,
				"converter_version": assets.CONVERTER_VERSION,
				"lock_sha256": assets.sha256_file(lock),
				"assets": {
					"flight_helmet": {"gltf": "meshes/model.gltf"},
					"sponza": {"gltf": "meshes/model.gltf"},
				},
				"files": {"meshes/model.gltf": expected},
			}
			(cache / "prepared.json").write_text(json.dumps(manifest), encoding="utf-8")
			_, errors = assets.verify_prepared_cache(cache, lock_path=lock)
			self.assertEqual(errors, [])
			manifest["files"] = {"meshes/model.gltf": hashlib.sha256(b"forged").hexdigest()}
			(cache / "prepared.json").write_text(json.dumps(manifest), encoding="utf-8")
			_, errors = assets.verify_prepared_cache(cache, lock_path=lock)
			self.assertIn("prepared asset file map does not match pinned runtime file hashes", errors)


if __name__ == "__main__":
	unittest.main()
