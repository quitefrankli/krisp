import hashlib
import json
import stat
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

import PIL
from PIL import Image

from applications.benchmarks.performance import benchmark_assets as assets


class BenchmarkAssetPreparationTests(unittest.TestCase):
	def test_default_cache_is_shared_under_checkout_build_directory(self):
		expected = Path(assets.__file__).resolve().parents[3] / "build" / "benchmark-assets"
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

	def test_png_conversion_writes_deterministic_single_level_bc3_and_preserves_source(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = root / "source.png"
			Image.new("RGBA", (8, 8), (40, 80, 120, 160)).save(source)
			original = source.read_bytes()
			first = root / "first.dds"
			second = root / "second.dds"
			self.assertEqual(assets._convert_png_to_dds(source, first), (8, 8, 64))
			assets._convert_png_to_dds(source, second)
			self.assertEqual(first.read_bytes(), second.read_bytes())
			self.assertEqual(source.read_bytes(), original)
			with Image.open(first) as decoded:
				self.assertEqual(decoded.size, (8, 8))
				self.assertEqual(decoded.mode, "RGBA")
				for actual, expected in zip(decoded.getpixel((2, 1)), (40, 80, 120, 160)):
					self.assertLessEqual(abs(actual - expected), 8)
			self.assertEqual(struct.unpack_from("<I", first.read_bytes(), 20)[0], 64)
			self.assertEqual(assets._dds_info(first), (8, 8, 64))

	def test_normal_png_conversion_writes_full_normalized_mip_chain(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = root / "normal.png"
			Image.new("RGBA", (5, 3), (200, 128, 128, 255)).save(source)
			output = root / "normal.dds"
			assets._convert_png_to_dds(source, output, normal_map=True)
			info = assets._dds_chain_info(output)
			self.assertEqual((info["width"], info["height"], info["mip_levels"]), (5, 3, 3))
			self.assertEqual(info["level_dimensions"], [(5, 3), (2, 1), (1, 1)])
			self.assertEqual(info["payload_bytes"], output.stat().st_size - assets.DDS_HEADER_SIZE)
			header = output.read_bytes()
			self.assertNotEqual(struct.unpack_from("<I", header, 8)[0] & 0x20000, 0)
			self.assertEqual(struct.unpack_from("<I", header, 108)[0] & 0x400008, 0x400008)
			with Image.open(output) as decoded:
				self.assertEqual(decoded.size, (5, 3))
				self.assertEqual(decoded.mode, "RGBA")
				red, _, blue, _ = decoded.getpixel((2, 1))
				self.assertGreater(red, 190)
				self.assertLess(abs(blue - 128), 10)

	def test_one_pixel_normal_dds_has_no_mipmap_header_flags(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = root / "normal.png"
			Image.new("RGBA", (1, 1), (128, 128, 255, 90)).save(source)
			output = root / "normal.dds"
			assets._convert_png_to_dds(source, output, normal_map=True)
			info = assets._dds_chain_info(output)
			header = output.read_bytes()
			self.assertEqual(info["mip_levels"], 1)
			self.assertEqual(struct.unpack_from("<I", header, 28)[0], 0)
			self.assertEqual(struct.unpack_from("<I", header, 8)[0] & 0x20000, 0)
			self.assertEqual(struct.unpack_from("<I", header, 108)[0] & 0x400008, 0)

	def test_opposing_normal_vectors_cancel_to_positive_z(self):
		base = Image.new("RGBA", (2, 1))
		base.putdata([(255, 128, 128, 0), (0, 127, 127, 100)])
		levels = assets._normal_mip_chain(base)
		self.assertEqual(levels[-1].getpixel((0, 0)), (128, 128, 255, 50))
		for level in levels:
			level.close()

	def test_odd_normal_mip_uses_pillow_box_filtering(self):
		base = Image.new("RGBA", (5, 1))
		base.putdata([
			(128, 128, 255, 0), (128, 128, 255, 20), (255, 128, 128, 40),
			(128, 128, 255, 60), (128, 128, 255, 80),
		])
		levels = assets._normal_mip_chain(base)
		self.assertEqual(list(levels[1].getdata()), [(185, 128, 241, 20), (128, 128, 255, 70)])
		for level in levels:
			level.close()

	def test_shared_normal_and_color_image_is_split_by_semantic_use(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source_root = root / "source"
			output_root = root / "runtime"
			source_root.mkdir()
			Image.new("RGBA", (4, 4), (128, 128, 255, 255)).save(source_root / "shared.png")
			model = {
				"materials": [
					{
						"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}},
						"normalTexture": {"index": 0},
					},
					{"normalTexture": {"index": 1}},
				],
				"textures": [{"source": 0}, {"source": 0}],
				"images": [{"uri": "shared.png", "mimeType": "image/png"}],
			}
			assets._convert_model_images_to_dds(model, source_root, output_root)
			base_info = assets._validate_texture_sampler(model, model["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"], "base")
			normal_info = assets._validate_texture_sampler(model, model["materials"][0]["normalTexture"], "normal")
			normal_info_2 = assets._validate_texture_sampler(model, model["materials"][1]["normalTexture"], "second normal")
			self.assertNotEqual(base_info, normal_info)
			self.assertEqual(normal_info, normal_info_2)
			self.assertEqual(len(model["images"]), 2)
			self.assertEqual(assets._dds_chain_info(output_root / model["images"][base_info]["uri"])["mip_levels"], 1)
			self.assertEqual(assets._dds_chain_info(output_root / model["images"][normal_info]["uri"])["mip_levels"], 3)
			metadata = {}
			assets._add_texture_memory(metadata, model, output_root / "fixture.gltf")
			memory = metadata["texture_memory"]
			self.assertEqual(memory["runtime_mip_levels"], 3)
			self.assertEqual(memory["bc3_runtime_bytes"], sum(item["bc3_runtime_bytes"] for item in memory["images"]))
			self.assertEqual([item["mip_levels"] for item in memory["images"]], [1, 3])

	def test_normal_output_names_avoid_existing_color_paths(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source_root = root / "source"
			output_root = root / "runtime"
			source_root.mkdir()
			for name in ("shared.png", "shared.normal-2.png"):
				Image.new("RGBA", (4, 4), (128, 128, 255, 255)).save(source_root / name)
			model = {
				"materials": [{
					"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}},
					"normalTexture": {"index": 0},
					"emissiveTexture": {"index": 1},
				}],
				"textures": [{"source": 0}, {"source": 1}],
				"images": [
					{"uri": "shared.png", "mimeType": "image/png"},
					{"uri": "shared.normal-2.png", "mimeType": "image/png"},
				],
			}
			assets._convert_model_images_to_dds(model, source_root, output_root)
			uris = [image["uri"] for image in model["images"]]
			self.assertEqual(len(uris), len(set(uris)))
			self.assertTrue(all((output_root / uri).is_file() for uri in uris))

	def test_gltf_conversion_uses_required_dds_extension_and_texture_metadata(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source_root = root / "source"
			output_root = root / "runtime"
			source_root.mkdir()
			png = source_root / "base.png"
			Image.new("RGBA", (4, 4), (10, 20, 30, 40)).save(png)
			original = png.read_bytes()
			model = {
				"asset": {"version": "2.0"},
				"materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
				"textures": [{"source": 0}], "images": [{"uri": "base.png", "mimeType": "image/png"}],
				"meshes": [],
			}
			assets._convert_model_images_to_dds(model, source_root, output_root)
			assets._validate_gltf_basics(model, "converted fixture")
			self.assertEqual(model["extensionsUsed"], ["MSFT_texture_dds"])
			self.assertEqual(model["extensionsRequired"], ["MSFT_texture_dds"])
			self.assertEqual(model["textures"], [{"extensions": {"MSFT_texture_dds": {"source": 0}}}])
			self.assertEqual(model["images"], [{"uri": "base.dds", "mimeType": "image/vnd-ms.dds"}])
			self.assertEqual(assets._validate_texture_sampler(model, {"index": 0}, "fixture"), 0)
			self.assertEqual(png.read_bytes(), original)
			self.assertFalse((output_root / "base.png").exists())
			metadata = {}
			assets._add_texture_memory(metadata, model, output_root / "fixture.gltf")
			texture_memory = metadata["texture_memory"]
			self.assertEqual(texture_memory["runtime_texture_format"], "BC3/DXT5")
			self.assertEqual(texture_memory["compression_encoder"], "Pillow 11.3.0 DDS pixel_format=DXT5 default encoder")
			self.assertEqual(texture_memory["runtime_mip_levels"], 1)
			self.assertEqual(texture_memory["bc3_base_level_bytes"], 16)
			self.assertEqual(texture_memory["rgba8_base_level_bytes"], 64)
			self.assertEqual(texture_memory["images"][0]["format"], "BC3/DXT5")
			self.assertEqual(texture_memory["images"][0]["mip_levels"], 1)

	def test_corrupt_dds_header_format_and_mip_chain_are_rejected(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			valid = root / "valid.dds"
			Image.new("RGBA", (4, 4), (10, 20, 30, 40)).save(valid, format="DDS", pixel_format="DXT5")
			original = valid.read_bytes()
			for name, damaged in (
				("header", b"bad!" + original[4:]),
				("format", original[:84] + b"DXT1" + original[88:]),
				("mips", original[:28] + struct.pack("<I", 2) + original[32:]),
				("cubemap", original[:112] + struct.pack("<I", 0x200) + original[116:]),
				("payload", original[:-1]),
			):
				with self.subTest(name=name):
					path = root / f"{name}.dds"
					path.write_bytes(damaged)
					with self.assertRaises(assets.AssetPreparationError):
						assets._dds_info(path)

	def test_converter_requires_exact_pillow_version_only_when_converting(self):
		with patch.object(PIL, "__version__", "11.2.0"):
			with self.assertRaisesRegex(assets.AssetPreparationError, "Pillow 11.3.0.*pip install Pillow==11.3.0"):
				assets._convert_png_to_dds(Path("unused.png"), Path("unused.dds"))

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
			with patch.object(assets, "_require_pillow", side_effect=AssertionError("verification must stay Pillow-free")):
				_, errors = assets.verify_prepared_cache(cache, lock_path=lock)
			self.assertEqual(errors, [])
			manifest["files"] = {"meshes/model.gltf": hashlib.sha256(b"forged").hexdigest()}
			(cache / "prepared.json").write_text(json.dumps(manifest), encoding="utf-8")
			with patch.object(assets, "_require_pillow", side_effect=AssertionError("verification must stay Pillow-free")):
				_, errors = assets.verify_prepared_cache(cache, lock_path=lock)
			self.assertIn("prepared asset file map does not match pinned runtime file hashes", errors)

	def _legacy_cache_fixture(self, root: Path) -> tuple[Path, Path]:
		cache = root / "benchmark-assets"
		(cache / "source" / "flight_helmet").mkdir(parents=True)
		(cache / "source" / "sponza").mkdir(parents=True)
		(cache / "source" / "sponza" / "main_sponza.zip").write_bytes(b"pinned source archive")
		(cache / "user-data.txt").write_text("preserve me")
		(cache / "prepared.json").write_text(json.dumps({
			"schema_version": 1,
			"profile": "engine-pbr-opaque-bc3-v1",
			"converter_version": "krisp-benchmark-assets/3",
		}))
		lock = root / "assets.lock.json"
		lock.write_text(json.dumps({"schema_version": 1, "profile": assets.PROFILE,
			"expected": {"sponza_views": []}, "sources": {}, "sponza_tangent_repairs": {"changes": []}}))
		return cache, lock

	def _mock_preparation_pipeline(self, cache: Path, lock: Path):
		def copy_sources(lock_value, stage, source_directory):
			self.assertEqual(source_directory, cache / "source")
			return stage / "source" / "flight_helmet", stage / "source" / "sponza" / "main_sponza.zip", {
				"flight_helmet": {"source_files": {}}, "sponza": {"source_files": {}}
			}
		return (
			patch.object(assets, "_copy_pinned_sources", side_effect=copy_sources),
			patch.object(assets, "_prepare_flight_helmet", return_value=({}, {"deltas": {}})),
			patch.object(assets, "_extract_sponza_inputs", return_value=(Path("source.gltf"), {}, {})),
			patch.object(assets, "_prepare_sponza", return_value=({}, {})),
			patch.object(assets, "_prepared_file_hashes", return_value={"meshes/fixture.dds": "derived-hash"}),
			patch.object(assets, "verify_prepared_cache", return_value=({}, [])),
		)

	def test_recognized_legacy_cache_is_rebuilt_from_cached_sources_and_backed_up(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			cache, lock = self._legacy_cache_fixture(root)
			self.assertTrue(assets.needs_preparation(cache))
			pipeline = self._mock_preparation_pipeline(cache, lock)
			with pipeline[0], pipeline[1], pipeline[2], pipeline[3], pipeline[4], pipeline[5]:
				assets.prepare_assets(cache, lock_path=lock)
			self.assertEqual(assets.needs_preparation(cache), False)
			manifest = json.loads((cache / "prepared.json").read_text())
			self.assertEqual(manifest["profile"], assets.PROFILE)
			self.assertEqual(manifest["converter_version"], assets.CONVERTER_VERSION)
			backups = list(root.glob(".benchmark-assets.backup-*"))
			self.assertEqual(len(backups), 1)
			self.assertEqual((backups[0] / "user-data.txt").read_text(), "preserve me")

	def test_legacy_cache_publication_failure_restores_original_cache(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			cache, lock = self._legacy_cache_fixture(root)
			pipeline = self._mock_preparation_pipeline(cache, lock)
			real_replace = assets.os.replace
			replacements = 0
			def fail_stage_publish(source, destination):
				nonlocal replacements
				replacements += 1
				if replacements == 2:
					raise OSError("fixture publish failure")
				real_replace(source, destination)
			with pipeline[0], pipeline[1], pipeline[2], pipeline[3], pipeline[4], pipeline[5], \
				patch.object(assets.os, "replace", side_effect=fail_stage_publish):
				with self.assertRaisesRegex(OSError, "fixture publish failure"):
					assets.prepare_assets(cache, lock_path=lock)
			self.assertEqual(json.loads((cache / "prepared.json").read_text())["profile"], "engine-pbr-opaque-bc3-v1")
			self.assertEqual((cache / "user-data.txt").read_text(), "preserve me")
			self.assertEqual(list(root.glob(".benchmark-assets.backup-*")), [])

	def test_current_cache_is_not_migrated_and_unknown_cache_identity_is_refused(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			cache = root / "current"
			cache.mkdir()
			(cache / "prepared.json").write_text(json.dumps({
				"profile": assets.PROFILE, "converter_version": assets.CONVERTER_VERSION,
			}))
			self.assertFalse(assets.needs_preparation(cache))
			unknown = root / "unknown"
			unknown.mkdir()
			(unknown / "prepared.json").write_text(json.dumps({
				"profile": "unrecognized-profile", "converter_version": "unrecognized-converter",
			}))
			with self.assertRaisesRegex(assets.AssetPreparationError, "unrecognized recipe"):
				assets.needs_preparation(unknown)

	def test_cached_source_layout_is_reused_and_revalidated_during_migration(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			sources = root / "cache-source"
			flight_file = sources / "flight_helmet" / "glTF" / "fixture.bin"
			flight_file.parent.mkdir(parents=True)
			flight_file.write_bytes(b"pinned flight input")
			archive = sources / "sponza" / "main_sponza.zip"
			archive.parent.mkdir()
			archive.write_bytes(b"pinned sponza input")
			lock = {
				"sources": {
					"flight_helmet": {"repository": "fixture", "revision": "pinned", "license": "fixture",
						"files": [{"path": "glTF/fixture.bin", "sha256": assets.sha256_file(flight_file)}]},
					"sponza": {"filename": archive.name, "sha256": assets.sha256_file(archive),
						"size_bytes": archive.stat().st_size, "url": "fixture://unused", "license": "fixture"},
				},
			}
			stage = root / "stage"
			with patch.object(assets, "_download", side_effect=AssertionError("migration must use cached pinned sources")):
				flight_root, copied_archive, source_metadata = assets._copy_pinned_sources(lock, stage, sources)
			self.assertEqual((flight_root / "glTF" / "fixture.bin").read_bytes(), flight_file.read_bytes())
			self.assertEqual(copied_archive.read_bytes(), archive.read_bytes())
			self.assertEqual(source_metadata["flight_helmet"]["source_files"]["glTF/fixture.bin"], assets.sha256_file(flight_file))


if __name__ == "__main__":
	unittest.main()
