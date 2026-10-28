"""Prepare the pinned, offline assets used by the performance benchmark."""

from __future__ import annotations

import argparse
import copy
import hashlib
import io
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


PROFILE = "engine-pbr-opaque-bc3-normal-mips-v1"
CONVERTER_VERSION = "krisp-benchmark-assets/4"
PILLOW_VERSION = "11.3.0"
PILLOW_PIXEL_FORMAT = "DXT5"
PREVIOUS_PROFILES = {
	("engine-pbr-opaque-v1", "krisp-benchmark-assets/2"),
	("engine-pbr-opaque-bc3-v1", "krisp-benchmark-assets/3"),
}
LOCK_PATH = Path(__file__).with_name("assets.lock.json")
GLTF_SCENE = "NewSponza_Main_glTF_003.gltf"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DDS_MAGIC = b"DDS "
DDS_HEADER_SIZE = 128
DDS_DXT5_FOURCC = b"DXT5"
_SUPPORTED_GLTF_EXTENSIONS = {"KHR_lights_punctual", "KHR_materials_transmission", "MSFT_texture_dds"}
_SUPPORTED_REQUIRED_EXTENSIONS = {"KHR_materials_transmission", "MSFT_texture_dds"}
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
	local_flight_root = None
	if source_directory is not None:
		for candidate in (source_directory / "FlightHelmet", source_directory / "flight_helmet"):
			if all((candidate / _safe_relative_path(item["path"])).is_file() for item in flight["files"]):
				local_flight_root = candidate
				break
		if local_flight_root is None:
			raise AssetPreparationError(f"local Flight Helmet source files are missing under {source_directory}")
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
		candidates = (
			source_directory / sponza["filename"],
			source_directory / "Sponza" / sponza["filename"],
			source_directory / "sponza" / sponza["filename"],
		)
		local_archive = next((candidate for candidate in candidates if candidate.is_file()), candidates[0])
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
				if relative.suffix.lower() != ".png":
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
	texture_extensions = texture.get("extensions", {})
	if not isinstance(texture_extensions, dict):
		raise AssetPreparationError(f"{label} texture extensions must be an object")
	unknown_texture_extensions = set(texture_extensions) - {"MSFT_texture_dds"}
	if unknown_texture_extensions:
		raise AssetPreparationError(f"{label} texture has unsupported extensions: {sorted(unknown_texture_extensions)}")
	dds = texture_extensions.get("MSFT_texture_dds")
	if dds is None:
		image_index = texture.get("source", -1)
	else:
		if "source" in texture or not isinstance(dds, dict):
			raise AssetPreparationError(f"{label} DDS texture must use only MSFT_texture_dds.source")
		image_index = dds.get("source", -1)
	if image_index < 0 or image_index >= len(model.get("images", [])):
		raise AssetPreparationError(f"{label} references an invalid image")
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
	unknown_required = set(model.get("extensionsRequired", [])) - _SUPPORTED_REQUIRED_EXTENSIONS
	if unknown_required:
		raise AssetPreparationError(f"{label} declares unsupported required extensions: {sorted(unknown_required)}")
	unknown = set(model.get("extensionsUsed", [])) - _SUPPORTED_GLTF_EXTENSIONS
	if unknown:
		raise AssetPreparationError(f"{label} declares unsupported extensions: {sorted(unknown)}")
	used_extensions = set(model.get("extensionsUsed", []))
	required_extensions = set(model.get("extensionsRequired", []))
	if "MSFT_texture_dds" in used_extensions and "MSFT_texture_dds" not in required_extensions:
		raise AssetPreparationError(f"{label} must require MSFT_texture_dds when DDS textures are used")
	if "MSFT_texture_dds" in required_extensions and "MSFT_texture_dds" not in used_extensions:
		raise AssetPreparationError(f"{label} requires MSFT_texture_dds without declaring it used")
	if "MSFT_texture_dds" in used_extensions:
		for index, image in enumerate(model.get("images", [])):
			if not isinstance(image.get("uri"), str) or Path(image["uri"]).suffix.lower() != ".dds":
				raise AssetPreparationError(f"{label} image {index} must reference a runtime DDS file")
			if image.get("mimeType") != "image/vnd-ms.dds":
				raise AssetPreparationError(f"{label} image {index} must declare image/vnd-ms.dds")
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


def _require_pillow():
	try:
		import PIL
		from PIL import Image
	except ImportError as exc:
		raise AssetPreparationError(
			f"BC3 texture preparation requires Pillow {PILLOW_VERSION}; install it with "
			f"python3 -m pip install Pillow=={PILLOW_VERSION}"
		) from exc
	if PIL.__version__ != PILLOW_VERSION:
		raise AssetPreparationError(
			f"BC3 texture preparation requires Pillow {PILLOW_VERSION}, found {PIL.__version__}; "
			f"install it with python3 -m pip install Pillow=={PILLOW_VERSION}"
		)
	return Image


def _dds_chain_info(path: Path) -> dict[str, Any]:
	try:
		with path.open("rb") as source:
			header = source.read(DDS_HEADER_SIZE)
			size = path.stat().st_size
	except OSError as exc:
		raise AssetPreparationError(f"cannot read DDS image {path}: {exc}") from exc
	if len(header) != DDS_HEADER_SIZE or header[:4] != DDS_MAGIC:
		raise AssetPreparationError(f"invalid DDS header: {path}")
	if struct.unpack_from("<I", header, 4)[0] != 124:
		raise AssetPreparationError(f"invalid DDS header size: {path}")
	height, width = struct.unpack_from("<II", header, 12)
	mip_count = struct.unpack_from("<I", header, 28)[0] or 1
	flags = struct.unpack_from("<I", header, 8)[0]
	pixel_format_size, pixel_flags = struct.unpack_from("<II", header, 76)
	fourcc = header[84:88]
	if width <= 0 or height <= 0:
		raise AssetPreparationError(f"invalid DDS dimensions: {path}")
	if pixel_format_size != 32 or not pixel_flags & 0x4 or fourcc != DDS_DXT5_FOURCC:
		raise AssetPreparationError(f"DDS texture must use BC3/DXT5: {path}")
	caps, caps2 = struct.unpack_from("<II", header, 108)
	if caps & ~(0x1000 | 0x8 | 0x400000) or not caps & 0x1000 or caps2 != 0:
		raise AssetPreparationError(f"DDS texture must be a plain 2D texture: {path}")
	base_bytes = max(1, (width + 3) // 4) * max(1, (height + 3) // 4) * 16
	if struct.unpack_from("<I", header, 20)[0] != base_bytes:
		raise AssetPreparationError(f"DDS linear size does not match its BC3 base level: {path}")
	maximum_levels = int(math.floor(math.log2(max(width, height)))) + 1
	if mip_count > maximum_levels or bool(flags & 0x20000) != (mip_count > 1):
		raise AssetPreparationError(f"DDS mip count or flags are invalid: {path}")
	if mip_count > 1 and (caps & (0x8 | 0x400000)) != (0x8 | 0x400000):
		raise AssetPreparationError(f"DDS mipmap caps are invalid: {path}")
	level_dimensions = []
	payload_bytes = 0
	level_width, level_height = width, height
	for _ in range(mip_count):
		level_dimensions.append((level_width, level_height))
		payload_bytes += max(1, (level_width + 3) // 4) * max(1, (level_height + 3) // 4) * 16
		level_width, level_height = max(1, level_width // 2), max(1, level_height // 2)
	if size != DDS_HEADER_SIZE + payload_bytes:
		raise AssetPreparationError(f"DDS payload size or mip chain is invalid: {path}")
	return {"width": width, "height": height, "base_level_bytes": base_bytes,
		"mip_levels": mip_count, "level_dimensions": level_dimensions,
		"payload_bytes": payload_bytes, "runtime_file_bytes": size}


def _dds_info(path: Path) -> tuple[int, int, int]:
	info = _dds_chain_info(path)
	return info["width"], info["height"], info["base_level_bytes"]


def _normal_float_components(image):
	"""Return float RGB normal components and alpha without changing source pixels."""
	_require_pillow()
	from PIL import ImageMath

	red, green, blue, alpha = image.split()
	channels = []
	for channel in (red, green, blue):
		channel_float = channel.convert("F")
		channels.append(ImageMath.lambda_eval(
			lambda args: args["channel"] / 127.5 - 1.0, channel=channel_float))
		channel_float.close()
	return channels, alpha


def _normalize_normal_components(red, green, blue):
	"""Normalize float normal planes, using +Z when averaged vectors cancel."""
	from PIL import ImageMath

	length_squared = ImageMath.lambda_eval(
		lambda args: args["red"] ** 2 + args["green"] ** 2 + args["blue"] ** 2,
		red=red, green=green, blue=blue)
	length = ImageMath.lambda_eval(lambda args: args["length_squared"] ** 0.5,
		length_squared=length_squared)
	safe_length = ImageMath.lambda_eval(lambda args: args["max"](args["length"], 1e-6), length=length)
	cancelled = ImageMath.lambda_eval(lambda args: args["length_squared"] <= 1e-12,
		length_squared=length_squared)
	components = [
		ImageMath.lambda_eval(lambda args: args["component"] / args["length"],
			component=component, length=safe_length)
		for component in (red, green, blue)
	]
	active = ImageMath.lambda_eval(lambda args: 1.0 - args["cancelled"], cancelled=cancelled)
	components = [ImageMath.lambda_eval(
		lambda args: args["component"] * args["active"], component=component, active=active)
		for component in components]
	components[2] = ImageMath.lambda_eval(
		lambda args: args["component"] + args["cancelled"],
		component=components[2], cancelled=cancelled)
	length_squared.close()
	length.close()
	safe_length.close()
	cancelled.close()
	active.close()
	return components


def _encode_normal_mip(red, green, blue, alpha):
	"""Round float channels to RGBA8 with the same component mapping as the engine."""
	Image = _require_pillow()
	from PIL import ImageMath

	def encode(channel):
		return ImageMath.lambda_eval(
			lambda args: args["convert"]((args["channel"] + 1.0) * 127.5 + 0.5, "L"),
			channel=channel)

	def encode_alpha(channel):
		return ImageMath.lambda_eval(
			lambda args: args["convert"](args["channel"] + 0.5, "L"),
			channel=channel)

	return Image.merge("RGBA", (encode(red), encode(green), encode(blue), encode_alpha(alpha)))


def _normal_mip_chain(base):
	"""Build a full NPOT chain with Pillow BOX filtering and vector normalization."""
	Image = _require_pillow()
	levels = [base]
	source = base
	while source.size != (1, 1):
		width, height = source.size
		destination_size = (max(1, width // 2), max(1, height // 2))
		components, alpha = _normal_float_components(source)
		# Pillow's BOX reducer is deterministic but differs from the runtime's exact odd-size area weights.
		filtered = [component.resize(destination_size, Image.Resampling.BOX) for component in components]
		filtered_alpha = alpha.convert("F").resize(destination_size, Image.Resampling.BOX)
		normalized = _normalize_normal_components(*filtered)
		mip = _encode_normal_mip(*normalized, filtered_alpha)
		levels.append(mip)
		for plane in components + filtered + normalized + [alpha, filtered_alpha]:
			plane.close()
		source = mip
	return levels


def _convert_png_to_dds(source_path: Path, destination_path: Path, *, normal_map: bool = False) -> tuple[int, int, int]:
	Image = _require_pillow()
	try:
		expected_width, expected_height = _png_size(source_path)
		with Image.open(source_path) as source_image:
			if source_image.format != "PNG":
				raise AssetPreparationError(f"source texture is not PNG: {source_path}")
			source_image.load()
			if source_image.size != (expected_width, expected_height):
				raise AssetPreparationError(f"PNG dimensions disagree with the decoded image: {source_path}")
			rgba = source_image.convert("RGBA")
			destination_path.parent.mkdir(parents=True, exist_ok=True)
			if normal_map:
				levels = _normal_mip_chain(rgba)
				encoded = []
				for level in levels:
					buffer = io.BytesIO()
					level.save(buffer, format="DDS", pixel_format=PILLOW_PIXEL_FORMAT)
					encoded.append(buffer.getvalue())
					buffer.close()
				for level in levels[1:]:
					level.close()
				payload = b"".join(item[DDS_HEADER_SIZE:] for item in encoded)
				header = bytearray(encoded[0][:DDS_HEADER_SIZE])
				if len(encoded) > 1:
					struct.pack_into("<I", header, 8, struct.unpack_from("<I", header, 8)[0] | 0x20000)
					struct.pack_into("<I", header, 28, len(encoded))
					struct.pack_into("<I", header, 108, struct.unpack_from("<I", header, 108)[0] | 0x8 | 0x400000)
				destination_path.write_bytes(header + payload)
				rgba.close()
			else:
				rgba.save(destination_path, format="DDS", pixel_format=PILLOW_PIXEL_FORMAT)
				rgba.close()
		# Pillow 11.3.0 writes a row-size value here despite setting DDSD_LINEARSIZE.
		# Store the full BC3 base-level payload size required by the DDS header.
		with destination_path.open("r+b") as destination:
			header = destination.read(DDS_HEADER_SIZE)
			height, width = struct.unpack_from("<II", header, 12)
			base_bytes = max(1, (width + 3) // 4) * max(1, (height + 3) // 4) * 16
			destination.seek(20)
			destination.write(struct.pack("<I", base_bytes))
	except AssetPreparationError:
		raise
	except Exception as exc:
		destination_path.unlink(missing_ok=True)
		raise AssetPreparationError(f"cannot convert PNG texture {source_path} to BC3/DXT5: {exc}") from exc
	width, height, base_bytes = _dds_info(destination_path)
	if (width, height) != (expected_width, expected_height):
		destination_path.unlink(missing_ok=True)
		raise AssetPreparationError(f"converted DDS dimensions do not match source PNG: {source_path}")
	return width, height, base_bytes


def _convert_model_images_to_dds(model: dict[str, Any], source_root: Path, output_root: Path) -> None:
	"""Convert source PNGs, retaining distinct images when one serves different map semantics."""
	normal_texture_indices = set()
	non_normal_texture_indices = set()
	for material in model.get("materials", []):
		for slot, block_name in _MATERIAL_TEXTURE_SLOTS:
			block = material.get(block_name, {}) if block_name else material
			info = block.get(slot) if isinstance(block, dict) else None
			if info is not None:
				texture_index = info.get("index", -1)
				if slot == "normalTexture":
					normal_texture_indices.add(texture_index)
				else:
					non_normal_texture_indices.add(texture_index)
	normal_image_indices = set()
	textures = model.get("textures", [])
	images = model.get("images", [])
	shared_image_indices = {
		textures[index].get("source", -1) for index in non_normal_texture_indices
		if 0 <= index < len(textures)
	}
	normal_clones_by_source_image = {}
	for texture_index in sorted(normal_texture_indices):
		if texture_index < 0 or texture_index >= len(textures):
			continue
		texture = textures[texture_index]
		image_index = texture.get("source", -1)
		if image_index < 0 or image_index >= len(images):
			continue
		if image_index not in shared_image_indices and texture_index not in non_normal_texture_indices:
			normal_image_indices.add(image_index)
			continue
		clone_image_index = normal_clones_by_source_image.get(image_index)
		if clone_image_index is None:
			clone_image_index = len(images)
			images.append(copy.deepcopy(images[image_index]))
			normal_clones_by_source_image[image_index] = clone_image_index
		clone_texture = copy.deepcopy(texture)
		clone_texture["source"] = clone_image_index
		clone_texture_index = len(textures)
		textures.append(clone_texture)
		for material in model.get("materials", []):
			normal_info = material.get("normalTexture")
			if normal_info is not None and normal_info.get("index") == texture_index:
				normal_info["index"] = clone_texture_index
		normal_image_indices.add(clone_image_index)
	source_relatives = []
	for index, image in enumerate(images):
		uri = image.get("uri")
		if not isinstance(uri, str) or Path(uri).suffix.lower() != ".png":
			raise AssetPreparationError(f"benchmark source image {index} must use a PNG file URI")
		source_relatives.append(_uri_path(uri, "image"))
	output_relatives = {}
	reserved_output_paths = set()
	for index in range(len(images)):
		if index not in normal_image_indices:
			relative = source_relatives[index]
			output_relative = relative.with_suffix(".dds")
			output_relatives[index] = output_relative
			reserved_output_paths.add(output_relative)
	for index in sorted(normal_image_indices):
		relative = source_relatives[index]
		base_name = f"{relative.stem}.normal-{index}.dds"
		output_relative = relative.with_name(base_name)
		variant = 1
		while output_relative in reserved_output_paths:
			output_relative = relative.with_name(f"{relative.stem}.normal-{index}-{variant}.dds")
			variant += 1
		output_relatives[index] = output_relative
		reserved_output_paths.add(output_relative)
	for index, image in enumerate(model.get("images", [])):
		relative = source_relatives[index]
		source_path = source_root / relative
		output_relative = output_relatives[index]
		_convert_png_to_dds(source_path, output_root / output_relative, normal_map=index in normal_image_indices)
		image["uri"] = output_relative.as_posix()
		image["mimeType"] = "image/vnd-ms.dds"
	for texture in model.get("textures", []):
		if "source" not in texture or texture.get("extensions"):
			raise AssetPreparationError("benchmark source textures must use the standard source field without extensions")
		image_index = texture.pop("source")
		texture["extensions"] = {"MSFT_texture_dds": {"source": image_index}}
	for key in ("extensionsUsed", "extensionsRequired"):
		values = model.setdefault(key, [])
		if "MSFT_texture_dds" not in values:
			values.append("MSFT_texture_dds")


def _add_texture_memory(metadata: dict[str, Any], model: dict[str, Any], model_path: Path) -> None:
	images: dict[int, dict[str, Any]] = {}
	for index, image in enumerate(model.get("images", [])):
		uri = image.get("uri")
		if not isinstance(uri, str) or Path(uri).suffix.lower() != ".dds":
			raise AssetPreparationError("runtime benchmark images must use DDS file URIs")
		path = model_path.parent / _uri_path(uri, "image")
		info = _dds_chain_info(path)
		images[index] = {"path": path, **info}
	used = set()
	normal_used = set()
	for material in model.get("materials", []):
		for slot, block_name in _MATERIAL_TEXTURE_SLOTS:
			block = material.get(block_name, {}) if block_name else material
			info = block.get(slot) if isinstance(block, dict) else None
			if info is not None:
				image_index = _validate_texture_sampler(model, info, f"texture memory {slot}")
				used.add(image_index)
				if slot == "normalTexture":
					normal_used.add(image_index)
	texture_records = []
	decoded_rgba_bytes = 0
	bc3_base_bytes = 0
	bc3_runtime_bytes = 0
	runtime_file_bytes = 0
	runtime_mip_levels = 1
	for image_index in sorted(used):
		image = images[image_index]
		expected_mips = int(math.floor(math.log2(max(image["width"], image["height"])))) + 1 if image_index in normal_used else 1
		if image["mip_levels"] != expected_mips:
			semantic = "normal" if image_index in normal_used else "non-normal"
			raise AssetPreparationError(f"{semantic} image {image_index} has {image['mip_levels']} DDS mip levels; expected {expected_mips}")
		bytes_per_image = image["width"] * image["height"] * 4
		decoded_rgba_bytes += bytes_per_image
		bc3_base_bytes += image["base_level_bytes"]
		bc3_runtime_bytes += image["payload_bytes"]
		runtime_file_bytes += image["runtime_file_bytes"]
		runtime_mip_levels = max(runtime_mip_levels, image["mip_levels"])
		texture_records.append({
			"image_index": image_index,
			"width": image["width"], "height": image["height"],
			"format": "BC3/DXT5",
			"mip_levels": image["mip_levels"],
			"runtime_file_bytes": image["runtime_file_bytes"],
			"bc3_base_level_bytes": image["base_level_bytes"],
			"bc3_runtime_bytes": image["payload_bytes"],
			"rgba8_base_level_bytes": bytes_per_image,
			"rgba8_full_mipmap_estimate_bytes": (bytes_per_image * 4 + 2) // 3,
		})
	metadata["texture_memory"] = {
		"unique_referenced_images": len(used),
		"runtime_texture_format": "BC3/DXT5",
		"compression_encoder": f"Pillow {PILLOW_VERSION} DDS pixel_format={PILLOW_PIXEL_FORMAT} default encoder",
		"runtime_mip_levels": runtime_mip_levels,
		"bc3_base_level_bytes": bc3_base_bytes,
		"bc3_runtime_bytes": bc3_runtime_bytes,
		"runtime_file_bytes": runtime_file_bytes,
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
	print("Preparing Flight Helmet BC3 textures...", flush=True)
	for source in (flight_root / "glTF").iterdir():
		if source.is_file() and source.name != "FlightHelmet.gltf" and source.suffix.lower() != ".png":
			shutil.copyfile(source, output / source.name)
	_convert_model_images_to_dds(model, source_model.parent, output)
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
	output.mkdir(parents=True, exist_ok=True)
	print("Preparing Sponza BC3 textures...", flush=True)
	_convert_model_images_to_dds(model, sponza_source_gltf.parent, output)
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


def needs_preparation(cache_dir: Path) -> bool:
	"""Return whether a missing or recognized old-recipe cache needs rebuilding.

	Current-recipe contents are validated separately by verify_prepared_cache so
	corrupt current caches remain fail-closed instead of being silently replaced.
	"""
	if not cache_dir.exists():
		return True
	try:
		manifest = _load_json(cache_dir / "prepared.json")
	except AssetPreparationError as exc:
		raise AssetPreparationError(f"cannot identify existing asset cache: {exc}") from exc
	identity = manifest.get("profile"), manifest.get("converter_version")
	if identity == (PROFILE, CONVERTER_VERSION):
		return False
	if identity in PREVIOUS_PROFILES:
		return True
	raise AssetPreparationError(
		f"existing asset cache uses an unrecognized recipe {identity[0]!r}/{identity[1]!r}; refusing to replace it"
	)


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
	migrating_previous = False
	if cache_dir.exists():
		if not needs_preparation(cache_dir):
			_, errors = verify_prepared_cache(cache_dir, lock_hash, lock_path)
			if errors:
				raise AssetPreparationError("existing asset cache is immutable and invalid: " + "; ".join(errors))
			return cache_dir
		migrating_previous = True
		if source_directory is None:
			source_directory = cache_dir / "source"
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
		if migrating_previous:
			backup = cache_dir.parent / f".{cache_dir.name}.backup-{uuid.uuid4().hex}"
			os.replace(cache_dir, backup)
			try:
				os.replace(stage, cache_dir)
			except Exception:
				if not cache_dir.exists():
					os.replace(backup, cache_dir)
				raise
		else:
			os.replace(stage, cache_dir)
		return cache_dir
	except Exception:
		shutil.rmtree(stage, ignore_errors=True)
		raise


def default_cache_path() -> Path:
	return Path(__file__).resolve().parents[3] / "build" / "benchmark-assets"


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
