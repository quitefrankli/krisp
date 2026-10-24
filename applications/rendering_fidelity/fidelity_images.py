"""Lossless image decoding and exact RGBA8 comparison."""

from __future__ import annotations

import hashlib
import io
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops, ImageEnhance, UnidentifiedImageError


def decode_rgba(path: Path) -> tuple[tuple[int, int], bytes]:
	"""Decode a PNG without considering its encoding or ancillary metadata."""
	with Image.open(path) as image:
		if image.format != "PNG":
			raise ValueError(f"expected PNG image data in {path}")
		image.load()
		return image.size, image.convert("RGBA").tobytes()


def pixel_sha256(size: tuple[int, int], pixels: bytes) -> str:
	digest = hashlib.sha256()
	digest.update(size[0].to_bytes(8, "big"))
	digest.update(size[1].to_bytes(8, "big"))
	digest.update(pixels)
	return digest.hexdigest()


def compare_pixels(left_size: tuple[int, int], left: bytes, right_size: tuple[int, int], right: bytes) -> dict[str, Any]:
	"""Compare every decoded byte and return diagnostics that never affect equality."""
	if left_size != right_size:
		return {
			"equal": False, "reason": "dimensions-differ", "left_size": list(left_size),
			"right_size": list(right_size), "changed_pixels": None, "bounds": None,
			"changed_channels": None,
		}
	if len(left) != len(right) or len(left) != left_size[0] * left_size[1] * 4:
		return {"equal": False, "reason": "invalid-decoded-pixel-length", "changed_pixels": None,
			"bounds": None, "changed_channels": None}
	if left == right:
		return {"equal": True, "reason": None, "changed_pixels": 0, "bounds": None, "changed_channels": 0}

	width, height = left_size
	changed_pixels = 0
	changed_channels = 0
	min_x, min_y, max_x, max_y = width, height, -1, -1
	for pixel_index in range(width * height):
		start = pixel_index * 4
		different = [left[start + channel] != right[start + channel] for channel in range(4)]
		if any(different):
			changed_pixels += 1
			changed_channels += sum(different)
			x, y = pixel_index % width, pixel_index // width
			min_x, min_y = min(min_x, x), min(min_y, y)
			max_x, max_y = max(max_x, x), max(max_y, y)
	return {
		"equal": False, "reason": "pixels-differ", "changed_pixels": changed_pixels,
		"bounds": [min_x, min_y, max_x, max_y], "changed_channels": changed_channels,
	}


def compare_pngs(left: Path, right: Path) -> dict[str, Any]:
	try:
		left_size, left_pixels = decode_rgba(left)
		right_size, right_pixels = decode_rgba(right)
	except (OSError, ValueError, UnidentifiedImageError) as error:
		return {"equal": False, "reason": "image-decode-error", "error": str(error),
			"changed_pixels": None, "bounds": None, "changed_channels": None}
	return compare_pixels(left_size, left_pixels, right_size, right_pixels)


def diagnostic_difference(left: Path, right: Path) -> bytes | None:
	"""Return an amplified RGB difference PNG payload for offline review only."""
	try:
		with Image.open(left) as first, Image.open(right) as second:
			first.load()
			second.load()
			if first.size != second.size:
				return None
			first_rgba, second_rgba = first.convert("RGBA"), second.convert("RGBA")
			rgb_difference = ImageEnhance.Brightness(
				ImageChops.difference(first_rgba.convert("RGB"), second_rgba.convert("RGB"))
			).enhance(4.0)
			red, green, blue = rgb_difference.split()
			alpha_difference = ImageChops.difference(first_rgba.getchannel("A"), second_rgba.getchannel("A"))
			alpha_difference = ImageEnhance.Brightness(alpha_difference).enhance(4.0)
			difference = Image.merge("RGB", (
				ImageChops.lighter(red, alpha_difference), green, ImageChops.lighter(blue, alpha_difference),
			))
			output = io.BytesIO()
			difference.save(output, format="PNG", optimize=False)
			return output.getvalue()
	except (OSError, ValueError, UnidentifiedImageError):
		return None
