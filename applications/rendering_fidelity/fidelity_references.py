"""Generate optional, independent renderer references for the fidelity fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import base64
import signal
from pathlib import Path
from typing import Any, Iterable


SCHEMA_VERSION = 1
DEFAULT_SCENES = Path(__file__).parent / "fixtures" / "reference_cases"
DEFAULT_MANIFEST_NAME = "manifest.json"
REFERENCE_MANIFEST = "manifest.json"


def _adapter_hashes(renderer: str) -> dict[str, str]:
	root = Path(__file__).parent
	paths = [Path(__file__), root / f"reference_{renderer}.py"]
	if renderer == "khronos":
		web = root / "reference_web"
		paths += [web / name for name in ("renderer.js", "index.html", "package-lock.json", "assets/images/lut_sheen_E.png")]
	return {str(path.relative_to(root)): _sha256(path) for path in paths}


class ReferenceError(RuntimeError):
	"""Raised when references cannot be generated or verified."""


def _sha256(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def _json_hash(value: Any) -> str:
	return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def _asset_entry(manifest: dict[str, Any], name: str) -> dict[str, Any]:
	try:
		entry = manifest["assets"][name]
	except (KeyError, TypeError) as error:
		raise ReferenceError(f"asset {name!r} is not present in the fidelity asset manifest") from error
	if not isinstance(entry, dict) or not isinstance(entry.get("gltf"), str):
		raise ReferenceError(f"asset manifest entry {name!r} must contain a gltf path")
	return entry


def _load_json(path: Path) -> dict[str, Any]:
	try:
		value = json.loads(path.read_text(encoding="utf-8"))
	except (OSError, json.JSONDecodeError) as error:
		raise ReferenceError(f"could not read {path}: {error}") from error
	if not isinstance(value, dict):
		raise ReferenceError(f"{path} must contain a JSON object")
	return value


def _scene_files(scenes: Path, selected: Iterable[str] | None = None) -> list[Path]:
	if selected:
		files = [scenes / f"{case}.json" for case in selected]
		missing = [path for path in files if not path.is_file()]
		if missing:
			raise ReferenceError("missing reference scene file(s): " + ", ".join(map(str, missing)))
		return files
	files = sorted(scenes.glob("*.json"))
	if not files:
		raise ReferenceError(f"no per-case reference scenes found in {scenes}")
	return files


def _case_captures(case: dict[str, Any]) -> dict[str, dict[str, Any]]:
	captures = case.get("captures")
	if not isinstance(captures, dict) or not captures:
		raise ReferenceError(f"{case.get('case_id', '<unknown>')}: captures must be a non-empty object")
	for name, capture in captures.items():
		if not isinstance(name, str) or not isinstance(capture, dict):
			raise ReferenceError("capture entries must map names to JSON objects")
	return captures


def _file_inventory(entry: dict[str, Any], assets_root: Path) -> dict[str, str]:
	files = entry.get("files")
	if not isinstance(files, dict) or not files:
		gltf_path = (assets_root / entry["gltf"]).resolve()
		if not gltf_path.is_file():
			raise ReferenceError(f"asset glTF is missing: {gltf_path}")
		return {str(gltf_path.relative_to(assets_root.resolve())): _sha256(gltf_path)}
	result = {}
	for relative, expected in sorted(files.items()):
		path = (assets_root / relative).resolve()
		try:
			path.relative_to(assets_root.resolve())
		except ValueError as error:
			raise ReferenceError(f"asset path escapes cache root: {relative}") from error
		if not path.is_file():
			raise ReferenceError(f"asset dependency is missing: {path}")
		actual = _sha256(path)
		if expected and actual != expected:
			raise ReferenceError(f"cached asset checksum mismatch: {relative}")
		result[str(relative)] = actual
	return result


def _renderer_version(executable: str, args: list[str]) -> str:
	try:
		completed = subprocess.run([executable, *args], capture_output=True, text=True, timeout=30, check=False)
	except (OSError, subprocess.TimeoutExpired) as error:
		raise ReferenceError(f"cannot query renderer version from {executable!r}: {error}") from error
	output = (completed.stdout + completed.stderr).strip()
	if completed.returncode != 0 or not output:
		raise ReferenceError(f"renderer version query failed for {executable!r}: {output}")
	return output.splitlines()[0]


def _resolve_scene_asset(case: dict[str, Any], assets_root: Path, manifest: dict[str, Any]) -> tuple[Path, dict[str, Any], dict[str, str]]:
	asset_name = case.get("asset")
	if not isinstance(asset_name, str):
		raise ReferenceError(f"{case.get('case_id', '<unknown>')}: asset must be a string")
	entry = _asset_entry(manifest, asset_name)
	gltf_path = (assets_root / entry["gltf"]).resolve()
	try:
		gltf_path.relative_to(assets_root.resolve())
	except ValueError as error:
		raise ReferenceError(f"asset path escapes cache root: {entry['gltf']}") from error
	if not gltf_path.is_file():
		raise ReferenceError(f"asset glTF is missing: {gltf_path}")
	return gltf_path, entry, _file_inventory(entry, assets_root)


def _case_input(case_path: Path, case: dict[str, Any], assets_root: Path, asset_manifest: dict[str, Any]) -> dict[str, Any]:
	if not isinstance(case.get("reference_scene"), dict):
		raise ReferenceError(f"{case_path}: reference_scene must be an object")
	asset_path, asset_entry, asset_files = _resolve_scene_asset(case, assets_root, asset_manifest)
	reference_files = {}
	environment = case["reference_scene"].get("environment", {})
	for field in ("path", "renderer_ibl_path"):
		if environment.get(field):
			path = (case_path.parent / environment[field]).resolve()
			if not path.is_file():
				raise ReferenceError(f"reference scene dependency is missing: {path}")
			reference_files[str(path)] = _sha256(path)
	return {
		"case_id": case.get("case_id"),
		"case_sha256": _sha256(case_path),
		"reference_scene_sha256": _json_hash(case["reference_scene"]),
		"asset": case["asset"],
		"asset_gltf": str(asset_path),
		"asset_files": asset_files,
		"asset_source_commit": asset_entry.get("source_commit"),
		"reference_files": reference_files,
	}


def _capture_spec(case: dict[str, Any], capture: dict[str, Any]) -> dict[str, Any]:
	scene = dict(case["reference_scene"])
	for key in ("camera", "environment", "floor", "point_lights", "resolution", "render"):
		if key in capture:
			scene[key] = capture[key]
	poses = capture.get("poses")
	instances = scene.get("instances")
	if poses is not None:
		if not isinstance(poses, list) or not isinstance(instances, list) or len(poses) != len(instances):
			raise ReferenceError(f"{case.get('case_id')}: capture poses must match reference_scene.instances")
		scene["poses"] = poses
	else:
		scene["poses"] = [instance.get("pose", {"type": "rest"}) for instance in instances or []]
	return scene


def _run_cycles_capture(blender: str, case_path: Path, asset_path: Path, scene: dict[str, Any], output: Path) -> dict[str, Any]:
	script = Path(__file__).with_name("reference_cycles.py")
	command = [blender, "-b", "--factory-startup", "-noaudio", "-P", str(script), "--", str(case_path), str(asset_path), json.dumps(scene), str(output)]
	try:
		completed = subprocess.run(command, capture_output=True, text=True, timeout=300, check=False)
	except (OSError, subprocess.TimeoutExpired) as error:
		raise ReferenceError(f"Blender Cycles render could not complete: {error}") from error
	if completed.returncode != 0 or not output.is_file():
		log = (completed.stdout + completed.stderr)[-6000:]
		raise ReferenceError(f"Blender Cycles failed for {case_path.name}:\n{log}")
	for line in reversed(completed.stdout.splitlines()):
		if line.startswith("KRISP_REFERENCE_METADATA="):
			return json.loads(line.partition("=")[2])
	raise ReferenceError(f"Blender Cycles did not report effective render settings for {case_path.name}")


def _run_khronos_capture(renderer_dir: Path, case_path: Path, asset_path: Path, scene: dict[str, Any], output: Path) -> dict[str, Any]:
	command = [sys.executable, str(Path(__file__).with_name("reference_khronos.py")),
		str(renderer_dir), str(case_path), str(asset_path), str(output)]
	with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
			stderr=subprocess.PIPE, text=True, start_new_session=True) as process:
		try:
			stdout, stderr = process.communicate(json.dumps(scene), timeout=300)
		except subprocess.TimeoutExpired as error:
			os.killpg(process.pid, signal.SIGKILL)
			process.communicate()
			raise ReferenceError("Khronos reference exceeded its 300 second deadline") from error
		if process.returncode != 0 or not output.is_file():
			raise ReferenceError(f"Khronos reference failed for {case_path.name}:\n{stderr[-6000:]}")
		return json.loads(stdout)


def generate_references(
	assets_dir: str | Path,
	output_dir: str | Path,
	*,
	renderers: Iterable[str] = ("khronos", "cycles"),
	cases_dir: str | Path = DEFAULT_SCENES,
	cases: Iterable[str] | None = None,
	blender: str | None = None,
	khronos_renderer: str | Path | None = None,
	force: bool = False,
) -> dict[str, Any]:
	"""Render requested optional references and write provenance to output_dir."""
	assets_root = Path(assets_dir).resolve()
	output_root = Path(output_dir).resolve()
	scenes_root = Path(cases_dir).resolve()
	manifest_path = assets_root / DEFAULT_MANIFEST_NAME
	asset_manifest = _load_json(manifest_path)
	selected_renderers = tuple(dict.fromkeys(renderers))
	unknown = sorted(set(selected_renderers) - {"cycles", "khronos"})
	if unknown:
		raise ReferenceError("unsupported reference renderer(s): " + ", ".join(unknown))
	if not selected_renderers:
		raise ReferenceError("at least one reference renderer must be selected")

	cycles_executable = blender or shutil.which("blender")
	if "cycles" in selected_renderers and not cycles_executable:
		raise ReferenceError("Blender is required for the Cycles reference renderer")
	if "khronos" in selected_renderers:
		if not khronos_renderer:
			khronos_renderer = Path(__file__).with_name("reference_web")
		khronos_renderer = Path(khronos_renderer).resolve()
		if not (khronos_renderer / "node_modules" / "@khronosgroup" / "gltf-viewer").is_dir():
			raise ReferenceError(
				"Khronos glTF Sample Renderer package is not installed; run "
				f"`npm install --prefix {khronos_renderer}`"
			)

	cycles_version = _renderer_version(cycles_executable, ["--version"]) if "cycles" in selected_renderers else None
	khronos_version = "@khronosgroup/gltf-viewer " + _load_json(khronos_renderer / "node_modules" / "@khronosgroup" / "gltf-viewer" / "package.json")["version"] if "khronos" in selected_renderers else None
	output_root.mkdir(parents=True, exist_ok=True)
	previous_items = {}
	previous_manifest_path = output_root / REFERENCE_MANIFEST
	if previous_manifest_path.is_file():
		try:
			previous_manifest = _load_json(previous_manifest_path)
			previous_items = {
				(item.get("case_id"), item.get("capture_id"), item.get("renderer")): item
				for item in previous_manifest.get("references", []) if isinstance(item, dict)
			}
		except ReferenceError:
			previous_items = {}
	items = []
	case_ids = []
	for case_path in _scene_files(scenes_root, cases):
		case = _load_json(case_path)
		if not isinstance(case.get("case_id"), str):
			raise ReferenceError(f"{case_path}: case_id must be a string")
		case_ids.append(case["case_id"])
		input_record = _case_input(case_path, case, assets_root, asset_manifest)
		for capture_name, capture in _case_captures(case).items():
			scene = _capture_spec(case, capture)
			for renderer in selected_renderers:
				image_path = output_root / renderer / case["case_id"] / f"{capture_name}.png"
				image_path.parent.mkdir(parents=True, exist_ok=True)
				renderer_version = cycles_version if renderer == "cycles" else khronos_version
				existing = previous_items.get((case["case_id"], capture_name, renderer))
				if (
					not force and existing is not None and image_path.is_file()
					and existing.get("file_sha256") == _sha256(image_path)
					and all(existing.get(field) == input_record[source] for field, source in (("case_definition_sha256", "case_sha256"), ("reference_scene_sha256", "reference_scene_sha256"), ("asset_hashes", "asset_files"), ("asset_source_commit", "asset_source_commit"), ("reference_files", "reference_files")))
					and existing.get("scene_sha256") == _json_hash(scene)
					and existing.get("renderer_version") == renderer_version
					and existing.get("adapter_hashes") == _adapter_hashes(renderer)
				):
					items.append(existing)
					continue
				if renderer == "cycles":
					adapter_record = _run_cycles_capture(cycles_executable, case_path, Path(input_record["asset_gltf"]), scene, image_path)
				else:
					adapter_record = _run_khronos_capture(khronos_renderer, case_path, Path(input_record["asset_gltf"]), scene, image_path)
				items.append({
					"case_id": case["case_id"],
					"capture_id": capture_name,
					"renderer": renderer,
					"image": image_path.relative_to(output_root).as_posix(),
					"file_sha256": _sha256(image_path),
					"case_definition_sha256": input_record["case_sha256"],
					"reference_scene_sha256": input_record["reference_scene_sha256"],
					"asset": input_record["asset"],
					"asset_hashes": input_record["asset_files"],
					"asset_source_commit": input_record["asset_source_commit"],
					"reference_files": input_record["reference_files"],
					"renderer_version": renderer_version,
					"adapter_hashes": _adapter_hashes(renderer),
					"render_capabilities": {"cast_shadows": renderer == "cycles", "shadow_note": None if renderer == "cycles" else "Khronos glTF Sample Renderer does not implement cast shadows."} if renderer == "khronos" else {"cast_shadows": True},
					**adapter_record,
					"scene_sha256": _json_hash(scene),
					"settings": scene,
					"limitations": [] if renderer == "cycles" else ["Khronos Sample Renderer uses the same camera, floor, point-light positions and intensities, and LDR environment; cast shadows are unsupported."],
				})
	manifest = {
		"schema_version": SCHEMA_VERSION,
		"suite": "rendering-fidelity-references",
		"assets_manifest": str(manifest_path),
		"assets_manifest_sha256": _sha256(manifest_path),
		"renderers": list(selected_renderers),
		"renderer_versions": {"cycles": cycles_version, "khronos": khronos_version},
		"cases": case_ids,
		"references": items,
	}
	(output_root / REFERENCE_MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
	return manifest


def verify_references(
	assets_dir: str | Path,
	references_dir: str | Path,
	*,
	cases_dir: str | Path = DEFAULT_SCENES,
	blender: str | None = None,
	khronos_renderer: str | Path | None = None,
	check_versions: bool = False,
) -> tuple[dict[str, Any], list[str]]:
	"""Return the recorded manifest and actionable stale/missing evidence errors."""
	assets_root, output_root, scenes_root = Path(assets_dir).resolve(), Path(references_dir).resolve(), Path(cases_dir).resolve()
	manifest_path = output_root / REFERENCE_MANIFEST
	if not manifest_path.is_file():
		return {}, [f"reference manifest missing: {manifest_path}"]
	try:
		manifest = _load_json(manifest_path)
		asset_manifest_path = assets_root / DEFAULT_MANIFEST_NAME
		asset_manifest = _load_json(asset_manifest_path)
	except ReferenceError as error:
		return {}, [str(error)]
	errors = []
	if manifest.get("schema_version") != SCHEMA_VERSION:
		errors.append("reference manifest schema version is unsupported")
	if manifest.get("assets_manifest_sha256") != _sha256(asset_manifest_path):
		errors.append("asset manifest changed since references were generated")
	versions = manifest.get("renderer_versions", {})
	if check_versions:
		for renderer in manifest.get("renderers", []):
			if renderer == "cycles":
				executable = blender or shutil.which("blender")
				if not executable:
					errors.append("Blender is required to verify the recorded Cycles renderer version")
				else:
					try:
						current = _renderer_version(executable, ["--version"])
					except ReferenceError as error:
						errors.append(str(error))
					else:
						if current != versions.get("cycles"):
							errors.append("Blender version changed since Cycles references were generated")
			elif renderer == "khronos":
				tool_root = Path(khronos_renderer or Path(__file__).with_name("reference_web")).resolve()
				package_path = tool_root / "node_modules" / "@khronosgroup" / "gltf-viewer" / "package.json"
				try:
					current = "@khronosgroup/gltf-viewer " + _load_json(package_path)["version"]
				except (ReferenceError, KeyError) as error:
					errors.append(f"Khronos Sample Renderer package is required to verify renderer versions: {error}")
				else:
					if current != versions.get("khronos"):
						errors.append("Khronos Sample Renderer version changed since references were generated")
	items = manifest.get("references")
	if not isinstance(items, list):
		return manifest, errors + ["reference manifest references must be an array"]
	if not isinstance(manifest.get("renderers"), list) or not all(renderer in {"cycles", "khronos"} for renderer in manifest["renderers"]):
		return manifest, errors + ["reference manifest renderers must contain supported renderer ids"]
	if not isinstance(manifest.get("cases"), list) or not all(isinstance(case, str) for case in manifest["cases"]):
		return manifest, errors + ["reference manifest cases must be an array of case ids"]
	known = {}
	for item in items:
		if not isinstance(item, dict):
			errors.append("reference record must be an object")
			continue
		key = (item.get("case_id"), item.get("capture_id"), item.get("renderer"))
		if key in known:
			errors.append(f"duplicate reference record: {key}")
		else:
			known[key] = item
	expected_keys = set()
	for case_path in _scene_files(scenes_root, manifest.get("cases")):
		try:
			case = _load_json(case_path)
			input_record = _case_input(case_path, case, assets_root, asset_manifest)
		except ReferenceError as error:
			errors.append(str(error))
			continue
		for capture_name in _case_captures(case):
			for renderer in manifest.get("renderers", []):
				key = (case.get("case_id"), capture_name, renderer)
				expected_keys.add(key)
				item = known.get(key)
				if item is None:
					errors.append(f"reference missing from manifest: {key}")
					continue
				if item.get("adapter_hashes") != _adapter_hashes(renderer):
					errors.append(f"{key}: renderer adapter changed; reference is stale")
				for field, source in (("case_definition_sha256", "case_sha256"), ("reference_scene_sha256", "reference_scene_sha256"), ("asset_hashes", "asset_files"), ("asset_source_commit", "asset_source_commit"), ("reference_files", "reference_files")):
					if item.get(field) != input_record[source]:
						errors.append(f"{key}: {field} changed; reference is stale")
				scene = _capture_spec(case, _case_captures(case)[capture_name])
				if item.get("scene_sha256") != _json_hash(scene) or item.get("settings") != scene:
					errors.append(f"{key}: settings changed or do not match the reference scene")
				image_relative = item.get("image")
				if not isinstance(image_relative, str):
					errors.append(f"{key}: image path must be relative")
					continue
				image_path = (output_root / image_relative).resolve()
				try:
					image_path.relative_to(output_root)
				except ValueError:
					errors.append(f"{key}: image path escapes references directory")
					continue
				if not image_path.is_file():
					errors.append(f"{key}: image missing: {image_path}")
				elif item.get("file_sha256") != _sha256(image_path):
					errors.append(f"{key}: image checksum changed: {image_path}")
	for key in sorted(set(known) - expected_keys):
		errors.append(f"unexpected reference record: {key}")
	return manifest, errors


def main(argv: list[str] | None = None) -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	subparsers = parser.add_subparsers(dest="command", required=True)
	render = subparsers.add_parser("render", help="render optional external references")
	render.add_argument("--assets", type=Path, required=True)
	render.add_argument("--output", type=Path, required=True)
	render.add_argument("--cases-dir", type=Path, default=DEFAULT_SCENES)
	render.add_argument("--case", action="append", dest="cases")
	render.add_argument("--renderer", choices=("cycles", "khronos"), action="append", dest="renderers")
	render.add_argument("--blender")
	render.add_argument("--khronos-renderer", type=Path)
	render.add_argument("--force", action="store_true")
	verify = subparsers.add_parser("verify", help="check reference provenance and staleness")
	verify.add_argument("--assets", type=Path, required=True)
	verify.add_argument("--references", type=Path, required=True)
	verify.add_argument("--cases-dir", type=Path, default=DEFAULT_SCENES)
	args = parser.parse_args(argv)
	try:
		if args.command == "render":
			manifest = generate_references(args.assets, args.output, renderers=args.renderers or ("khronos", "cycles"), cases_dir=args.cases_dir, cases=args.cases, blender=args.blender, khronos_renderer=args.khronos_renderer, force=args.force)
			print(f"Generated {len(manifest['references'])} external reference image(s) in {args.output}")
			return 0
		_, errors = verify_references(args.assets, args.references, cases_dir=args.cases_dir)
		if errors:
			print("Reference verification failed:\n" + "\n".join(f"- {error}" for error in errors), file=sys.stderr)
			return 1
		print("External reference images and provenance are current")
		return 0
	except ReferenceError as error:
		print(f"Reference rendering error: {error}", file=sys.stderr)
		return 2


if __name__ == "__main__":
	raise SystemExit(main())
