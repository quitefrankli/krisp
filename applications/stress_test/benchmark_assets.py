"""Prepare the pinned, offline assets used by the stress-test benchmark."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import stat
import struct
import sys
import urllib.error
import urllib.request
import uuid
import zipfile
import zlib
from pathlib import Path, PurePosixPath
from typing import Any, Iterable
from urllib.parse import unquote


PROFILE = "engine-pbr-opaque-v1"
CONVERTER_VERSION = "krisp-benchmark-assets/2"
LOCK_PATH = Path(__file__).with_name("assets.lock.json")
GLTF_SCENE = "NewSponza_Main_glTF_003.gltf"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
_SUPPORTED_GLTF_EXTENSIONS = {"KHR_lights_punctual", "KHR_materials_transmission"}
_MATERIAL_TEXTURE_SLOTS = (
	("baseColorTexture", "pbrMetallicRoughness"),
	("metallicRoughnessTexture", "pbrMetallicRoughness"),
	("normalTexture", None),
	("emissiveTexture", None),
	("occlusionTexture", None),
)


class AssetPreparationError(RuntimeError):
	"""A pinned source or prepared asset failed validation."""


def sha256_file(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def _load_json(path: Path) -> dict[str, Any]:
	try:
		value = json.loads(path.read_text(encoding="utf-8"))
	except (OSError, json.JSONDecodeError) as exc:
		raise AssetPreparationError(f"cannot read {path}: {exc}") from exc
	if not isinstance(value, dict):
		raise AssetPreparationError(f"{path} must contain a JSON object")
	return value


def _safe_relative_path(name: str) -> Path:
	decoded = unquote(name)
	if not decoded or "\\" in decoded or "\x00" in decoded:
		raise AssetPreparationError(f"unsafe asset path {name!r}")
	if any(part in {"", ".", ".."} for part in decoded.split("/")):
		raise AssetPreparationError(f"unsafe asset path {name!r}")
	path = PurePosixPath(decoded)
	if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
		raise AssetPreparationError(f"unsafe asset path {name!r}")
	if path.parts and ":" in path.parts[0]:
		raise AssetPreparationError(f"unsafe asset path {name!r}")
	return Path(*path.parts)


def _checked_source_file(path: Path, expected_hash: str, label: str) -> None:
	if not path.is_file():
		raise AssetPreparationError(f"missing pinned source file {label}: {path}")
	actual = sha256_file(path)
	if actual != expected_hash:
		raise AssetPreparationError(
			f"SHA256 mismatch for {label}: expected {expected_hash}, got {actual}")


def _download(url: str, destination: Path, expected_size: int | None, expected_hash: str) -> None:
	destination.parent.mkdir(parents=True, exist_ok=True)
	partial = destination.with_name(destination.name + ".part")
	digest = hashlib.sha256()
	count = 0
	request = urllib.request.Request(url, headers={"User-Agent": "KrispBenchmarkAssetPrep/1"})
	try:
		with urllib.request.urlopen(request, timeout=60) as response, partial.open("wb") as output:
			while chunk := response.read(1024 * 1024):
				count += len(chunk)
				if expected_size is not None and count > expected_size:
					raise AssetPreparationError(f"download exceeds pinned size for {url}")
				output.write(chunk)
				digest.update(chunk)
		if expected_size is not None and count != expected_size:
			raise AssetPreparationError(
				f"download size mismatch for {url}: expected {expected_size}, got {count}")
		actual_hash = digest.hexdigest()
		if actual_hash != expected_hash:
			raise AssetPreparationError(
				f"download SHA256 mismatch for {url}: expected {expected_hash}, got {actual_hash}")
		os.replace(partial, destination)
	except (OSError, urllib.error.URLError, TimeoutError) as exc:
		partial.unlink(missing_ok=True)
		raise AssetPreparationError(f"failed to download {url}: {exc}") from exc
	finally:
		partial.unlink(missing_ok=True)


def _copy_pinned_sources(
	lock: dict[str, Any], stage: Path, source_directory: Path | None,
) -> tuple[Path, Path, dict[str, Any]]:
	flight = lock["sources"]["flight_helmet"]
	flight_root = stage / "source" / "flight_helmet"
	local_flight_root = source_directory / "FlightHelmet" if source_directory else None
	flight_manifest: dict[str, str] = {}
	for item in flight["files"]:
		relative = _safe_relative_path(item["path"])
		destination = flight_root / relative
		destination.parent.mkdir(parents=True, exist_ok=True)
		if local_flight_root is not None:
			local = local_flight_root / relative
			shutil.copyfile(local, destination)
		else:
			url = f"{flight['repository']}/raw/{flight['revision']}/2.0/FlightHelmet/{relative.as_posix()}"
			_download(url, destination, item.get("size_bytes"), item["sha256"])
		_checked_source_file(destination, item["sha256"], item["path"])
		flight_manifest[relative.as_posix()] = item["sha256"]

	sponza = lock["sources"]["sponza"]
	archive = stage / "source" / "sponza" / sponza["filename"]
	archive.parent.mkdir(parents=True, exist_ok=True)
	if source_directory is not None:
		local_archive = source_directory / sponza["filename"]
		if not local_archive.is_file():
			local_archive = source_directory / "Sponza" / sponza["filename"]
		shutil.copyfile(local_archive, archive)
	else:
		_download(sponza["url"], archive, sponza["size_bytes"], sponza["sha256"])
	_checked_source_file(archive, sponza["sha256"], "Intel Sponza source archive")
	source_metadata = {
		"flight_helmet": {
			"repository": flight["repository"], "revision": flight["revision"],
			"license": flight["license"], "source_files": flight_manifest,
		},
		"sponza": {
			"url": sponza["url"], "filename": sponza["filename"],
			"size_bytes": sponza["size_bytes"], "sha256": sponza["sha256"],
			"license": sponza["license"],
		},
	}
	return flight_root, archive, source_metadata


def _read_zip_member(archive: zipfile.ZipFile, info: zipfile.ZipInfo, maximum: int) -> bytes:
	if info.file_size > maximum:
		raise AssetPreparationError(f"ZIP member is too large: {info.filename}")
	try:
		with archive.open(info) as source:
			data = source.read(maximum + 1)
	except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
		raise AssetPreparationError(f"cannot read ZIP member {info.filename}: {exc}") from exc
	if len(data) != info.file_size:
		raise AssetPreparationError(f"ZIP member size mismatch: {info.filename}")
	return data


def _safe_zip_index(archive: zipfile.ZipFile) -> dict[str, zipfile.ZipInfo]:
	result: dict[str, zipfile.ZipInfo] = {}
	for info in archive.infolist():
		_safe_relative_path(info.filename.rstrip("/"))
		mode = (info.external_attr >> 16) & 0xFFFF
		if stat.S_ISLNK(mode):
			raise AssetPreparationError(f"ZIP symlink is not allowed: {info.filename}")
		if info.flag_bits & 0x1:
			raise AssetPreparationError(f"encrypted ZIP member is not allowed: {info.filename}")
		if info.filename in result:
			raise AssetPreparationError(f"duplicate ZIP member: {info.filename}")
		result[info.filename] = info
	return result


def _uri_path(uri: str, label: str) -> Path:
	if ":" in uri or "?" in uri or "#" in uri:
		raise AssetPreparationError(f"unsupported external or decorated {label} URI: {uri}")
	return _safe_relative_path(uri)


def _extract_sponza_inputs(archive_path: Path, stage: Path) -> tuple[Path, dict[str, Any], dict[str, str]]:
	try:
		archive = zipfile.ZipFile(archive_path)
	except (OSError, zipfile.BadZipFile) as exc:
		raise AssetPreparationError(f"invalid Intel Sponza ZIP archive: {exc}") from exc
	with archive:
		members = _safe_zip_index(archive)
		gltf_member = f"main_sponza/{GLTF_SCENE}"
		if gltf_member not in members:
			raise AssetPreparationError(f"Sponza source is missing {gltf_member}")
		gltf_bytes = _read_zip_member(archive, members[gltf_member], 16 * 1024 * 1024)
		try:
			model = json.loads(gltf_bytes)
		except (UnicodeDecodeError, json.JSONDecodeError) as exc:
			raise AssetPreparationError(f"invalid Sponza GLTF JSON: {exc}") from exc
		dependencies = {gltf_member, "main_sponza/credits_license.txt"}
		for buffer in model.get("buffers", []):
			dependencies.add("main_sponza/" + _uri_path(buffer.get("uri", ""), "buffer").as_posix())
		for image in model.get("images", []):
			if "uri" not in image:
				raise AssetPreparationError("Sponza images must use file URIs")
			dependencies.add("main_sponza/" + _uri_path(image["uri"], "image").as_posix())
		missing = dependencies - members.keys()
		if missing:
			raise AssetPreparationError("Sponza ZIP is missing referenced files: " + ", ".join(sorted(missing)))
		output_root = stage / "meshes" / "sponza"
		source_root = stage / "source" / "sponza" / "base"
		for name in sorted(dependencies):
			info = members[name]
			data = _read_zip_member(archive, info, 512 * 1024 * 1024)
			relative = _safe_relative_path(name).relative_to("main_sponza")
			source_path = source_root / relative
			source_path.parent.mkdir(parents=True, exist_ok=True)
			source_path.write_bytes(data)
			if name != gltf_member and name != "main_sponza/credits_license.txt":
				destination = output_root / relative
				destination.parent.mkdir(parents=True, exist_ok=True)
				destination.write_bytes(data)
		license_path = stage / "source" / "sponza" / "credits_license.txt"
		license_path.parent.mkdir(parents=True, exist_ok=True)
		license_path.write_bytes(_read_zip_member(archive, members["main_sponza/credits_license.txt"], 1024 * 1024))
		source_files = {
			path.relative_to(stage / "source" / "sponza").as_posix(): sha256_file(path)
			for path in sorted((stage / "source" / "sponza").rglob("*")) if path.is_file() and path != archive_path
		}
		return source_root / GLTF_SCENE, model, source_files


def _accessor_data(model: dict[str, Any], buffers: list[bytes], accessor_index: int) -> tuple[dict[str, Any], memoryview, int]:
	accessor = model["accessors"][accessor_index]
	if accessor.get("sparse") or accessor.get("bufferView", -1) < 0:
		raise AssetPreparationError("sparse or bufferless accessors are unsupported")
	view = model["bufferViews"][accessor["bufferView"]]
	buffer_index = view.get("buffer", 0)
	if buffer_index < 0 or buffer_index >= len(buffers):
		raise AssetPreparationError("accessor references an invalid buffer")
	component_sizes = {5121: 1, 5123: 2, 5125: 4, 5126: 4}
	component_size = component_sizes.get(accessor.get("componentType"))
	component_counts = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
	component_count = component_counts.get(accessor.get("type"))
	if component_size is None or component_count is None:
		raise AssetPreparationError("accessor uses an unsupported component or shape")
	packed = component_size * component_count
	stride = view.get("byteStride", packed)
	if stride < packed or stride % component_size:
		raise AssetPreparationError("accessor has an invalid stride")
	start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
	count = accessor.get("count", -1)
	end = start if count == 0 else start + (count - 1) * stride + packed
	if start < 0 or count < 0 or end > len(buffers[buffer_index]):
		raise AssetPreparationError("accessor range is outside its buffer")
	return accessor, memoryview(buffers[buffer_index])[start:end], stride


def _validate_white_vertex_colours(model: dict[str, Any], buffers: list[bytes]) -> int:
	checked = 0
	for mesh in model.get("meshes", []):
		for primitive in mesh.get("primitives", []):
			index = primitive.get("attributes", {}).get("COLOR_0")
			if index is None:
				continue
			accessor, data, stride = _accessor_data(model, buffers, index)
			if accessor.get("type") != "VEC4" or accessor.get("componentType") != 5126:
				raise AssetPreparationError("Sponza COLOR_0 must be float VEC4 for safe redundancy validation")
			for vertex in range(accessor["count"]):
				values = struct.unpack_from("<4f", data, vertex * stride)
				if any(not math.isfinite(value) or value != 1.0 for value in values):
					raise AssetPreparationError("Sponza COLOR_0 contains non-white vertex colour data")
			checked += accessor["count"]
	return checked


def _validate_texture_sampler(model: dict[str, Any], texture_info: dict[str, Any], label: str) -> int:
	if texture_info.get("texCoord", 0) != 0:
		raise AssetPreparationError(f"{label} binds unsupported TEXCOORD_{texture_info['texCoord']}")
	if texture_info.get("extensions"):
		raise AssetPreparationError(f"{label} has unsupported texture-info extensions")
	index = texture_info.get("index", -1)
	textures = model.get("textures", [])
	if index < 0 or index >= len(textures):
		raise AssetPreparationError(f"{label} references an invalid texture")
	texture = textures[index]
	image_index = texture.get("source", -1)
	if image_index < 0 or image_index >= len(model.get("images", [])):
		raise AssetPreparationError(f"{label} references an invalid image")
	if texture.get("extensions"):
		raise AssetPreparationError(f"{label} texture has unsupported extensions")
	samplers = model.get("samplers", [])
	sampler_index = texture.get("sampler", -1)
	if sampler_index < 0:
		return image_index
	if sampler_index >= len(samplers):
		raise AssetPreparationError(f"{label} references an invalid sampler")
	sampler = samplers[sampler_index]
	if sampler.get("extensions"):
		raise AssetPreparationError(f"{label} sampler has unsupported extensions")
	if sampler.get("magFilter", 9729) != 9729:
		raise AssetPreparationError(f"{label} uses unsupported magnification filtering")
	if sampler.get("minFilter", 9987) not in {9729, 9986, 9987}:
		raise AssetPreparationError(f"{label} uses unsupported minification filtering")
	if sampler.get("wrapS", 10497) != sampler.get("wrapT", 10497):
		raise AssetPreparationError(f"{label} uses different S/T wrapping")
	if sampler.get("wrapS", 10497) not in {10497, 33071}:
		raise AssetPreparationError(f"{label} uses unsupported wrapping")
	return image_index


def _material_images(model: dict[str, Any], material: dict[str, Any], material_index: int) -> set[int]:
	label = material.get("name", f"material {material_index}")
	if material.get("extensions"):
		unknown = set(material["extensions"]) - {"KHR_materials_transmission"}
		if unknown:
			raise AssetPreparationError(f"{label} uses unsupported material extensions: {sorted(unknown)}")
	if material.get("pbrMetallicRoughness", {}).get("extensions"):
		raise AssetPreparationError(f"{label} uses unsupported PBR extensions")
	if material.get("normalTexture", {}).get("extensions"):
		raise AssetPreparationError(f"{label} uses unsupported normal-texture extensions")
	images: set[int] = set()
	for slot, block_name in _MATERIAL_TEXTURE_SLOTS:
		block = material.get(block_name, {}) if block_name else material
		texture_info = block.get(slot) if isinstance(block, dict) else None
		if texture_info is not None:
			images.add(_validate_texture_sampler(model, texture_info, f"{label} {slot}"))
	return images


def _read_source_buffers(model: dict[str, Any], model_path: Path) -> list[bytes]:
	buffers = []
	for buffer in model.get("buffers", []):
		uri = buffer.get("uri")
		if not uri:
			raise AssetPreparationError("benchmark assets require URI-backed buffers")
		path = model_path.parent / _uri_path(uri, "buffer")
		try:
			data = path.read_bytes()
		except OSError as exc:
			raise AssetPreparationError(f"cannot read buffer {path}: {exc}") from exc
		if len(data) != buffer.get("byteLength"):
			raise AssetPreparationError(f"buffer byteLength mismatch: {path}")
		buffers.append(data)
	return buffers


def _validate_gltf_basics(
	model: dict[str, Any],
	label: str,
	*,
	allow_sponza_light_bulb_roundoff: bool = False,
) -> None:
	if model.get("asset", {}).get("version") != "2.0":
		raise AssetPreparationError(f"{label} must be glTF 2.0")
	unknown_required = set(model.get("extensionsRequired", [])) - {"KHR_materials_transmission"}
	if unknown_required:
		raise AssetPreparationError(f"{label} declares unsupported required extensions: {sorted(unknown_required)}")
	unknown = set(model.get("extensionsUsed", [])) - _SUPPORTED_GLTF_EXTENSIONS
	if unknown:
		raise AssetPreparationError(f"{label} declares unsupported extensions: {sorted(unknown)}")
	if model.get("skins") or model.get("animations"):
		raise AssetPreparationError(f"{label} contains unsupported skins or animations")
	if model.get("materials") is None or model.get("meshes") is None:
		raise AssetPreparationError(f"{label} is missing materials or meshes")
	for index, material in enumerate(model.get("materials", [])):
		if material.get("alphaMode", "OPAQUE") not in {"OPAQUE", "MASK", "BLEND"}:
			raise AssetPreparationError(f"{label} material {index} has an unsupported alpha mode")
		if material.get("alphaMode", "OPAQUE") != "OPAQUE" and material.get("name") not in {"LensesMat", "dirt_decal"}:
			raise AssetPreparationError(f"{label} has an unapproved transparent material")
		if material.get("extensions"):
			unknown_material = set(material["extensions"]) - {"KHR_materials_transmission"}
			if unknown_material:
				raise AssetPreparationError(f"{label} material {index} has unsupported extensions")
		pbr = material.get("pbrMetallicRoughness", {})
		for field, default in (("baseColorFactor", [1, 1, 1, 1]), ("metallicFactor", 1), ("roughnessFactor", 1)):
			values = pbr.get(field, default)
			values = values if isinstance(values, list) else [values]
			if not values or any(not isinstance(value, (int, float)) or not math.isfinite(value) or value < 0 or value > 1 for value in values):
				raise AssetPreparationError(f"{label} material {index} has invalid {field}")
		emissive = material.get("emissiveFactor", [0, 0, 0])
		valid_emissive = (
			isinstance(emissive, list) and len(emissive) == 3
			and all(isinstance(value, (int, float)) and math.isfinite(value) and 0 <= value <= 1 for value in emissive)
		)
		approved_roundoff = (
			allow_sponza_light_bulb_roundoff
			and material.get("name") == "light_bulb"
			and emissive == [1.0, 0.99999994, 1.00000012]
		)
		if not valid_emissive and not approved_roundoff:
			raise AssetPreparationError(f"{label} material {index} has invalid emissiveFactor")
		if not isinstance(material.get("doubleSided", False), bool):
			raise AssetPreparationError(f"{label} material {index} has invalid doubleSided")
		if "normalTexture" in material:
			scale = material["normalTexture"].get("scale", 1.0)
			if not isinstance(scale, (int, float)) or not math.isfinite(scale):
				raise AssetPreparationError(f"{label} material {index} has invalid normalTexture scale")
	for index, material in enumerate(model.get("materials", [])):
		_material_images(model, material, index)
	for mesh in model.get("meshes", []):
		for primitive in mesh.get("primitives", []):
			if primitive.get("mode", 4) != 4:
				raise AssetPreparationError(f"{label} uses a non-triangle primitive")
			attributes = primitive.get("attributes", {})
			allowed = {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0", "TEXCOORD_1", "COLOR_0"}
			unknown_attributes = set(attributes) - allowed
			if unknown_attributes:
				raise AssetPreparationError(f"{label} uses unsupported vertex attributes: {sorted(unknown_attributes)}")
			if not {"POSITION", "NORMAL"}.issubset(attributes):
				raise AssetPreparationError(f"{label} primitive is missing POSITION or NORMAL")
			if "TEXCOORD_1" in attributes and "TEXCOORD_0" not in attributes:
				raise AssetPreparationError(f"{label} has TEXCOORD_1 without TEXCOORD_0")


def _mat_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
	return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def _node_matrix(node: dict[str, Any]) -> list[list[float]]:
	if "matrix" in node:
		values = node["matrix"]
		return [[float(values[c * 4 + r]) for c in range(4)] for r in range(4)]
	x, y, z, w = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
	sx, sy, sz = node.get("scale", [1.0, 1.0, 1.0])
	rotation = [
		[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
		[2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
		[2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
	]
	translation = node.get("translation", [0.0, 0.0, 0.0])
	return [[rotation[r][c] * (sx, sy, sz)[c] for c in range(3)] + [translation[r]] for r in range(3)] + [[0.0, 0.0, 0.0, 1.0]]


def _transform(matrix: list[list[float]], point: tuple[float, float, float]) -> tuple[float, float, float]:
	value = [*point, 1.0]
	return tuple(sum(matrix[r][c] * value[c] for c in range(4)) for r in range(3))


def _scene_metrics(model: dict[str, Any], buffers: list[bytes], label: str) -> dict[str, Any]:
	scenes = model.get("scenes", [])
	scene_index = model.get("scene", 0)
	if not scenes or scene_index < 0 or scene_index >= len(scenes):
		raise AssetPreparationError(f"{label} has no valid default scene")
	used_nodes: set[int] = set()
	used_meshes: set[int] = set()
	used_materials: set[int] = set()
	used_images: set[int] = set()
	positions: list[tuple[float, float, float]] = []
	vertex_count = index_count = primitive_count = 0

	def walk(node_index: int, parent: list[list[float]]) -> None:
		nonlocal vertex_count, index_count, primitive_count
		if node_index < 0 or node_index >= len(model.get("nodes", [])) or node_index in used_nodes:
			raise AssetPreparationError(f"{label} scene has invalid or multiply-parented nodes")
		used_nodes.add(node_index)
		node = model["nodes"][node_index]
		world = _mat_mul(parent, _node_matrix(node))
		mesh_index = node.get("mesh", -1)
		if mesh_index >= 0:
			if mesh_index >= len(model["meshes"]):
				raise AssetPreparationError(f"{label} node references an invalid mesh")
			used_meshes.add(mesh_index)
			for primitive in model["meshes"][mesh_index].get("primitives", []):
				primitive_count += 1
				attributes = primitive["attributes"]
				position = model["accessors"][attributes["POSITION"]]
				if position.get("type") != "VEC3" or position.get("componentType") != 5126:
					raise AssetPreparationError(f"{label} positions must be float VEC3")
				if "min" not in position or "max" not in position:
					raise AssetPreparationError(f"{label} positions need bounded accessor metadata")
				vertex_count += position["count"]
				for x in (position["min"][0], position["max"][0]):
					for y in (position["min"][1], position["max"][1]):
						for z in (position["min"][2], position["max"][2]):
							positions.append(_transform(world, (x, y, z)))
				index_accessor_index = primitive.get("indices")
				if index_accessor_index is None:
					primitive_indices = position["count"]
				else:
					index_accessor = model["accessors"][index_accessor_index]
					if index_accessor.get("type") != "SCALAR" or index_accessor.get("componentType") not in {5121, 5123, 5125}:
						raise AssetPreparationError(f"{label} indices use an unsupported accessor format")
					primitive_indices = index_accessor["count"]
					accessor, data, stride = _accessor_data(model, buffers, index_accessor_index)
					fmt = {5121: "B", 5123: "H", 5125: "I"}[accessor["componentType"]]
					for offset in range(accessor["count"]):
						value = struct.unpack_from("<" + fmt, data, offset * stride)[0]
						if value >= position["count"]:
							raise AssetPreparationError(f"{label} has an out-of-range mesh index")
				index_count += primitive_indices
				if primitive_indices % 3:
					raise AssetPreparationError(f"{label} triangle index count is not divisible by three")
				material_index = primitive.get("material", -1)
				if material_index >= 0:
					if material_index >= len(model["materials"]):
						raise AssetPreparationError(f"{label} primitive references an invalid material")
					used_materials.add(material_index)
					used_images.update(_material_images(model, model["materials"][material_index], material_index))
		for child in node.get("children", []):
			walk(child, world)

	identity = [[1.0 if r == c else 0.0 for c in range(4)] for r in range(4)]
	for root in scenes[scene_index].get("nodes", []):
		walk(root, identity)
	if not positions or not primitive_count:
		raise AssetPreparationError(f"{label} default scene contains no renderable geometry")
	minimum = [min(point[axis] for point in positions) for axis in range(3)]
	maximum = [max(point[axis] for point in positions) for axis in range(3)]
	vertex_bytes = vertex_count * 48
	index_bytes = index_count * 4
	return {
		"renderables_per_instance": primitive_count,
		"mesh_count": len(used_meshes),
		"material_count": len(used_materials),
		"texture_count": len(used_images),
		"vertex_count": vertex_count,
		"index_count": index_count,
		"triangle_count": index_count // 3,
		"bounds": {"min": minimum, "max": maximum},
		"resource_memory": {
			"decoded_vertex_bytes_at_48_per_vertex": vertex_bytes,
			"decoded_index_bytes_at_32_bits": index_bytes,
			"decoded_mesh_bytes": vertex_bytes + index_bytes,
			"vertex_layout_bytes": 48,
		},
	}


def _png_size(path: Path) -> tuple[int, int]:
	with path.open("rb") as image:
		header = image.read(24)
	if len(header) < 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
		raise AssetPreparationError(f"unsupported or invalid PNG image: {path}")
	return struct.unpack_from(">II", header, 16)


def _add_texture_memory(metadata: dict[str, Any], model: dict[str, Any], model_path: Path) -> None:
	images: dict[int, dict[str, Any]] = {}
	for index, image in enumerate(model.get("images", [])):
		uri = image.get("uri")
		if not uri:
			raise AssetPreparationError("benchmark images must use PNG file URIs")
		path = model_path.parent / _uri_path(uri, "image")
		width, height = _png_size(path)
		if width <= 0 or height <= 0:
			raise AssetPreparationError(f"invalid PNG dimensions in {path}")
		images[index] = {"path": path, "width": width, "height": height}
	used = set()
	for material in model.get("materials", []):
		for slot, block_name in _MATERIAL_TEXTURE_SLOTS:
			block = material.get(block_name, {}) if block_name else material
			info = block.get(slot) if isinstance(block, dict) else None
			if info is not None:
				used.add(model["textures"][info["index"]]["source"])
	texture_records = []
	decoded_rgba_bytes = 0
	for image_index in sorted(used):
		image = images[image_index]
		bytes_per_image = image["width"] * image["height"] * 4
		decoded_rgba_bytes += bytes_per_image
		texture_records.append({
			"image_index": image_index,
			"width": image["width"], "height": image["height"],
			"source_file_bytes": image["path"].stat().st_size,
			"rgba8_base_level_bytes": bytes_per_image,
			"rgba8_full_mipmap_estimate_bytes": (bytes_per_image * 4 + 2) // 3,
		})
	metadata["texture_memory"] = {
		"unique_referenced_images": len(used),
		"rgba8_base_level_bytes": decoded_rgba_bytes,
		"rgba8_full_mipmap_estimate_bytes": (decoded_rgba_bytes * 4 + 2) // 3,
		"images": texture_records,
	}


def _validate_zero_lights(model: dict[str, Any], label: str) -> None:
	if "KHR_lights_punctual" not in model.get("extensionsUsed", []):
		return
	lights = model.get("extensions", {}).get("KHR_lights_punctual", {}).get("lights", [])
	if not lights or any(light.get("intensity", 1.0) != 0.0 for light in lights):
		raise AssetPreparationError(f"{label} has non-zero or malformed punctual lights")
	for node in model.get("nodes", []):
		extension = node.get("extensions", {}).get("KHR_lights_punctual")
		if extension and extension.get("light", -1) >= len(lights):
			raise AssetPreparationError(f"{label} node references an invalid punctual light")


def _normalize_sponza_light_bulb_emissive(model: dict[str, Any]) -> dict[str, Any]:
	for material in model.get("materials", []):
		if material.get("name") != "light_bulb":
			continue
		expected = [1.0, 0.99999994, 1.00000012]
		if material.get("emissiveFactor") != expected:
			raise AssetPreparationError("Sponza light_bulb emissiveFactor differs from the approved source vector")
		material["emissiveFactor"][2] = 1.0
		return {
			"material": "light_bulb",
			"source_emissive_factor": expected,
			"prepared_emissive_factor": [1.0, 0.99999994, 1.0],
			"normalized_component": 2,
		}
	raise AssetPreparationError("Sponza source is missing the approved light_bulb material")


def _write_gltf(path: Path, model: dict[str, Any]) -> None:
	path.write_text(json.dumps(model, indent=2, sort_keys=True, ensure_ascii=False) + "\n", encoding="utf-8")


def _apply_flight_recipe(model: dict[str, Any]) -> dict[str, Any]:
	"""Apply the only permitted Flight Helmet material changes."""
	deltas = {"removed_occlusion_bindings": 0, "opaque_materials": [], "removed_transmission_extensions": 0}
	for material in model.get("materials", []):
		if "occlusionTexture" in material:
			del material["occlusionTexture"]
			deltas["removed_occlusion_bindings"] += 1
		if material.get("name") == "LensesMat":
			if material.get("alphaMode", "OPAQUE") != "BLEND":
				raise AssetPreparationError("pinned Flight Helmet LensesMat alpha mode changed")
			material["alphaMode"] = "OPAQUE"
			deltas["opaque_materials"].append("LensesMat")
		if "KHR_materials_transmission" in material.get("extensions", {}):
			del material["extensions"]["KHR_materials_transmission"]
			deltas["removed_transmission_extensions"] += 1
			if not material["extensions"]:
				del material["extensions"]
	for key in ("extensionsUsed", "extensionsRequired"):
		if "KHR_materials_transmission" in model.get(key, []):
			model[key].remove("KHR_materials_transmission")
			if not model[key]:
				del model[key]
	return deltas


def _prepare_flight_helmet(stage: Path, flight_root: Path) -> tuple[dict[str, Any], dict[str, Any]]:
	source_model = flight_root / "glTF" / "FlightHelmet.gltf"
	model = _load_json(source_model)
	_validate_gltf_basics(model, "Flight Helmet")
	buffers = _read_source_buffers(model, source_model)
	metrics = _scene_metrics(model, buffers, "Flight Helmet")
	deltas = _apply_flight_recipe(model)
	output = stage / "meshes" / "flight_helmet" / "glTF"
	output.mkdir(parents=True, exist_ok=True)
	for source in (flight_root / "glTF").iterdir():
		if source.is_file() and source.name != "FlightHelmet.gltf":
			shutil.copyfile(source, output / source.name)
	derived_gltf = output / "FlightHelmet.gltf"
	_write_gltf(derived_gltf, model)
	derived = _load_json(derived_gltf)
	_validate_gltf_basics(derived, "prepared Flight Helmet")
	derived_buffers = _read_source_buffers(derived, derived_gltf)
	prepared_metrics = _scene_metrics(derived, derived_buffers, "prepared Flight Helmet")
	prepared_metrics["import_scale"] = 0.7 / (metrics["bounds"]["max"][1] - metrics["bounds"]["min"][1])
	_add_texture_memory(prepared_metrics, derived, derived_gltf)
	return prepared_metrics, {
		"source_model_sha256": sha256_file(source_model),
		"derived_model_sha256": sha256_file(derived_gltf),
		"deltas": deltas,
	}


def _repair_pinned_tangents(model: dict[str, Any], buffers: list[bytes], output: Path, repairs: list[dict[str, Any]]) -> int:
	changed: dict[int, bytearray] = {}
	for repair in repairs:
		primitive = model["meshes"][repair["mesh"]]["primitives"][0]
		if primitive["attributes"].get("TANGENT") != repair["accessor"]:
			raise AssetPreparationError("Sponza tangent repair accessor differs from pinned source")
		accessor, data, stride = _accessor_data(model, buffers, repair["accessor"])
		vertex = repair["vertex"]
		if accessor["componentType"] != 5126 or accessor["type"] != "VEC4" or not 0 <= vertex < accessor["count"]:
			raise AssetPreparationError("Sponza tangent repair has an invalid accessor or vertex")
		before = struct.pack("<4f", *repair["before"])
		if bytes(data[vertex * stride:vertex * stride + 16]) != before:
			raise AssetPreparationError("Sponza tangent differs from the approved source value")
		view = model["bufferViews"][accessor["bufferView"]]
		buffer_index = view.get("buffer", 0)
		buffer = changed.setdefault(buffer_index, bytearray(buffers[buffer_index]))
		offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0) + vertex * stride
		buffer[offset:offset + 16] = struct.pack("<4f", *repair["after"])
	for index, buffer in changed.items():
		(output / _safe_relative_path(model["buffers"][index]["uri"])).write_bytes(buffer)
	return len(repairs)


def _prepare_sponza(stage: Path, sponza_source_gltf: Path, original_model: dict[str, Any], tangent_repairs: list[dict[str, Any]]) -> tuple[dict[str, Any], dict[str, Any]]:
	model = original_model
	_validate_gltf_basics(model, "Intel Sponza", allow_sponza_light_bulb_roundoff=True)
	_validate_zero_lights(model, "Intel Sponza")
	if any("KHR_materials_transmission" in material.get("extensions", {}) for material in model.get("materials", [])):
		raise AssetPreparationError("Intel Sponza has transmission materials outside the pinned preparation recipe")
	buffers = _read_source_buffers(model, sponza_source_gltf)
	white_vertices = _validate_white_vertex_colours(model, buffers)
	metrics = _scene_metrics(model, buffers, "Intel Sponza")
	emissive_delta = _normalize_sponza_light_bulb_emissive(model)
	output = stage / "meshes" / "sponza"
	derived_gltf = output / GLTF_SCENE
	repaired_tangents = _repair_pinned_tangents(model, buffers, output, tangent_repairs)
	_write_gltf(derived_gltf, model)
	prepared = _load_json(derived_gltf)
	_validate_gltf_basics(prepared, "prepared Intel Sponza")
	prepared_metrics = _scene_metrics(prepared, _read_source_buffers(prepared, derived_gltf), "prepared Intel Sponza")
	prepared_metrics["import_scale"] = 1.0
	prepared_metrics["light_position"] = [0.0, 12.0, 2.0]
	prepared_metrics["light_intensity"] = 800.0
	prepared_metrics["white_color_vertices_validated"] = white_vertices
	prepared_metrics["ignored_unbound_texcoord_1_primitives"] = sum(
		1 for mesh in model.get("meshes", []) for primitive in mesh.get("primitives", [])
		if "TEXCOORD_1" in primitive.get("attributes", {})
	)
	_add_texture_memory(prepared_metrics, prepared, derived_gltf)
	return prepared_metrics, {
		"source_model_sha256": sha256_file(sponza_source_gltf),
		"derived_model_sha256": sha256_file(derived_gltf),
		"normalized_emissive_factor": emissive_delta,
		"repaired_tangents": repaired_tangents,
		"removed_occlusion_bindings": 0,
		"white_color_vertices_validated": white_vertices,
		"source_geometry_retained": True,
	}


def _prepared_file_hashes(root: Path) -> dict[str, str]:
	return {
		path.relative_to(root).as_posix(): sha256_file(path)
		for path in sorted((root / "meshes").rglob("*")) if path.is_file()
	}


def verify_prepared_cache(
	cache_dir: Path,
	expected_lock_hash: str | None = None,
	lock_path: Path = LOCK_PATH,
) -> tuple[dict[str, Any], list[str]]:
	errors = []
	try:
		manifest = _load_json(cache_dir / "prepared.json")
	except AssetPreparationError as exc:
		return {}, [str(exc)]
	if manifest.get("schema_version") != 1:
		errors.append("prepared asset schema_version must be 1")
	if manifest.get("profile") != PROFILE:
		errors.append(f"prepared asset profile must be {PROFILE}")
	if expected_lock_hash and manifest.get("lock_sha256") != expected_lock_hash:
		errors.append("prepared asset lock fingerprint does not match this checkout")
	try:
		lock = _load_json(lock_path)
	except AssetPreparationError as exc:
		return manifest, errors + [f"cannot validate against asset lock: {exc}"]
	if manifest.get("lock_sha256") != sha256_file(lock_path):
		errors.append("prepared asset lock fingerprint does not match checked-in lock")
	if manifest.get("converter_version") != CONVERTER_VERSION:
		errors.append("prepared asset converter version does not match this checkout")
	files = manifest.get("files")
	if not isinstance(files, dict) or not files:
		errors.append("prepared asset manifest has no files checksum map")
		return manifest, errors
	if lock.get("expected_runtime_files") is not None and files != lock.get("expected_runtime_files"):
		errors.append("prepared asset file map does not match pinned runtime file hashes")
	for name, expected in files.items():
		try:
			relative = _safe_relative_path(name)
			path = (cache_dir / relative).resolve()
			path.relative_to(cache_dir.resolve())
		except (AssetPreparationError, ValueError):
			errors.append(f"prepared file path escapes cache: {name}")
			continue
		if not path.is_file():
			errors.append(f"prepared asset file is missing: {name}")
		elif sha256_file(path) != expected:
			errors.append(f"prepared asset checksum mismatch: {name}")
	for asset in ("flight_helmet", "sponza"):
		asset_data = manifest.get("assets", {}).get(asset, {})
		gltf = asset_data.get("gltf")
		if not isinstance(gltf, str) or gltf not in files:
			errors.append(f"prepared manifest is missing assets.{asset}.gltf or its checksum")
	for name, expected in lock.get("expected", {}).items():
		if name == "sponza_views":
			actual = manifest.get("assets", {}).get("sponza", {}).get("views")
		elif name == "sponza_normalized_emissive_factor":
			actual = manifest.get("assets", {}).get("sponza", {}).get("deltas", {}).get("normalized_emissive_factor")
		else:
			asset_name = "flight_helmet" if name.startswith("flight_helmet_") else "sponza"
			field = name.removeprefix("flight_helmet_") if asset_name == "flight_helmet" else name.removeprefix("sponza_")
			actual = manifest.get("assets", {}).get(asset_name, {}).get(field)
		if actual != expected:
			errors.append(f"prepared manifest metadata mismatch for {name}")
	if manifest.get("profile") != lock.get("profile") or manifest.get("schema_version") != lock.get("schema_version"):
		errors.append("prepared manifest identity does not match checked-in lock")
	return manifest, errors


def prepare_assets(
	cache_dir: Path,
	*,
	source_directory: Path | None = None,
	lock_path: Path = LOCK_PATH,
) -> Path:
	lock = _load_json(lock_path)
	if lock.get("schema_version") != 1 or lock.get("profile") != PROFILE:
		raise AssetPreparationError("asset lock schema or preparation profile is unsupported")
	lock_hash = sha256_file(lock_path)
	if cache_dir.exists():
		_, errors = verify_prepared_cache(cache_dir, lock_hash, lock_path)
		if errors:
			raise AssetPreparationError("existing asset cache is immutable and invalid: " + "; ".join(errors))
		return cache_dir
	cache_dir.parent.mkdir(parents=True, exist_ok=True)
	stage = cache_dir.parent / f".{cache_dir.name}.staging-{uuid.uuid4().hex}"
	stage.mkdir()
	try:
		flight_root, sponza_archive, source_metadata = _copy_pinned_sources(lock, stage, source_directory)
		flight_metrics, flight_deltas = _prepare_flight_helmet(stage, flight_root)
		sponza_gltf, original_sponza, sponza_source_files = _extract_sponza_inputs(sponza_archive, stage)
		sponza_metrics, sponza_deltas = _prepare_sponza(stage, sponza_gltf, original_sponza, lock["sponza_tangent_repairs"]["changes"])
		source_metadata["sponza"]["source_files"] = sponza_source_files
		assets = {
			"flight_helmet": {
				"gltf": "meshes/flight_helmet/glTF/FlightHelmet.gltf",
				**flight_metrics,
				"deltas": flight_deltas["deltas"],
			},
			"sponza": {
				"gltf": f"meshes/sponza/{GLTF_SCENE}",
				**sponza_metrics,
				"views": lock["expected"]["sponza_views"],
				"deltas": sponza_deltas,
			},
		}
		for name, expected in lock["expected"].items():
			if name == "sponza_views":
				continue
			if name == "sponza_normalized_emissive_factor":
				actual = assets["sponza"]["deltas"].get("normalized_emissive_factor")
				if actual != expected:
					raise AssetPreparationError(f"sponza normalization mismatch: expected {expected}, got {actual}")
				continue
			asset_name = "flight_helmet" if name.startswith("flight_helmet_") else "sponza"
			field = name.removeprefix("flight_helmet_") if asset_name == "flight_helmet" else name.removeprefix("sponza_")
			actual = assets[asset_name].get(field)
			if actual != expected:
				raise AssetPreparationError(f"{asset_name} {field} count mismatch: expected {expected}, got {actual}")
		manifest = {
			"schema_version": 1,
			"profile": PROFILE,
			"converter_version": CONVERTER_VERSION,
			"lock_sha256": lock_hash,
			"sources": source_metadata,
			"assets": assets,
			"files": _prepared_file_hashes(stage),
		}
		(stage / "prepared.json").write_text(
			json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
		_, errors = verify_prepared_cache(stage, lock_hash, lock_path)
		if errors:
			raise AssetPreparationError("prepared cache validation failed: " + "; ".join(errors))
		os.replace(stage, cache_dir)
		return cache_dir
	except Exception:
		shutil.rmtree(stage, ignore_errors=True)
		raise


def default_cache_path() -> Path:
	return Path(__file__).resolve().parents[2] / "build" / "benchmark-assets"


def build_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description="Prepare pinned offline benchmark assets")
	parser.add_argument("--default-asset-cache", type=Path, default=default_cache_path(), help=argparse.SUPPRESS)
	parser.add_argument("--source-directory", type=Path, help="use locally supplied pinned source files and archive")
	parser.add_argument("--lock-file", type=Path, default=LOCK_PATH, help=argparse.SUPPRESS)
	return parser


def main(argv: Iterable[str] | None = None) -> int:
	args = build_parser().parse_args(argv)
	try:
		path = prepare_assets(args.default_asset_cache, source_directory=args.source_directory, lock_path=args.lock_file)
	except AssetPreparationError as exc:
		print(f"prepare-assets: {exc}", file=sys.stderr)
		return 1
	print(path)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
