import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import pack_kasset


class PackKassetTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def read_index(self, package_path):
        archive = package_path.read_bytes()
        index_size = struct.unpack_from("<Q", archive, 16)[0]
        index_end = 24 + index_size
        return archive, json.loads(archive[24:index_end]), index_end

    def write_glb(self, path, document):
        json_chunk = json.dumps(document, separators=(",", ":")).encode("utf-8")
        json_chunk += b" " * ((4 - len(json_chunk) % 4) % 4)
        total_size = 12 + 8 + len(json_chunk)
        path.write_bytes(
            struct.pack("<4sII", b"glTF", 2, total_size)
            + struct.pack("<II", len(json_chunk), 0x4E4F534A)
            + json_chunk
        )

    def test_direct_gltf_exports_stable_selectors_and_embeds_dependencies(self):
        source_dir = self.root / "models"
        source_dir.mkdir()
        source_path = source_dir / "actor.gltf"
        document = {
            "scene": 0,
            "scenes": [{"name": "Main", "nodes": [0, 1, 2]}],
            "nodes": [
                {"name": "Duplicate", "mesh": 0, "skin": 0},
                {"name": "Duplicate", "mesh": 1, "skin": 1},
                {"mesh": 2},
            ],
            "meshes": [
                {"primitives": [{"material": 0}, {"material": 1}]},
                {"primitives": [{"material": 0}]},
                {"primitives": [{}]},
            ],
            "materials": [{"name": "Duplicate"}, {"name": "Duplicate"}],
            "images": [{"name": "Duplicate", "uri": "textures/albedo.png"}],
            "skins": [
                {"name": "Duplicate", "joints": [0]},
                {"name": "Duplicate", "joints": [1]},
            ],
            "animations": [
                {
                    "name": "Move",
                    "samplers": [{"input": 0, "output": 1}],
                    "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}],
                },
                {
                    "name": "Move",
                    "samplers": [{"input": 0, "output": 1}],
                    "channels": [{"sampler": 0, "target": {"node": 1, "path": "rotation"}}],
                },
            ],
            "buffers": [{"uri": "geometry.bin", "byteLength": 4}],
        }
        source_path.write_text(json.dumps(document), encoding="utf-8")
        (source_dir / "geometry.bin").write_bytes(b"geom")
        (source_dir / "textures").mkdir()
        (source_dir / "textures" / "albedo.png").write_bytes(b"pixels")

        package_path = self.root / "actor.kasset"
        pack_kasset.pack(source_path, package_path)
        archive, index, index_end = self.read_index(package_path)
        exports = index["exports"]
        names = [export["name"] for export in exports]
        self.assertEqual(len(names), len(set(names)))

        by_kind = {}
        for export in exports:
            by_kind.setdefault(export["kind"], []).append(export)
        self.assertEqual(
            {kind: len(by_kind.get(kind, [])) for kind in ("mesh", "material", "texture", "skeleton", "animation", "assembly")},
            {"mesh": 4, "material": 2, "texture": 1, "skeleton": 2, "animation": 2, "assembly": 1},
        )
        self.assertEqual(
            {tuple(sorted(export["selector"].items())) for export in by_kind["mesh"]},
            {
                (("node", 0), ("primitive", 0), ("scene", 0)),
                (("node", 0), ("primitive", 1), ("scene", 0)),
                (("node", 1), ("primitive", 0), ("scene", 0)),
                (("node", 2), ("primitive", 0), ("scene", 0)),
            },
        )
        self.assertEqual(
            {export["selector"]["material"] for export in by_kind["material"]}, {0, 1}
        )
        self.assertEqual(
            {export["selector"]["image"] for export in by_kind["texture"]}, {0}
        )
        self.assertEqual(
            {export["selector"]["skin"] for export in by_kind["skeleton"]}, {0, 1}
        )
        self.assertEqual(
            {
                (export["selector"]["animation"], export["selector"]["skin"])
                for export in by_kind["animation"]
            },
            {(0, 0), (1, 1)},
        )
        self.assertEqual(by_kind["assembly"][0]["selector"], {"scene": 0})
        for export in exports:
            self.assertTrue(export["name"].startswith(f"actor.{export['kind']}.") or export["name"] == "actor.texture")
        self.assertEqual(
            [entry["path"] for entry in index["files"]],
            ["models/actor.gltf", "models/geometry.bin", "models/textures/albedo.png"],
        )
        for entry in index["files"]:
            start = entry["offset"]
            self.assertGreaterEqual(start, index_end)
            self.assertEqual(
                archive[start : start + entry["size"]],
                {
                    "models/actor.gltf": source_path.read_bytes(),
                    "models/geometry.bin": b"geom",
                    "models/textures/albedo.png": b"pixels",
                }[entry["path"]],
            )

        repeated_path = self.root / "actor-copy.kasset"
        pack_kasset.pack(source_path, repeated_path)
        self.assertEqual(package_path.read_bytes(), repeated_path.read_bytes())

    def test_direct_glb_and_standalone_image_are_packable(self):
        glb_path = self.root / "scene.glb"
        self.write_glb(glb_path, {"scene": 0, "scenes": [{"nodes": []}], "nodes": []})
        glb_package = self.root / "scene.kasset"
        pack_kasset.pack(glb_path, glb_package)
        _, glb_index, _ = self.read_index(glb_package)
        self.assertEqual([entry["path"] for entry in glb_index["files"]], ["models/scene.glb"])
        self.assertEqual(glb_index["exports"], [{"name": "scene.assembly.0", "kind": "assembly", "file": "models/scene.glb", "selector": {"scene": 0}}])

        image_path = self.root / "surface.png"
        image_path.write_bytes(b"image-bytes")
        image_package = self.root / "surface.kasset"
        pack_kasset.pack(image_path, image_package)
        archive, image_index, index_end = self.read_index(image_package)
        self.assertEqual(image_index["exports"], [{"name": "surface.texture", "kind": "texture", "file": "textures/surface.png", "selector": {}}])
        entry = image_index["files"][0]
        self.assertGreaterEqual(entry["offset"], index_end)
        self.assertEqual(archive[entry["offset"] : entry["offset"] + entry["size"]], b"image-bytes")

    def test_direct_gltf_rejects_animation_without_a_target_skin(self):
        source_path = self.root / "orphan.gltf"
        source_path.write_text(
            json.dumps(
                {
                    "scenes": [{"nodes": [0]}],
                    "nodes": [{}],
                    "animations": [
                        {
                            "samplers": [{"input": 0, "output": 1}],
                            "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}],
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        with self.assertRaises(ValueError):
            pack_kasset.pack(source_path, self.root / "orphan.kasset")

    def test_collects_percent_decoded_gltf_dependencies_and_writes_absolute_ranges(self):
        source_dir = self.root / "source"
        source_dir.mkdir()
        document = {"buffers": [{"uri": "mesh%20data.bin"}], "images": [{"uri": "tex.png"}]}
        (source_dir / "asset.gltf").write_text(json.dumps(document), encoding="utf-8")
        (source_dir / "mesh data.bin").write_bytes(b"mesh")
        (source_dir / "tex.png").write_bytes(b"image")
        recipe = {
            "files": [{"path": "models/asset.gltf", "source": "source/asset.gltf"}],
            "exports": [{"name": "asset", "kind": "assembly", "file": "models/asset.gltf", "selector": {"scene": 0}}],
        }
        recipe_path = self.root / "recipe.json"
        recipe_path.write_text(json.dumps(recipe), encoding="utf-8")
        package_path = self.root / "asset.kasset"
        pack_kasset.pack(recipe_path, package_path)

        archive = package_path.read_bytes()
        self.assertEqual(archive[:8], pack_kasset.MAGIC)
        index_size = struct.unpack_from("<Q", archive, 16)[0]
        index_end = 24 + index_size
        index = json.loads(archive[24:index_end])
        self.assertEqual(
            [entry["path"] for entry in index["files"]],
            ["models/asset.gltf", "models/mesh data.bin", "models/tex.png"],
        )
        for entry in index["files"]:
            self.assertGreaterEqual(entry["offset"], index_end)
            start = entry["offset"]
            self.assertLessEqual(start + entry["size"], len(archive))
        mesh = next(entry for entry in index["files"] if entry["path"] == "models/mesh data.bin")
        self.assertEqual(archive[mesh["offset"] : mesh["offset"] + mesh["size"]], b"mesh")

        second_path = self.root / "asset-copy.kasset"
        pack_kasset.pack(recipe_path, second_path)
        self.assertEqual(package_path.read_bytes(), second_path.read_bytes())

    def test_rejects_external_missing_and_package_escaping_dependencies(self):
        source_dir = self.root / "source"
        source_dir.mkdir()
        recipe_path = self.root / "recipe.json"
        package_path = self.root / "asset.kasset"
        for uri in ("https://example.invalid/data.bin", "../../outside.bin", "missing.bin"):
            (source_dir / "asset.gltf").write_text(
                json.dumps({"buffers": [{"uri": uri}]}), encoding="utf-8"
            )
            recipe = {
                "files": [{"path": "models/asset.gltf", "source": "source/asset.gltf"}],
                "exports": [{"name": "asset", "kind": "assembly", "file": "models/asset.gltf", "selector": {}}],
            }
            recipe_path.write_text(json.dumps(recipe), encoding="utf-8")
            with self.subTest(uri=uri), self.assertRaises(ValueError):
                pack_kasset.pack(recipe_path, package_path)

    def test_collects_glb_json_chunk_dependencies(self):
        source_dir = self.root / "source"
        source_dir.mkdir()
        document = json.dumps({"images": [{"uri": "texture.bin"}]}).encode("utf-8")
        document += b" " * ((4 - len(document) % 4) % 4)
        total_size = 12 + 8 + len(document)
        glb = struct.pack("<4sII", b"glTF", 2, total_size)
        glb += struct.pack("<II", len(document), 0x4E4F534A) + document
        (source_dir / "asset.glb").write_bytes(glb)
        (source_dir / "texture.bin").write_bytes(b"image-data")
        recipe = {
            "files": [{"path": "models/asset.glb", "source": "source/asset.glb"}],
            "exports": [{"name": "asset", "kind": "assembly", "file": "models/asset.glb", "selector": {}}],
        }
        recipe_path = self.root / "recipe.json"
        recipe_path.write_text(json.dumps(recipe), encoding="utf-8")
        package_path = self.root / "asset.kasset"
        pack_kasset.pack(recipe_path, package_path)
        index_size = struct.unpack_from("<Q", package_path.read_bytes(), 16)[0]
        index = json.loads(package_path.read_bytes()[24 : 24 + index_size])
        self.assertIn("models/texture.bin", [entry["path"] for entry in index["files"]])


if __name__ == "__main__":
    unittest.main()
