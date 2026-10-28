import copy
import tempfile
import unittest
from pathlib import Path

from applications.rendering_fidelity.fidelity_report import write_report


class FidelityReportReferenceTests(unittest.TestCase):
	def test_external_references_are_review_only_and_do_not_change_verdict(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			current = root / "current.png"
			external = root / "external.png"
			current.write_bytes(b"current image")
			external.write_bytes(b"external image")
			results = {
				"status": "pass",
				"cases": [{
					"case_id": "sample",
					"captures": [{
						"capture_id": "default",
						"status": "pass",
						"image_path": str(current),
						"external_references": [{
						"renderer": "Cycles <script>bad()</script>",
							"image_path": str(external),
						"metadata": {"renderer_version": "4.5.0", "asset": "Avocado", "provenance": "<img onerror=bad()>"},
							"limitations": ["Reference shading omits <engine-specific> features."],
						}],
					}],
				}],
				"reference_errors": [],
			}
			before = copy.deepcopy(results)
			report = root / "report.html"

			write_report(results, report)

			body = report.read_text(encoding="utf-8")
			self.assertEqual(results, before)
			self.assertIn("Run status: <strong>pass</strong>", body)
			self.assertIn("External reference review", body)
			self.assertIn("Current Krisp", body)
			self.assertIn("Cycles &lt;script&gt;bad()&lt;/script&gt;", body)
			self.assertIn("Cycles &lt;script&gt;bad()&lt;/script&gt; provenance and limitations", body)
			self.assertIn("&lt;img onerror=bad()&gt;", body)
			self.assertIn("data:image/png;base64,", body)
			self.assertIn("not affect the fidelity verdict", body)
			self.assertIn("&lt;engine-specific&gt;", body)
			self.assertNotIn("<engine-specific>", body)
			self.assertNotIn("<script>bad()</script>", body)

	def test_reference_errors_are_escaped_and_do_not_create_a_verdict(self):
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			results = {"status": "nondeterministic", "cases": [], "reference_errors": ["<script>bad()</script>"]}
			before = copy.deepcopy(results)
			report = root / "report.html"

			write_report(results, report)

			body = report.read_text(encoding="utf-8")
			self.assertEqual(results, before)
			self.assertIn("External reference issues", body)
			self.assertIn("&lt;script&gt;bad()&lt;/script&gt;", body)
			self.assertNotIn("<script>bad()</script>", body)
			self.assertIn("Run status: <strong>nondeterministic</strong>", body)
			self.assertIn("do not affect the fidelity verdict", body)


if __name__ == "__main__":
	unittest.main()
