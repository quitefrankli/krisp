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


def _external_reference_section(capture: dict[str, Any], current: Path | None) -> str:
	references = capture.get("external_references", [])
	if not references:
		return ""
	figures = [f'<figure><figcaption>Current Krisp</figcaption>{_image(current)}</figure>']
	details: list[str] = []
	for reference in references:
		renderer = html.escape(str(reference.get("renderer", "unknown renderer")))
		image_path = Path(reference["image_path"]) if reference.get("image_path") else None
		figures.append(f'<figure><figcaption>{renderer}</figcaption>{_image(image_path)}</figure>')
		limitations = reference.get("limitations", [])
		if isinstance(limitations, str):
			limitations = [limitations]
		elif not isinstance(limitations, list):
			limitations = [_pretty(limitations)]
		limitation_html = "".join(f"<li>{html.escape(str(item))}</li>" for item in limitations)
		details.append(
			f'<article><h4>{renderer} provenance and limitations</h4>'
			f'<details><summary>Renderer metadata</summary><pre>{html.escape(_pretty(reference.get("metadata")))}</pre></details>'
			f'<p>Limitations:</p><ul>{limitation_html or "<li>None reported</li>"}</ul></article>'
		)
	return (
		'<section class="review-only"><h3>External reference review</h3>'
		'<p>Review only. These independent renders and their metadata do not affect the fidelity verdict.</p>'
		f'<div class="images">{"".join(figures)}</div>{"".join(details)}</section>'
	)


def _external_reference_errors(errors: Any) -> str:
	if not errors:
		return ""
	if not isinstance(errors, list):
		errors = [errors]
	items = "".join(f"<li>{html.escape(str(error))}</li>" for error in errors)
	return (
		'<section class="review-only"><h2>External reference issues</h2>'
		'<p>Review only. Reference generation issues do not affect the fidelity verdict.</p>'
		f"<ul>{items}</ul></section>"
	)


def write_report(results: dict[str, Any], output_path: Path) -> None:
	"""Write one portable HTML file; diagnostics never feed back into results."""
	rows: list[str] = []
	for case in results.get("cases", []):
		for capture in case.get("captures", []):
			baseline = Path(capture["baseline_path"]) if capture.get("baseline_path") else None
			current = Path(capture["image_path"]) if capture.get("image_path") else None
			reference_section = _external_reference_section(capture, current)
			difference = None
			if baseline and current and baseline.is_file() and current.is_file():
				payload = diagnostic_difference(baseline, current)
				if payload is not None:
					encoded = base64.b64encode(payload).decode("ascii")
					difference = f'<img src="data:image/png;base64,{encoded}" alt="Amplified diagnostic difference">'
			status = html.escape(capture.get("status", "error"))
			reasons = html.escape("; ".join(dict.fromkeys(capture.get("reasons", []))))
			failure_messages = list(dict.fromkeys(
				item.get("worker", {}).get("stderr_tail", "").strip()
				for item in capture.get("repetitions", [])
				if item.get("worker", {}).get("stderr_tail", "").strip()))
			failure_summary = ('<p><strong>Worker error:</strong> ' + html.escape("; ".join(failure_messages)) + "</p>"
				if capture.get("status") == "error" and failure_messages else "")
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
			worker_diagnostics = ""
			if capture.get("status") == "error":
				worker_diagnostics = (
					'<details><summary>Worker failure diagnostics</summary><pre>'
					+ html.escape(_pretty([{"repetition": item.get("repetition"),
						"error": item.get("error"), "worker": item.get("worker")}
						for item in repetitions])) + '</pre></details>')
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
				f'<p><strong>Status:</strong> {status} {reasons}</p>{failure_summary}'
				f'<p><strong>Repeatability:</strong> {html.escape(repeat_text)}; '
				f'<strong>Changed pixels:</strong> {html.escape(changed_label)}; '
				f'<strong>Bounds:</strong> {html.escape(bounds_label)}</p>'
				f'<details><summary>Environment and effective settings</summary><pre>{html.escape(_pretty(capture.get("environment")))}</pre>'
				f'<pre>{html.escape(_pretty(capture.get("effective_settings")))}</pre></details>'
				f'<div class="images"><figure><figcaption>Approved baseline</figcaption>{_image(baseline)}</figure>'
				f'<figure><figcaption>Current capture</figcaption>{_image(current)}</figure>'
				f'<figure><figcaption>Amplified difference (diagnostic only)</figcaption>{difference or "<div class=\"missing\">Difference unavailable</div>"}</figure></div>{worker_diagnostics}{repetition_section}{reference_section}</section>'
			)
	status = html.escape(results.get("status", "error"))
	reference_errors = _external_reference_errors(results.get("reference_errors", []))
	page = """<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>Rendering fidelity evidence</title><style>
body{font:15px system-ui,sans-serif;margin:2rem;background:#f4f5f7;color:#18202a}main{max-width:1500px;margin:auto}section{background:white;padding:1rem 1.4rem;margin:1.2rem 0;border:1px solid #ccd2da;border-radius:8px}h1{font-size:1.7rem}.images{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:1rem}figure{margin:0}figcaption{font-weight:600;margin:.5rem 0}img{display:block;max-width:100%;height:auto;image-rendering:pixelated;border:1px solid #bbb}.missing{min-height:140px;display:grid;place-items:center;background:#eee;color:#555}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#f3f4f6;padding:.7rem}.review-only{border-left:5px solid #64748b;background:#f8fafc}.review-only article{border-top:1px solid #ccd2da;margin-top:1rem;padding-top:.5rem}
</style><main><h1>Rendering fidelity evidence</h1><p>Run status: <strong>""" + status + """</strong>. Amplified difference images are diagnostic only and do not affect the exact pixel verdict.</p>""" + "\n".join(rows) + reference_errors + "</main></html>"
	output_path.parent.mkdir(parents=True, exist_ok=True)
	output_path.write_text(page, encoding="utf-8")


def _pretty(value: Any) -> str:
	import json
	return json.dumps(value, indent=2, sort_keys=True) if value is not None else "unavailable"
