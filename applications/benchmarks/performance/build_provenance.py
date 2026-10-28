"""Capture build and runtime identity for performance benchmark results."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Any, Sequence


_SOURCE_ROOTS = ("src", "shared_code", "shaders", "applications")
_ROOT_FILES = ("meson.build", "meson.options", "meson_options.txt", "conanfile.py", "conanfile.txt", "conan.lock", "conan_clang_profile")
_SOURCE_SUFFIXES = {".cpp", ".hpp", ".ipp", ".glsl", ".vert", ".frag", ".comp", ".py", ".json", ".ini", ".txt", ".build", ".cmake"}
_EXCLUDED_PARTS = {"build", "results", "cache", "assets", "__pycache__", ".pytest_cache", ".git"}
_RUNTIME_TOOL_NAMES = (
	"benchmark_runner.py", "benchmark_statistics.py", "benchmark_assets.py",
	"build_provenance.py", "assets.lock.json",
)


def _source_paths(root: Path) -> list[Path]:
	command = ["git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", *_SOURCE_ROOTS, *_ROOT_FILES]
	try:
		output = subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
		paths = [root / name.decode("utf-8", "surrogateescape") for name in output.split(b"\0") if name]
	except (OSError, subprocess.CalledProcessError):
		paths = []
		for directory in _SOURCE_ROOTS:
			base = root / directory
			if base.exists():
				paths.extend(path for path in base.rglob("*") if path.is_file())
		paths.extend(root / name for name in _ROOT_FILES if (root / name).is_file())
	return sorted({path for path in paths if path.is_file() and _is_source_input(root, path)})


def _is_source_input(root: Path, path: Path) -> bool:
	relative = path.relative_to(root)
	if any(part in _EXCLUDED_PARTS or part.startswith(".") for part in relative.parts):
		return False
	if len(relative.parts) == 1:
		return relative.name in _ROOT_FILES
	return relative.parts[0] in _SOURCE_ROOTS and path.suffix.lower() in _SOURCE_SUFFIXES


def source_fingerprint(source_root: Path) -> str:
	"""Hash relevant tracked and untracked source/build inputs deterministically."""
	root = source_root.resolve()
	digest = hashlib.sha256()
	for path in _source_paths(root):
		relative = path.relative_to(root).as_posix().encode("utf-8", "surrogateescape")
		digest.update(len(relative).to_bytes(8, "big"))
		digest.update(relative)
		with path.open("rb") as source:
			for chunk in iter(lambda: source.read(1024 * 1024), b""):
				digest.update(chunk)
	return digest.hexdigest()


def source_revision(source_root: Path) -> tuple[str | None, bool | None]:
	"""Return Git HEAD and relevant dirty state, or unknowns outside a Git tree."""
	try:
		revision = subprocess.run(
			["git", "-C", str(source_root), "rev-parse", "HEAD"], check=True,
			stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
		).stdout.strip()
		tracked = subprocess.run(
			["git", "-C", str(source_root), "diff", "HEAD", "--name-only", "--", *_SOURCE_ROOTS, *_ROOT_FILES],
			check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
		).stdout.splitlines()
		untracked = subprocess.run(
			["git", "-C", str(source_root), "ls-files", "--others", "--exclude-standard", "-z", "--", *_SOURCE_ROOTS, *_ROOT_FILES],
			check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
		).stdout.split(b"\0")
		dirty_paths = [source_root / name for name in tracked if name]
		dirty_paths.extend(source_root / name.decode("utf-8", "surrogateescape") for name in untracked if name)
		return revision, any(_is_source_input(source_root, path) for path in dirty_paths)
	except (OSError, subprocess.CalledProcessError):
		return None, None


def _meson_data(build_root: Path, key: str) -> Any:
	try:
		result = subprocess.run(
			["meson", "introspect", str(build_root), key], check=True,
			stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
		)
		return json.loads(result.stdout)
	except (OSError, subprocess.CalledProcessError, json.JSONDecodeError):
		return None


def _build_configuration(source_root: Path, build_root: Path, build_type_override: str | None, compiler_override: str | None) -> dict[str, Any]:
	options = _meson_data(build_root, "--buildoptions") or []
	option_map = {option.get("name"): option.get("value") for option in options if isinstance(option, dict)}
	build_type = build_type_override or option_map.get("buildtype") or "unknown"
	ndebug = option_map.get("b_ndebug")
	optimization = option_map.get("optimization")
	optimized = optimization not in {None, 0, "0", "g", "none"} if optimization is not None else None
	if ndebug is True:
		assertions = False
	elif ndebug is False:
		assertions = True
	elif ndebug == "if-release":
		assertions = build_type not in {"release", "minsize"}
	else:
		assertions = build_type == "debug" if build_type != "unknown" else None
	validation = None
	compiler = compiler_override
	compiler_version = None
	compilers = _meson_data(build_root, "--compilers") or {}
	if isinstance(compilers, dict):
		for language in ("cpp", "c"):
			entry = compilers.get(language)
			if not isinstance(entry, dict):
				continue
			compiler = compiler or entry.get("id") or entry.get("cmd_array", [None])[0]
			compiler_version = entry.get("version")
			if compiler_version:
				break
	if compiler and not compiler_version:
		try:
			compiler_version = subprocess.run(
				[*shlex.split(compiler), "--version"], check=False, stdout=subprocess.PIPE,
				stderr=subprocess.STDOUT, text=True, timeout=5,
			).stdout.splitlines()[0]
		except (OSError, subprocess.TimeoutExpired, IndexError):
			compiler_version = None
	dependencies = _meson_data(build_root, "--dependencies")
	dependency_versions = _canonical_dependency_versions(dependencies)
	profile_path = source_root / "conan_clang_profile"
	native_file = build_root / "conan" / "conan_meson_native.ini"
	if not native_file.is_file():
		native_file = build_root.parent / "conan" / "conan_meson_native.ini"
	configuration = {
		"build_type": build_type,
		"build_class": "debug" if build_type == "debug" else "optimized" if build_type in {"release", "minsize"} else "debugoptimized" if build_type == "debugoptimized" else None,
		"optimised": optimized,
		"debug_macro": build_type == "debug" if build_type != "unknown" else None,
		"assertions": assertions,
		"validation": validation,
		"compiler": compiler,
		"compiler_version": compiler_version,
		"optimization_level": optimization,
		"optimization_level_known": optimization is not None,
		"dependency_versions": dependency_versions,
		"conan_profile_sha256": _hash_file(profile_path) if profile_path.is_file() else None,
		"conan_native_file_sha256": _hash_file(native_file) if native_file.is_file() else None,
		"dependency_options": {
			name: value for name, value in option_map.items()
			if name in {"cpp_args", "c_args", "default_library", "b_lto", "b_pie", "b_staticpic"}
		},
	}
	configuration["configuration_fingerprint"] = hashlib.sha256(
		json.dumps(configuration, sort_keys=True, separators=(",", ":"), default=str).encode()
	).hexdigest()
	return configuration


def _canonical_dependency_versions(dependencies: Any) -> list[dict[str, str]] | None:
	"""Keep stable external package identity and discard Meson internal IDs."""
	if not isinstance(dependencies, list):
		return None
	canonical = []
	for dependency in dependencies:
		if not isinstance(dependency, dict):
			continue
		name, dependency_type = dependency.get("name"), dependency.get("type")
		if not isinstance(name, str) or not name or name.startswith("dep") and name[3:].isdigit():
			continue
		if dependency_type == "internal":
			continue
		entry = {"name": name}
		if isinstance(dependency_type, str) and dependency_type:
			entry["type"] = dependency_type
		version = dependency.get("version")
		if isinstance(version, str) and version:
			entry["version"] = version
		canonical.append(entry)
	return sorted(canonical, key=lambda item: (item["name"], item.get("type", ""), item.get("version", "")))


def _machine_id() -> str | None:
	for path in (Path("/etc/machine-id"), Path("/var/lib/dbus/machine-id")):
		try:
			value = path.read_text(encoding="utf-8").strip()
			if value:
				return hashlib.sha256(value.encode()).hexdigest()
		except OSError:
			continue
	return None


def _cpu_identity() -> str | None:
	try:
		for line in Path("/proc/cpuinfo").read_text(encoding="utf-8", errors="replace").splitlines():
			if line.lower().startswith(("model name", "hardware")) and ":" in line:
				return line.split(":", 1)[1].strip()
	except OSError:
		pass
	return platform.processor() or None


def machine_identity() -> dict[str, Any]:
	"""Return stable, non-hostname machine fields for runtime compatibility."""
	try:
		release = platform.freedesktop_os_release()
		os_distribution = {"id": release.get("ID"), "version": release.get("VERSION_ID")}
	except (AttributeError, OSError):
		os_distribution = None
	return {
		"machine_id": _machine_id(),
		"cpu": _cpu_identity(),
		"os": platform.system() or None,
		"os_distribution": os_distribution,
		"kernel": platform.release() or None,
		"gpu": None,
		"driver": None,
	}


def create_build_manifest(
	source_root: Path, build_root: Path, *, build_type: str | None = None,
	compiler: str | None = None, binary: Path | None = None,
) -> dict[str, Any]:
	revision, dirty = source_revision(source_root)
	manifest = {
		"schema_version": 1,
		"source_revision": revision,
		"dirty": dirty,
		"source_fingerprint": source_fingerprint(source_root),
		"runtime_tools_sha256": runtime_tools_sha256(source_root),
		**_build_configuration(source_root, build_root, build_type, compiler),
	}
	if binary is not None:
		manifest["binary_sha256"] = _hash_file(binary)
	return manifest


def _hash_file(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def runtime_tools_sha256(source_root: Path) -> dict[str, str]:
	"""Hash runtime Python tools and their asset lock as built with the executable."""
	tool_root = source_root / "applications" / "benchmarks" / "performance"
	return {name: _hash_file(tool_root / name) for name in _RUNTIME_TOOL_NAMES}


def _write_atomic(path: Path, value: dict[str, Any]) -> None:
	content = json.dumps(value, indent=2, sort_keys=True) + "\n"
	try:
		if path.read_text(encoding="utf-8") == content:
			return
	except FileNotFoundError:
		pass
	path.parent.mkdir(parents=True, exist_ok=True)
	temporary = path.with_name(f".{path.name}.tmp")
	with temporary.open("w", encoding="utf-8") as output:
		output.write(content)
		output.flush()
		os.fsync(output.fileno())
	temporary.replace(path)


def _parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--source-root", required=True, type=Path)
	parser.add_argument("--build-root", required=True, type=Path)
	parser.add_argument("--output", required=True, type=Path)
	parser.add_argument("--binary", type=Path)
	parser.add_argument("--build-type")
	parser.add_argument("--compiler")
	return parser


def main(argv: Sequence[str] | None = None) -> int:
	args = _parser().parse_args(argv)
	manifest = create_build_manifest(
		args.source_root, args.build_root, build_type=args.build_type,
		compiler=args.compiler, binary=args.binary,
	)
	_write_atomic(args.output, manifest)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
