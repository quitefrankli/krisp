"""Prepare and verify the pinned offline glTF reference assets."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import tempfile
import urllib.error
import urllib.request
import uuid
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import unquote


LOCK_PATH = Path(__file__).with_name("assets.lock.json")
REPOSITORY = "https://github.com/KhronosGroup/glTF-Sample-Assets"
RAW_REPOSITORY = "https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Assets"
MANIFEST_NAME = "manifest.json"
SUPPORTED_EXTENSIONS: set[str] = set()


class AssetPreparationError(RuntimeError):
	"""A pinned source file or prepared cache failed validation."""


def default_cache_path(source_root: Path | None = None) -> Path:
	return (source_root or Path(__file__).resolve().parents[2]) / "build" / "fidelity-assets"


def sha256_file(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def _safe_relative_path(value: str) -> Path:
	decoded = unquote(value)
	if not decoded or "\\" in decoded or "\x00" in decoded:
		raise AssetPreparationError(f"unsafe asset path {value!r}")
	path = PurePosixPath(decoded)
	if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
		raise AssetPreparationError(f"unsafe asset path {value!r}")
	if ":" in path.parts[0] or "?" in decoded or "#" in decoded:
		raise AssetPreparationError(f"unsupported asset URI {value!r}")
	return Path(*path.parts)


def _read_json(path: Path) -> dict[str, Any]:
	try:
		value = json.loads(path.read_text(encoding="utf-8"))
	except (OSError, json.JSONDecodeError) as error:
		raise AssetPreparationError(f"cannot read JSON file {path}: {error}") from error
	if not isinstance(value, dict):
		raise AssetPreparationError(f"expected a JSON object in {path}")
	return value


def _load_lock(lock_path: Path) -> dict[str, Any]:
	lock = _read_json(lock_path)
	if lock.get("schema_version") != 1 or lock.get("repository") != REPOSITORY:
		raise AssetPreparationError(f"unsupported asset lock schema or repository: {lock_path}")
	commit = lock.get("source_commit")
	assets = lock.get("assets")
	if not isinstance(commit, str) or len(commit) != 40 or not isinstance(assets, dict) or not assets:
		raise AssetPreparationError(f"invalid source commit or empty asset inventory in {lock_path}")
	for name, entry in assets.items():
		if not isinstance(entry, dict) or not isinstance(entry.get("files"), list):
			raise AssetPreparationError(f"invalid lock entry for {name}")
		paths = set()
		for item in entry["files"]:
			path = item.get("path") if isinstance(item, dict) else None
			digest = item.get("sha256") if isinstance(item, dict) else None
			if not isinstance(path, str) or not isinstance(digest, str) or len(digest) != 64:
				raise AssetPreparationError(f"invalid pinned file entry for {name}")
			_safe_relative_path(path)
			if path in paths:
				raise AssetPreparationError(f"duplicate pinned file {path} for {name}")
			paths.add(path)
		gltf = entry.get("gltf")
		if not isinstance(gltf, str) or gltf not in paths:
			raise AssetPreparationError(f"glTF entry is not pinned for {name}")
	return lock


def _read_source_file(source_root: Path | None, path: str, commit: str,
		destination: Path, expected_sha256: str, expected_size: int | None) -> None:
	destination.parent.mkdir(parents=True, exist_ok=True)
	if source_root is not None:
		local = source_root / _safe_relative_path(path)
		try:
			shutil.copyfile(local, destination)
		except OSError as error:
			raise AssetPreparationError(f"cannot read local pinned source {path}: {error}") from error
	else:
		url = f"{RAW_REPOSITORY}/{commit}/{path}"
		request = urllib.request.Request(url, headers={"User-Agent": "KrispRenderingFidelityAssets/1"})
		partial = destination.with_name(destination.name + ".part")
		count = 0
		digest = hashlib.sha256()
		try:
			with urllib.request.urlopen(request, timeout=60) as response, partial.open("wb") as output:
				while chunk := response.read(1024 * 1024):
					count += len(chunk)
					if expected_size is not None and count > expected_size:
						raise AssetPreparationError(f"download exceeds pinned size for {path}")
					output.write(chunk)
					digest.update(chunk)
			if expected_size is not None and count != expected_size:
				raise AssetPreparationError(f"download size mismatch for {path}: expected {expected_size}, got {count}")
			if digest.hexdigest() != expected_sha256:
				raise AssetPreparationError(f"download SHA256 mismatch for {path}: expected {expected_sha256}, got {digest.hexdigest()}")
			os.replace(partial, destination)
		except (OSError, urllib.error.URLError, TimeoutError) as error:
			partial.unlink(missing_ok=True)
			raise AssetPreparationError(f"failed to download {url}: {error}") from error
		finally:
			partial.unlink(missing_ok=True)
	actual = sha256_file(destination)
	if actual != expected_sha256:
		raise AssetPreparationError(f"SHA256 mismatch for {path}: expected {expected_sha256}, got {actual}")


def _cache_path_for_source(name: str, source_path: str) -> Path:
	parts = PurePosixPath(source_path).parts
	if len(parts) < 3 or parts[0] != "Models" or parts[1] != name:
		return Path("notices") / name / PurePosixPath(source_path).name
	if parts[2] == "README.md":
		return Path("notices") / name / "README.md"
	return Path("meshes") / name / Path(*parts[2:])


def _validate_gltf(root: Path, gltf_path: Path) -> None:
	model = _read_json(gltf_path)
	used = set(model.get("extensionsUsed", []))
	required = set(model.get("extensionsRequired", []))
	unsupported = (used | required) - SUPPORTED_EXTENSIONS
	if unsupported:
		raise AssetPreparationError(
			f"{gltf_path} declares unsupported glTF extension(s): {', '.join(sorted(unsupported))}; source was not modified")
	for collection in ("buffers", "images"):
		for item in model.get(collection, []):
			uri = item.get("uri") if isinstance(item, dict) else None
			if uri is None:
				continue
			if uri.startswith("data:"):
				continue
			dependency = gltf_path.parent / _safe_relative_path(uri)
			try:
				dependency.resolve().relative_to(root.resolve())
			except ValueError as error:
				raise AssetPreparationError(f"glTF dependency escapes cache: {uri}") from error
			if not dependency.is_file():
				raise AssetPreparationError(f"missing glTF dependency {uri} referenced by {gltf_path}")


def _manifest_for_cache(root: Path, lock: dict[str, Any]) -> dict[str, Any]:
	assets: dict[str, Any] = {}
	notices: dict[str, dict[str, str]] = {}
	for name, entry in lock["assets"].items():
		file_hashes: dict[str, str] = {}
		notice_hashes: dict[str, str] = {}
		for item in entry["files"]:
			source = item["path"]
			cache_path = _cache_path_for_source(name, source)
			cache_name = cache_path.as_posix()
			if cache_path.parts[0] == "notices":
				notice_hashes[cache_name] = item["sha256"]
			else:
				file_hashes[cache_name] = item["sha256"]
		gltf = _cache_path_for_source(name, entry["gltf"]).as_posix()
		assets[name] = {"gltf": gltf, "files": file_hashes,
			"source_commit": lock["source_commit"],
			"notice": f"notices/{name}/README.md",
			"license": entry["license"]}
		notices[name] = notice_hashes
	return {"schema_version": 1, "repository": REPOSITORY,
		"source_commit": lock["source_commit"], "assets": assets, "notices": notices}


def prepare_assets(cache_dir: Path, source_directory: Path | None = None,
		lock_path: Path = LOCK_PATH) -> dict[str, Any]:
	"""Build a complete verified cache from GitHub or an exact local source tree."""
	cache_dir = Path(cache_dir).resolve()
	lock = _load_lock(Path(lock_path))
	cache_dir.parent.mkdir(parents=True, exist_ok=True)
	stage = Path(tempfile.mkdtemp(prefix=f".{cache_dir.name}.stage-", dir=cache_dir.parent))
	backup = cache_dir.with_name(f".{cache_dir.name}.backup-{uuid.uuid4().hex}")
	try:
		for name, entry in lock["assets"].items():
			for item in entry["files"]:
				source_path = item["path"]
				relative = _cache_path_for_source(name, source_path)
				_read_source_file(source_directory, source_path, lock["source_commit"],
					stage / relative, item["sha256"], item.get("size_bytes"))
			gltf = stage / _cache_path_for_source(name, entry["gltf"])
			_validate_gltf(stage, gltf)
		manifest = _manifest_for_cache(stage, lock)
		manifest_path = stage / MANIFEST_NAME
		manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
		if cache_dir.exists():
			cache_dir.replace(backup)
		try:
			stage.replace(cache_dir)
		except OSError:
			if backup.exists() and not cache_dir.exists():
				backup.replace(cache_dir)
			raise
		if backup.exists():
			shutil.rmtree(backup)
		return manifest
	except Exception:
		if stage.exists():
			shutil.rmtree(stage)
		raise


def verify_cache(cache_dir: Path, lock_path: Path = LOCK_PATH) -> tuple[dict[str, Any] | None, list[str]]:
	"""Verify a prepared cache without network access or modifying its files."""
	errors: list[str] = []
	try:
		lock = _load_lock(Path(lock_path))
		expected = _manifest_for_cache(Path(cache_dir), lock)
		manifest_path = Path(cache_dir) / MANIFEST_NAME
		manifest = _read_json(manifest_path)
		if manifest != expected:
			errors.append("prepared asset manifest does not match the active lock file")
		for relative, expected_hash in sorted(_all_cached_files(expected).items()):
			path = Path(cache_dir) / _safe_relative_path(relative)
			if not path.is_file():
				errors.append(f"missing prepared asset file: {relative}")
				continue
			actual_hash = sha256_file(path)
			if actual_hash != expected_hash:
				errors.append(f"SHA256 mismatch for prepared asset {relative}: expected {expected_hash}, got {actual_hash}")
		for name, entry in expected["assets"].items():
			try:
				_validate_gltf(Path(cache_dir), Path(cache_dir) / entry["gltf"])
			except (OSError, AssetPreparationError) as error:
				errors.append(f"invalid prepared asset {name}: {error}")
		return manifest, errors
	except (OSError, AssetPreparationError) as error:
		return None, [str(error)]


def _all_cached_files(manifest: dict[str, Any]) -> dict[str, str]:
	files: dict[str, str] = {}
	for entry in manifest.get("assets", {}).values():
		files.update(entry.get("files", {}))
	for notice_files in manifest.get("notices", {}).values():
		files.update(notice_files)
	return files
