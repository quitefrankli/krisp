# Tasks

## 1. Generic runtime scheduling and startup settings

- [x] 1.1 Add application-neutral runtime options before window/graphics construction for initial extent, pacing, presentation preference, UI/input policy and optional observation; verify existing mock-engine consumers and default construction retain their behaviour.
- [x] 1.2 Implement monotonic high-precision deadline pacing and opt-in fixed 1/60-second simulation steps, including skipped-slot accounting and bounded overload behaviour; verify exact 60 Hz period accumulation and overload/no-catch-up behaviour with an injected clock.
- [x] 1.3 Wire independent graphics pacing and requested/effective swapchain presentation selection, plus startup framebuffer extent verification; verify present-mode fallback with low-level selection tests and a real-graphics configuration smoke check.
- [x] 1.4 Document override semantics, deadline boundaries and ordinary-consumer defaults in engine documentation and docs/PERFORMANCE.md; verify the documented values match runtime metadata and scheduling tests.

## 2. CPU observation and snapshot attribution

- [x] 2.1 Add bounded, opt-in per-producer observation buffers and explicit lifecycle/overflow counters; verify disabled observation allocates no telemetry resources and overflow is observable in focused buffer tests.
- [x] 2.2 Instrument monotonic simulation/publication and graphics work, fence/acquire/submit/present and pacing intervals with documented parent/child boundaries; verify interval attribution and success/failure counters with synthetic observations and existing engine mock seams.
- [x] 2.3 Carry simulation-tick and publication timing metadata alongside snapshot IDs and distinct graphics submission IDs; verify repeats/skips and snapshot age through existing render-frame/mailbox tests without adding a frame handshake.
- [x] 2.4 Document record units, nesting, thread ownership and measurement overhead limits in docs/PERFORMANCE.md; verify sample records and documentation agree and that no frame producer performs report I/O.

## 3. GPU observation and effective graphics metadata

- [x] 3.1 Retain graphics-family timestamp capabilities and expose selected device/driver, effective graphics settings, extent and presentation metadata; verify unsupported capability handling and requested/effective distinction with low-level fixtures.
- [x] 3.2 Implement frame-owned timestamp query lifecycle, nonblocking completed-submission retrieval and bounded final draining; verify valid-bit wrap, timestamp-period conversion, delayed attribution, unsupported results and frame-resource ownership with focused tests and Vulkan validation on a short graphics run.
- [x] 3.3 Ensure timing failures and missing queries remain explicit and no extra per-frame wait/device-idle is introduced; verify missing-result behaviour and inspect an observation-enabled run for query reuse/lifetime validation errors.
- [x] 3.4 Document the GPU command-span boundaries, possible device stalls and distinction from CPU/presentation timing in docs/PERFORMANCE.md; verify the described bracket matches command recording.

## 4. Pinned benchmark asset preparation

- [x] 4.1 Define pinned source URLs, exact revisions, source-package checksums, licenses and preserved license/attribution files for Intel Sponza and Khronos Flight Helmet; verify the source records and SHA256 manifests identify the exact distributed inputs.
- [x] 4.2 Add the application-owned one-time `prepare-assets` command with safe staged extraction and validation, then populate an immutable local cache outside git; verify traversal/link protections, checksum failure handling, cache reuse and that benchmark `run` does not fetch or modify assets.
- [x] 4.3 Implement a deterministic engine-compatible PBR preparation profile and record converter/profile versions, source-to-derived deltas and derived SHA256 checksums; preserve geometry, base color, normal and metallic-roughness data, explicitly remove unsupported AO bindings and transmission, use opaque helmet lenses, apply only the approved Sponza light_bulb emissive component 1.00000012-to-1.0 normalisation with a pinned delta, repair only the 39 approved invalid source tangents with pinned before/after values while preserving geometry and textures, and fail on any other unsupported feature instead of silently downgrading it; verify the exact material deltas, retained texture bindings and rejection behaviour with neutral synthetic fixtures and prepared-asset inspection.
- [x] 4.4 Emit and validate a pinned asset manifest with expected mesh/material/texture/renderable counts and resource-memory audit data; verify mismatches, unsupported features and corrupted derived files fail preparation, and that source/license files remain preserved.
- [x] 4.5 Document asset preparation, cache location, compatibility limits and manifest interpretation in application documentation; verify all asset-specific policy remains in the owning application and no benchmark assets or policy enter engine code.

## 5. Controlled scenarios and worker lifecycle

- [x] 5.1 Define the three cube scenarios (`static-objects`, `moving-transforms`, `point-shadows`) at 100/500/1000 objects with a shared cube mesh, eight shared PBR materials, fixed camera/floor/light and explicit raster settings; verify counts, shared resources, full-grid framing and same-tick motion determinism at the lowest meaningful level.
- [x] 5.2 Add textured-object Flight Helmet cases at counts 1/16/64 using shared mesh/material handles; verify the manifest's expected renderable/resource counts, same-tick repeatability and that instances reuse prepared resources.
- [x] 5.3 Add the textured Intel Sponza base scene only, with fixed atrium, corridor and material-closeup views whose camera transforms are recorded in the pinned manifest, using the pinned package texture resolution without adaptive downgrade; verify view transforms and expected manifest counts, resource memory, and that no Sponza addons are loaded.
- [x] 5.4 Keep both asset workloads on the suite's explicit raster settings and 1280x720 target; verify object/view changes do not change unrelated effective settings and workload identity captures scenario, view and asset-manifest versions.
- [x] 5.5 Implement worker preflight/readiness/warm-up/measurement/drain/completion phases and shared phase metadata; verify startup and boundary exclusion, delayed measured GPU samples and deterministic measured tick reset with synthetic lifecycle tests.
- [x] 5.6 Disable benchmark interaction/capture and monitor focus, iconification, framebuffer extent, runtime settings and telemetry coverage; verify each observable disturbance produces an invalid reason and that OS close aborts cleanly.
- [x] 5.7 Add explicit worker completion/error artifacts and bounded startup/drain behaviour; verify normal completion, partial GPU support and missing-completion failures in short worker integration cases.
- [x] 5.8 Document scenario definitions, asset identities and worker validity rules in application documentation; verify generated worker metadata contains documented settings, asset identities and phase durations.

## 6. Raw results, statistics and build provenance

- [x] 6.1 Persist a versioned manifest, per-worker metadata/summary and batched raw JSONL with explicit units, phase membership and validity; verify serialization round trips and failed/partial cases without substituting missing values with zero.
- [x] 6.2 Implement per-repetition rates, nearest-rank median/p95/p99, snapshot reuse/skip/age metrics and 60 Hz deadline/cadence counts with recorded 1 ms tolerance; verify known synthetic distributions, skipped slots, failed submissions and boundary intervals.
- [x] 6.3 Embed build-associated revision/dirty source-content identity and collect binary, dependency, shader/asset, preparation-profile/converter version, source/derived asset checksums, build/validation, machine/device/driver and protocol metadata; verify a dirty source build is distinguishable from its commit and runtime tree changes are not falsely attributed to the binary.
- [x] 6.4 Document result fields, metric boundaries, build-class interpretation, asset provenance and evidence limitations in application documentation and docs/PERFORMANCE.md; verify a generated report clearly separates CPU work, GPU spans, waits and CPU-side present cadence.

## 7. Unattended coordinator

- [x] 7.1 Implement the public run CLI, application-owned Linux child-process adapter and private one-case worker command, with unique output directories and no interactive prompts; verify subprocess argument handling, output collision rejection and standalone execution.
- [x] 7.2 Define standard and smoke protocol schedules, deterministic shuffled case order, progress/estimated duration and fresh sequential workers; verify repeatable order and that smoke/overrides have distinct protocol identity. The former longer schedule was superseded by task 10.2.
- [x] 7.3 Require standard benchmark runs to verify all pinned asset manifest checksums and compatibility before launching workers, and fail clearly on incompatible prepared assets; verify cache validation and preflight failure starts no workers.
- [x] 7.4 Add watchdogs, failure isolation, atomic completion checks, interrupt handling and child reaping while preserving finished artifacts; verify with controlled short-lived, failing and hanging child fixtures rather than timing long GPU runs.
- [x] 7.5 Document one-command invocation, asset preparation, default output paths, protocol estimate, preflight prerequisites and exit status semantics; verify the documented smoke command exits unattended and preserves a final report.
- [x] 7.6 Default the shared Debug/Release asset cache to `build/benchmark-assets`; have `run` prepare a missing cache before workers or warm-up, accept `--source-directory` for offline pinned inputs, and reject corrupt existing caches. Verify manual and automatic preparation use the same default and that no worker starts before preparation succeeds.

## 8. Compatible baseline comparison and reporting

- [x] 8.1 Implement explicit read-only baseline loading and case compatibility checks for machine, driver, protocol, scenario, build class, effective settings and prepared asset identity/checksums while allowing source revisions to differ; verify field-level mismatch explanations, unknown provenance exclusion and baseline preservation.
- [x] 8.2 Compare independent repetition summaries with median/range/MAD and the documented deterministic bootstrap interval; verify identical/noisy samples are inconclusive, clear synthetic changes have the expected direction, unavailable metrics remain unavailable and frame samples are not treated as independent runs.
- [x] 8.3 Generate readable reports showing complete/partial/invalid/failed cases, constrained presentation, Debug/validation labels, absolute/relative changes and uncertainty without an overall score or automatic regression exit code; verify report fixtures cover these externally observable distinctions.
- [x] 8.4 Document explicit baseline selection, compatibility keys including asset identity, the comparison method and environmental limitations; verify no-baseline and incompatible-baseline commands still produce standalone evidence without modifying the baseline.

## 9. Cross-component verification

- [x] 9.1 Follow the repository's Conan/Debug Meson reconfiguration sequence when build inputs change, then build sandbox and stress_test only under build/debug and run meson test -C build/debug -j 6; verify the existing suite and new meaningful tests pass with concise output, leaving build/release untouched.
- [x] 9.2 Run the unattended smoke protocol with diagnostic cubes, a prepared Flight Helmet and a prepared Sponza view on an available real graphics session in both modes, checking readiness, finite exit, sample coverage, GPU association, effective settings, prepared asset identities and report files; verify a second compatible smoke result can be compared and that this correctness run makes no standard-suite performance claim.
- [x] 9.3 Review implementation boundaries and diffs against both specs; verify engine code contains no scenario/baseline/report or asset-preparation policy, ray tracing remains disabled, unrelated user edits are preserved and docs/PERFORMANCE.md reflects the actual measurement overhead and limitations without invented timings.

## 10. Routine suite protocol

- [x] 10.1 (Historical initial protocol; superseded by task 12.1.) Make `standard` the initial default with `static-objects-500`, `moving-transforms-500`, `point-shadows-500`, and `textured-objects-16`, two modes, two repetitions, 2-second warm-up and 5-second measurement; verify that protocol had 16 workers and 112 seconds of timed work.
- [x] 10.2 Remove the former long suite protocol and its CLI selection; verify `standard` became the default short performance suite, `smoke` remained correctness-only, and Sponza assets and the fixed smoke view remained available.
- [x] 10.3 Keep `smoke` as correctness-only, default suite selection as `standard`, and require Release for standard while allowing Debug smoke; verify compatibility identity distinguishes the suites and smoke results cannot be interpreted as performance results.
- [x] 10.4 (Historical initial protocol; superseded by task 12.1.) Verify the initial four-workload standard run completed in 2m55s on a supported machine with prepared assets; this result does not describe the current default suite with Sponza.
- [x] 10.5 Keep asset loading excluded from current benchmark metrics and record a separate asset-loading benchmark as future work; verify standard metrics begin after loading and GPU upload.
- [x] 10.6 Add the opt-in, default-off engine FPS/TPS overlay setting and enable it for every benchmark worker; verify it remains visible through warm-up and measurement and loading where supported, effective settings distinguish it, and overlay CPU/GPU cost remains in measured work. At the time, the initial protocol had 16 workers and 112 seconds of timed work; task 12.1 adds Sponza to the default.

## 11. Fixed public paths

- [x] 11.1 Remove public `--output` and `--asset-cache` options from `run` and `prepare-assets`; use unique result directories under `$XDG_DATA_HOME/krisp-engine/stress-test` (falling back to `~/.local/share/krisp-engine/stress-test`) and the shared `build/benchmark-assets` cache. Verify public help/examples expose no path overrides, while private worker `--output`/`--asset-cache` and gateway `--default-asset-cache` remain internal and `--source-directory` remains available for offline sources.
- [x] 11.2 Add public `--workload CASE_ID` filtering for an existing case in the selected suite; default to all cases, preserve the selected case's modes, repetitions, timings, metrics, effective settings, and per-case baseline compatibility, and reject unknown or wrong-suite IDs before asset preparation or worker launch. Verify a selected standard case produces four workers at defaults and document all standard/smoke IDs.

## 12. Sponza standard workloads

- [x] 12.1 Include `textured-environment-atrium` in the standard default and allow `textured-environment-corridor` and `textured-environment-material-closeup` as optional standard workloads using pinned full-resolution assets and fixed views. Preserve the standard modes, timings and two repetitions: the default becomes 20 workers and 140 seconds of configured timed work; one selected view uses four workers. Emit per-view metadata/raw/report/baseline identity and remove the under-five-minute guarantee due to hardware-dependent Sponza startup. Verify the updated default and optional one-repetition/two-worker invocation.
