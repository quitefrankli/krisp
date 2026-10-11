#!/usr/bin/env python3
"""Pack a source asset or recipe into a deterministic .kasset container.

Supported direct inputs are glTF/GLB models and PNG/JPG/JPEG/DDS images.
Models export every selector supported by the engine asset registry. Images
export as a standalone texture. Use a JSON recipe for explicit package layouts.

Recipe format:
{
  "files": [{"path": "models/asset.gltf", "source": "asset.gltf"}],
  "exports": [{"name": "asset", "kind": "assembly", "file": "models/asset.gltf",
               "selector": {"scene": 0}}]
}
File sources are resolved relative to the recipe. glTF/GLB buffer and image
URIs are collected using their package-relative paths.
"""

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import struct
import tempfile
from urllib.parse import urlsplit


MAGIC = b"KASSET\0\0"
VERSION = 1
FLAGS = 0
KINDS = {"mesh", "material", "texture", "skeleton", "animation", "assembly"}
SELECTORS = {"scene", "node", "primitive", "material", "image", "skin", "animation"}


def decode_uri_path(value):
    decoded = bytearray()
    index = 0
    while index < len(value):
        if value[index] != "%":
            decoded.extend(value[index].encode("utf-8"))
            index += 1
            continue
        if index + 2 >= len(value):
            raise ValueError(f"malformed percent escape in URI: {value}")
        try:
            decoded.append(int(value[index + 1 : index + 3], 16))
        except ValueError as error:
            raise ValueError(f"malformed percent escape in URI: {value}") from error
        index += 3
    try:
        result = decoded.decode("utf-8")
    except UnicodeDecodeError as error:
        raise ValueError(f"URI path is not UTF-8: {value}") from error
    if "\0" in result:
        raise ValueError(f"NUL byte in URI: {value}")
    return result


def canonical_member_path(value):
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise ValueError(f"invalid package member path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in ("", ".", "..") for part in value.split("/")):
        raise ValueError(f"invalid package member path: {value!r}")
    return value


def resolve_uri(document_path, uri):
    parsed = urlsplit(uri)
    if parsed.scheme or parsed.netloc or parsed.query or parsed.fragment or uri.startswith("/"):
        raise ValueError(f"external or unsupported glTF URI: {uri}")
    decoded = decode_uri_path(parsed.path)
    if "\0" in decoded or "\\" in decoded:
        raise ValueError(f"invalid glTF URI: {uri}")
    if "?" in decoded or "#" in decoded:
        raise ValueError(f"query or fragment is not supported in glTF URI: {uri}")
    if decoded.endswith("/"):
        raise ValueError(f"URI does not name a package member: {uri}")
    resolved = PurePosixPath(document_path).parent.joinpath(decoded)
    parts = []
    for part in resolved.parts:
        if part in ("", "."):
            continue
        if part == "..":
            if not parts:
                raise ValueError(f"glTF URI escapes package: {uri}")
            parts.pop()
        else:
            parts.append(part)
    if not parts:
        raise ValueError(f"invalid glTF URI: {uri}")
    return canonical_member_path("/".join(parts))


def gltf_document(path, content):
    if path.lower().endswith(".glb"):
        if len(content) < 20:
            raise ValueError(f"truncated GLB: {path}")
        magic, version, declared_size = struct.unpack_from("<4sII", content)
        if magic != b"glTF" or version != 2 or declared_size != len(content):
            raise ValueError(f"invalid GLB header: {path}")
        offset = 12
        while offset + 8 <= len(content):
            chunk_size, chunk_type = struct.unpack_from("<II", content, offset)
            offset += 8
            end = offset + chunk_size
            if end > len(content):
                raise ValueError(f"truncated GLB chunk: {path}")
            if chunk_type == 0x4E4F534A:
                return json.loads(content[offset:end].decode("utf-8").rstrip(" \t\r\n\0"))
            offset = end
        raise ValueError(f"GLB has no JSON chunk: {path}")
    return json.loads(content.decode("utf-8"))


def collect_dependencies(document_path, source_path, content):
    if not document_path.lower().endswith((".gltf", ".glb")):
        return []
    document = gltf_document(document_path, content)
    dependencies = []
    for group in ("buffers", "images"):
        for item in document.get(group, []):
            uri = item.get("uri")
            if not uri:
                continue
            if not isinstance(uri, str):
                raise ValueError(f"invalid glTF URI in {source_path}")
            if urlsplit(uri).scheme.lower() == "data":
                continue
            package_path = resolve_uri(document_path, uri)
            source_uri = decode_uri_path(urlsplit(uri).path)
            source = (source_path.parent / source_uri).resolve()
            if not source.is_file():
                raise ValueError(f"missing glTF dependency {uri!r} referenced by {source_path}")
            dependencies.append((package_path, source))
    return dependencies


def _pack_recipe(recipe, recipe_directory, output_path):
    if not isinstance(recipe, dict) or not isinstance(recipe.get("files"), list) or not isinstance(recipe.get("exports"), list):
        raise ValueError("recipe must contain files and exports arrays")

    members = {}
    for item in recipe["files"]:
        if not isinstance(item, dict):
            raise ValueError("file entries must be objects")
        member = canonical_member_path(item.get("path"))
        source_value = item.get("source")
        if not isinstance(source_value, str) or not source_value:
            raise ValueError(f"missing source for {member}")
        source = (recipe_directory / source_value).resolve()
        if not source.is_file():
            raise ValueError(f"missing source file: {source}")
        data = source.read_bytes()
        if member in members and members[member][0] != data:
            raise ValueError(f"conflicting sources for package member {member}")
        members[member] = (data, source)

    for member, (data, source) in list(members.items()):
        for dependency_member, dependency_source in collect_dependencies(member, source, data):
            dependency_data = dependency_source.read_bytes()
            if dependency_member in members and members[dependency_member][0] != dependency_data:
                raise ValueError(f"conflicting sources for package member {dependency_member}")
            members[dependency_member] = (dependency_data, dependency_source)

    exports = []
    names = set()
    for item in recipe["exports"]:
        if not isinstance(item, dict):
            raise ValueError("export entries must be objects")
        name, kind, member = item.get("name"), item.get("kind"), canonical_member_path(item.get("file"))
        if not isinstance(name, str) or not name or name in names:
            raise ValueError(f"duplicate or invalid export name: {name!r}")
        if not isinstance(kind, str) or kind not in KINDS:
            raise ValueError(f"unsupported export kind: {kind!r}")
        if member not in members:
            raise ValueError(f"export {name!r} references missing member {member!r}")
        selector = item.get("selector", {})
        if not isinstance(selector, dict) or any(key not in SELECTORS or type(value) is not int for key, value in selector.items()):
            raise ValueError(f"invalid selector for export {name!r}")
        names.add(name)
        exports.append({"name": name, "kind": kind, "file": member, "selector": selector})

    entries = []
    relative_offset = 0
    for member in sorted(members):
        data = members[member][0]
        entries.append({"path": member, "offset": relative_offset, "size": len(data)})
        relative_offset += len(data)
    exports.sort(key=lambda item: item["name"])
    base_offset = len(MAGIC) + struct.calcsize("<IIQ")
    while True:
        for entry in entries:
            entry["offset"] += base_offset
        index = json.dumps({"files": entries, "exports": exports}, sort_keys=True, separators=(",", ":")).encode("utf-8")
        if len(index) > 16 * 1024 * 1024:
            raise ValueError("package index exceeds 16 MiB")
        updated_base = len(MAGIC) + struct.calcsize("<IIQ") + len(index)
        if updated_base == base_offset:
            break
        # Return entries to payload-relative offsets before recalculating.
        for entry in entries:
            entry["offset"] -= base_offset
        base_offset = updated_base

    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output_path.parent, prefix=output_path.name + ".", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(MAGIC)
            stream.write(struct.pack("<IIQ", VERSION, FLAGS, len(index)))
            stream.write(index)
            for member in sorted(members):
                stream.write(members[member][0])
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output_path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def _authored_name(item):
    name = item.get("name")
    return name if isinstance(name, str) and name else None


def _named_export(stem, kind, indices, item):
    name = f"{stem}.{kind}.{indices}"
    authored_name = _authored_name(item)
    return name if authored_name is None else f"{name}.{authored_name}"


def _selectors(document, stem, member):
    exports = []
    nodes = document.get("nodes", [])
    meshes = document.get("meshes", [])
    scenes = document.get("scenes", [])

    for scene_index, scene in enumerate(scenes):
        exports.append({
            "name": _named_export(stem, "assembly", str(scene_index), scene),
            "kind": "assembly",
            "file": member,
            "selector": {"scene": scene_index},
        })
        visited = set()

        def visit(node_index):
            if type(node_index) is not int or node_index < 0 or node_index >= len(nodes):
                raise ValueError("invalid glTF node metadata")
            if node_index in visited:
                return
            visited.add(node_index)
            node = nodes[node_index]
            if "mesh" in node:
                mesh_index = node["mesh"]
                if type(mesh_index) is not int or mesh_index < 0 or mesh_index >= len(meshes):
                    raise ValueError("invalid glTF mesh metadata")
                mesh = meshes[mesh_index]
                primitives = mesh.get("primitives")
                if not isinstance(primitives, list):
                    raise ValueError("invalid glTF primitive metadata")
                authored_name = _authored_name(node) or _authored_name(mesh)
                for primitive_index in range(len(primitives)):
                    name = f"{stem}.mesh.{scene_index}.{node_index}.{primitive_index}"
                    if authored_name is not None:
                        name += f".{authored_name}"
                    exports.append({
                        "name": name,
                        "kind": "mesh",
                        "file": member,
                        "selector": {
                            "scene": scene_index,
                            "node": node_index,
                            "primitive": primitive_index,
                        },
                    })
            children = node.get("children", [])
            if not isinstance(children, list):
                raise ValueError("invalid glTF node metadata")
            for child in children:
                visit(child)

        roots = scene.get("nodes", [])
        if not isinstance(roots, list):
            raise ValueError("invalid glTF scene metadata")
        for node_index in roots:
            visit(node_index)

    for group, kind, selector_key in (
        ("materials", "material", "material"),
        ("images", "texture", "image"),
        ("skins", "skeleton", "skin"),
    ):
        for index, item in enumerate(document.get(group, [])):
            exports.append({
                "name": _named_export(stem, kind, str(index), item),
                "kind": kind,
                "file": member,
                "selector": {selector_key: index},
            })

    skins = document.get("skins", [])
    for animation_index, animation in enumerate(document.get("animations", [])):
        matched = False
        channels = animation.get("channels", [])
        for skin_index, skin in enumerate(skins):
            joints = skin.get("joints", [])
            targets_skin = any(
                channel.get("target", {}).get("node", -1) in joints
                for channel in channels
            )
            if targets_skin:
                matched = True
                name = f"{stem}.animation.{animation_index}.{skin_index}"
                authored_name = _authored_name(animation)
                if authored_name is not None:
                    name += f".{authored_name}"
                exports.append({
                    "name": name,
                    "kind": "animation",
                    "file": member,
                    "selector": {"animation": animation_index, "skin": skin_index},
                })
        if not matched:
            raise ValueError("animation does not target a supported skeletal rig")
    return exports


def _direct_recipe(source_path):
    source_path = Path(source_path).resolve()
    suffix = source_path.suffix.lower()
    stem = source_path.stem or "asset"
    if suffix in (".png", ".jpg", ".jpeg", ".dds"):
        member = canonical_member_path(f"textures/{source_path.name}")
        return {
            "files": [{"path": member, "source": str(source_path)}],
            "exports": [{"name": f"{stem}.texture", "kind": "texture", "file": member}],
        }
    if suffix not in (".gltf", ".glb"):
        raise ValueError(f"unsupported direct input type: {source_path.suffix or source_path.name}")
    member = canonical_member_path(f"models/{source_path.name}")
    content = source_path.read_bytes()
    document = gltf_document(member, content)
    return {
        "files": [{"path": member, "source": str(source_path)}],
        "exports": _selectors(document, stem, member),
    }


def pack(input_path, output_path):
    input_path = Path(input_path).resolve()
    if input_path.suffix.lower() == ".json":
        recipe = json.loads(input_path.read_text(encoding="utf-8"))
    else:
        recipe = _direct_recipe(input_path)
    _pack_recipe(recipe, input_path.parent, output_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="glTF/GLB model, PNG/JPG/JPEG/DDS image, or JSON package recipe")
    parser.add_argument("output", help="output .kasset path")
    arguments = parser.parse_args()
    try:
        pack(arguments.input, arguments.output)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
