import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from applications.rendering_fidelity import fidelity_assets as assets


def digest(data):
	return hashlib.sha256(data).hexdigest()


class FidelityAssetCacheTests(unittest.TestCase):
	def make_source(self, root, extensions_used=()):
		model_root = root / "Models" / "Sample" / "glTF"
		model_root.mkdir(parents=True)
		files = {
			"buffer.bin": b"geometry bytes",
			"surface.png": b"texture bytes",
			"Sample.gltf": json.dumps({
				"asset": {"version": "2.0"}, "extensionsUsed": list(extensions_used),
				"buffers": [{"uri": "buffer.bin"}], "images": [{"uri": "surface.png"}],
			}).encode(),
		}
		for name, data in files.items():
			(model_root / name).write_bytes(data)
		readme = root / "Models" / "Sample" / "README.md"
		readme.write_text("Sample asset CC0 notice\n")
		lock = {
			"schema_version": 1,
			"repository": assets.REPOSITORY,
			"source_commit": "a" * 40,
			"assets": {"Sample": {
				"gltf": "Models/Sample/glTF/Sample.gltf",
				"license": "CC0-1.0",
				"files": [
					{"path": f"Models/Sample/glTF/{name}", "sha256": digest(data), "size_bytes": len(data)}
					for name, data in files.items()
				] + [{"path": "Models/Sample/README.md", "sha256": digest(readme.read_bytes()),
					"size_bytes": readme.stat().st_size}],
			}},
		}
		lock_path = root / "assets.lock.json"
		lock_path.write_text(json.dumps(lock))
		return lock_path

	def test_preparation_retains_glTF_dependencies_notice_and_verifies_offline(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = root / "source"
			lock_path = self.make_source(source)
			cache = root / "cache"
			manifest = assets.prepare_assets(cache, source, lock_path)
			entry = manifest["assets"]["Sample"]
			self.assertEqual(entry["gltf"], "meshes/Sample/glTF/Sample.gltf")
			self.assertEqual(set(entry["files"]), {
				"meshes/Sample/glTF/Sample.gltf", "meshes/Sample/glTF/buffer.bin", "meshes/Sample/glTF/surface.png",
			})
			self.assertEqual(manifest["notices"]["Sample"]["notices/Sample/README.md"],
				digest(b"Sample asset CC0 notice\n"))
			with patch.object(assets.urllib.request, "urlopen", side_effect=AssertionError("verification must be offline")):
				verified, errors = assets.verify_cache(cache, lock_path)
			self.assertEqual(verified, manifest)
			self.assertEqual(errors, [])
			model = json.loads((cache / entry["gltf"]).read_text())
			self.assertTrue((cache / "meshes/Sample/glTF" / model["buffers"][0]["uri"]).is_file())

	def test_tampered_dependency_is_rejected_and_failed_prepare_preserves_existing_cache(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = root / "source"
			lock_path = self.make_source(source)
			cache = root / "cache"
			assets.prepare_assets(cache, source, lock_path)
			before = (cache / assets.MANIFEST_NAME).read_bytes()
			(source / "Models/Sample/glTF/buffer.bin").write_bytes(b"tampered")
			with self.assertRaises(assets.AssetPreparationError):
				assets.prepare_assets(cache, source, lock_path)
			self.assertEqual((cache / assets.MANIFEST_NAME).read_bytes(), before)
			(cache / "meshes/Sample/glTF/surface.png").write_bytes(b"tampered")
			_, errors = assets.verify_cache(cache, lock_path)
			self.assertTrue(any("SHA256 mismatch" in error for error in errors))

	def test_unsupported_extension_is_reported_without_stripping_source_data(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			source = root / "source"
			lock_path = self.make_source(source, extensions_used=("KHR_materials_unobtainium",))
			with self.assertRaisesRegex(assets.AssetPreparationError, "unsupported glTF extension"):
				assets.prepare_assets(root / "cache", source, lock_path)
			self.assertIn("KHR_materials_unobtainium",
				(source / "Models/Sample/glTF/Sample.gltf").read_text())


if __name__ == "__main__":
	unittest.main()
