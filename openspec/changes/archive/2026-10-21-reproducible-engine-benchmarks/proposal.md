# Proposal

## Why

The current `stress_test` scene exercises rendering but cannot produce unattended, attributable performance comparisons: it has no measurement lifecycle or results, both engine loops are capped near 60 Hz, and existing CPU loop timers do not measure GPU execution. A same-machine benchmark suite is needed to distinguish throughput regressions from poor frame pacing without manual scene setup or log interpretation.

## What Changes

- Turn `stress_test` into an unattended suite runner with preflight, isolated repetitions, startup readiness, warm-up, measurement, result export, and automatic termination.
- Make `standard` the default routine suite: the four existing workloads plus the full-resolution Sponza atrium view, in two scheduling modes with two repetitions, two seconds of warm-up, and five seconds of measurement (20 workers; 140 seconds of timed work). Sponza startup is hardware-dependent, so there is no under-five-minute completion guarantee.
- Add versioned static-object, moving-transform, and shadow workloads at fixed sizes, with deterministic simulation-tick motion and explicit camera, lighting, and graphics settings.
- Add Khronos Flight Helmet for repeated textured meshes and Intel Sponza Base Scene for smoke correctness coverage and three full-resolution standard view workloads. Include the atrium in the standard default; allow corridor and material-closeup as optional selections. Pin sources, licences and checksums; automatically prepare a missing cache before a run using an explicitly documented engine-compatible PBR profile (including the approved single-component Sponza emissive normalisation). Allow pinned local source inputs for offline preparation.
- Measure uncapped rendering throughput with a 60 Hz simulation target and a separate mode targeting 60 Hz for both loops, preserving the engine's independent threads and latest-wins render mailbox.
- Add an application-neutral `RuntimeOptions.performance_overlay` switch, disabled by default and explicitly enabled by every benchmark worker. Show live FPS/TPS during warm-up and measurement and during loading where supported; record the effective setting and include overlay work in measured costs.
- Expose opt-in, application-neutral engine scheduling and telemetry mechanisms: monotonic CPU timings, asynchronous Vulkan GPU timestamps, waits, successful submissions/present calls, and simulation snapshot identity.
- Export raw samples and per-repetition summaries with build, hardware, driver, scenario, and effective configuration metadata. Compare compatible saved results with variation-aware conclusions and explicit baseline selection.
- Replace the experimental scene CLI with the suite interface; no compatibility obligation for the initial flags. Keep benchmark policy in the application and shared engine mechanisms generic.
- Use a unique result directory under the user's XDG data home (falling back to `~/.local/share`) and the shared `build/benchmark-assets` cache, without public output or cache path overrides; retain `--source-directory` for offline preparation.
- Keep smoke as a correctness-only protocol; treat asset-loading performance measurement as a separate future benchmark.
- Add an optional `--workload CASE_ID` filter that selects one existing case from the chosen suite without changing its metrics, effective settings, or per-case baseline compatibility; reject unknown and wrong-suite cases before workers or asset preparation.

## Capabilities

### New Capabilities

- `runtime-performance-observation`: Optional engine scheduling controls and trustworthy CPU/GPU/frame observation across independent simulation and graphics loops.
- `performance-benchmarking`: Unattended deterministic workload execution, persisted evidence, and compatible same-machine comparisons owned by `stress_test`.

### Modified Capabilities

None. The existing resource-path-resolution capability remains unchanged; benchmark result and asset locations use application-owned defaults.

## Impact

Touches `applications/stress_test`, application build registration, engine loop scheduling, render-frame metadata, Vulkan frame submission/query lifecycle, graphics configuration and observation interfaces, relevant tests, and `docs/PERFORMANCE.md`. Adds application-owned preparation and local caching of the two third-party asset packages, with required attribution and derived-asset provenance. No external application dependencies, ray-tracing work, automatic machine reconfiguration, or modifications to the user's `build/release` workflow are included. Agents build and validate only in `build/debug`; performance reports distinguish Debug/validation runs from optimised runs. Initial comparisons are advisory rather than automatic regression gates; actual display scanout timing and cross-machine ranking are out of scope.

Approved tangent correction: the pinned Sponza source contains 39 tangents parallel to their normals (37 in decals_1st_floor, two in lamps_2ndfloor_02). Preparation replaces only these exact pinned tangent vectors with the approved deterministic perpendicular unit vectors, preserving handedness and all positions, normals, indices and textures. The lock records every before/after value and derived buffer checksum; any source mismatch fails preparation.
