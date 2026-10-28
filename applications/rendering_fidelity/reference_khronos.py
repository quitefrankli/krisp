"""Headless adapter for the official Khronos glTF Sample Renderer."""

from __future__ import annotations

import base64
import json
import math
import os
import shutil
import threading
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import quote


def _linear_srgb(value: float) -> float:
	value /= 255.0
	return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def _rgbe(rgb: tuple[float, float, float]) -> bytes:
	maximum = max(rgb)
	if maximum < 1.0e-32:
		return b"\0\0\0\0"
	exponent = math.floor(math.log2(maximum)) + 1
	scale = 256.0 / (2.0**exponent)
	channels = [max(0, min(255, int(channel * scale))) for channel in rgb]
	return bytes((*channels, exponent + 128))


def _neutral_to_radiance(color: tuple[float, float, float], radiance_scale: float) -> tuple[bytes, dict[str, object]]:
	header = b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n"
	return header + _rgbe(tuple(channel * radiance_scale for channel in color)), {
		"conversion": "linear neutral radiance encoded as one RGBE environment pixel",
		"radiance_scale": radiance_scale,
		"width": 1,
		"height": 1,
	}


def _ldr_to_radiance(source: Path, radiance_scale: float) -> tuple[bytes, dict[str, object]]:
	try:
		from PIL import Image
	except ImportError as error:
		raise RuntimeError("Pillow is required to decode the shared LDR environment for Khronos IBL") from error
	with Image.open(source) as image:
		image = image.convert("RGB")
		width, height = image.size
		pixels = image.load()
		rows = []
		for y in range(height):
			row = bytearray()
			for x in range(width):
				row.extend(_rgbe(tuple(_linear_srgb(channel) * radiance_scale for channel in pixels[x, y])))
			rows.append(row)
	header = f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode("ascii")
	# Old-style flat RGBE scanlines are valid Radiance encoding and avoid any
	# filtering, tone mapping or invented range when lifting the LDR input.
	return header + b"".join(rows), {
		"source": str(source),
		"source_sha256": _sha256(source),
		"conversion": "sRGB decode to linear RGB; encode unchanged values as RGBE; no exposure or tone-map adjustment",
		"radiance_scale": radiance_scale,
		"width": width,
		"height": height,
	}


def _sha256(path: Path) -> str:
	import hashlib

	digest = hashlib.sha256()
	with path.open("rb") as source:
		for chunk in iter(lambda: source.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def _serve(root: Path):
	class QuietHandler(SimpleHTTPRequestHandler):
		def log_message(self, format, *args):
			pass
	server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(root)))
	thread = threading.Thread(target=server.serve_forever, daemon=True)
	thread.start()
	return server, thread


def render_capture(renderer_dir: Path, case_path: Path, asset_path: Path, scene: dict, output_path: Path) -> dict:
	try:
		from playwright.sync_api import sync_playwright
	except ImportError as error:
		raise RuntimeError("Python Playwright is required for headless Khronos Sample Renderer captures") from error

	# Installed tools still render the source fixtures used by the C++ worker.
	repository_root = case_path.resolve().parents[4]
	try:
		asset_relative = asset_path.resolve().relative_to(repository_root)
	except ValueError as error:
		raise RuntimeError("Khronos headless server expects cached assets and case JSON inside the repository") from error
	resolution = scene.get("resolution", [640, 480])
	asset_url_path = "/" + quote(asset_relative.as_posix())
	environment_bytes = None
	environment_record = None
	environment = scene.get("environment", {})
	if environment.get("type") == "equirectangular":
		environment_path = (case_path.parent / environment["path"]).resolve()
		try:
			environment_path.relative_to(repository_root)
		except ValueError as error:
			raise RuntimeError("reference environment must be stored inside the repository") from error
		environment_bytes, environment_record = _ldr_to_radiance(environment_path, float(environment.get("radiance_scale", 1.0)))
	elif environment.get("type", "neutral") == "neutral":
		radiance = tuple(float(value) for value in environment.get("radiance", [0.18, 0.18, 0.18]))
		environment_bytes, environment_record = _neutral_to_radiance(radiance, float(environment.get("radiance_scale", 1.0)))
	else:
		raise RuntimeError(f"unsupported Khronos environment descriptor: {environment}")

	job = {
		"assetUrl": asset_url_path,
		"scene": scene,
		"environmentBytes": base64.b64encode(environment_bytes).decode("ascii") if environment_bytes else None,
		"environmentConversion": environment_record,
	}
	server, thread = _serve(repository_root)
	console_errors = []
	try:
		with sync_playwright() as playwright:
			browser_path = os.environ.get("CHROME_BIN") or shutil.which("google-chrome") or shutil.which("chromium") or shutil.which("chromium-browser")
			launch = {"headless": True, "args": ["--no-sandbox", "--enable-webgl", "--ignore-gpu-blocklist", "--disable-dev-shm-usage",
				"--use-angle=gl", "--disable-software-rasterizer"]}
			if browser_path:
				launch["executable_path"] = browser_path
			browser = playwright.chromium.launch(**launch)
			page = browser.new_page(viewport={"width": int(resolution[0]), "height": int(resolution[1])}, device_scale_factor=1)
			page.on("console", lambda message: console_errors.append(message.text) if message.type == "error" else None)
			page.set_default_timeout(240000)
			page.goto(f"http://127.0.0.1:{server.server_port}/applications/rendering_fidelity/reference_web/index.html", wait_until="load", timeout=60000)
			try:
				result = page.evaluate("job => window.renderKhronosReference(job)", job)
			except Exception as error:
				browser.close()
				diagnostics = "\n".join(console_errors[-20:])
				raise RuntimeError(f"Khronos Sample Renderer failed: {error}\n{diagnostics}") from error
			browser.close()
	finally:
		server.shutdown()
		server.server_close()
		thread.join(timeout=2)

	output_path.parent.mkdir(parents=True, exist_ok=True)
	output_path.write_bytes(base64.b64decode(result.pop("png")))
	return {**result, "environment_conversion": environment_record}


if __name__ == "__main__":
	import sys
	try:
		print(json.dumps(render_capture(*(Path(value) for value in sys.argv[1:4]),
			json.load(sys.stdin), Path(sys.argv[4]))))
	except Exception as error:
		print(str(error), file=sys.stderr)
		sys.exit(1)
