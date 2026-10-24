"""Create a self-contained offline HTML view of immutable run evidence."""

from __future__ import annotations

import base64
import html
from pathlib import Path
from typing import Any

if __package__ in (None, ""):
	from fidelity_images import diagnostic_difference
else:
	from .fidelity_images import diagnostic_difference


def _image(path: Path | None) -> str:
	if path is None or not path.is_file():
		return '<div class="missing">Image unavailable</div>'
	try:
		encoded = base64.b64encode(path.read_bytes()).decode("ascii")
		return f'<img src="data:image/png;base64,{encoded}" alt="PNG evidence">'
	except OSError:
		return '<div class="missing">Image unavailable</div>'


def write_report(results: dict[str, Any], output_path: Path) -> None:
	"""Write one portable HTML file; diagnostics never feed back into results."""
	rows: list[str] = []
	for case in results.get("cases", []):
		for capture in case.get("captures", []):
			baseline = Path(capture["baseline_path"]) if capture.get("baseline_path") else None
			current = Path(capture["image_path"]) if capture.get("image_path") else None
			difference = None
			if baseline and current and baseline.is_file() and current.is_file():
				payload = diagnostic_difference(baseline, current)
				if payload is not None:
					encoded = base64.b64encode(payload).decode("ascii")
					difference = f'<img src="data:image/png;base64,{encoded}" alt="Amplified diagnostic difference">'
			status = html.escape(capture.get("status", "error"))
			reasons = html.escape("; ".join(capture.get("reasons", [])))
			diagnostics = capture.get("comparison") or {}
			changed = diagnostics.get("changed_pixels")
			bounds = diagnostics.get("bounds")
			changed_label = str(changed) if changed is not None else "not available"
			bounds_label = str(bounds) if bounds is not None else "not available"
			repeatability = capture.get("repeatability")
			repeat_text = "not available" if repeatability is None else (
				"identical" if repeatability.get("equal") else
				f"different ({repeatability.get('changed_pixels')} changed pixels, bounds {repeatability.get('bounds')})"
			)
			repetition_section = ""
			repetitions = capture.get("repetitions", [])
			if repeatability is not None and not repeatability.get("equal") and len(repetitions) >= 2:
				first = Path(repetitions[0]["image_path"]) if repetitions[0].get("image_path") else None
				second = Path(repetitions[1]["image_path"]) if repetitions[1].get("image_path") else None
				repeat_diff = None
				if first and second:
					payload = diagnostic_difference(first, second)
					if payload is not None:
						encoded = base64.b64encode(payload).decode("ascii")
						repeat_diff = f'<img src="data:image/png;base64,{encoded}" alt="Amplified repeatability difference">'
				repetition_section = (
					'<h3>Repeatability evidence (diagnostic only)</h3><div class="images">'
					f'<figure><figcaption>Fresh process 1</figcaption>{_image(first)}</figure>'
					f'<figure><figcaption>Fresh process 2</figcaption>{_image(second)}</figure>'
					f'<figure><figcaption>Amplified repeatability difference</figcaption>{repeat_diff or "<div class=\"missing\">Difference unavailable</div>"}</figure></div>'
				)
			rows.append(
				f'<section><h2>{html.escape(case.get("case_id", "?"))} / {html.escape(capture.get("capture_id", "?"))}</h2>'
				f'<p><strong>Status:</strong> {status} {reasons}</p>'
				f'<p><strong>Repeatability:</strong> {html.escape(repeat_text)}; '
				f'<strong>Changed pixels:</strong> {html.escape(changed_label)}; '
				f'<strong>Bounds:</strong> {html.escape(bounds_label)}</p>'
				f'<details><summary>Environment and effective settings</summary><pre>{html.escape(_pretty(capture.get("environment")))}</pre>'
				f'<pre>{html.escape(_pretty(capture.get("effective_settings")))}</pre></details>'
				f'<div class="images"><figure><figcaption>Approved baseline</figcaption>{_image(baseline)}</figure>'
				f'<figure><figcaption>Current capture</figcaption>{_image(current)}</figure>'
				f'<figure><figcaption>Amplified difference (diagnostic only)</figcaption>{difference or "<div class=\"missing\">Difference unavailable</div>"}</figure></div>{repetition_section}</section>'
			)
	status = html.escape(results.get("status", "error"))
	page = """<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>Rendering fidelity evidence</title><style>
body{font:15px system-ui,sans-serif;margin:2rem;background:#f4f5f7;color:#18202a}main{max-width:1500px;margin:auto}section{background:white;padding:1rem 1.4rem;margin:1.2rem 0;border:1px solid #ccd2da;border-radius:8px}h1{font-size:1.7rem}.images{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:1rem}figure{margin:0}figcaption{font-weight:600;margin:.5rem 0}img{display:block;max-width:100%;height:auto;image-rendering:pixelated;border:1px solid #bbb}.missing{min-height:140px;display:grid;place-items:center;background:#eee;color:#555}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#f3f4f6;padding:.7rem}
</style><main><h1>Rendering fidelity evidence</h1><p>Run status: <strong>""" + status + """</strong>. Amplified difference images are diagnostic only and do not affect the exact pixel verdict.</p>""" + "\n".join(rows) + "</main></html>"
	output_path.parent.mkdir(parents=True, exist_ok=True)
	output_path.write_text(page, encoding="utf-8")


def _pretty(value: Any) -> str:
	import json
	return json.dumps(value, indent=2, sort_keys=True) if value is not None else "unavailable"
