# Design

## Context

See proposal.md for motivation and the two capability deltas for the behavioural contract. Inspection found:

- `applications/stress_test/stress_test.cpp` creates a shared-mesh grid but changes camera distance, floor size, and light intensity with count. Motion integrates wall-clock delta and execution never terminates automatically.
- `GameEngine::run` and `GraphicsEngine::run` each use a hardcoded 17 ms `Utility::LoopSleeper`. They run independently; `RenderFrameMailbox` is latest-wins. Frame numbers already connect publications and accepted snapshots.
- `Analytics` uses `system_clock` and rolling mean/deviation/min/max. Graphics loop timing includes draw calls and waits; it is not GPU execution time.
- `GraphicsEngineFrame::draw` contains fence waits, command recording, image acquisition, uniform updates, submission and presentation. `update_command_buffer` records the graphics work. These are existing observation seams; there are no timestamp queries.
- Physical-device properties already expose `timestampPeriod`, but queue-family discovery discards `timestampValidBits`. Present mode is hardcoded to prefer MAILBOX over FIFO. Window dimensions are currently fixed at startup; swapchain extent is authoritative and dynamic resize handling is incomplete.
- Startup already waits for the completed initial scene frame to be presented. Shutdown joins graphics and eventually waits for device idle. Debug builds enable Vulkan validation. Existing tests include mock graphics, render-frame, recording-session and graphics-buffer coverage.

## Goals / Non-Goals

**Goals:** Keep benchmark policy outside the engine; retain production thread/mailbox behaviour; make every reported metric traceable to raw observations and effective configuration; automate the full run without requiring system configuration changes.

**Non-Goals:** Display scanout measurement, cross-machine scores, an automatic regression gate, thermal/power control, arbitrary scene editing, new ray-tracing paths, dynamic resolution scaling, a general profiler UI, or lockstep capture. A visible real GPU/window session is required for representative results; software/headless smoke runs establish correctness only.

## Decisions

### 1. One executable with a coordinator and isolated workers

`stress_test run --suite standard [--workload CASE_ID] [--baseline PATH]` is the public entry point. Omitting `--workload` runs every case in the selected suite; specifying it selects exactly one existing case. The filter does not alter that case's metrics, effective settings, or per-case baseline compatibility. Reject unknown IDs and IDs belonging to the other suite during preflight, before asset preparation or worker launch. Each invocation writes to a unique run directory under `$XDG_DATA_HOME/krisp-engine/stress-test`, falling back to `~/.local/share/krisp-engine/stress-test` when `XDG_DATA_HOME` is unset. The shared prepared-asset cache remains `build/benchmark-assets`. The public coordinator has no output-directory or asset-cache override. Its private worker subcommand accepts internal `--output` and `--asset-cache` arguments, and the gateway's hidden `--default-asset-cache` argument remains internal; none are user-facing CLI options. A private worker runs exactly one case/repetition and never launches more children. The coordinator itself does not construct an engine. It starts one worker at a time, checks bounded readiness and completion records, enforces a watchdog, and writes a final manifest/report even when workers fail. Use a small application-owned process adapter for the current Linux runtime rather than introducing a distributed runner or putting process policy in engine code.

Store each worker's logs and artifacts separately. Write completion files atomically, check worker exit status as well as the completion record, and never overwrite an existing run directory. On interrupt, terminate/reap the active child and preserve completed output. Operational failure returns nonzero; a reported slowdown does not. Unsupported GPU timing produces a valid partial CPU-only result, not an operational failure.

The default `standard` protocol runs `static-objects-500`, `moving-transforms-500`, `point-shadows-500`, `textured-objects-16`, and `textured-environment-atrium` in throughput and paced modes with two repetitions. Two seconds of warm-up and five seconds of measurement yield 20 fresh workers and 140 seconds of configured timed work. Selecting one standard workload preserves both modes, both repetitions, and the default timings, yielding four workers. Sponza startup is hardware-dependent; the default standard suite has no under-five-minute completion guarantee. Print the configured timed-work estimate and progress automatically. Keep `smoke` as a separate correctness-only protocol, never comparable with standard. Smoke retains `static-objects-100`, `textured-objects-1`, and `textured-environment-atrium` in both modes, with one repetition, 0.2 seconds of warm-up, and 0.5 seconds of measurement. Permit explicit timing/repetition overrides and record them in the compatibility key. Default startup and final-collection timeouts are 120 and 30 seconds; the coordinator bounds total worker time using those limits plus configured warm-up/measurement and a shutdown margin. Asset-loading performance measurement is separate future work; current metrics exclude preparation, loading and GPU upload.

Use a fixed shuffled case order per repetition round to spread drift across cases while making the schedule reproducible; record its seed and order. Sequential execution avoids benchmark workers contending with each other. No automatic baseline discovery/promotion: omitting `--baseline` produces a standalone report.

Alternative: running all cases in one engine would be faster to start but would retain GPU allocations, pipeline caches, and application state between cases. Fresh workers make isolation and failure recovery simpler.

### 2. Versioned scene definitions with controlled differences

Define three application-owned scenarios: `static-objects`, `moving-transforms`, and `point-shadows`. All share one cube mesh, eight shared PBR materials, a fixed floor, one light, 1280x720 framebuffer target, fixed exposure, and the same fixed perspective camera. Use the existing rasterization/PBR path with explicitly resolved settings, no physics, editor overlays, GUI panels, screenshots, or video recording. These three diagnostic cases use generated geometry; the textured cases below import pinned real assets. Keep engine-default environment assets fixed and include their content identity in the workload fingerprint.

Use a fixed 32x32 world-space lattice sized for the maximum workload. Select a deterministic, spatially distributed prefix of lattice cells for each count; never reframe the camera or rescale lighting/floor to fit a count. Static objects have a fixed pose. Moving transforms rotate all cubes using a direct function of the scenario simulation-tick index, not accumulated wall-clock delta. Shadows use the static scene with the same light's shadow flag enabled. Record generated object/renderable counts and verify them before warm-up, including that the full intended grid is within the camera frustum.

Version scene semantics, workload assets, suite protocol and result schema separately. A change in intended workload invalidates old comparisons even if the display name remains the same. An optimisation to engine execution is not itself a scenario-version change.

Add two named asset-backed scenarios to complement the generated geometry:

- `textured-objects`: Flight Helmet at 1, 16 and 64 complete model instances. Share imported mesh/material/texture handles rather than reimporting each instance. Use a fixed spatial layout, fixed camera/framing, light and texture resolution across those counts. Each instance retains the model's multiple mesh parts and PBR materials; report instances separately from actual renderable/draw/triangle counts. The workload is static so texture/material/geometry costs can be compared without transform motion as another changing factor.
- `textured-environment`: one complete Intel Sponza Base Scene, without optional add-on packages, at three separately identified stationary views: `atrium`, `corridor`, and `material-closeup`. Each view has pinned camera transforms/projection and fixed lighting in the scenario definition; do not manually navigate or use wall-time camera motion. Preserve the selected package's geometry and texture dimensions; record bounds, visible/total geometry, resource counts and texture memory estimates. Choose and visually verify concrete camera coordinates against the pinned imported scene during implementation, then version them. All views run at the same explicit framebuffer/settings as other cases.

The default routine workloads are the three diagnostic cube scenarios at 500 objects, `textured-objects` at 16 instances, and `textured-environment-atrium`. The other two Sponza views, `textured-environment-corridor` and `textured-environment-material-closeup`, are optional standard selections. All use the pinned full-resolution scene and fixed view. Selecting one standard Sponza view runs both modes with two repetitions and produces four fresh workers. The atrium case also remains in smoke. Sponza startup is hardware-dependent, so the default standard suite has no under-five-minute completion guarantee.

These are representative PBR geometry/texture workloads, not full material-fidelity tests. Exact prepared asset identity, material profile and view are part of the case compatibility key. No automatic mesh decimation, material merging, texture resizing or workload reduction is allowed to make a run fit a machine. Capacity failures are explicit. Reuse existing asset registration/acquisition/import paths; asset names and preparation rules stay in the application.

Alternative: replacing the cubes entirely would lose useful diagnostic isolation. Arbitrary scene selection would weaken repeatability. Skeletal animation, physics and dedicated transparency remain future suites.

### Asset acquisition and reproducible preparation

Provide application-owned `stress_test prepare-assets` for manual preparation and let `stress_test run` automatically prepare assets when its cache directory is missing, before workers or warm-up. The default cache is `build/benchmark-assets`, shared by Debug and Release builds; neither public command accepts a cache-directory override. Preparation downloads the two exact pinned sources unless verified local source archives/files are supplied for offline preparation. It verifies SHA-256 checksums, extracts only safe relative paths into staging, applies a deterministic versioned preparation recipe, verifies the outputs and atomically installs an immutable cache entry. It is idempotent for an already verified cache and never silently refreshes to upstream latest. A present but damaged or incompatible cache fails closed rather than being overwritten. Keep large binaries outside Git; commit only manifests, attribution and preparation recipes. Asset preparation and downloads are not part of the engine build, tests or measured execution.

The checked-in manifest records source URL, upstream revision/package identity, source and every dependency checksum, included source licence/attribution, conversion tool/version/options, explicit modifications and derived checksums. Preserve original licence and attribution documents alongside prepared content; record the exact Intel package terms and Flight Helmet model/metadocumentation terms rather than assuming the hosting repository has one licence. The acquisition source references are [Intel GPU Research Samples](https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html) for Sponza Base Scene (glTF included) and [Khronos Flight Helmet](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/FlightHelmet) for the uncompressed glTF variant. Pin actual package hashes and a concrete repository commit during implementation; no placeholder/latest asset is a valid standard benchmark input.

The current importer rejects material extensions and separate occlusion textures (`resource_loader_material.ipp`), while current Flight Helmet declares transmission. Define a named, versioned `engine-pbr-opaque` derived profile: retain all intended mesh parts and base-colour, normal and metallic/roughness texture bindings; remove separate occlusion bindings and transmission declarations, representing transmissive surfaces as explicitly opaque materials. Preserve original packed texture data and geometry, and list every changed material and binding. This measures the supported PBR path, not original transmission/AO fidelity. The approved Sponza recipe also normalises only material `light_bulb` emissive component 2 from the pinned source value `1.00000012` to `1.0`, retaining the other components `[1.0, 0.99999994]`. This exact exporter-rounding adjustment is recorded in the derived material deltas; any other out-of-range value fails preparation. Apply only explicitly enumerated transformations; unsupported features outside the recipe fail preparation instead of being dropped silently. Do not weaken importer validation or introduce transmission/AO shader support as part of this change.

Preparation validates all referenced local dependencies, source/prepared geometry and material counts, texture bindings/resolutions, aggregate vertex/index/resource capacities and a visual reference for each scenario. It must demonstrate that no intended geometry or supported PBR maps were lost. Resource-limit issues are diagnosed before measurement; any necessary engine capacity improvement must be application-neutral and preserve the exact workload. Preparation is outside timing and cannot substitute cheaper assets per machine.

`run` completes any needed preparation before launching measurement workers. A missing cache triggers preparation; `--source-directory` supplies pinned source files for offline preparation, while the default acquisition path downloads the pinned sources. A present cache is verified and reused without preparation; altered, incompatible or unsupported assets produce a bounded preflight failure. Neither performance suite silently drops its textured workload. The three-case smoke suite retains one diagnostic case, one Flight Helmet case and one Sponza view in both modes; pure unit tests use neutral synthetic fixtures and do not download gigabytes of assets.

### 3. Engine-owned runtime controls with explicit benchmark overrides

Add a generic runtime-options seam passed before engine/window/graphics construction, covering initial window extent, independent loop pacing, fixed simulation timestep, presentation preference, UI/input policy and optional telemetry. Leave ordinary consumer defaults unchanged. Benchmark workers supply complete explicit settings in memory and do not read or rewrite saved interactive settings as workload policy.

Add `RuntimeOptions.performance_overlay`, defaulting to false for ordinary
consumers. Benchmark workers always set it true so live FPS/TPS remain visible
through warm-up and measurement and on loading screens where available. This
overlay is a narrow diagnostic display: benchmark workers keep editor panels,
application UI and input/capture controls disabled. Record
`effective_settings.performance_overlay=true` in worker results so runs with
different overlay settings are incompatible. Overlay update and draw work is
part of the measured CPU and GPU costs and is never subtracted. GUI rates are
diagnostic only; persisted raw observations and their summaries remain the
reported benchmark metrics.

For benchmark simulation, execute one fixed 1/60-second update at each scheduled tick. Use `steady_clock` absolute deadlines at high precision. On overload, advance to the next future deadline and record missed opportunities rather than running an unbounded catch-up burst. Tick index advances only for actual simulation updates, so poses are repeatable at an index while achieved ticks/second reveals overload. The existing fixed-step physics mechanism is not replaced by this scheduling policy.

Throughput rendering bypasses the engine frame limiter, requests IMMEDIATE presentation first, then MAILBOX, then FIFO. Record requested/effective mode and mark MAILBOX/FIFO results as presentation-constrained. Even IMMEDIATE can face compositor/driver limits; describe it as engine-uncapped, not proof of isolated GPU capacity. Paced rendering targets exactly 60 Hz, prefers MAILBOX then FIFO, and records its effective mode. Both modes preserve independent threads and latest-wins snapshots; do not borrow the recording session's handshake or readback machinery.

Benchmark camera movement, editor toggles and gameplay shortcuts are disabled; OS close remains an abort. Monitor effective framebuffer extent, focus and iconification. Unsupported requested extent fails preflight; changes during measurement invalidate the repetition. Configure startup dimensions rather than adding runtime resize/recreation support to this change.

Alternative: merely disabling sleep would leave swapchain throttling hidden. Running simulation uncapped would change CPU contention and the amount of work being benchmarked, so simulation retains its explicit 60 Hz target in both modes.

### 4. Optional bounded engine observation

Introduce an engine-owned observation interface with documented value records and no application/report dependencies. Use bounded, preallocated per-producer buffers for simulation and graphics. Writes do not perform disk I/O or allocate per sample. Lost records and overflow are explicit. A drain consumer outside measured work persists batches; it must not hold simulation/UI/graphics locks. Disabled observation creates neither sample buffers nor GPU queries and adds no new cross-thread synchronisation on the hot path.

CPU records use `steady_clock` timestamps and explicit units. Record simulation start/end including snapshot construction/publication; graphics iteration and active-work intervals; frame-fence and image-fence waits; acquire, submit and present call intervals; pacing deadlines and sleep intervals. Publish an interval hierarchy so parent durations are never summed with their children. Active work excludes measured waiting APIs and pacing but remains CPU elapsed time, not CPU utilisation or pure instruction execution time.

Every graphics attempt has a unique ID separate from the simulation snapshot number. A successful submission records that snapshot number, publication time and simulation-tick index. Failed acquire/submit/present attempts have status records and do not inflate successful throughput counters. Store monotonic present-call completion timestamps for cadence; do not call these display timestamps. First accepted measured snapshot and subsequent ID gaps permit repeated/skipped counts. Throughput naturally repeats snapshots above 60 FPS; reports describe this rather than automatically classifying repeats as stutter.

### 5. GPU timestamps follow existing frame-resource ownership

Retain the selected graphics queue family's `timestampValidBits` and read the physical device's `timestampPeriod`. Enable queries only for usable support. Own a timestamp pair and its pending submission metadata with each in-flight graphics frame resource. Reset and write timestamps in its command buffer, bracketing the complete graphics command region, including shadow and presentation rendering. Disable capture and benchmark UI to keep the region consistent. The duration is a graphics-command span that can include device-side stalls; it is not solely shader arithmetic or end-to-end latency.

After the frame's existing completion fence signals, retrieve its previous timestamp results with availability information, without WAIT or an additional measurement fence. Associate results with the original submission before resetting/reusing queries. Convert using the supported counter width and period; reject ambiguous or invalid durations. Keep ownership correct across frame moves, swapchain destruction, and device shutdown.

When measurement stops, retain measured submissions until their query results can be drained under the bounded final-collection deadline. Completion waiting here is outside the measurement window; never add per-frame device-idle or queue-idle calls. Shutdown may use its existing device-idle path to release remaining resources. Unsupported timestamps yield explicit partial coverage; unexplained missing results on supported hardware invalidate the GPU metric.

Alternative: timing `draw()` with a CPU clock would mix queueing and waits with CPU work. Blocking query reads would change the workload under test. Per-pass GPU profiling is deferred until the total-span observations are reliable.

### 6. Phase membership is explicit across threads

Worker lifecycle:

```text
Preflight --> Load --> Ready --> Warm-up --> Measure --> Drain --> Complete
                                  |            |          |
                                  +------------+----------+--> Invalid/Failed
```

Ready means the existing initial-scene presentation acknowledgement has completed and intended scene/configuration validation has passed, including all imported meshes, supported texture maps and live GPU resources. Download, decode, import, GPU upload and pipeline warm-up remain outside the measured window. The FPS/TPS performance overlay is visible during warm-up and measurement, and during loading where the loading screen supports it; its updates and drawing remain inside measured frame costs. Establish a shared monotonic measurement interval after warm-up. Reset the scenario's measured tick index at its first measured simulation update; tag snapshot publications with tick, publication time and phase. Do not synchronise each graphics frame to that tick.

Count work samples whose full CPU interval lies in the measurement window; frame/GPU statistics require a submission in that window using a measured-phase snapshot. Keep boundary samples in raw data with exclusion reasons. Present-call cadence intervals require both endpoints in the window. Report coverage/counts for each metric instead of assuming one game tick corresponds to one GPU sample. Delayed GPU retrieval retains its submission's phase. Startup, warm-up, drain and shutdown have separate durations and never enter measured percentiles.

### 7. Results are evidence, not one score

Use a versioned JSON manifest, per-worker JSON metadata/summaries, and batched JSONL raw records, plus a Markdown report. Reuse existing serialization dependencies where practical. All durations carry explicit units. Raw records retain IDs, phase/exclusion flags, interval boundaries, statuses and GPU availability.

For each repetition, calculate achieved simulation ticks/second, successful graphics submissions/second and successful present calls/second over the measurement wall interval; report median, p95 and p99 CPU work, GPU spans, waits and present-call intervals separately. Use the documented nearest-rank percentile method. Missing/unsupported samples are null with a reason, not zero.

For paced cases, record target deadlines and lateness for both loops. Report strict positive lateness and a separate missed-deadline count with a default 1 ms tolerance above the scheduled deadline. Record the tolerance as part of protocol compatibility. Skipped schedule slots are missed deadlines even when no iteration exists for that slot. Present-call intervals exceeding 16.666667 ms plus that tolerance are a separate cadence metric, not a count of physical display misses. Report snapshot age and repeated/skipped snapshot counts alongside cadence.

Compare per-repetition summaries, not pooled frames treated as independent experiments. Show all five repetition values, median, range and median absolute deviation. For each chosen primary metric, use a deterministic 10,000-resample bootstrap of independent repetition summaries for a 95% interval on the median candidate-minus-baseline difference. Report direction only when that interval excludes zero; otherwise label inconclusive. Also show absolute/percentage changes and flag low-sample runs. This is advisory empirical uncertainty, not a guarantee against environmental bias or a multiple-comparison-corrected regression gate. Do not collapse CPU, GPU and cadence into one score.

### 8. Provenance and compatibility are explicit

Capture build-time source revision, dirty state and source-content fingerprint covering relevant tracked and untracked source/build inputs; associate these with the built binary rather than assuming the current working tree produced it. Record executable/runtime shader and asset identities plus compiler, build type, optimisation/assertion/validation state and dependency configuration. If provenance cannot be established, flag it unknown and exclude automatic directional comparisons. Commit/source fingerprints may differ across comparisons; scenario assets and protocol fingerprints must match.

The machine compatibility key includes a local machine identity, CPU/GPU identity, selected GPU, OS/kernel and driver. The case key includes scenario/version/count or fixed view, exact prepared asset/profile/recipe hashes, mode, resolution, all effective graphics settings, presentation mode, simulation/pacing policy, instrumentation version, warm-up/measurement/repetitions and tolerance. Build class must match while source revisions may differ. Do not record a raw host name unnecessarily; use a stable local identifier. Fail comparisons closed on unknown compatibility fields; display a field-by-field mismatch explanation while preserving standalone results.

Preflight records accessible power/session information and background-load observations as context but never adjusts system-wide policy. Identical metadata cannot prove identical thermal or background conditions. Five independent repetitions and visible variation bound the claim; they do not eliminate this limitation. Existing user baseline files are read-only inputs. Selecting a candidate as a future baseline requires an explicit path/action outside automatic suite execution.

## Risks / Trade-offs

- Instrumentation perturbs workloads --> Keep opt-in buffers bounded and preallocated, avoid per-frame output or blocking readback, include instrumentation version in comparisons, and document remaining overhead in `docs/PERFORMANCE.md` without unmeasured claims.
- The diagnostic overlay adds work --> Keep it limited to live FPS/TPS, include its effective setting in compatibility, and retain its CPU/GPU work in measured timings without claiming an unmeasured adjustment.
- Compositor/driver throttling obscures throughput --> Request and record presentation modes; label constrained results and expose waits; do not present CPU submission rate as GPU-only capacity.
- Focus loss or background activity spoils runs --> Invalidate observable session changes, isolate workers, show repetition spread, and report uncertainty. Do not promise identical wall-clock results from deterministic scenes.
- Fresh processes still share OS/driver caches --> Treat this as a warmed steady-state benchmark, explicitly record warm-up and avoid cold-start claims.
- Two repetitions give limited uncertainty estimates --> Keep conclusions advisory, retain raw evidence, and permit explicit timing/repetition overrides without comparing mismatched protocols.
- Existing graphics errors can be caught before process exit --> Worker success requires a valid completion record and telemetry consistency, not just exit code zero.
- Routine suite target can be exceeded by worker startup, collection, or asset preparation --> Publish the timed-work estimate, state the prepared-asset assumption, save partial progress, and avoid interactive prompts.
- Benchmark GUI focus requirements can prevent unattended background runs --> Preflight and invalidation explain that a usable visible graphics session is required; avoid claiming a software/headless run represents hardware performance.

## Migration Plan

1. Add generic engine mechanisms with existing consumer defaults intact, then integrate the worker and coordinator.
2. Add verified asset preparation and replace the initial stress_test flags with `prepare-assets`, `run` and a private worker command; no legacy CLI or results-format compatibility is required.
3. Validate in `build/debug` using low-level tests and a short real-graphics smoke suite containing the prepared real assets. Leave `build/release` untouched; users can obtain optimised reports through their own build workflow.
4. Document reproducible invocation, output interpretation, failure/partial states and baseline compatibility. Establish a baseline only after reviewing initial valid runs; never manufacture performance numbers from correctness checks.
5. Rollback consists of disabling opt-in observation/overrides or reverting this feature; no persistent engine scene migration or machine configuration reversal is needed.

Approved tangent correction: the pinned Sponza source contains 39 tangents parallel to their normals (37 in decals_1st_floor, two in lamps_2ndfloor_02). Preparation replaces only these exact pinned tangent vectors with the approved deterministic perpendicular unit vectors, preserving handedness and all positions, normals, indices and textures. The lock records every before/after value and derived buffer checksum; any source mismatch fails preparation.
