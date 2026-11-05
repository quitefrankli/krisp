#!/usr/bin/env python3
"""Run the sandbox under an index-relabeling Vulkan interposer.

Requires a C compiler, Vulkan and X11 development headers, and ``xvfb-run``.
Usage: ``python3 test/test_swapchain_acquisition.py [sandbox-executable]``.
"""

import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
PROBE = Path(__file__).with_name("swapchain_acquisition_probe.c")
MARKER = re.compile(
    r"SWAPCHAIN_ACQUISITION_PROBE: success=(\d+) presents=(\d+) "
    r"acquisitions=(\d+) nonsequential=(\d+) image_count=(\d+)"
)
VALIDATION_ERROR = re.compile(r"VUID-|ValidationLayerMessage|Validation Error", re.I)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "executable",
        nargs="?",
        type=Path,
        default=ROOT / "build/debug/applications/sandbox/sandbox",
        help="sandbox executable (default: build/debug/applications/sandbox/sandbox)",
    )
    args = parser.parse_args()
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error(f"sandbox executable not found: {executable}")

    with tempfile.TemporaryDirectory(prefix="krisp-swapchain-probe-") as temporary:
        library = Path(temporary) / "swapchain_acquisition_probe.so"
        compile_result = subprocess.run(
            [
                "cc",
                "-std=c11",
                "-shared",
                "-fPIC",
                str(PROBE),
                "-o",
                str(library),
                "-ldl",
                "-lX11",
            ],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if compile_result.returncode != 0:
            raise RuntimeError(f"probe compilation failed:\n{compile_result.stdout}")

        environment = os.environ.copy()
        existing_preload = environment.get("LD_PRELOAD")
        preload = (
            f"{library}:{existing_preload}" if existing_preload else str(library)
        )
        try:
            run_result = subprocess.run(
                ["xvfb-run", "-a", "env", f"LD_PRELOAD={preload}", str(executable)],
                cwd=ROOT,
                env=environment,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=120,
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            output = error.stdout or ""
            if isinstance(output, bytes):
                output = output.decode(errors="replace")
            raise RuntimeError(f"sandbox timed out after 120 seconds:\n{output}") from error

    output = run_result.stdout
    if run_result.returncode != 0:
        raise RuntimeError(f"sandbox exited with {run_result.returncode}:\n{output}")
    if VALIDATION_ERROR.search(output):
        raise RuntimeError(f"Vulkan validation reported an error:\n{output}")
    match = MARKER.search(output)
    if match is None:
        raise RuntimeError(f"probe result marker was missing:\n{output}")
    success, presents, acquisitions, nonsequential, image_count = map(int, match.groups())
    if success != 1 or nonsequential != 1:
        raise RuntimeError(f"probe did not prove nonsequential acquisition:\n{output}")
    print(
        "Swapchain acquisition regression passed: "
        f"{acquisitions} acquisitions, {presents} presents, "
        f"{image_count} images, nonsequential indices observed."
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        raise SystemExit(str(error))
